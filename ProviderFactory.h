#pragma once

#include "GatewayConfig.h"
#include "LlmProvider.h"

#include <memory>

class ProviderFactory {
 public:
  static std::unique_ptr<ILlmProvider> create(const GatewayConfig &config);
  static std::unique_ptr<ILlmProvider> create(const GatewayConfig &config,
                                             const std::string &providerName);
};
