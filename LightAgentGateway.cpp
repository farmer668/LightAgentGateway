#include "LightAgentGateway.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <sstream>
#include <utility>

namespace {

using Clock = std::chrono::steady_clock;

const Clock::time_point kStartedAt = Clock::now();
std::atomic<unsigned long long> gApiRequests{0};
std::atomic<unsigned long long> gChatRequests{0};

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

std::optional<std::string> getenvString(const char *name) {
  if (const char *value = std::getenv(name)) {
    if (*value != '\0') return std::string(value);
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

}  // namespace

std::optional<LightAgentGateway::Response> LightAgentGateway::handle(
    const Request &request) {
  if (!startsWith(request.path, "/api/")) {
    return std::nullopt;
  }

  ++gApiRequests;

  if (request.path == "/api/health") {
    if (request.method != Method::Get && request.method != Method::Head) {
      return methodNotAllowed("GET, HEAD");
    }
    return health();
  }

  if (request.path == "/api/metrics") {
    if (request.method != Method::Get && request.method != Method::Head) {
      return methodNotAllowed("GET, HEAD");
    }
    return metrics();
  }

  if (request.path == "/api/chat") {
    if (request.method != Method::Post) {
      return methodNotAllowed("POST");
    }
    return chat(request);
  }

  return jsonResponse(404, "Not Found",
                      "{\"error\":\"api_route_not_found\"}");
}

LightAgentGateway::Response LightAgentGateway::health() {
  const auto provider = getenvString("LIGHTAGENT_PROVIDER").value_or("mock");
  const bool geminiKeyConfigured = getenvString("GEMINI_API_KEY").has_value();
  const auto root = std::filesystem::current_path().generic_string();

  std::ostringstream body;
  body << "{"
       << "\"status\":\"ok\","
       << "\"service\":\"LightAgent Gateway\","
       << "\"stage\":\"phase-1\","
       << "\"provider\":\"" << jsonEscape(provider) << "\","
       << "\"gemini_api_key_configured\":"
       << (geminiKeyConfigured ? "true" : "false") << ","
       << "\"fallback_provider\":\"ollama\","
       << "\"static_root\":\"" << jsonEscape(root) << "\""
       << "}";
  return jsonResponse(200, "OK", body.str());
}

LightAgentGateway::Response LightAgentGateway::metrics() {
  const auto uptime =
      std::chrono::duration_cast<std::chrono::seconds>(Clock::now() -
                                                       kStartedAt)
          .count();
  const std::map<std::string, unsigned long long> counters{
      {"api_requests_total", gApiRequests.load()},
      {"chat_requests_total", gChatRequests.load()},
  };

  std::ostringstream body;
  body << "{"
       << "\"service\":\"LightAgent Gateway\","
       << "\"uptime_seconds\":" << uptime;
  for (const auto &[name, value] : counters) {
    body << ",\"" << name << "\":" << value;
  }
  body << "}";
  return jsonResponse(200, "OK", body.str());
}

LightAgentGateway::Response LightAgentGateway::chat(const Request &request) {
  ++gChatRequests;

  std::ostringstream body;
  body << "{"
       << "\"id\":\"mock-chat-1\","
       << "\"object\":\"chat.completion\","
       << "\"provider\":\"mock\","
       << "\"model\":\"mock-lightagent-phase1\","
       << "\"answer\":\"This is a mock answer from LightAgent Gateway. "
          "Gemini and Ollama integration is not enabled in phase 1.\","
       << "\"received_body\":\"" << jsonEscape(request.body) << "\""
       << "}";
  return jsonResponse(200, "OK", body.str());
}
