#include "LlmProvider.h"

#include <utility>

ChatRequest makeChatRequest(std::string message,
                            std::optional<std::string> session_id,
                            std::optional<std::string> system_prompt) {
  ChatRequest request;
  request.message = std::move(message);
  request.session_id = std::move(session_id);
  request.system_prompt = std::move(system_prompt);
  return request;
}
