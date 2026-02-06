#include "server.hpp"

#include "http.hpp"
#include "utils.hpp"
#include "logger.hpp"

#include <atomic>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <thread>
#include <vector>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace minihttpd {

static std::atomic<uint32_t> g_active_clients{0};

static void close_quiet(int fd) {
  if (fd >= 0) ::close(fd);
}

static void set_socket_timeouts(int fd, uint32_t rcv_timeout_sec) {
  timeval tv{};
  tv.tv_sec = (int)rcv_timeout_sec;
  tv.tv_usec = 0;
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

static bool send_all(int fd, const void* data, size_t len) {
  const char* p = (const char*)data;
  size_t sent = 0;
  while (sent < len) {
    ssize_t n = ::send(fd, p + sent, len - sent, 0);
    if (n < 0) {
      if (errno == EINTR) continue;
      return false;
    }
    if (n == 0) return false;
    sent += (size_t)n;
  }
  return true;
}

static bool send_string(int fd, const std::string& s) {
  return send_all(fd, s.data(), s.size());
}

static bool wants_keepalive(const HttpRequest& req, const ServerConfig& cfg) {
  if (!cfg.keep_alive) return false;

  auto it = req.headers.find("connection");
  std::string conn = (it != req.headers.end()) ? to_lower(it->second) : "";

  if (req.version == "HTTP/1.1") {
    if (conn.find("close") != std::string::npos) return false;
    return true;
  }

  if (conn.find("keep-alive") != std::string::npos) return true;
  return false;
}

static void send_error(int fd, int status, bool keep_alive) {
  HttpResponseHead head;
  head.status = status;
  head.reason = status_reason(status);

  std::string body = error_page_html(status, head.reason, "minihttpd could not process your request.");

  head.headers["Date"] = http_date_now();
  head.headers["Server"] = "minihttpd";
  head.headers["Content-Type"] = "text/html; charset=utf-8";
  head.headers["Content-Length"] = std::to_string(body.size());
  head.headers["Connection"] = keep_alive ? "keep-alive" : "close";

  std::string hdr = build_response_head(head);
  (void)send_string(fd, hdr);
  (void)send_string(fd, body);
}

static void send_stub_response(int fd, const ServerConfig& cfg, const HttpRequest& req) {
  bool ka = wants_keepalive(req, cfg);

  int status = (req.method == "GET") ? 200 : 501;

  HttpResponseHead head;
  head.status = status;
  head.reason = status_reason(status);

  std::ostringstream body;
  body << "<!doctype html><html><head><meta charset=\"utf-8\"/>"
       << "<title>" << head.status << " " << html_escape(head.reason) << "</title>"
       << "</head><body style=\"font-family:sans-serif;\">"
       << "<h1>" << head.status << " " << html_escape(head.reason) << "</h1>"
       << "<p><b>Method:</b> " << html_escape(req.method) << "</p>"
       << "<p><b>Target:</b> " << html_escape(req.target) << "</p>"
       << "<p>This is Module 5 (socket core). Routing + storage comes in Module 6.</p>"
       << "</body></html>";

  std::string body_str = body.str();

  head.headers["Date"] = http_date_now();
  head.headers["Server"] = "minihttpd";
  head.headers["Content-Type"] = "text/html; charset=utf-8";
  head.headers["Content-Length"] = std::to_string(body_str.size());
  head.headers["Connection"] = ka ? "keep-alive" : "close";
  if (ka) {
    head.headers["Keep-Alive"] =
      "timeout=" + std::to_string(cfg.keep_alive_timeout_sec) +
      ", max=" + std::to_string(cfg.keep_alive_max_requests);
  }

  std::string hdr = build_response_head(head);
  (void)send_string(fd, hdr);
  (void)send_string(fd, body_str);
}

static bool drain_body(int fd, const ServerConfig& cfg, uint64_t content_length, std::string& already, std::string& pending_out) {
  pending_out.clear();

  if (content_length == 0) {
    pending_out = std::move(already);
    already.clear();
    return true;
  }

  uint64_t have = already.size();
  uint64_t take = (have > content_length) ? content_length : have;

  if (have > take) {
    pending_out = already.substr((size_t)take);
  }

  uint64_t remaining = content_length - take;

  std::vector<char> buf(cfg.recv_chunk_size);
  while (remaining > 0) {
    size_t want = (remaining > (uint64_t)buf.size()) ? buf.size() : (size_t)remaining;
    ssize_t n = ::recv(fd, buf.data(), want, 0);
    if (n < 0) {
      if (errno == EINTR) continue;
      return false;
    }
    if (n == 0) return false;
    remaining -= (uint64_t)n;
  }

  already.clear();
  return true;
}

static void handle_client(int client_fd, const ServerConfig& cfg) {
  struct Guard {
    int fd;
    ~Guard() { close_quiet(fd); g_active_clients.fetch_sub(1); }
  } guard{client_fd};

  set_socket_timeouts(client_fd, cfg.keep_alive_timeout_sec);

  uint32_t handled = 0;
  std::string pending;

  while (true) {
    if (cfg.keep_alive && handled >= cfg.keep_alive_max_requests) {
      LOG_DEBUG("keep-alive max requests reached, closing");
      break;
    }

    std::string buf = pending;
    pending.clear();

    std::vector<char> tmp(cfg.recv_chunk_size);
    size_t header_end = std::string::npos;

    while (header_end == std::string::npos) {
      header_end = buf.find("\r\n\r\n");
      if (header_end != std::string::npos) { header_end += 4; break; }

      ssize_t n = ::recv(client_fd, tmp.data(), tmp.size(), 0);
      if (n < 0) {
        if (errno == EINTR) continue;
        return;
      }
      if (n == 0) return;
      buf.append(tmp.data(), (size_t)n);

      if (buf.size() > cfg.read_header_max_bytes) {
        LOG_WARN("Header too large -> 400");
        send_error(client_fd, 400, false);
        return;
      }
    }

    std::string header_blob = buf.substr(0, header_end);
    std::string after = buf.substr(header_end); 
    HttpRequest req;
    std::string perr;
    if (!parse_http_request_headers(header_blob, req, perr)) {
      LOG_WARN("Bad request: " + perr);
      send_error(client_fd, 400, false);
      return;
    }

    bool ka = wants_keepalive(req, cfg);
    LOG_INFO(req.method + " " + req.target + " (" + (ka ? "keep-alive" : "close") + ")");

    std::string next_pending;
    if (!drain_body(client_fd, cfg, req.content_length, after, next_pending)) {
      return;
    }
    pending = std::move(next_pending);

    if (req.method == "GET" || req.method == "POST" || req.method == "DELETE") {
      send_stub_response(client_fd, cfg, req);
    } else {
      send_error(client_fd, 501, ka);
    }

    handled++;
    if (!ka) break;
  }
}

HttpServer::HttpServer(ServerConfig cfg) : cfg_(std::move(cfg)) {}

int HttpServer::run() {
  int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    LOG_FATAL(std::string("socket() failed: ") + std::strerror(errno));
    return 1;
  }

  int yes = 1;
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(cfg_.port);

  if (::inet_pton(AF_INET, cfg_.server_ip.c_str(), &addr.sin_addr) != 1) {
    LOG_FATAL("Invalid server_ip: " + cfg_.server_ip);
    close_quiet(listen_fd);
    return 1;
  }

  if (::bind(listen_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
    LOG_FATAL(std::string("bind() failed: ") + std::strerror(errno));
    close_quiet(listen_fd);
    return 1;
  }

  if (::listen(listen_fd, (int)cfg_.max_clients) < 0) {
    LOG_FATAL(std::string("listen() failed: ") + std::strerror(errno));
    close_quiet(listen_fd);
    return 1;
  }

  LOG_INFO("Listening on " + cfg_.server_ip + ":" + std::to_string(cfg_.port));

  while (true) {
    sockaddr_in caddr{};
    socklen_t clen = sizeof(caddr);

    int client_fd = ::accept(listen_fd, (sockaddr*)&caddr, &clen);
    if (client_fd < 0) {
      if (errno == EINTR) continue;
      LOG_ERROR(std::string("accept() failed: ") + std::strerror(errno));
      continue;
    }

    uint32_t cur = g_active_clients.load();
    if (cur >= cfg_.max_clients) {
      LOG_WARN("Max clients reached, sending 503");
      send_error(client_fd, 503, false);
      close_quiet(client_fd);
      continue;
    }

    g_active_clients.fetch_add(1);
    std::thread([client_fd, cfg = cfg_]() { handle_client(client_fd, cfg); }).detach();
  }

  close_quiet(listen_fd);
  return 0;
}

}
