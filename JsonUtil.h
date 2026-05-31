#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "LlmProvider.h"

std::string escapeJsonString(std::string_view input);
std::optional<std::string> extractJsonStringField(std::string_view body,
                                                  std::string_view field);
std::optional<std::string> extractGeminiText(std::string_view responseBody);
std::optional<std::string> extractOllamaText(std::string_view responseBody);
std::string buildErrorJson(std::string_view errorMessage);
std::string buildChatJson(std::string_view id, const ChatRequest &request,
                          const ChatResult &result);
