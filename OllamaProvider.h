#pragma once

#include "GatewayConfig.h"
#include "LlmProvider.h"

#include <string>

class OllamaProvider : public ILlmProvider {
 public:
  explicit OllamaProvider(const GatewayConfig &config);

  std::string name() const override;
  ChatResult chat(const ChatRequest &request) override;

 private:
  std::string base_url_;
  std::string model_;
  int request_timeout_ms_;
  bool enable_real_ollama_;
};
