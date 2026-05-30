#include "MockProvider.h"

#include <utility>

MockProvider::MockProvider(std::string model) : model_(std::move(model)) {}

std::string MockProvider::name() const { return "mock"; }

ChatResult MockProvider::chat(const ChatRequest &request) {
  ChatResult result;
  result.answer = "This is a mock answer from LightAgent Gateway phase-2.";
  if (!request.message.empty()) {
    result.answer += " Received message: " + request.message;
  }
  result.provider = name();
  result.model = model_;
  result.success = true;
  return result;
}
