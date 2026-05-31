#include "KnowledgeBase.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <sstream>
#include <string_view>

namespace {

constexpr size_t kMaxDocumentBytes = 1024 * 1024;

const std::vector<std::string> &importantTerms() {
  static const std::vector<std::string> terms{
      "rag",     "llm",      "agent",  "lightagent", "gateway",
      "provider", "mock",     "gemini", "ollama",     "qwen",
      "reactor", "webserver"};
  return terms;
}

bool isImportantTerm(const std::string &term) {
  const auto &terms = importantTerms();
  return std::find(terms.begin(), terms.end(), term) != terms.end();
}

bool startsWithAt(std::string_view input, size_t offset,
                  std::string_view needle) {
  return offset + needle.size() <= input.size() &&
         input.compare(offset, needle.size(), needle) == 0;
}

bool isChinesePunctuationAt(std::string_view input, size_t offset,
                            size_t *punctuationBytes) {
  static const std::vector<std::string_view> punctuations{
      "\xEF\xBC\x9F",  // ？
      "\xE3\x80\x82",  // 。
      "\xEF\xBC\x8C",  // ，
      "\xEF\xBC\x9A",  // ：
      "\xEF\xBC\x9B",  // ；
      "\xEF\xBC\x81",  // ！
      "\xEF\xBC\x88",  // （
      "\xEF\xBC\x89",  // ）
      "\xE3\x80\x8A",  // 《
      "\xE3\x80\x8B"   // 》
  };
  for (const auto punctuation : punctuations) {
    if (startsWithAt(input, offset, punctuation)) {
      *punctuationBytes = punctuation.size();
      return true;
    }
  }
  return false;
}

bool isAsciiPunctuationToDrop(unsigned char ch) {
  switch (ch) {
    case '?':
    case '.':
    case ',':
    case ':':
    case ';':
    case '!':
    case '"':
    case '\'':
    case '(':
    case ')':
      return true;
    default:
      return false;
  }
}

void appendSpaceIfNeeded(std::string *output) {
  if (!output->empty() && output->back() != ' ') output->push_back(' ');
}

std::string lowerAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  return value;
}

bool isSupportedFile(const std::filesystem::path &path) {
  const std::string ext = lowerAscii(path.extension().string());
  return ext == ".txt" || ext == ".md";
}

std::string readFileLimited(const std::filesystem::path &path) {
  std::error_code ec;
  const auto size = std::filesystem::file_size(path, ec);
  if (ec || size > kMaxDocumentBytes) return "";

  std::ifstream input(path, std::ios::binary);
  if (!input) return "";
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

}  // namespace

std::string normalizeForSearch(const std::string &input) {
  std::string normalized;
  normalized.reserve(input.size());

  for (size_t i = 0; i < input.size();) {
    const unsigned char ch = static_cast<unsigned char>(input[i]);
    size_t punctuationBytes = 0;
    if (isChinesePunctuationAt(input, i, &punctuationBytes)) {
      appendSpaceIfNeeded(&normalized);
      i += punctuationBytes;
      continue;
    }

    if (ch < 0x80) {
      if (std::isalnum(ch)) {
        normalized.push_back(static_cast<char>(std::tolower(ch)));
      } else if (std::isspace(ch) || isAsciiPunctuationToDrop(ch)) {
        appendSpaceIfNeeded(&normalized);
      } else {
        normalized.push_back(static_cast<char>(ch));
      }
      ++i;
      continue;
    }

    normalized.push_back(input[i]);
    ++i;
  }

  while (!normalized.empty() && normalized.front() == ' ') {
    normalized.erase(normalized.begin());
  }
  while (!normalized.empty() && normalized.back() == ' ') {
    normalized.pop_back();
  }
  return normalized;
}

std::vector<std::string> extractSearchTerms(const std::string &query) {
  std::set<std::string> deduped;
  const std::string normalizedQuery = normalizeForSearch(query);

  std::istringstream normalizedInput(normalizedQuery);
  std::string token;
  while (normalizedInput >> token) {
    if (token.size() > 1 || isImportantTerm(token)) deduped.insert(token);
  }

  std::string asciiRun;
  for (unsigned char ch : query) {
    if (std::isalnum(ch)) {
      asciiRun.push_back(static_cast<char>(std::tolower(ch)));
    } else if (!asciiRun.empty()) {
      if (asciiRun.size() > 1 || isImportantTerm(asciiRun)) {
        deduped.insert(asciiRun);
      }
      asciiRun.clear();
    }
  }
  if (!asciiRun.empty() &&
      (asciiRun.size() > 1 || isImportantTerm(asciiRun))) {
    deduped.insert(asciiRun);
  }

  for (const auto &term : importantTerms()) {
    if (normalizedQuery.find(term) != std::string::npos) {
      deduped.insert(term);
    }
  }

  return {deduped.begin(), deduped.end()};
}

KnowledgeBase::KnowledgeBase(const GatewayConfig &config) : config_(config) {}

bool KnowledgeBase::load() {
  chunks_.clear();
  last_error_.clear();

  std::error_code ec;
  if (!std::filesystem::exists(config_.knowledge_base_dir, ec)) {
    std::filesystem::create_directories(config_.knowledge_base_dir, ec);
    if (ec) {
      last_error_ = "failed to create knowledge_base directory";
      return false;
    }
    return true;
  }

  if (!std::filesystem::is_directory(config_.knowledge_base_dir, ec)) {
    last_error_ = "knowledge_base path is not a directory";
    return false;
  }

  const int chunkSize = std::max(100, config_.rag_chunk_size);
  for (const auto &entry :
       std::filesystem::recursive_directory_iterator(config_.knowledge_base_dir,
                                                     ec)) {
    if (ec) break;
    if (!entry.is_regular_file(ec) || !isSupportedFile(entry.path())) continue;

    const std::string content = readFileLimited(entry.path());
    if (content.empty()) continue;

    for (size_t offset = 0; offset < content.size();
         offset += static_cast<size_t>(chunkSize)) {
      DocumentChunk chunk;
      chunk.file_path = entry.path().generic_string();
      chunk.title = entry.path().filename().string();
      chunk.content = content.substr(offset, static_cast<size_t>(chunkSize));
      chunk.start_offset = offset;
      chunks_.push_back(std::move(chunk));
    }
  }

  if (ec) {
    last_error_ = "failed to scan knowledge_base directory";
    return false;
  }
  return true;
}

std::vector<DocumentChunk> KnowledgeBase::search(const std::string &query,
                                                 int top_k) const {
  std::vector<DocumentChunk> hits;
  const std::string normalizedQuery = normalizeForSearch(query);
  const std::vector<std::string> terms = extractSearchTerms(query);

  for (const auto &chunk : chunks_) {
    DocumentChunk scored = chunk;
    const std::string normalizedContent = normalizeForSearch(chunk.content);
    const std::string normalizedPath = normalizeForSearch(chunk.file_path);
    const std::string normalizedTitle = normalizeForSearch(chunk.title);

    if (!normalizedQuery.empty() &&
        normalizedContent.find(normalizedQuery) != std::string::npos) {
      scored.score += 10;
    }

    for (const auto &term : terms) {
      if (term.empty()) continue;
      const bool important = isImportantTerm(term);
      if (normalizedContent.find(term) != std::string::npos) {
        scored.score += important ? 4 : 2;
      }
      if (normalizedTitle.find(term) != std::string::npos) {
        scored.score += important ? 3 : 1;
      }
      if (normalizedPath.find(term) != std::string::npos) {
        scored.score += important ? 2 : 1;
      }
    }

    if (scored.score > 0) hits.push_back(std::move(scored));
  }

  std::stable_sort(hits.begin(), hits.end(), [](const auto &lhs,
                                                const auto &rhs) {
    if (lhs.score != rhs.score) return lhs.score > rhs.score;
    if (lhs.file_path != rhs.file_path) return lhs.file_path < rhs.file_path;
    return lhs.start_offset < rhs.start_offset;
  });

  if (top_k > 0 && static_cast<int>(hits.size()) > top_k) {
    hits.resize(static_cast<size_t>(top_k));
  }
  return hits;
}

std::string KnowledgeBase::lastError() const { return last_error_; }
