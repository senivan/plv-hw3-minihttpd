#pragma once
#include <string>
#include <unordered_map>
#include <cstdint>

namespace minihttpd {

struct HttpRequest {
  std::string method;
  std::string target;
  std::string version;

  std::unordered_map<std::string, std::string> headers;

  uint64_t content_length = 0;
};

struct HttpResponseHead {
  int status = 200;
  std::string reason = "OK";
  std::unordered_map<std::string, std::string> headers;
};

std::string http_date_now();
std::string status_reason(int status);
std::string content_type_for_path(const std::string& path);

bool parse_http_request_headers(
  const std::string& header_blob,
  HttpRequest& out,
  std::string& err);

std::string build_response_head(const HttpResponseHead& head);

} 
