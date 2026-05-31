#include "HttpClient.h"

#include <curl/curl.h>

#include <mutex>
#include <sstream>

namespace {

size_t writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
  auto *body = static_cast<std::string *>(userdata);
  const size_t total = size * nmemb;
  body->append(ptr, total);
  return total;
}

bool ensureCurlInitialized() {
  static std::once_flag once;
  static CURLcode initCode = CURLE_FAILED_INIT;
  std::call_once(once, []() { initCode = curl_global_init(CURL_GLOBAL_DEFAULT); });
  return initCode == CURLE_OK;
}

std::vector<std::string> splitLines(const std::string &body) {
  std::vector<std::string> lines;
  std::istringstream input(body);
  std::string line;
  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (!line.empty()) lines.push_back(line);
  }
  return lines;
}

}  // namespace

HttpResponse HttpClient::postJson(const std::string &url,
                                  const std::string &body,
                                  const std::vector<std::string> &headers,
                                  long timeout_ms) {
  HttpResponse response;
  if (!ensureCurlInitialized()) {
    response.error_message = "curl global initialization failed";
    return response;
  }

  CURL *curl = curl_easy_init();
  if (!curl) {
    response.error_message = "curl_easy_init failed";
    return response;
  }

  struct curl_slist *headerList = nullptr;
  headerList = curl_slist_append(headerList, "Content-Type: application/json");
  for (const auto &header : headers) {
    headerList = curl_slist_append(headerList, header.c_str());
  }

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, timeout_ms);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

  CURLcode code = curl_easy_perform(curl);
  long status = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  response.status_code = static_cast<int>(status);

  if (code != CURLE_OK) {
    response.error_message = curl_easy_strerror(code);
  } else if (response.status_code < 200 || response.status_code >= 300) {
    std::ostringstream msg;
    msg << "HTTP non-2xx status: " << response.status_code;
    response.error_message = msg.str();
  } else if (response.body.empty()) {
    response.error_message = "empty HTTP response body";
  } else {
    response.success = true;
  }

  curl_slist_free_all(headerList);
  curl_easy_cleanup(curl);
  return response;
}

StreamHttpResponse HttpClient::postJsonStream(
    const std::string &url, const std::string &body,
    const std::vector<std::string> &headers, long timeout_ms) {
  StreamHttpResponse response;
  if (!ensureCurlInitialized()) {
    response.error_message = "curl global initialization failed";
    return response;
  }

  CURL *curl = curl_easy_init();
  if (!curl) {
    response.error_message = "curl_easy_init failed";
    return response;
  }

  std::string responseBody;
  struct curl_slist *headerList = nullptr;
  headerList = curl_slist_append(headerList, "Content-Type: application/json");
  for (const auto &header : headers) {
    headerList = curl_slist_append(headerList, header.c_str());
  }

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, timeout_ms);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

  CURLcode code = curl_easy_perform(curl);
  long status = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  response.status_code = static_cast<int>(status);

  if (code != CURLE_OK) {
    response.error_message = curl_easy_strerror(code);
  } else if (response.status_code < 200 || response.status_code >= 300) {
    std::ostringstream msg;
    msg << "HTTP non-2xx status: " << response.status_code;
    response.error_message = msg.str();
  } else if (responseBody.empty()) {
    response.error_message = "empty HTTP stream response body";
  } else {
    response.lines = splitLines(responseBody);
    response.success = !response.lines.empty();
    if (!response.success) response.error_message = "empty HTTP stream lines";
  }

  curl_slist_free_all(headerList);
  curl_easy_cleanup(curl);
  return response;
}
