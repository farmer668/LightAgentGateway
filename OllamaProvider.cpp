#include "OllamaProvider.h"

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

std::string buildGenerateJson(const ChatRequest &request,
                              const std::string &model) {
  std::ostringstream body;
  body << "{"
       << "\"model\":\"" << escapeJsonString(model) << "\","
       << "\"prompt\":\"" << escapeJsonString(buildPrompt(request)) << "\","
       << "\"stream\":false"
       << "}";
  return body.str();
}

ChatResult failure(std::string model, std::string errorMessage) {
  ChatResult result;
  result.answer.clear();
  result.provider = "ollama";
  result.model = std::move(model);
  result.success = false;
  result.error_message = std::move(errorMessage);
  return result;
}

}  // namespace

OllamaProvider::OllamaProvider(const GatewayConfig &config)
    : base_url_(trimTrailingSlash(config.ollama_base_url)),
      model_(config.ollama_model),
      request_timeout_ms_(config.ollama_request_timeout_ms > 0
                              ? config.ollama_request_timeout_ms
                              : config.request_timeout_ms),
      enable_real_ollama_(config.enable_real_ollama) {}

std::string OllamaProvider::name() const { return "ollama"; }

ChatResult OllamaProvider::chat(const ChatRequest &request) {
  if (!enable_real_ollama_) {
    return failure(model_, "real Ollama HTTP call is disabled by config");
  }

  const std::string url = base_url_ + "/api/generate";
  const std::string requestBody = buildGenerateJson(request, model_);

  HttpClient client;
  HttpResponse response =
      client.postJson(url, requestBody, {}, request_timeout_ms_);

  if (!response.success) {
    std::ostringstream error;
    error << "Ollama request failed"
          << " (base_url=" << base_url_ << ", model=" << model_
          << ", timeout_ms=" << request_timeout_ms_;
    if (response.status_code > 0) error << ", status=" << response.status_code;
    error << ")";
    if (!response.error_message.empty()) error << ": " << response.error_message;
    return failure(model_, error.str());
  }

  auto answer = extractOllamaText(response.body);
  if (!answer || answer->empty()) {
    std::ostringstream error;
    error << "failed to parse Ollama response field 'response'"
          << " (base_url=" << base_url_ << ", model=" << model_
          << ", timeout_ms=" << request_timeout_ms_
          << ", status=" << response.status_code << ")";
    return failure(model_, error.str());
  }

  ChatResult result;
  result.answer = *answer;
  result.provider = name();
  result.model = model_;
  result.success = true;
  return result;
}
