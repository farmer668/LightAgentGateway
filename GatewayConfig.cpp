#include "GatewayConfig.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string_view>

namespace {

std::optional<std::string> getenvString(const char *name) {
  if (const char *value = std::getenv(name)) {
    if (*value != '\0') return std::string(value);
  }
  return std::nullopt;
}

std::optional<int> parseInt(std::string_view value) {
  try {
    size_t consumed = 0;
    int parsed = std::stoi(std::string(value), &consumed);
    if (consumed != value.size()) return std::nullopt;
    return parsed;
  } catch (...) {
    return std::nullopt;
  }
}

std::string trim(std::string_view value) {
  const auto begin = value.find_first_not_of(" \t\r\n");
  if (begin == std::string_view::npos) return "";
  const auto end = value.find_last_not_of(" \t\r\n");
  return std::string(value.substr(begin, end - begin + 1));
}

std::optional<bool> parseBool(std::string_view value) {
  std::string normalized = trim(value);
  for (char &ch : normalized) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  if (normalized == "true" || normalized == "1" || normalized == "yes") {
    return true;
  }
  if (normalized == "false" || normalized == "0" || normalized == "no") {
    return false;
  }
  return std::nullopt;
}

std::string unquote(std::string value) {
  value = trim(value);
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
    value = value.substr(1, value.size() - 2);
  }
  return value;
}

std::map<std::string, std::string> parseFlatJsonObject(const std::string &text) {
  std::map<std::string, std::string> values;
  size_t pos = text.find('{');
  const size_t end = text.rfind('}');
  if (pos == std::string::npos || end == std::string::npos || pos >= end) {
    return values;
  }

  ++pos;
  while (pos < end) {
    pos = text.find('"', pos);
    if (pos == std::string::npos || pos >= end) break;
    const size_t keyEnd = text.find('"', pos + 1);
    if (keyEnd == std::string::npos || keyEnd >= end) break;
    std::string key = text.substr(pos + 1, keyEnd - pos - 1);

    const size_t colon = text.find(':', keyEnd + 1);
    if (colon == std::string::npos || colon >= end) break;

    size_t valueStart = colon + 1;
    while (valueStart < end &&
           (text[valueStart] == ' ' || text[valueStart] == '\t' ||
            text[valueStart] == '\r' || text[valueStart] == '\n')) {
      ++valueStart;
    }

    std::string value;
    if (valueStart < end && text[valueStart] == '"') {
      size_t valueEnd = valueStart + 1;
      bool escaped = false;
      for (; valueEnd < end; ++valueEnd) {
        if (escaped) {
          escaped = false;
          continue;
        }
        if (text[valueEnd] == '\\') {
          escaped = true;
          continue;
        }
        if (text[valueEnd] == '"') break;
      }
      if (valueEnd >= end) break;
      value = text.substr(valueStart, valueEnd - valueStart + 1);
      pos = valueEnd + 1;
    } else {
      size_t valueEnd = text.find(',', valueStart);
      if (valueEnd == std::string::npos || valueEnd > end) valueEnd = end;
      value = text.substr(valueStart, valueEnd - valueStart);
      pos = valueEnd + 1;
    }

    values[key] = unquote(value);
  }

  return values;
}

std::map<std::string, std::string> loadConfigFile() {
  std::filesystem::path configPath =
      getenvString("LIGHTAGENT_CONFIG").value_or("config.json");
  if (!std::filesystem::exists(configPath)) return {};

  std::ifstream input(configPath);
  if (!input) return {};

  std::ostringstream buffer;
  buffer << input.rdbuf();
  return parseFlatJsonObject(buffer.str());
}

void applyValue(GatewayConfig &config, const std::string &key,
                const std::string &value) {
  if (key == "service_name") {
    config.service_name = value;
  } else if (key == "version") {
    config.version = value;
  } else if (key == "stage") {
    config.stage = value;
  } else if (key == "default_provider") {
    config.default_provider = value;
  } else if (key == "fallback_provider") {
    config.fallback_provider = value;
  } else if (key == "gemini_api_key") {
    if (!value.empty()) config.gemini_api_key = value;
  } else if (key == "gemini_model") {
    config.gemini_model = value;
  } else if (key == "gemini_api_base") {
    config.gemini_api_base = value;
  } else if (key == "ollama_base_url") {
    config.ollama_base_url = value;
  } else if (key == "ollama_model") {
    config.ollama_model = value;
  } else if (key == "static_root") {
    config.static_root = value;
  } else if (key == "knowledge_base_dir") {
    config.knowledge_base_dir = value;
  } else if (key == "rag_top_k") {
    if (auto parsed = parseInt(value)) config.rag_top_k = *parsed;
  } else if (key == "rag_chunk_size") {
    if (auto parsed = parseInt(value)) config.rag_chunk_size = *parsed;
  } else if (key == "rag_enable_llm_answer") {
    if (auto parsed = parseBool(value)) config.rag_enable_llm_answer = *parsed;
  } else if (key == "rag_provider") {
    config.rag_provider = value;
  } else if (key == "rag_max_context_chars") {
    if (auto parsed = parseInt(value)) config.rag_max_context_chars = *parsed;
  } else if (key == "request_timeout_ms") {
    if (auto parsed = parseInt(value)) config.request_timeout_ms = *parsed;
  } else if (key == "gemini_request_timeout_ms") {
    if (auto parsed = parseInt(value)) config.gemini_request_timeout_ms = *parsed;
  } else if (key == "ollama_request_timeout_ms") {
    if (auto parsed = parseInt(value)) config.ollama_request_timeout_ms = *parsed;
  } else if (key == "stream_enabled") {
    if (auto parsed = parseBool(value)) config.stream_enabled = *parsed;
  } else if (key == "stream_mode") {
    config.stream_mode = value;
  } else if (key == "stream_chunk_size") {
    if (auto parsed = parseInt(value)) config.stream_chunk_size = *parsed;
  } else if (key == "enable_real_gemini") {
    if (auto parsed = parseBool(value)) config.enable_real_gemini = *parsed;
  } else if (key == "enable_real_ollama") {
    if (auto parsed = parseBool(value)) config.enable_real_ollama = *parsed;
  } else if (key == "enable_ollama_fallback") {
    if (auto parsed = parseBool(value)) config.enable_ollama_fallback = *parsed;
  }
}

void applyEnv(GatewayConfig &config) {
  const std::map<std::string, const char *> envKeys{
      {"service_name", "LIGHTAGENT_SERVICE_NAME"},
      {"version", "LIGHTAGENT_VERSION"},
      {"stage", "LIGHTAGENT_STAGE"},
      {"default_provider", "LIGHTAGENT_DEFAULT_PROVIDER"},
      {"fallback_provider", "LIGHTAGENT_FALLBACK_PROVIDER"},
      {"gemini_api_key", "GEMINI_API_KEY"},
      {"gemini_model", "GEMINI_MODEL"},
      {"gemini_api_base", "GEMINI_API_BASE"},
      {"ollama_base_url", "OLLAMA_BASE_URL"},
      {"ollama_model", "OLLAMA_MODEL"},
      {"static_root", "LIGHTAGENT_STATIC_ROOT"},
      {"knowledge_base_dir", "LIGHTAGENT_KB_DIR"},
      {"rag_top_k", "LIGHTAGENT_RAG_TOP_K"},
      {"rag_chunk_size", "LIGHTAGENT_RAG_CHUNK_SIZE"},
      {"rag_enable_llm_answer", "LIGHTAGENT_RAG_ENABLE_LLM_ANSWER"},
      {"rag_provider", "LIGHTAGENT_RAG_PROVIDER"},
      {"rag_max_context_chars", "LIGHTAGENT_RAG_MAX_CONTEXT_CHARS"},
      {"request_timeout_ms", "LIGHTAGENT_REQUEST_TIMEOUT_MS"},
      {"gemini_request_timeout_ms", "LIGHTAGENT_GEMINI_REQUEST_TIMEOUT_MS"},
      {"ollama_request_timeout_ms", "LIGHTAGENT_OLLAMA_REQUEST_TIMEOUT_MS"},
      {"stream_enabled", "LIGHTAGENT_STREAM_ENABLED"},
      {"stream_mode", "LIGHTAGENT_STREAM_MODE"},
      {"stream_chunk_size", "LIGHTAGENT_STREAM_CHUNK_SIZE"},
      {"enable_real_gemini", "LIGHTAGENT_ENABLE_REAL_GEMINI"},
      {"enable_real_ollama", "LIGHTAGENT_ENABLE_REAL_OLLAMA"},
      {"enable_ollama_fallback", "LIGHTAGENT_ENABLE_OLLAMA_FALLBACK"},
  };

  for (const auto &[key, envName] : envKeys) {
    if (auto value = getenvString(envName)) {
      applyValue(config, key, *value);
    }
  }

  if (auto value = getenvString("GEMINI_REQUEST_TIMEOUT_MS")) {
    applyValue(config, "gemini_request_timeout_ms", *value);
  }
  if (auto value = getenvString("OLLAMA_REQUEST_TIMEOUT_MS")) {
    applyValue(config, "ollama_request_timeout_ms", *value);
  }
}

}  // namespace

GatewayConfig GatewayConfig::load() {
  GatewayConfig config;

  for (const auto &[key, value] : loadConfigFile()) {
    applyValue(config, key, value);
  }
  applyEnv(config);

  return config;
}
