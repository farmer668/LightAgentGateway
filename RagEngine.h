#pragma once

#include "GatewayConfig.h"
#include "KnowledgeBase.h"
#include "LlmProvider.h"

#include <string>
#include <vector>

struct RagResult {
  bool success = false;
  std::string question;
  std::string answer;
  std::string provider;
  std::string model;
  std::string error_message;
  int top_k = 0;
  std::vector<DocumentChunk> retrieved_chunks;
  std::optional<std::string> fallback_from;
  std::optional<std::string> fallback_to;
  long long latency_ms = 0;
};

struct RagPreparedRequest {
  RagResult result;
  ChatRequest chat_request;
  bool ready = false;
};

class RagEngine {
 public:
  explicit RagEngine(const GatewayConfig &config);

  RagResult query(const std::string &question, int top_k);
  RagPreparedRequest prepareRequest(const std::string &question, int top_k);

 private:
  GatewayConfig config_;

  ChatResult callProvider(const ChatRequest &request) const;
  std::string buildPrompt(const std::string &question,
                          const std::vector<DocumentChunk> &chunks) const;
};
