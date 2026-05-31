#pragma once

#include <filesystem>
#include <optional>
#include <string>

struct GatewayConfig {
  std::string service_name = "LightAgent Gateway";
  std::string version = "0.4.0";
  std::string stage = "phase-4";
  std::string default_provider = "mock";
  std::string fallback_provider = "ollama";
  std::optional<std::string> gemini_api_key;
  std::string gemini_model = "gemini-1.5-flash";
  std::string gemini_api_base =
      "https://generativelanguage.googleapis.com/v1beta";
  std::string ollama_base_url = "http://127.0.0.1:11434";
  std::filesystem::path static_root = std::filesystem::current_path();
  int request_timeout_ms = 30000;
  bool enable_real_gemini = true;

  static GatewayConfig load();
};
