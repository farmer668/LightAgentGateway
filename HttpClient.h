#pragma once

#include <string>
#include <vector>

struct HttpResponse {
  int status_code = 0;
  std::string body;
  std::string error_message;
  bool success = false;
};

class HttpClient {
 public:
  HttpResponse postJson(const std::string &url, const std::string &body,
                        const std::vector<std::string> &headers,
                        long timeout_ms);
};
