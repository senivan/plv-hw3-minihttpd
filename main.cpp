#include "config.hpp"
#include "logger.hpp"
#include <iostream>

int main(int argc, char** argv) {
  const char* cfg_path = "./example.config.json";
  if (argc >= 2) cfg_path = argv[1];

  try {
    auto cfg = minihttpd::load_config_json(cfg_path);

    minihttpd::Logger::instance().configure(
      cfg.log_file,
      minihttpd::parse_level(cfg.log_level)
    );

    LOG_INFO("Config loaded successfully");
    LOG_INFO("server_ip=" + cfg.server_ip + " port=" + std::to_string(cfg.port));
    LOG_INFO("root_dir=" + cfg.root_dir);
    LOG_INFO("max_clients=" + std::to_string(cfg.max_clients));
    LOG_INFO("keep_alive=" + std::string(cfg.keep_alive ? "true" : "false"));
    LOG_DEBUG("keep_alive_timeout_sec=" + std::to_string(cfg.keep_alive_timeout_sec));
    LOG_DEBUG("keep_alive_max_requests=" + std::to_string(cfg.keep_alive_max_requests));
    LOG_DEBUG("read_header_max_bytes=" + std::to_string(cfg.read_header_max_bytes));
    LOG_DEBUG("recv_chunk_size=" + std::to_string(cfg.recv_chunk_size));

    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Fatal: " << e.what() << "\n";
    return 1;
  }
}
