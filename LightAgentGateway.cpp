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
#include "OllamaProvider.h"
#include "ProviderFactory.h"
#include "RagEngine.h"
#include "StreamUtil.h"

namespace {

using Clock = std::chrono::steady_clock;

struct GatewayMetrics {
  Clock::time_point started_at = Clock::now();
  std::atomic<unsigned long long> total_requests{0};
  std::atomic<unsigned long long> chat_requests{0};
  std::atomic<unsigned long long> rag_requests{0};
  std::atomic<unsigned long long> chat_stream_requests{0};
  std::atomic<unsigned long long> rag_stream_requests{0};
  std::atomic<unsigned long long> health_requests{0};
  std::atomic<unsigned long long> metrics_requests{0};
  std::atomic<unsigned long long> fallback_count{0};
  std::atomic<long long> last_chat_latency_ms{0};
  std::atomic<bool> last_chat_success{false};
  std::atomic<long long> last_stream_latency_ms{0};
  std::atomic<bool> last_stream_success{false};
  std::mutex last_provider_mutex;
  std::string last_provider = "none";
  std::string last_chat_error;
  long long last_rag_latency_ms = 0;
  bool last_rag_success = false;
  std::string last_rag_error;
  int last_rag_chunks = 0;
  std::string last_rag_provider;
  std::string last_stream_error;
  std::string last_stream_provider;
  int last_stream_chunks = 0;
  std::string last_stream_type;
  std::string last_stream_mode;
  std::string last_upstream_stream_mode;
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

LightAgentGateway::Response streamResponse(int statusCode, std::string reason,
                                           std::string body) {
  return {statusCode, std::move(reason), "text/event-stream; charset=utf-8",
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

void setLastStreamStatus(std::string type, const std::string &provider,
                         bool success, std::string error, int chunks,
                         long long latencyMs, std::string streamMode,
                         std::string upstreamStreamMode,
                         std::string fallbackFrom = "",
                         std::string fallbackTo = "") {
  auto &metrics = metricsState();
  metrics.last_stream_latency_ms = latencyMs;
  metrics.last_stream_success = success;
  std::lock_guard<std::mutex> lock(metrics.last_provider_mutex);
  metrics.last_stream_type = std::move(type);
  metrics.last_stream_provider = provider;
  metrics.last_stream_error = std::move(error);
  metrics.last_stream_chunks = chunks;
  metrics.last_stream_mode = std::move(streamMode);
  metrics.last_upstream_stream_mode = std::move(upstreamStreamMode);
  if (!fallbackFrom.empty()) metrics.last_fallback_from = std::move(fallbackFrom);
  if (!fallbackTo.empty()) metrics.last_fallback_to = std::move(fallbackTo);
}

std::string getLastStreamError() {
  auto &metrics = metricsState();
  std::lock_guard<std::mutex> lock(metrics.last_provider_mutex);
  return metrics.last_stream_error;
}

std::string getLastStreamProvider() {
  auto &metrics = metricsState();
  std::lock_guard<std::mutex> lock(metrics.last_provider_mutex);
  return metrics.last_stream_provider;
}

int getLastStreamChunks() {
  auto &metrics = metricsState();
  std::lock_guard<std::mutex> lock(metrics.last_provider_mutex);
  return metrics.last_stream_chunks;
}

std::string getLastStreamType() {
  auto &metrics = metricsState();
  std::lock_guard<std::mutex> lock(metrics.last_provider_mutex);
  return metrics.last_stream_type;
}

std::string getLastStreamMode() {
  auto &metrics = metricsState();
  std::lock_guard<std::mutex> lock(metrics.last_provider_mutex);
  return metrics.last_stream_mode;
}

std::string getLastUpstreamStreamMode() {
  auto &metrics = metricsState();
  std::lock_guard<std::mutex> lock(metrics.last_provider_mutex);
  return metrics.last_upstream_stream_mode;
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

size_t streamChunkSize(const GatewayConfig &config) {
  return static_cast<size_t>(std::max(1, config.stream_chunk_size));
}

std::string providerForRagRequest(const GatewayConfig &config) {
  if (!config.rag_provider.empty()) return config.rag_provider;
  return config.default_provider;
}

bool isOllamaStreamEnabled(const GatewayConfig &config) {
  return config.stream_enabled && config.enable_real_ollama &&
         config.ollama_stream;
}

std::string joinDeltas(const std::vector<std::string> &deltas) {
  std::ostringstream joined;
  for (const auto &delta : deltas) joined << delta;
  return joined.str();
}

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

struct ChatExecution {
  ChatRequest request;
  ChatResult result;
  long long latency_ms = 0;
  bool bad_request = false;
};

struct RagExecution {
  RagResult result;
  bool bad_request = false;
};

struct ChatStreamExecution {
  ChatRequest request;
  ChatResult pseudo_result;
  ChatStreamResult stream_result;
  bool bad_request = false;
  bool used_upstream_stream = false;
  bool fallback_used = false;
  std::optional<std::string> fallback_from;
  std::optional<std::string> fallback_to;
  long long latency_ms = 0;
};

struct RagStreamExecution {
  RagResult rag_result;
  ChatStreamResult stream_result;
  bool bad_request = false;
  bool used_upstream_stream = false;
  long long latency_ms = 0;
};

ChatExecution executeChatInternal(std::string_view requestBody) {
  const auto started = Clock::now();
  ChatExecution execution;

  auto message = extractJsonStringField(requestBody, "message");
  execution.request =
      makeChatRequest(message.value_or(""),
                      extractJsonStringField(requestBody, "session_id"),
                      extractJsonStringField(requestBody, "system_prompt"));

  if (!message || message->empty()) {
    execution.bad_request = true;
    execution.result.answer.clear();
    execution.result.provider = gatewayConfig().default_provider;
    execution.result.model = "";
    execution.result.success = false;
    execution.result.error_message = "message field is required";
  } else {
    const GatewayConfig &config = gatewayConfig();
    std::unique_ptr<ILlmProvider> provider = ProviderFactory::create(config);
    execution.result = applyOllamaFallbackIfNeeded(
        config, execution.request, provider->chat(execution.request));
  }

  execution.latency_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() -
                                                            started)
          .count();
  return execution;
}

RagExecution executeRagInternal(std::string_view requestBody) {
  RagExecution execution;
  auto question = extractJsonStringField(requestBody, "question");
  if (!question || question->empty()) {
    question = extractJsonStringField(requestBody, "message");
  }

  if (!question || question->empty()) {
    execution.bad_request = true;
    execution.result.question = "";
    execution.result.error_message = "question field is required";
    execution.result.latency_ms = 0;
    return execution;
  }

  const GatewayConfig &config = gatewayConfig();
  const int topK =
      clampTopK(extractJsonIntField(requestBody, "top_k").value_or(
          config.rag_top_k));

  RagEngine engine(config);
  execution.result = engine.query(*question, topK);
  if (execution.result.fallback_from) ++metricsState().fallback_count;
  return execution;
}

ChatStreamExecution executeChatStreamInternal(std::string_view requestBody) {
  const auto started = Clock::now();
  ChatStreamExecution execution;

  auto message = extractJsonStringField(requestBody, "message");
  execution.request =
      makeChatRequest(message.value_or(""),
                      extractJsonStringField(requestBody, "session_id"),
                      extractJsonStringField(requestBody, "system_prompt"));

  const GatewayConfig &config = gatewayConfig();
  if (!message || message->empty()) {
    execution.bad_request = true;
    execution.pseudo_result.provider = config.default_provider;
    execution.pseudo_result.success = false;
    execution.pseudo_result.error_message = "message field is required";
  } else if (lowerCopy(config.default_provider) == "ollama" &&
             isOllamaStreamEnabled(config)) {
    OllamaProvider ollama(config);
    execution.stream_result = ollama.streamChat(execution.request);
    execution.used_upstream_stream = true;
  } else if (lowerCopy(config.default_provider) == "gemini" &&
             isOllamaStreamEnabled(config)) {
    std::unique_ptr<ILlmProvider> provider = ProviderFactory::create(config);
    ChatResult primary = provider->chat(execution.request);
    if (primary.success || !shouldFallbackToOllama(config, primary)) {
      execution.pseudo_result = primary;
    } else {
      ++metricsState().fallback_count;
      OllamaProvider ollama(config);
      execution.stream_result = ollama.streamChat(execution.request);
      execution.used_upstream_stream = true;
      execution.fallback_used = true;
      execution.fallback_from = "gemini";
      execution.fallback_to = "ollama";
      if (!execution.stream_result.success) {
        execution.stream_result.error_message =
            "Gemini failed: " +
            primary.error_message.value_or("primary provider failed") +
            "; Ollama stream fallback failed: " +
            execution.stream_result.error_message;
      }
    }
  } else {
    ChatExecution pseudo = executeChatInternal(requestBody);
    execution.request = pseudo.request;
    execution.pseudo_result = pseudo.result;
    execution.bad_request = pseudo.bad_request;
  }

  execution.latency_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() -
                                                            started)
          .count();
  return execution;
}

RagStreamExecution executeRagStreamInternal(std::string_view requestBody) {
  const auto started = Clock::now();
  RagStreamExecution execution;

  auto question = extractJsonStringField(requestBody, "question");
  if (!question || question->empty()) {
    question = extractJsonStringField(requestBody, "message");
  }

  const GatewayConfig &config = gatewayConfig();
  if (!question || question->empty()) {
    execution.bad_request = true;
    execution.rag_result.question = "";
    execution.rag_result.error_message = "question field is required";
    execution.rag_result.latency_ms = 0;
  } else if ((lowerCopy(providerForRagRequest(config)) == "ollama" ||
              lowerCopy(providerForRagRequest(config)) == "gemini") &&
             isOllamaStreamEnabled(config)) {
    const int topK =
        clampTopK(extractJsonIntField(requestBody, "top_k").value_or(
            config.rag_top_k));
    RagEngine engine(config);
    RagPreparedRequest prepared = engine.prepareRequest(*question, topK);
    execution.rag_result = prepared.result;

    if (!prepared.ready) {
      // Keep the retrieval error in rag_result.
    } else if (lowerCopy(providerForRagRequest(config)) == "ollama") {
      OllamaProvider ollama(config);
      execution.stream_result = ollama.streamChat(prepared.chat_request);
      execution.used_upstream_stream = true;
      execution.rag_result.provider = execution.stream_result.provider;
      execution.rag_result.model = execution.stream_result.model;
      execution.rag_result.success = execution.stream_result.success;
      execution.rag_result.answer = joinDeltas(execution.stream_result.deltas);
      execution.rag_result.error_message = execution.stream_result.error_message;
    } else {
      std::unique_ptr<ILlmProvider> provider =
          ProviderFactory::create(config, "gemini");
      ChatResult primary = provider->chat(prepared.chat_request);
      if (primary.success || !shouldFallbackToOllama(config, primary)) {
        execution.rag_result.success = primary.success;
        execution.rag_result.answer = primary.answer;
        execution.rag_result.provider = primary.provider;
        execution.rag_result.model = primary.model;
        execution.rag_result.error_message = primary.error_message.value_or("");
      } else {
        ++metricsState().fallback_count;
        OllamaProvider ollama(config);
        execution.stream_result = ollama.streamChat(prepared.chat_request);
        execution.used_upstream_stream = true;
        execution.rag_result.provider = execution.stream_result.provider;
        execution.rag_result.model = execution.stream_result.model;
        execution.rag_result.success = execution.stream_result.success;
        execution.rag_result.answer = joinDeltas(execution.stream_result.deltas);
        execution.rag_result.fallback_from = "gemini";
        execution.rag_result.fallback_to = "ollama";
        if (!execution.stream_result.success) {
          execution.rag_result.error_message =
              "Gemini failed: " +
              primary.error_message.value_or("primary provider failed") +
              "; Ollama stream fallback failed: " +
              execution.stream_result.error_message;
          execution.stream_result.error_message =
              execution.rag_result.error_message;
        }
      }
    }
  } else {
    RagExecution pseudo = executeRagInternal(requestBody);
    execution.rag_result = pseudo.result;
    execution.bad_request = pseudo.bad_request;
  }

  execution.latency_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() -
                                                            started)
          .count();
  execution.rag_result.latency_ms = execution.latency_ms;
  return execution;
}

int chatStatusCode(const ChatExecution &execution) {
  if (execution.result.success) return 200;
  return execution.bad_request ? 400 : 503;
}

std::string chatReason(const ChatExecution &execution) {
  const int status = chatStatusCode(execution);
  if (status == 200) return "OK";
  return status == 400 ? "Bad Request" : "Service Unavailable";
}

int ragStatusCode(const RagExecution &execution) {
  if (execution.result.success) return 200;
  return execution.bad_request ? 400 : 503;
}

std::string ragReason(const RagExecution &execution) {
  const int status = ragStatusCode(execution);
  if (status == 200) return "OK";
  return status == 400 ? "Bad Request" : "Service Unavailable";
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

  if (request.path == "/api/chat/stream") {
    ++state.chat_stream_requests;
    if (request.method != Method::Post) {
      return methodNotAllowed("POST");
    }
    return chatStream(request);
  }

  if (request.path == "/api/chat") {
    ++state.chat_requests;
    if (request.method != Method::Post) {
      return methodNotAllowed("POST");
    }
    return chat(request);
  }

  if (request.path == "/api/rag/query/stream") {
    ++state.rag_stream_requests;
    if (request.method != Method::Post) {
      return methodNotAllowed("POST");
    }
    return ragQueryStream(request);
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
       << "\"stream_enabled\":"
       << (config.stream_enabled ? "true" : "false") << ","
       << "\"stream_mode\":\"" << escapeJsonString(config.stream_mode) << "\","
       << "\"stream_chunk_size\":" << config.stream_chunk_size << ","
       << "\"ollama_stream_supported\":true,"
       << "\"ollama_stream\":" << (config.ollama_stream ? "true" : "false")
       << ","
       << "\"static_root\":\""
       << escapeJsonString(config.static_root.generic_string()) << "\","
       << "\"request_timeout_ms\":" << config.request_timeout_ms << ","
       << "\"gemini_request_timeout_ms\":"
       << (config.gemini_request_timeout_ms > 0
               ? config.gemini_request_timeout_ms
               : config.request_timeout_ms)
       << ","
       << "\"ollama_request_timeout_ms\":"
       << (config.ollama_request_timeout_ms > 0
               ? config.ollama_request_timeout_ms
               : config.request_timeout_ms)
       << ","
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
      {"stream_requests_total", metrics.chat_stream_requests.load() +
                                      metrics.rag_stream_requests.load()},
      {"chat_stream_requests_total", metrics.chat_stream_requests.load()},
      {"rag_stream_requests_total", metrics.rag_stream_requests.load()},
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
  body << ",\"last_stream_latency_ms\":"
       << metrics.last_stream_latency_ms.load();
  body << ",\"last_stream_success\":"
       << (metrics.last_stream_success.load() ? "true" : "false");
  body << ",\"last_stream_error\":\""
       << escapeJsonString(getLastStreamError()) << "\"";
  body << ",\"last_stream_provider\":\""
       << escapeJsonString(getLastStreamProvider()) << "\"";
  body << ",\"last_stream_chunks\":" << getLastStreamChunks();
  body << ",\"last_stream_type\":\"" << escapeJsonString(getLastStreamType())
       << "\"";
  body << ",\"last_stream_mode\":\"" << escapeJsonString(getLastStreamMode())
       << "\"";
  body << ",\"last_upstream_stream_mode\":\""
       << escapeJsonString(getLastUpstreamStreamMode()) << "\"";
  body << ",\"last_fallback_from\":\""
       << escapeJsonString(getLastFallbackFrom()) << "\"";
  body << ",\"last_fallback_to\":\""
       << escapeJsonString(getLastFallbackTo()) << "\"";
  body << "}";
  return jsonResponse(200, "OK", body.str());
}

LightAgentGateway::Response LightAgentGateway::chat(const Request &request) {
  auto &metrics = metricsState();
  ChatExecution execution = executeChatInternal(request.body);

  metrics.last_chat_latency_ms = execution.latency_ms;
  metrics.last_chat_success = execution.result.success;
  setLastChatStatus(execution.result.provider,
                    execution.result.error_message.value_or(""),
                    execution.result.fallback_from.value_or(""),
                    execution.result.fallback_to.value_or(""));

  return jsonResponse(chatStatusCode(execution), chatReason(execution),
                      buildChatJson("lightagent-chat-1", execution.request,
                                    execution.result));
}

LightAgentGateway::Response LightAgentGateway::chatStream(
    const Request &request) {
  const GatewayConfig &config = gatewayConfig();
  const auto started = Clock::now();

  if (!config.stream_enabled) {
    StreamBuildResult error = buildStreamError(
        "lightagent-chat-stream-1", "chat.completion.chunk",
        "stream API is disabled by config");
    setLastStreamStatus("chat", config.default_provider, false,
                        "stream API is disabled by config",
                        static_cast<int>(error.chunks), 0, "disabled", "");
    return streamResponse(503, "Service Unavailable", error.body);
  }

  ChatStreamExecution execution = executeChatStreamInternal(request.body);
  StreamBuildResult stream;
  bool success = execution.used_upstream_stream
                     ? execution.stream_result.success
                     : execution.pseudo_result.success;
  std::string provider = execution.used_upstream_stream
                             ? execution.stream_result.provider
                             : execution.pseudo_result.provider;
  std::string model = execution.used_upstream_stream
                          ? execution.stream_result.model
                          : execution.pseudo_result.model;
  std::string errorMessage =
      execution.used_upstream_stream
          ? execution.stream_result.error_message
          : execution.pseudo_result.error_message.value_or("");
  std::string streamMode =
      execution.used_upstream_stream ? "upstream_real" : "pseudo";
  std::string upstreamMode =
      execution.used_upstream_stream ? execution.stream_result.upstream_stream_mode
                                     : "";

  if (success && execution.used_upstream_stream) {
    stream = buildStreamFromDeltas(
        "lightagent-chat-stream-1", "chat.completion.chunk", provider, model,
        execution.stream_result.deltas, execution.fallback_from,
        execution.fallback_to, "", streamMode, upstreamMode);
  } else if (success) {
    stream = buildStreamFromAnswer(
        "lightagent-chat-stream-1", "chat.completion.chunk", provider, model,
        execution.pseudo_result.answer, streamChunkSize(config),
        execution.pseudo_result.fallback_from, execution.pseudo_result.fallback_to,
        "", streamMode, upstreamMode);
  } else {
    stream = buildStreamError(
        "lightagent-chat-stream-1", "chat.completion.chunk",
        errorMessage.empty() ? "chat stream failed" : errorMessage);
  }

  const auto latencyMs =
      std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() -
                                                            started)
          .count();
  setLastStreamStatus("chat", provider, success, errorMessage,
                      static_cast<int>(stream.chunks), latencyMs, streamMode,
                      upstreamMode, execution.fallback_from.value_or(""),
                      execution.fallback_to.value_or(""));

  const int statusCode = success ? 200 : (execution.bad_request ? 400 : 503);
  return streamResponse(statusCode,
                        success ? "OK"
                                : (statusCode == 400 ? "Bad Request"
                                                     : "Service Unavailable"),
                        stream.body);
}

LightAgentGateway::Response LightAgentGateway::ragQuery(
    const Request &request) {
  RagExecution execution = executeRagInternal(request.body);
  setLastRagStatus(execution.result);

  return jsonResponse(ragStatusCode(execution), ragReason(execution),
                      buildRagJson(execution.result));
}

LightAgentGateway::Response LightAgentGateway::ragQueryStream(
    const Request &request) {
  const GatewayConfig &config = gatewayConfig();
  const auto started = Clock::now();

  if (!config.stream_enabled) {
    StreamBuildResult error = buildStreamError(
        "lightagent-rag-stream-1", "rag.query.chunk",
        "stream API is disabled by config");
    setLastStreamStatus("rag", config.default_provider, false,
                        "stream API is disabled by config",
                        static_cast<int>(error.chunks), 0, "disabled", "");
    return streamResponse(503, "Service Unavailable", error.body);
  }

  RagStreamExecution execution = executeRagStreamInternal(request.body);
  setLastRagStatus(execution.rag_result);

  std::ostringstream metadata;
  metadata << "\"question\":\"" << escapeJsonString(execution.rag_result.question)
           << "\","
           << "\"top_k\":" << execution.rag_result.top_k << ","
           << "\"retrieved_chunks_count\":"
           << execution.rag_result.retrieved_chunks.size();

  StreamBuildResult stream;
  const bool success = execution.rag_result.success;
  const std::string streamMode =
      execution.used_upstream_stream ? "upstream_real" : "pseudo";
  const std::string upstreamMode =
      execution.used_upstream_stream ? execution.stream_result.upstream_stream_mode
                                     : "";
  if (success && execution.used_upstream_stream) {
    stream = buildStreamFromDeltas(
        "lightagent-rag-stream-1", "rag.query.chunk",
        execution.rag_result.provider, execution.rag_result.model,
        execution.stream_result.deltas, execution.rag_result.fallback_from,
        execution.rag_result.fallback_to, metadata.str(), streamMode,
        upstreamMode);
  } else if (success) {
    stream = buildStreamFromAnswer(
        "lightagent-rag-stream-1", "rag.query.chunk",
        execution.rag_result.provider, execution.rag_result.model,
        execution.rag_result.answer, streamChunkSize(config),
        execution.rag_result.fallback_from, execution.rag_result.fallback_to,
        metadata.str(), streamMode, upstreamMode);
  } else {
    stream = buildStreamError(
        "lightagent-rag-stream-1", "rag.query.chunk",
        execution.rag_result.error_message.empty() ? "rag stream failed"
                                                   : execution.rag_result.error_message);
  }

  const auto latencyMs =
      std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() -
                                                            started)
          .count();
  setLastStreamStatus("rag", execution.rag_result.provider,
                      execution.rag_result.success,
                      execution.rag_result.error_message,
                      static_cast<int>(stream.chunks), latencyMs, streamMode,
                      upstreamMode,
                      execution.rag_result.fallback_from.value_or(""),
                      execution.rag_result.fallback_to.value_or(""));

  const int statusCode = success ? 200 : (execution.bad_request ? 400 : 503);
  return streamResponse(statusCode,
                        success ? "OK"
                                : (statusCode == 400 ? "Bad Request"
                                                     : "Service Unavailable"),
                        stream.body);
}
