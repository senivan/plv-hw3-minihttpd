#pragma once
#include <string>
#include <cstdint>

namespace minihttpd {

struct ServerConfig {
  std::string server_ip = "127.0.0.1";
  uint16_t port = 8080;

  uint32_t max_clients = 128;

  std::string root_dir = "./www";

  std::string log_file = "./server.log";
  std::string log_level = "INFO"; 

  bool keep_alive = true;
  uint32_t keep_alive_timeout_sec = 10;
  uint32_t keep_alive_max_requests = 100;

  uint32_t read_header_max_bytes = 32768; 
  uint32_t recv_chunk_size = 65536;       

};

ServerConfig load_config_json(const std::string& path);

} 
