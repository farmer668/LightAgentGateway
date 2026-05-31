#pragma once

#include <optional>
#include <string>
#include <vector>

struct StreamBuildResult {
  std::string body;
  size_t chunks = 0;
};

std::vector<std::string> splitTextForStream(const std::string &text,
                                            size_t chunkSize);
std::string buildSseDataEvent(const std::string &jsonPayload);
StreamBuildResult buildStreamFromAnswer(
    const std::string &id, const std::string &object,
    const std::string &provider, const std::string &model,
    const std::string &answer, size_t chunkSize,
    const std::optional<std::string> &fallbackFrom = std::nullopt,
    const std::optional<std::string> &fallbackTo = std::nullopt,
    const std::string &metadataJsonFields = "");
StreamBuildResult buildStreamError(const std::string &id,
                                   const std::string &object,
                                   const std::string &errorMessage);
