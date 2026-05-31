#include "GeminiProvider.h"

#include <sstream>
#include <utility>

#include "HttpClient.h"
#include "JsonUtil.h"

namespace {

std::string trimTrailingSlash(std::string value) {
  while (!value.empty() && value.back() == '/') {
    value.pop_back();
  }
  return value;
}

std::string buildPrompt(const ChatRequest &request) {
  if (request.system_prompt && !request.system_prompt->empty()) {
    return *request.system_prompt + "\n\nUser: " + request.message;
  }
  return request.message;
}

std::string buildGenerateContentJson(const ChatRequest &request) {
  const std::string prompt = buildPrompt(request);
  std::ostringstream body;
  body << "{"
       << "\"contents\":[{"
       << "\"parts\":[{"
       << "\"text\":\"" << escapeJsonString(prompt) << "\""
       << "}]"
       << "}]"
       << "}";
  return body.str();
}

ChatResult failure(std::string model, std::string errorMessage) {
  ChatResult result;
  result.answer.clear();
  result.provider = "gemini";
  result.model = std::move(model);
  result.success = false;
  result.error_message = std::move(errorMessage);
  return result;
}

}  // namespace

GeminiProvider::GeminiProvider(const GatewayConfig &config)
    : api_key_(config.gemini_api_key),
      model_(config.gemini_model),
      api_base_(trimTrailingSlash(config.gemini_api_base)),
      request_timeout_ms_(config.request_timeout_ms),
      enable_real_gemini_(config.enable_real_gemini) {}

std::string GeminiProvider::name() const { return "gemini"; }

ChatResult GeminiProvider::chat(const ChatRequest &request) {
  if (!api_key_) {
    return failure(model_, "GEMINI_API_KEY is not configured");
  }

  if (!enable_real_gemini_) {
    return failure(model_, "real Gemini HTTP call is disabled by config");
  }

  const std::string url = api_base_ + "/models/" + model_ +
                          ":generateContent?key=" + *api_key_;
  const std::string requestBody = buildGenerateContentJson(request);

  HttpClient client;
  HttpResponse response =
      client.postJson(url, requestBody, {}, request_timeout_ms_);

  if (!response.success) {
    std::ostringstream error;
    error << "Gemini HTTP request failed";
    if (response.status_code > 0) error << " (status " << response.status_code << ")";
    if (!response.error_message.empty()) error << ": " << response.error_message;
    return failure(model_, error.str());
  }

  auto answer = extractGeminiText(response.body);
  if (!answer || answer->empty()) {
    return failure(model_, "failed to parse Gemini response text");
  }

  ChatResult result;
  result.answer = *answer;
  result.provider = name();
  result.model = model_;
  result.success = true;
  return result;
}
