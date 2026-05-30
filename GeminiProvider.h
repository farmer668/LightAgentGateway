#pragma once

#include "GatewayConfig.h"
#include "LlmProvider.h"

#include <optional>
#include <string>

class GeminiProvider : public ILlmProvider {
 public:
  explicit GeminiProvider(const GatewayConfig &config);

  std::string name() const override;
  ChatResult chat(const ChatRequest &request) override;

 private:
  std::optional<std::string> api_key_;
  int request_timeout_ms_;
};
