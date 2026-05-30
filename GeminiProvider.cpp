#include "GeminiProvider.h"

GeminiProvider::GeminiProvider(const GatewayConfig &config)
    : api_key_(config.gemini_api_key),
      request_timeout_ms_(config.request_timeout_ms) {}

std::string GeminiProvider::name() const { return "gemini"; }

ChatResult GeminiProvider::chat(const ChatRequest &request) {
  (void)request;

  ChatResult result;
  result.answer.clear();
  result.provider = name();
  result.model = "gemini-placeholder";
  result.success = false;

  if (!api_key_) {
    result.error_message = "GEMINI_API_KEY is not configured";
    return result;
  }

  (void)request_timeout_ms_;
  result.error_message = "Gemini HTTP call is not implemented in Stage 3";
  return result;
}
