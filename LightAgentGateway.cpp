#include "LightAgentGateway.h"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <utility>
#include <vector>

#include "GatewayConfig.h"
#include "JsonUtil.h"
#include "ProviderFactory.h"
#include "RagEngine.h"

namespace {

using Clock = std::chrono::steady_clock;

struct GatewayMetrics {
  Clock::time_point started_at = Clock::now();
  std::atomic<unsigned long long> total_requests{0};
  std::atomic<unsigned long long> chat_requests{0};
  std::atomic<unsigned long long> rag_requests{0};
  std::atomic<unsigned long long> health_requests{0};
  std::atomic<unsigned long long> metrics_requests{0};
  std::atomic<unsigned long long> fallback_count{0};
  std::atomic<long long> last_chat_latency_ms{0};
  std::atomic<bool> last_chat_success{false};
  std::mutex last_provider_mutex;
  std::string last_provider = "none";
  std::string last_chat_error;
  long long last_rag_latency_ms = 0;
  bool last_rag_success = false;
  std::string last_rag_error;
  int last_rag_chunks = 0;
  std::string last_rag_provider;
  std::string last_fallback_from;
  std::string last_fallback_to;
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

std::string lowerCopy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  return value;
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

void setLastChatStatus(const std::string &provider, std::string error,
                       std::string fallbackFrom, std::string fallbackTo) {
  auto &metrics = metricsState();
  std::lock_guard<std::mutex> lock(metrics.last_provider_mutex);
  metrics.last_provider = provider;
  metrics.last_chat_error = std::move(error);
  metrics.last_fallback_from = std::move(fallbackFrom);
  metrics.last_fallback_to = std::move(fallbackTo);
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

std::string getLastFallbackFrom() {
  auto &metrics = metricsState();
  std::lock_guard<std::mutex> lock(metrics.last_provider_mutex);
  return metrics.last_fallback_from;
}

std::string getLastFallbackTo() {
  auto &metrics = metricsState();
  std::lock_guard<std::mutex> lock(metrics.last_provider_mutex);
  return metrics.last_fallback_to;
}

void setLastRagStatus(const RagResult &result) {
  auto &metrics = metricsState();
  std::lock_guard<std::mutex> lock(metrics.last_provider_mutex);
  metrics.last_rag_latency_ms = result.latency_ms;
  metrics.last_rag_success = result.success;
  metrics.last_rag_error = result.error_message;
  metrics.last_rag_chunks = static_cast<int>(result.retrieved_chunks.size());
  metrics.last_rag_provider = result.provider;
  if (result.fallback_from) metrics.last_fallback_from = *result.fallback_from;
  if (result.fallback_to) metrics.last_fallback_to = *result.fallback_to;
}

long long getLastRagLatencyMs() {
  auto &metrics = metricsState();
  std::lock_guard<std::mutex> lock(metrics.last_provider_mutex);
  return metrics.last_rag_latency_ms;
}

bool getLastRagSuccess() {
  auto &metrics = metricsState();
  std::lock_guard<std::mutex> lock(metrics.last_provider_mutex);
  return metrics.last_rag_success;
}

std::string getLastRagError() {
  auto &metrics = metricsState();
  std::lock_guard<std::mutex> lock(metrics.last_provider_mutex);
  return metrics.last_rag_error;
}

int getLastRagChunks() {
  auto &metrics = metricsState();
  std::lock_guard<std::mutex> lock(metrics.last_provider_mutex);
  return metrics.last_rag_chunks;
}

std::string getLastRagProvider() {
  auto &metrics = metricsState();
  std::lock_guard<std::mutex> lock(metrics.last_provider_mutex);
  return metrics.last_rag_provider;
}

bool shouldFallbackToOllama(const GatewayConfig &config,
                            const ChatResult &result) {
  return !result.success && config.enable_ollama_fallback &&
         lowerCopy(config.default_provider) == "gemini" &&
         lowerCopy(config.fallback_provider) == "ollama";
}

ChatResult applyOllamaFallbackIfNeeded(const GatewayConfig &config,
                                       const ChatRequest &request,
                                       const ChatResult &primaryResult) {
  if (!shouldFallbackToOllama(config, primaryResult)) return primaryResult;

  ++metricsState().fallback_count;
  std::unique_ptr<ILlmProvider> fallbackProvider =
      ProviderFactory::create(config, "ollama");
  ChatResult fallbackResult = fallbackProvider->chat(request);
  fallbackResult.fallback_from = "gemini";
  fallbackResult.fallback_to = "ollama";

  if (!fallbackResult.success) {
    const std::string primaryError =
        primaryResult.error_message.value_or("primary provider failed");
    const std::string fallbackError =
        fallbackResult.error_message.value_or("fallback provider failed");
    fallbackResult.error_message =
        "Gemini failed: " + primaryError +
        "; Ollama fallback failed: " + fallbackError;
  }
  return fallbackResult;
}

int clampTopK(int value) { return std::max(1, std::min(10, value)); }

std::string chunkPreview(std::string content) {
  constexpr size_t kMaxChunkChars = 500;
  if (content.size() > kMaxChunkChars) content.resize(kMaxChunkChars);
  return content;
}

std::string buildRagJson(const RagResult &result) {
  std::ostringstream body;
  body << "{"
       << "\"id\":\"lightagent-rag-1\","
       << "\"object\":\"rag.query\","
       << "\"success\":" << (result.success ? "true" : "false") << ","
       << "\"question\":\"" << escapeJsonString(result.question) << "\","
       << "\"answer\":\"" << escapeJsonString(result.answer) << "\","
       << "\"provider\":\"" << escapeJsonString(result.provider) << "\","
       << "\"model\":\"" << escapeJsonString(result.model) << "\","
       << "\"top_k\":" << result.top_k << ","
       << "\"retrieved_chunks\":[";
  for (size_t i = 0; i < result.retrieved_chunks.size(); ++i) {
    const auto &chunk = result.retrieved_chunks[i];
    if (i > 0) body << ",";
    body << "{"
         << "\"file_path\":\"" << escapeJsonString(chunk.file_path) << "\","
         << "\"title\":\"" << escapeJsonString(chunk.title) << "\","
         << "\"score\":" << chunk.score << ","
         << "\"start_offset\":" << chunk.start_offset << ","
         << "\"content\":\"" << escapeJsonString(chunkPreview(chunk.content))
         << "\""
         << "}";
  }
  body << "]";
  if (result.fallback_from) {
    body << ",\"fallback_from\":\"" << escapeJsonString(*result.fallback_from)
         << "\"";
  }
  if (result.fallback_to) {
    body << ",\"fallback_to\":\"" << escapeJsonString(*result.fallback_to)
         << "\"";
  }
  body << ",\"latency_ms\":" << result.latency_ms;
  if (!result.error_message.empty()) {
    body << ",\"error_message\":\"" << escapeJsonString(result.error_message)
         << "\"";
  }
  body << "}";
  return body.str();
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

  if (request.path == "/api/rag/query") {
    ++state.rag_requests;
    if (request.method != Method::Post) {
      return methodNotAllowed("POST");
    }
    return ragQuery(request);
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
       << "\"ollama_model\":\"" << escapeJsonString(config.ollama_model)
       << "\","
       << "\"knowledge_base_dir\":\""
       << escapeJsonString(config.knowledge_base_dir.generic_string())
       << "\","
       << "\"rag_top_k\":" << config.rag_top_k << ","
       << "\"rag_chunk_size\":" << config.rag_chunk_size << ","
       << "\"rag_enable_llm_answer\":"
       << (config.rag_enable_llm_answer ? "true" : "false") << ","
       << "\"rag_max_context_chars\":" << config.rag_max_context_chars << ","
       << "\"static_root\":\""
       << escapeJsonString(config.static_root.generic_string()) << "\","
       << "\"request_timeout_ms\":" << config.request_timeout_ms << ","
       << "\"enable_real_gemini\":"
       << (config.enable_real_gemini ? "true" : "false") << ","
       << "\"enable_real_ollama\":"
       << (config.enable_real_ollama ? "true" : "false") << ","
       << "\"enable_ollama_fallback\":"
       << (config.enable_ollama_fallback ? "true" : "false")
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
      {"rag_requests_total", metrics.rag_requests.load()},
      {"health_requests", metrics.health_requests.load()},
      {"metrics_requests", metrics.metrics_requests.load()},
      {"fallback_count", metrics.fallback_count.load()},
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
  body << ",\"last_rag_latency_ms\":" << getLastRagLatencyMs();
  body << ",\"last_rag_success\":"
       << (getLastRagSuccess() ? "true" : "false");
  body << ",\"last_rag_error\":\"" << escapeJsonString(getLastRagError())
       << "\"";
  body << ",\"last_rag_chunks\":" << getLastRagChunks();
  body << ",\"last_rag_provider\":\""
       << escapeJsonString(getLastRagProvider()) << "\"";
  body << ",\"last_fallback_from\":\""
       << escapeJsonString(getLastFallbackFrom()) << "\"";
  body << ",\"last_fallback_to\":\""
       << escapeJsonString(getLastFallbackTo()) << "\"";
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
    const GatewayConfig &config = gatewayConfig();
    std::unique_ptr<ILlmProvider> provider = ProviderFactory::create(config);
    result = applyOllamaFallbackIfNeeded(config, chatRequest,
                                         provider->chat(chatRequest));
  }

  const auto latencyMs =
      std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() -
                                                            started)
          .count();
  metrics.last_chat_latency_ms = latencyMs;
  metrics.last_chat_success = result.success;
  setLastChatStatus(result.provider, result.error_message.value_or(""),
                    result.fallback_from.value_or(""),
                    result.fallback_to.value_or(""));

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

LightAgentGateway::Response LightAgentGateway::ragQuery(
    const Request &request) {
  auto question = extractJsonStringField(request.body, "question");
  if (!question || question->empty()) {
    question = extractJsonStringField(request.body, "message");
  }

  if (!question || question->empty()) {
    RagResult result;
    result.question = "";
    result.error_message = "question field is required";
    result.latency_ms = 0;
    setLastRagStatus(result);
    return jsonResponse(400, "Bad Request", buildRagJson(result));
  }

  const GatewayConfig &config = gatewayConfig();
  const int topK =
      clampTopK(extractJsonIntField(request.body, "top_k").value_or(
          config.rag_top_k));

  RagEngine engine(config);
  RagResult result = engine.query(*question, topK);
  if (result.fallback_from) ++metricsState().fallback_count;
  setLastRagStatus(result);

  const int statusCode = result.success ? 200 : 503;
  return jsonResponse(statusCode,
                      result.success ? "OK" : "Service Unavailable",
                      buildRagJson(result));
}
