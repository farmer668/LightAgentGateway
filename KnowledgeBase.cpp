#include "KnowledgeBase.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace {

constexpr size_t kMaxDocumentBytes = 1024 * 1024;

std::string lowerCopy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  return value;
}

std::vector<std::string> tokenize(const std::string &query) {
  std::vector<std::string> tokens;
  std::istringstream input(lowerCopy(query));
  std::string token;
  while (input >> token) {
    if (!token.empty()) tokens.push_back(token);
  }
  if (tokens.empty() && !query.empty()) tokens.push_back(lowerCopy(query));
  return tokens;
}

bool isSupportedFile(const std::filesystem::path &path) {
  const std::string ext = lowerCopy(path.extension().string());
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
  const std::string loweredQuery = lowerCopy(query);
  const std::vector<std::string> tokens = tokenize(query);

  for (const auto &chunk : chunks_) {
    DocumentChunk scored = chunk;
    const std::string loweredContent = lowerCopy(chunk.content);

    if (!loweredQuery.empty() &&
        loweredContent.find(loweredQuery) != std::string::npos) {
      scored.score += 5;
    }
    for (const auto &token : tokens) {
      if (!token.empty() && loweredContent.find(token) != std::string::npos) {
        scored.score += 1;
      }
    }
    if (scored.score > 0) hits.push_back(std::move(scored));
  }

  std::sort(hits.begin(), hits.end(), [](const auto &lhs, const auto &rhs) {
    if (lhs.score != rhs.score) return lhs.score > rhs.score;
    return lhs.file_path < rhs.file_path;
  });

  if (top_k > 0 && static_cast<int>(hits.size()) > top_k) {
    hits.resize(static_cast<size_t>(top_k));
  }
  return hits;
}

std::string KnowledgeBase::lastError() const { return last_error_; }
