#pragma once

#include "LlmProvider.h"

#include <string>

class MockProvider : public ILlmProvider {
 public:
  explicit MockProvider(std::string model = "mock-llm");

  std::string name() const override;
  ChatResult chat(const ChatRequest &request) override;

 private:
  std::string model_;
};
