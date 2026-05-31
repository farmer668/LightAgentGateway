#pragma once

#include "GatewayConfig.h"

#include <string>
#include <vector>

struct DocumentChunk {
  std::string file_path;
  std::string title;
  std::string content;
  int score = 0;
  size_t start_offset = 0;
};

class KnowledgeBase {
 public:
  explicit KnowledgeBase(const GatewayConfig &config);

  bool load();
  std::vector<DocumentChunk> search(const std::string &query, int top_k) const;
  std::string lastError() const;

 private:
  GatewayConfig config_;
  std::vector<DocumentChunk> chunks_;
  std::string last_error_;
};
