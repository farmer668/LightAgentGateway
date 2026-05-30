#pragma once

#include <optional>
#include <string>
#include <string_view>

class LightAgentGateway {
 public:
  enum class Method { Get, Post, Head, Unsupported };

  struct Request {
    Method method;
    std::string_view path;
    std::string_view body;
  };

  struct Response {
    int statusCode;
    std::string reason;
    std::string contentType;
    std::string body;
  };

  static std::optional<Response> handle(const Request &request);

 private:
  static Response health();
  static Response metrics();
  static Response chat(const Request &request);
};
