#include "GatewayConfig.h"

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
  } else if (key == "ollama_base_url") {
    config.ollama_base_url = value;
  } else if (key == "static_root") {
    config.static_root = value;
  } else if (key == "request_timeout_ms") {
    if (auto parsed = parseInt(value)) config.request_timeout_ms = *parsed;
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
      {"ollama_base_url", "OLLAMA_BASE_URL"},
      {"static_root", "LIGHTAGENT_STATIC_ROOT"},
      {"request_timeout_ms", "LIGHTAGENT_REQUEST_TIMEOUT_MS"},
  };

  for (const auto &[key, envName] : envKeys) {
    if (auto value = getenvString(envName)) {
      applyValue(config, key, *value);
    }
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
