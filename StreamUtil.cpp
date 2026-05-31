#include "StreamUtil.h"

#include <algorithm>
#include <sstream>

#include "JsonUtil.h"

namespace {

size_t utf8CodepointBytes(unsigned char lead) {
  if ((lead & 0x80) == 0) return 1;
  if ((lead & 0xE0) == 0xC0) return 2;
  if ((lead & 0xF0) == 0xE0) return 3;
  if ((lead & 0xF8) == 0xF0) return 4;
  return 1;
}

bool hasContinuationBytes(const std::string &text, size_t pos, size_t bytes) {
  if (pos + bytes > text.size()) return false;
  for (size_t i = 1; i < bytes; ++i) {
    const unsigned char ch = static_cast<unsigned char>(text[pos + i]);
    if ((ch & 0xC0) != 0x80) return false;
  }
  return true;
}

void appendProviderFields(std::ostringstream &event, const std::string &provider,
                          const std::string &model,
                          const std::optional<std::string> &fallbackFrom,
                          const std::optional<std::string> &fallbackTo) {
  event << ",\"provider\":\"" << escapeJsonString(provider) << "\""
        << ",\"model\":\"" << escapeJsonString(model) << "\"";
  if (fallbackFrom) {
    event << ",\"fallback_from\":\"" << escapeJsonString(*fallbackFrom)
          << "\"";
  }
  if (fallbackTo) {
    event << ",\"fallback_to\":\"" << escapeJsonString(*fallbackTo) << "\"";
  }
}

}  // namespace

std::vector<std::string> splitTextForStream(const std::string &text,
                                            size_t chunkSize) {
  std::vector<std::string> chunks;
  if (text.empty()) return chunks;

  const size_t maxChars = std::max<size_t>(1, chunkSize);
  size_t chunkStart = 0;
  size_t charsInChunk = 0;

  for (size_t pos = 0; pos < text.size();) {
    size_t bytes = utf8CodepointBytes(static_cast<unsigned char>(text[pos]));
    if (!hasContinuationBytes(text, pos, bytes)) bytes = 1;

    pos += bytes;
    ++charsInChunk;

    if (charsInChunk >= maxChars) {
      chunks.push_back(text.substr(chunkStart, pos - chunkStart));
      chunkStart = pos;
      charsInChunk = 0;
    }
  }

  if (chunkStart < text.size()) {
    chunks.push_back(text.substr(chunkStart));
  }
  return chunks;
}

std::string buildSseDataEvent(const std::string &jsonPayload) {
  return "data: " + jsonPayload + "\n\n";
}

StreamBuildResult buildStreamFromAnswer(
    const std::string &id, const std::string &object,
    const std::string &provider, const std::string &model,
    const std::string &answer, size_t chunkSize,
    const std::optional<std::string> &fallbackFrom,
    const std::optional<std::string> &fallbackTo,
    const std::string &metadataJsonFields) {
  StreamBuildResult result;

  if (!metadataJsonFields.empty()) {
    std::ostringstream metadata;
    metadata << "{"
             << "\"id\":\"" << escapeJsonString(id) << "\","
             << "\"object\":\"" << escapeJsonString(object) << "\"";
    appendProviderFields(metadata, provider, model, fallbackFrom, fallbackTo);
    metadata << "," << metadataJsonFields << "}";
    result.body += buildSseDataEvent(metadata.str());
  }

  for (const auto &chunk : splitTextForStream(answer, chunkSize)) {
    std::ostringstream event;
    event << "{"
          << "\"id\":\"" << escapeJsonString(id) << "\","
          << "\"object\":\"" << escapeJsonString(object) << "\","
          << "\"delta\":\"" << escapeJsonString(chunk) << "\"";
    appendProviderFields(event, provider, model, fallbackFrom, fallbackTo);
    event << "}";
    result.body += buildSseDataEvent(event.str());
    ++result.chunks;
  }

  std::ostringstream done;
  done << "{"
       << "\"id\":\"" << escapeJsonString(id) << "\","
       << "\"object\":\"" << escapeJsonString(object) << "\","
       << "\"done\":true"
       << "}";
  result.body += buildSseDataEvent(done.str());
  return result;
}

StreamBuildResult buildStreamError(const std::string &id,
                                   const std::string &object,
                                   const std::string &errorMessage) {
  StreamBuildResult result;
  std::ostringstream error;
  error << "{"
        << "\"id\":\"" << escapeJsonString(id) << "\","
        << "\"object\":\"" << escapeJsonString(object) << "\","
        << "\"success\":false,"
        << "\"error_message\":\"" << escapeJsonString(errorMessage) << "\""
        << "}";
  result.body += buildSseDataEvent(error.str());

  std::ostringstream done;
  done << "{"
       << "\"id\":\"" << escapeJsonString(id) << "\","
       << "\"object\":\"" << escapeJsonString(object) << "\","
       << "\"done\":true"
       << "}";
  result.body += buildSseDataEvent(done.str());
  return result;
}
