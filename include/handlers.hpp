#pragma once
#include "config.hpp"
#include "http.hpp"
#include <string>
#include <cstdint>

namespace minihttpd {

void send_error_response(int fd, int status, bool keep_alive, const std::string& detail = "");

bool handle_request(
  int fd,
  const ServerConfig& cfg,
  const HttpRequest& req,
  const std::string& target_path_only,
  const std::string& body_prefix,
  uint64_t body_prefix_len
);

}
