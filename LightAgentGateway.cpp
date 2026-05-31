#include "LightAgentGateway.h"

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <utility>

#include "GatewayConfig.h"
#include "JsonUtil.h"
#include "ProviderFactory.h"

namespace {

using Clock = std::chrono::steady_clock;

struct GatewayMetrics {
  Clock::time_point started_at = Clock::now();
  std::atomic<unsigned long long> total_requests{0};
  std::atomic<unsigned long long> chat_requests{0};
  std::atomic<unsigned long long> health_requests{0};
  std::atomic<unsigned long long> metrics_requests{0};
  std::atomic<long long> last_chat_latency_ms{0};
  std::atomic<bool> last_chat_success{false};
  std::mutex last_provider_mutex;
  std::string last_provider = "none";
  std::string last_chat_error;
};

GatewayMetrics &metricsState() {
  static GatewayMetrics metrics;
  return metrics;
}

const GatewayConfig &gatewayConfig() {
  static const GatewayConfig config = GatewayConfig::load();
  return config;
}

bool startsWith(std::string_view value, std::string_view prefix) {
  return value.size() >= prefix.size() &&
         value.compare(0, prefix.size(), prefix) == 0;
}

LightAgentGateway::Response jsonResponse(int statusCode, std::string reason,
                                         std::string body) {
  return {statusCode, std::move(reason), "application/json; charset=utf-8",
          std::move(body)};
}

LightAgentGateway::Response methodNotAllowed(std::string_view allow) {
  std::ostringstream body;
  body << "{"
       << "\"error\":\"method_not_allowed\","
       << "\"allow\":\"" << allow << "\""
       << "}";
  return jsonResponse(405, "Method Not Allowed", body.str());
}

void setLastChatStatus(const std::string &provider, std::string error) {
  auto &metrics = metricsState();
  std::lock_guard<std::mutex> lock(metrics.last_provider_mutex);
  metrics.last_provider = provider;
  metrics.last_chat_error = std::move(error);
}

std::string getLastProvider() {
  auto &metrics = metricsState();
  std::lock_guard<std::mutex> lock(metrics.last_provider_mutex);
  return metrics.last_provider;
}

std::string getLastChatError() {
  auto &metrics = metricsState();
  std::lock_guard<std::mutex> lock(metrics.last_provider_mutex);
  return metrics.last_chat_error;
}

}  // namespace

std::optional<LightAgentGateway::Response> LightAgentGateway::handle(
    const Request &request) {
  if (!startsWith(request.path, "/api/")) {
    return std::nullopt;
  }

  auto &state = metricsState();
  ++state.total_requests;

  if (request.path == "/api/health") {
    ++state.health_requests;
    if (request.method != Method::Get && request.method != Method::Head) {
      return methodNotAllowed("GET, HEAD");
    }
    return health();
  }

  if (request.path == "/api/metrics") {
    ++state.metrics_requests;
    if (request.method != Method::Get && request.method != Method::Head) {
      return methodNotAllowed("GET, HEAD");
    }
    return metrics();
  }

  if (request.path == "/api/chat") {
    ++state.chat_requests;
    if (request.method != Method::Post) {
      return methodNotAllowed("POST");
    }
    return chat(request);
  }

  return jsonResponse(404, "Not Found",
                      "{\"error\":\"api_route_not_found\"}");
}

LightAgentGateway::Response LightAgentGateway::health() {
  const GatewayConfig &config = gatewayConfig();

  std::ostringstream body;
  body << "{"
       << "\"status\":\"ok\","
       << "\"service\":\"" << escapeJsonString(config.service_name) << "\","
       << "\"version\":\"" << escapeJsonString(config.version) << "\","
       << "\"stage\":\"" << escapeJsonString(config.stage) << "\","
       << "\"provider\":\"" << escapeJsonString(config.default_provider) << "\","
       << "\"gemini_api_key_configured\":"
       << (config.gemini_api_key.has_value() ? "true" : "false") << ","
       << "\"gemini_model\":\"" << escapeJsonString(config.gemini_model)
       << "\","
       << "\"gemini_api_base\":\"" << escapeJsonString(config.gemini_api_base)
       << "\","
       << "\"fallback_provider\":\"" << escapeJsonString(config.fallback_provider)
       << "\","
       << "\"ollama_base_url\":\"" << escapeJsonString(config.ollama_base_url)
       << "\","
       << "\"static_root\":\""
       << escapeJsonString(config.static_root.generic_string()) << "\","
       << "\"request_timeout_ms\":" << config.request_timeout_ms << ","
       << "\"enable_real_gemini\":"
       << (config.enable_real_gemini ? "true" : "false")
       << "}";
  return jsonResponse(200, "OK", body.str());
}

LightAgentGateway::Response LightAgentGateway::metrics() {
  const auto &config = gatewayConfig();
  auto &metrics = metricsState();
  const auto uptime =
      std::chrono::duration_cast<std::chrono::seconds>(Clock::now() -
                                                       metrics.started_at)
          .count();
  const std::map<std::string, unsigned long long> counters{
      {"total_requests", metrics.total_requests.load()},
      {"api_requests_total", metrics.total_requests.load()},
      {"chat_requests", metrics.chat_requests.load()},
      {"chat_requests_total", metrics.chat_requests.load()},
      {"health_requests", metrics.health_requests.load()},
      {"metrics_requests", metrics.metrics_requests.load()},
  };

  std::ostringstream body;
  body << "{"
       << "\"service\":\"" << escapeJsonString(config.service_name) << "\","
       << "\"version\":\"" << escapeJsonString(config.version) << "\","
       << "\"stage\":\"" << escapeJsonString(config.stage) << "\","
       << "\"uptime_seconds\":" << uptime;
  for (const auto &[name, value] : counters) {
    body << ",\"" << name << "\":" << value;
  }
  body << ",\"last_provider\":\"" << escapeJsonString(getLastProvider()) << "\"";
  body << ",\"last_chat_latency_ms\":"
       << metrics.last_chat_latency_ms.load();
  body << ",\"last_chat_success\":"
       << (metrics.last_chat_success.load() ? "true" : "false");
  body << ",\"last_chat_error\":\""
       << escapeJsonString(getLastChatError()) << "\"";
  body << "}";
  return jsonResponse(200, "OK", body.str());
}

LightAgentGateway::Response LightAgentGateway::chat(const Request &request) {
  auto &metrics = metricsState();
  const auto started = Clock::now();

  auto message = extractJsonStringField(request.body, "message");
  ChatRequest chatRequest =
      makeChatRequest(message.value_or(""),
                      extractJsonStringField(request.body, "session_id"),
                      extractJsonStringField(request.body, "system_prompt"));

  ChatResult result;
  if (!message || message->empty()) {
    result.answer.clear();
    result.provider = gatewayConfig().default_provider;
    result.model = "";
    result.success = false;
    result.error_message = "message field is required";
  } else {
    std::unique_ptr<ILlmProvider> provider =
        ProviderFactory::create(gatewayConfig());
    result = provider->chat(chatRequest);
  }

  const auto latencyMs =
      std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() -
                                                            started)
          .count();
  metrics.last_chat_latency_ms = latencyMs;
  metrics.last_chat_success = result.success;
  setLastChatStatus(result.provider, result.error_message.value_or(""));

  const int statusCode =
      result.success
          ? 200
          : (result.error_message == "message field is required" ? 400 : 503);
  return jsonResponse(statusCode,
                      result.success
                          ? "OK"
                          : (statusCode == 400 ? "Bad Request"
                                               : "Service Unavailable"),
                      buildChatJson("lightagent-chat-1", chatRequest, result));
}
