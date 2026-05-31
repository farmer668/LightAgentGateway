#pragma once

#include <filesystem>
#include <optional>
#include <string>

struct GatewayConfig {
  std::string service_name = "LightAgent Gateway";
  std::string version = "0.6.0";
  std::string stage = "phase-6";
  std::string default_provider = "mock";
  std::string fallback_provider = "ollama";
  std::optional<std::string> gemini_api_key;
  std::string gemini_model = "gemini-1.5-flash";
  std::string gemini_api_base =
      "https://generativelanguage.googleapis.com/v1beta";
  std::string ollama_base_url = "http://127.0.0.1:11434";
  std::string ollama_model = "qwen2.5:0.5b";
  std::filesystem::path static_root = std::filesystem::current_path();
  std::filesystem::path knowledge_base_dir = "./knowledge_base";
  int rag_top_k = 3;
  int rag_chunk_size = 800;
  bool rag_enable_llm_answer = true;
  std::string rag_provider;
  int rag_max_context_chars = 3000;
  int request_timeout_ms = 30000;
  bool enable_real_gemini = true;
  bool enable_real_ollama = true;
  bool enable_ollama_fallback = true;

  static GatewayConfig load();
};
