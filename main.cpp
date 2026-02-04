#include "config.hpp"
#include "logger.hpp"
#include "http.hpp"

#include <iostream>

int main(int argc, char** argv) {
  const char* cfg_path = "../example.config.json";
  if (argc >= 2) cfg_path = argv[1];

  try {
    auto cfg = minihttpd::load_config_json(cfg_path);

    minihttpd::Logger::instance().configure(
      cfg.log_file,
      minihttpd::parse_level(cfg.log_level)
    );

    LOG_INFO("Config loaded successfully");
    LOG_INFO("server_ip=" + cfg.server_ip + " port=" + std::to_string(cfg.port));

    const std::string sample =
      "GET /files/test.txt?x=1 HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "User-Agent: minihttpd-test\r\n"
      "Connection: keep-alive\r\n"
      "Content-Length: 123\r\n"
      "\r\n";

    minihttpd::HttpRequest req;
    std::string err;
    if (!minihttpd::parse_http_request_headers(sample, req, err)) {
      LOG_ERROR("HTTP parse failed: " + err);
      return 2;
    }

    LOG_INFO("HTTP parse OK");
    LOG_INFO("method=" + req.method);
    LOG_INFO("target=" + req.target);
    LOG_INFO("version=" + req.version);
    LOG_INFO("content_length=" + std::to_string(req.content_length));

    auto host_it = req.headers.find("host");
    if (host_it != req.headers.end()) LOG_INFO("host=" + host_it->second);

    auto conn_it = req.headers.find("connection");
    if (conn_it != req.headers.end()) LOG_INFO("connection=" + conn_it->second);

    minihttpd::HttpResponseHead head;
    head.status = 200;
    head.reason = minihttpd::status_reason(200);
    head.headers["Date"] = minihttpd::http_date_now();
    head.headers["Server"] = "minihttpd";
    head.headers["Content-Type"] = "text/plain; charset=utf-8";
    head.headers["Content-Length"] = "5";

    std::string resp_head = minihttpd::build_response_head(head);
    std::cout << "\n--- Sample response head ---\n" << resp_head << "\n";

    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Fatal: " << e.what() << "\n";
    return 1;
  }
}
