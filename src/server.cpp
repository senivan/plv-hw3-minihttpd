#include "server.hpp"

#include "http.hpp"
#include "utils.hpp"
#include "logger.hpp"
#include "handlers.hpp"

#include <atomic>
#include <cerrno>
#include <cstring>
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

static std::string strip_query(const std::string& target) {
  auto q = target.find('?');
  if (q == std::string::npos) return target;
  return target.substr(0, q);
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

static void handle_client(int client_fd, const ServerConfig& cfg) {
  struct Guard {
    int fd;
    ~Guard() { close_quiet(fd); g_active_clients.fetch_sub(1); }
  } guard{client_fd};

  set_socket_timeouts(client_fd, cfg.keep_alive_timeout_sec);

  uint32_t handled = 0;
  std::string pending;

  while (true) {
    if (cfg.keep_alive && handled >= cfg.keep_alive_max_requests) break;

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
        send_error_response(client_fd, 400, false, "Header too large.");
        return;
      }
    }

    std::string header_blob = buf.substr(0, header_end);
    std::string after = buf.substr(header_end);

    HttpRequest req;
    std::string perr;
    if (!parse_http_request_headers(header_blob, req, perr)) {
      LOG_WARN("Bad request: " + perr);
      send_error_response(client_fd, 400, false, "Bad request.");
      return;
    }

    bool keep_alive = wants_keepalive(req, cfg);
    if (!keep_alive) set_socket_timeouts(client_fd, 2);

    std::string target_path_only = strip_query(req.target);
    LOG_INFO(req.method + " " + req.target);

    std::string body_prefix;
    uint64_t body_prefix_len = 0;

    if (req.content_length > 0) {
      uint64_t take = (after.size() > req.content_length) ? req.content_length : (uint64_t)after.size();
      body_prefix.assign(after.data(), (size_t)take);
      body_prefix_len = take;
      if ((uint64_t)after.size() > take) {
        pending = after.substr((size_t)take);
      }
    } else {
      pending = after;
    }

    bool ok = handle_request(client_fd, cfg, req, target_path_only, body_prefix, body_prefix_len);
    handled++;

    if (!ok) return;
    if (!keep_alive) break;
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
      send_error_response(client_fd, 503, false, "Server is busy.");
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
