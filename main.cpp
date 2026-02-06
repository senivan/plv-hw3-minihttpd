#include "config.hpp"
#include "logger.hpp"
#include "server.hpp"

#include <iostream>

int main(int argc, char** argv) {
  const char* cfg_path = "./config.json";
  if (argc >= 2) cfg_path = argv[1];

  try {
    auto cfg = minihttpd::load_config_json(cfg_path);

    minihttpd::Logger::instance().configure(
      cfg.log_file,
      minihttpd::parse_level(cfg.log_level)
    );

    LOG_INFO("Config loaded.");
    minihttpd::HttpServer s(cfg);
    return s.run();
  } catch (const std::exception& e) {
    std::cerr << "Fatal: " << e.what() << "\n";
    return 1;
  }
}
