#include "config.hpp"
#include "logger.hpp"
#include "http.hpp"
#include "utils.hpp"

#include <iostream>
#include <filesystem>

int main(int argc, char** argv) {
  const char* cfg_path = "./config.json";
  if (argc >= 2) cfg_path = argv[1];

  try {
    auto cfg = minihttpd::load_config_json(cfg_path);

    minihttpd::Logger::instance().configure(
      cfg.log_file,
      minihttpd::parse_level(cfg.log_level)
    );

    LOG_INFO("root_dir=" + cfg.root_dir);

    std::string raw = "/files/uploads/%2e%2e/secret.txt";
    std::string decoded = minihttpd::url_decode(raw);
    LOG_INFO("decoded: " + decoded);

    bool ok = false;
    auto p1 = minihttpd::safe_join_under_root(std::filesystem::path(cfg.root_dir), "uploads/hello.txt", ok);
    LOG_INFO(std::string("safe_join uploads/hello.txt ok=") + (ok ? "true" : "false") +
             " path=" + (ok ? p1.string() : "(rejected)"));

    ok = false;
    auto p2 = minihttpd::safe_join_under_root(std::filesystem::path(cfg.root_dir), "../evil.txt", ok);
    LOG_INFO(std::string("safe_join ../evil.txt ok=") + (ok ? "true" : "false"));

    std::string html = minihttpd::error_page_html(403, "Forbidden", "Nope.");
    std::cout << "\n--- Sample 403 page (first 200 chars) ---\n"
              << html.substr(0, 200) << "...\n";

    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Fatal: " << e.what() << "\n";
    return 1;
  }
}
