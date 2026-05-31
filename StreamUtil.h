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
    const std::string &metadataJsonFields = "",
    const std::string &streamMode = "pseudo",
    const std::string &upstreamStreamMode = "");
StreamBuildResult buildStreamFromDeltas(
    const std::string &id, const std::string &object,
    const std::string &provider, const std::string &model,
    const std::vector<std::string> &deltas,
    const std::optional<std::string> &fallbackFrom = std::nullopt,
    const std::optional<std::string> &fallbackTo = std::nullopt,
    const std::string &metadataJsonFields = "",
    const std::string &streamMode = "upstream_real",
    const std::string &upstreamStreamMode = "ollama_stream_true");
StreamBuildResult buildStreamError(const std::string &id,
                                   const std::string &object,
                                   const std::string &errorMessage);
