#pragma once

#include <optional>
#include <string>

struct ChatRequest {
  std::string message;
  std::optional<std::string> session_id;
  std::optional<std::string> system_prompt;
};

struct ChatResult {
  std::string answer;
  std::string provider;
  std::string model;
  bool success = true;
  std::optional<std::string> error_message;
};

class ILlmProvider {
 public:
  virtual ~ILlmProvider() = default;
  virtual std::string name() const = 0;
  virtual ChatResult chat(const ChatRequest &request) = 0;
};

ChatRequest makeChatRequest(std::string message,
                            std::optional<std::string> session_id = std::nullopt,
                            std::optional<std::string> system_prompt =
                                std::nullopt);
