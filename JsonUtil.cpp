#include "JsonUtil.h"

#include <sstream>

std::string escapeJsonString(std::string_view input) {
  std::string escaped;
  escaped.reserve(input.size() + 8);
  for (char ch : input) {
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

std::optional<std::string> extractJsonStringField(std::string_view body,
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
        case '"':
        case '\\':
        case '/':
          value += ch;
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

std::optional<std::string> extractGeminiText(std::string_view responseBody) {
  // Lightweight extraction for candidates[0].content.parts[0].text. This
  // intentionally avoids assuming a full JSON parser is available.
  return extractJsonStringField(responseBody, "text");
}

std::optional<std::string> extractOllamaText(std::string_view responseBody) {
  return extractJsonStringField(responseBody, "response");
}

std::string buildErrorJson(std::string_view errorMessage) {
  std::ostringstream body;
  body << "{"
       << "\"success\":false,"
       << "\"error_message\":\"" << escapeJsonString(errorMessage) << "\""
       << "}";
  return body.str();
}

std::string buildChatJson(std::string_view id, const ChatRequest &request,
                          const ChatResult &result) {
  std::ostringstream body;
  body << "{"
       << "\"id\":\"" << escapeJsonString(id) << "\","
       << "\"object\":\"chat.completion\","
       << "\"success\":" << (result.success ? "true" : "false") << ","
       << "\"provider\":\"" << escapeJsonString(result.provider) << "\","
       << "\"model\":\"" << escapeJsonString(result.model) << "\","
       << "\"answer\":\"" << escapeJsonString(result.answer) << "\","
       << "\"message\":\"" << escapeJsonString(request.message) << "\"";
  if (request.session_id) {
    body << ",\"session_id\":\"" << escapeJsonString(*request.session_id)
         << "\"";
  }
  if (result.error_message) {
    body << ",\"error_message\":\"" << escapeJsonString(*result.error_message)
         << "\"";
  }
  if (result.fallback_from) {
    body << ",\"fallback_from\":\"" << escapeJsonString(*result.fallback_from)
         << "\"";
  }
  if (result.fallback_to) {
    body << ",\"fallback_to\":\"" << escapeJsonString(*result.fallback_to)
         << "\"";
  }
  body << "}";
  return body.str();
}
