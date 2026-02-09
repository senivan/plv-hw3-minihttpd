#include "handlers.hpp"

#include "utils.hpp"
#include "logger.hpp"

#include <filesystem>
#include <fstream>
#include <optional>
#include <system_error>
#include <vector>
#include <cerrno>
#include <cstring>

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>

#if defined(__linux__)
  #include <sys/sendfile.h>
#elif defined(__APPLE__)
  #include <sys/uio.h>
  #include <sys/socket.h>
#endif

namespace minihttpd {

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

static bool file_exists_regular(const std::filesystem::path& p) {
  std::error_code ec;
  return std::filesystem::exists(p, ec) && std::filesystem::is_regular_file(p, ec);
}

static bool path_is_directory(const std::filesystem::path& p) {
  std::error_code ec;
  return std::filesystem::exists(p, ec) && std::filesystem::is_directory(p, ec);
}

static std::optional<uint64_t> file_size_u64(const std::filesystem::path& p) {
  std::error_code ec;
  auto sz = std::filesystem::file_size(p, ec);
  if (ec) return std::nullopt;
  return (uint64_t)sz;
}

static bool send_file_body(int client_fd, const std::filesystem::path& file_path, uint64_t size, uint32_t chunk_size) {
#if defined(__linux__)
  int in_fd = ::open(file_path.c_str(), O_RDONLY);
  if (in_fd < 0) return false;

  off_t offset = 0;
  uint64_t remaining = size;

  while (remaining > 0) {
    size_t to_send = (remaining > (uint64_t)chunk_size) ? (size_t)chunk_size : (size_t)remaining;
    ssize_t n = ::sendfile(client_fd, in_fd, &offset, to_send);
    if (n < 0) {
      if (errno == EINTR) continue;
      if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
      ::close(in_fd);
      return false;
    }
    if (n == 0) break;
    remaining -= (uint64_t)n;
  }

  ::close(in_fd);
  return remaining == 0;

#elif defined(__APPLE__)
  int in_fd = ::open(file_path.c_str(), O_RDONLY);
  if (in_fd < 0) return false;

  off_t offset = 0;
  off_t len = (off_t)size;

  while (len > 0) {
    off_t sent = len; 
    int rc = ::sendfile(in_fd, client_fd, offset, &sent, nullptr, 0);
    if (rc == 0) {
      offset += sent;
      len -= sent;
      continue;
    }
    if (errno == EINTR) continue;
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      offset += sent;
      len -= sent;
      continue;
    }
    ::close(in_fd);
    return false;
  }

  ::close(in_fd);
  return true;

#else
  std::ifstream in(file_path, std::ios::binary);
  if (!in) return false;

  std::vector<char> buf(chunk_size);
  uint64_t remaining = size;

  while (remaining > 0) {
    size_t to_read = (remaining > (uint64_t)buf.size()) ? buf.size() : (size_t)remaining;
    in.read(buf.data(), (std::streamsize)to_read);
    std::streamsize got = in.gcount();
    if (got <= 0) break;
    if (!send_all(client_fd, buf.data(), (size_t)got)) return false;
    remaining -= (uint64_t)got;
  }

  return remaining == 0;
#endif
}

static bool drain_socket_bytes(int fd, const ServerConfig& cfg, uint64_t n) {
  std::vector<char> buf(cfg.recv_chunk_size);
  uint64_t remaining = n;
  while (remaining > 0) {
    size_t want = (remaining > (uint64_t)buf.size()) ? buf.size() : (size_t)remaining;
    ssize_t r = ::recv(fd, buf.data(), want, 0);
    if (r < 0) {
      if (errno == EINTR) continue;
      return false;
    }
    if (r == 0) return false;
    remaining -= (uint64_t)r;
  }
  return true;
}

void send_error_response(int fd, int status, bool keep_alive, const std::string& detail) {
  HttpResponseHead head;
  head.status = status;
  head.reason = status_reason(status);

  std::string msg = detail.empty()
    ? "minihttpd could not process your request."
    : detail;

  std::string body = error_page_html(status, head.reason, msg);

  head.headers["Date"] = http_date_now();
  head.headers["Server"] = "minihttpd";
  head.headers["Content-Type"] = "text/html; charset=utf-8";
  head.headers["Content-Length"] = std::to_string(body.size());
  head.headers["Connection"] = keep_alive ? "keep-alive" : "close";

  std::string hdr = build_response_head(head);
  (void)send_string(fd, hdr);
  (void)send_string(fd, body);
}

static void send_text_200(int fd, bool keep_alive, const std::string& body, const std::string& content_type = "text/plain; charset=utf-8") {
  HttpResponseHead head;
  head.status = 200;
  head.reason = "OK";
  head.headers["Date"] = http_date_now();
  head.headers["Server"] = "minihttpd";
  head.headers["Content-Type"] = content_type;
  head.headers["Content-Length"] = std::to_string(body.size());
  head.headers["Connection"] = keep_alive ? "keep-alive" : "close";

  std::string hdr = build_response_head(head);
  (void)send_string(fd, hdr);
  (void)send_string(fd, body);
}

static bool send_static_file(int fd, const ServerConfig& cfg, const std::filesystem::path& fs_path, bool keep_alive) {
  auto sz = file_size_u64(fs_path);
  if (!sz.has_value()) {
    send_error_response(fd, 503, keep_alive, "Could not read file size.");
    return true;
  }

  HttpResponseHead head;
  head.status = 200;
  head.reason = "OK";
  head.headers["Date"] = http_date_now();
  head.headers["Server"] = "minihttpd";
  head.headers["Content-Type"] = content_type_for_path(fs_path.string());
  head.headers["Content-Length"] = std::to_string(*sz);
  head.headers["Connection"] = keep_alive ? "keep-alive" : "close";

  std::string hdr = build_response_head(head);
  if (!send_string(fd, hdr)) return false;

  if (!send_file_body(fd, fs_path, *sz, cfg.recv_chunk_size)) {
    return false;
  }
  return true;
}

static bool handle_get_static(int fd, const ServerConfig& cfg, const std::string& target_path, bool keep_alive) {
  // target_path should be like "/index.html" (no query)
  std::filesystem::path root(cfg.root_dir);

  std::string decoded = url_decode(target_path);

  bool ok = false;
  auto fs_path = safe_join_under_root(root, decoded, ok);
  if (!ok) {
    send_error_response(fd, 403, keep_alive, "Forbidden path.");
    return true;
  }

  if (path_is_directory(fs_path)) {
    auto idx = fs_path / "index.html";
    if (file_exists_regular(idx)) {
      fs_path = idx;
    } else {
      send_error_response(fd, 403, keep_alive, "Directory listing is forbidden.");
      return true;
    }
  }

  if (!file_exists_regular(fs_path)) {
    send_error_response(fd, 404, keep_alive, "Not found.");
    return true;
  }

  return send_static_file(fd, cfg, fs_path, keep_alive);
}

static std::optional<std::filesystem::path> resolve_files_path(const ServerConfig& cfg, const std::string& target_path, bool& forbidden) {
  forbidden = false;

  if (target_path == "/files" || target_path == "/files/") return std::nullopt;
  if (target_path.rfind("/files/", 0) != 0) return std::nullopt;

  std::string rel = target_path.substr(std::string("/files/").size());
  rel = url_decode(rel);

  bool ok = false;
  auto p = safe_join_under_root(std::filesystem::path(cfg.root_dir), rel, ok);
  if (!ok) { forbidden = true; return std::nullopt; }
  return p;
}

static bool handle_files_get(int fd, const ServerConfig& cfg, const std::string& target_path, bool keep_alive) {
  bool forbidden = false;
  auto p = resolve_files_path(cfg, target_path, forbidden);
  if (!p.has_value()) {
    send_error_response(fd, forbidden ? 403 : 400, keep_alive, forbidden ? "Forbidden path." : "Bad /files path.");
    return true;
  }

  if (!file_exists_regular(*p)) {
    send_error_response(fd, 404, keep_alive, "Not found.");
    return true;
  }

  auto sz = file_size_u64(*p);
  if (!sz.has_value()) {
    send_error_response(fd, 503, keep_alive, "Could not read file size.");
    return true;
  }

  HttpResponseHead head;
  head.status = 200;
  head.reason = "OK";
  head.headers["Date"] = http_date_now();
  head.headers["Server"] = "minihttpd";
  head.headers["Content-Type"] = "application/octet-stream";
  head.headers["Content-Length"] = std::to_string(*sz);
  head.headers["Connection"] = keep_alive ? "keep-alive" : "close";

  std::string hdr = build_response_head(head);
  if (!send_string(fd, hdr)) return false;
  return send_file_body(fd, *p, *sz, cfg.recv_chunk_size);
}

static bool handle_files_delete(int fd, const ServerConfig& cfg, const std::string& target_path, bool keep_alive) {
  bool forbidden = false;
  auto p = resolve_files_path(cfg, target_path, forbidden);
  if (!p.has_value()) {
    send_error_response(fd, forbidden ? 403 : 400, keep_alive, forbidden ? "Forbidden path." : "Bad /files path.");
    return true;
  }

  std::error_code ec;
  if (!std::filesystem::exists(*p, ec)) {
    send_error_response(fd, 404, keep_alive, "Not found.");
    return true;
  }
  if (!std::filesystem::is_regular_file(*p, ec)) {
    send_error_response(fd, 403, keep_alive, "Not a regular file.");
    return true;
  }

  bool ok = std::filesystem::remove(*p, ec);
  if (!ok || ec) {
    send_error_response(fd, 403, keep_alive, "Could not delete file.");
    return true;
  }

  send_text_200(fd, keep_alive, "deleted\n");
  return true;
}

static bool handle_files_post_upload(
  int fd,
  const ServerConfig& cfg,
  const std::string& target_path,
  uint64_t content_length,
  const std::string& body_prefix,
  uint64_t body_prefix_len,
  bool keep_alive)
{
  bool forbidden = false;
  auto p = resolve_files_path(cfg, target_path, forbidden);
  if (!p.has_value()) {
    send_error_response(fd, forbidden ? 403 : 400, keep_alive, forbidden ? "Forbidden path." : "Bad /files path.");
    return true;
  }

  std::error_code ec;
  std::filesystem::create_directories(p->parent_path(), ec);
  if (ec) {
    send_error_response(fd, 503, keep_alive, "Could not create directories.");
    return true;
  }

  std::ofstream out(*p, std::ios::binary | std::ios::trunc);
  if (!out) {
    send_error_response(fd, 403, keep_alive, "Could not open file for writing.");
    return true;
  }

  if (body_prefix_len > 0) {
    out.write(body_prefix.data(), (std::streamsize)body_prefix_len);
    if (!out) {
      send_error_response(fd, 503, keep_alive, "Write error.");
      return true;
    }
  }

  uint64_t remaining = content_length - body_prefix_len;
  std::vector<char> buf(cfg.recv_chunk_size);

  while (remaining > 0) {
    size_t want = (remaining > (uint64_t)buf.size()) ? buf.size() : (size_t)remaining;
    ssize_t r = ::recv(fd, buf.data(), want, 0);
    if (r < 0) {
      if (errno == EINTR) continue;
      send_error_response(fd, 503, keep_alive, "Socket read error during upload.");
      return true;
    }
    if (r == 0) {
      send_error_response(fd, 400, false, "Unexpected EOF during upload.");
      return false; 
    }
    out.write(buf.data(), r);
    if (!out) {
      send_error_response(fd, 503, keep_alive, "Write error.");
      return true;
    }
    remaining -= (uint64_t)r;
  }

  out.close();
  send_text_200(fd, keep_alive, "uploaded\n");
  return true;
}

bool handle_request(
  int fd,
  const ServerConfig& cfg,
  const HttpRequest& req,
  const std::string& target_path_only,
  const std::string& body_prefix,
  uint64_t body_prefix_len)
{
  bool keep_alive = true;
  auto it = req.headers.find("connection");
  if (!cfg.keep_alive) keep_alive = false;
  else if (req.version == "HTTP/1.1") {
    keep_alive = !(it != req.headers.end() && to_lower(it->second).find("close") != std::string::npos);
  } else {
    keep_alive = (it != req.headers.end() && to_lower(it->second).find("keep-alive") != std::string::npos);
  }

  auto drain_ignored_body = [&](uint64_t already, uint64_t total) -> bool {
    if (total <= already) return true;
    return drain_socket_bytes(fd, cfg, total - already);
  };

  if (req.method == "GET") {
    if (!drain_ignored_body(body_prefix_len, req.content_length)) return false;

    if (target_path_only.rfind("/files", 0) == 0) {
      return handle_files_get(fd, cfg, target_path_only, keep_alive);
    }

    std::string t = target_path_only;
    if (t == "/") t = "/index.html";
    return handle_get_static(fd, cfg, t, keep_alive);
  }

  if (req.method == "DELETE") {
    if (!drain_ignored_body(body_prefix_len, req.content_length)) return false;

    if (target_path_only.rfind("/files", 0) != 0) {
      send_error_response(fd, 404, keep_alive, "Not found.");
      return true;
    }
    return handle_files_delete(fd, cfg, target_path_only, keep_alive);
  }

  if (req.method == "POST") {
    auto has_cl = req.headers.find("content-length") != req.headers.end();
    if (!has_cl) {
      send_error_response(fd, 400, keep_alive, "POST requires Content-Length.");
      return false;
    }

    if (target_path_only.rfind("/files", 0) != 0) {
      if (!drain_ignored_body(body_prefix_len, req.content_length)) return false;
      send_error_response(fd, 404, keep_alive, "Not found.");
      return true;
    }

    if (req.content_length < body_prefix_len) {
      send_error_response(fd, 400, keep_alive, "Bad Content-Length.");
      return false;
    }

    return handle_files_post_upload(
      fd, cfg, target_path_only,
      req.content_length,
      body_prefix, body_prefix_len,
      keep_alive
    );
  }
  if (!drain_ignored_body(body_prefix_len, req.content_length)) return false;
  send_error_response(fd, 501, keep_alive, "Method not implemented.");
  return true;
}

}
