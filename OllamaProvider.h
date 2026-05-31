#pragma once

#include "GatewayConfig.h"
#include "LlmProvider.h"

#include <string>
#include <vector>

struct ChatStreamResult {
  bool success = false;
  std::string provider;
  std::string model;
  std::vector<std::string> deltas;
  std::string error_message;
  bool upstream_real_stream = false;
  std::string upstream_stream_mode;
};

class OllamaProvider : public ILlmProvider {
 public:
  explicit OllamaProvider(const GatewayConfig &config);

  std::string name() const override;
  ChatResult chat(const ChatRequest &request) override;
  ChatStreamResult streamChat(const ChatRequest &request);

 private:
  std::string base_url_;
  std::string model_;
  int request_timeout_ms_;
  bool enable_real_ollama_;
  bool ollama_stream_;
};
