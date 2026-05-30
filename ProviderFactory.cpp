#include "ProviderFactory.h"

#include <algorithm>
#include <cctype>

#include "GeminiProvider.h"
#include "MockProvider.h"

namespace {

std::string lowerCopy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return value;
}

}  // namespace

std::unique_ptr<ILlmProvider> ProviderFactory::create(
    const GatewayConfig &config) {
  const std::string provider = lowerCopy(config.default_provider);
  if (provider == "gemini") {
    return std::unique_ptr<ILlmProvider>(new GeminiProvider(config));
  }
  if (provider == "mock") {
    return std::unique_ptr<ILlmProvider>(new MockProvider());
  }

  return std::unique_ptr<ILlmProvider>(new MockProvider());
}
