#include "LightAgentGateway.h"

#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <sstream>
#include <utility>

#include "GatewayConfig.h"
#include "MockProvider.h"

namespace {

using Clock = std::chrono::steady_clock;

struct GatewayMetrics {
  Clock::time_point started_at = Clock::now();
  std::atomic<unsigned long long> total_requests{0};
  std::atomic<unsigned long long> chat_requests{0};
  std::atomic<unsigned long long> health_requests{0};
  std::atomic<unsigned long long> metrics_requests{0};
  std::atomic<long long> last_chat_latency_ms{0};
  std::mutex last_provider_mutex;
  std::string last_provider = "none";
};

GatewayMetrics &metricsState() {
  static GatewayMetrics metrics;
  return metrics;
}

const GatewayConfig &gatewayConfig() {
  static const GatewayConfig config = GatewayConfig::load();
  return config;
}

MockProvider &mockProvider() {
  static MockProvider provider;
  return provider;
}

bool startsWith(std::string_view value, std::string_view prefix) {
  return value.size() >= prefix.size() &&
         value.compare(0, prefix.size(), prefix) == 0;
}

std::string jsonEscape(std::string_view value) {
  std::string escaped;
  escaped.reserve(value.size() + 8);
  for (char ch : value) {
    switch (ch) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        escaped += ch;
        break;
    }
  }
  return escaped;
}

std::optional<std::string> findJsonStringField(std::string_view body,
                                               std::string_view field) {
  const std::string quotedField = "\"" + std::string(field) + "\"";
  size_t pos = body.find(quotedField);
  if (pos == std::string_view::npos) return std::nullopt;

  pos = body.find(':', pos + quotedField.size());
  if (pos == std::string_view::npos) return std::nullopt;
  ++pos;
  while (pos < body.size() &&
         (body[pos] == ' ' || body[pos] == '\t' || body[pos] == '\r' ||
          body[pos] == '\n')) {
    ++pos;
  }
  if (pos >= body.size() || body[pos] != '"') return std::nullopt;

  std::string value;
  bool escaped = false;
  for (++pos; pos < body.size(); ++pos) {
    char ch = body[pos];
    if (escaped) {
      switch (ch) {
        case 'n':
          value += '\n';
          break;
        case 'r':
          value += '\r';
          break;
        case 't':
          value += '\t';
          break;
        default:
          value += ch;
          break;
      }
      escaped = false;
      continue;
    }
    if (ch == '\\') {
      escaped = true;
      continue;
    }
    if (ch == '"') return value;
    value += ch;
  }
  return std::nullopt;
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

void setLastProvider(const std::string &provider) {
  auto &metrics = metricsState();
  std::lock_guard<std::mutex> lock(metrics.last_provider_mutex);
  metrics.last_provider = provider;
}

std::string getLastProvider() {
  auto &metrics = metricsState();
  std::lock_guard<std::mutex> lock(metrics.last_provider_mutex);
  return metrics.last_provider;
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
       << "\"service\":\"" << jsonEscape(config.service_name) << "\","
       << "\"version\":\"" << jsonEscape(config.version) << "\","
       << "\"stage\":\"" << jsonEscape(config.stage) << "\","
       << "\"provider\":\"" << jsonEscape(config.default_provider) << "\","
       << "\"gemini_api_key_configured\":"
       << (config.gemini_api_key.has_value() ? "true" : "false") << ","
       << "\"fallback_provider\":\"" << jsonEscape(config.fallback_provider)
       << "\","
       << "\"ollama_base_url\":\"" << jsonEscape(config.ollama_base_url)
       << "\","
       << "\"static_root\":\""
       << jsonEscape(config.static_root.generic_string()) << "\","
       << "\"request_timeout_ms\":" << config.request_timeout_ms
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
       << "\"service\":\"" << jsonEscape(config.service_name) << "\","
       << "\"version\":\"" << jsonEscape(config.version) << "\","
       << "\"stage\":\"" << jsonEscape(config.stage) << "\","
       << "\"uptime_seconds\":" << uptime;
  for (const auto &[name, value] : counters) {
    body << ",\"" << name << "\":" << value;
  }
  body << ",\"last_provider\":\"" << jsonEscape(getLastProvider()) << "\"";
  body << ",\"last_chat_latency_ms\":"
       << metrics.last_chat_latency_ms.load();
  body << "}";
  return jsonResponse(200, "OK", body.str());
}

LightAgentGateway::Response LightAgentGateway::chat(const Request &request) {
  auto &metrics = metricsState();
  const auto started = Clock::now();

  ChatRequest chatRequest = makeChatRequest(
      findJsonStringField(request.body, "message").value_or("(empty message)"),
      findJsonStringField(request.body, "session_id"),
      findJsonStringField(request.body, "system_prompt"));

  ILlmProvider &provider = mockProvider();
  ChatResult result = provider.chat(chatRequest);

  const auto latencyMs =
      std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() -
                                                            started)
          .count();
  metrics.last_chat_latency_ms = latencyMs;
  setLastProvider(result.provider);

  std::ostringstream body;
  body << "{"
       << "\"id\":\"mock-chat-1\","
       << "\"object\":\"chat.completion\","
       << "\"success\":" << (result.success ? "true" : "false") << ","
       << "\"provider\":\"" << jsonEscape(result.provider) << "\","
       << "\"model\":\"" << jsonEscape(result.model) << "\","
       << "\"answer\":\"" << jsonEscape(result.answer) << "\","
       << "\"message\":\"" << jsonEscape(chatRequest.message) << "\"";
  if (chatRequest.session_id) {
    body << ",\"session_id\":\"" << jsonEscape(*chatRequest.session_id)
         << "\"";
  }
  if (result.error_message) {
    body << ",\"error_message\":\"" << jsonEscape(*result.error_message)
         << "\"";
  }
  body << "}";
  return jsonResponse(result.success ? 200 : 500,
                      result.success ? "OK" : "Internal Server Error",
                      body.str());
}
