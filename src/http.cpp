#include "http.hpp"
#include "utils.hpp"

#include <sstream>
#include <ctime>
#include <iomanip>
#include <vector>
#include <cctype>
#include <limits>

namespace minihttpd {

std::string http_date_now() {
  std::time_t t = std::time(nullptr);
  std::tm gm{};
#if defined(__unix__) || defined(__APPLE__)
  gmtime_r(&t, &gm);
#else
  gm = *std::gmtime(&t);
#endif
  std::ostringstream oss;
  oss << std::put_time(&gm, "%a, %d %b %Y %H:%M:%S GMT");
  return oss.str();
}

std::string status_reason(int status) {
  switch (status) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 501: return "Not Implemented";
    case 503: return "Service Unavailable";
    default:  return "Unknown";
  }
}

std::string content_type_for_path(const std::string& path) {
  auto dot = path.find_last_of('.');
  std::string ext = (dot == std::string::npos) ? "" : to_lower(path.substr(dot + 1));

  if (ext == "html" || ext == "htm") return "text/html; charset=utf-8";
  if (ext == "txt")  return "text/plain; charset=utf-8";
  if (ext == "css")  return "text/css; charset=utf-8";
  if (ext == "js")   return "application/javascript; charset=utf-8";
  if (ext == "json") return "application/json; charset=utf-8";

  if (ext == "png")  return "image/png";
  if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
  if (ext == "gif")  return "image/gif";
  if (ext == "svg")  return "image/svg+xml";
  if (ext == "pdf")  return "application/pdf";

  return "application/octet-stream";
}

static bool is_token_char(char c) {
  return std::isalnum((unsigned char)c) || c == '-' || c == '_';
}

static bool parse_content_length(const std::string& v, uint64_t& out) {
  if (v.empty()) return false;
  uint64_t acc = 0;
  for (char c : v) {
    if (!std::isdigit((unsigned char)c)) return false;
    uint64_t d = (uint64_t)(c - '0');
    if (acc > (std::numeric_limits<uint64_t>::max() - d) / 10) return false;
    acc = acc * 10 + d;
  }
  out = acc;
  return true;
}

bool parse_http_request_headers(const std::string& header_blob, HttpRequest& out, std::string& err) {
  out = HttpRequest{};
  err.clear();

  std::vector<std::string> lines;
  lines.reserve(32);

  size_t pos = 0;
  while (pos < header_blob.size()) {
    size_t e = header_blob.find("\r\n", pos);
    if (e == std::string::npos) break;
    lines.push_back(header_blob.substr(pos, e - pos));
    pos = e + 2;
    if (pos >= header_blob.size()) break;
  }

  if (lines.empty()) { err = "empty request"; return false; }
  {
    std::istringstream iss(lines[0]);
    if (!(iss >> out.method >> out.target >> out.version)) {
      err = "invalid request line";
      return false;
    }

    if (out.version != "HTTP/1.1" && out.version != "HTTP/1.0") {
      err = "unsupported http version";
      return false;
    }

    for (char c : out.method) {
      if (!std::isupper((unsigned char)c)) { err = "invalid method"; return false; }
      if (!is_token_char(c)) { err = "invalid method token"; return false; }
    }

    if (out.target.empty() || out.target[0] != '/') {
      err = "invalid target";
      return false;
    }
  }

  for (size_t i = 1; i < lines.size(); i++) {
    const std::string& ln = lines[i];
    if (ln.empty()) break;

    auto p = ln.find(':');
    if (p == std::string::npos) { err = "bad header line"; return false; }

    std::string key = trim(ln.substr(0, p));
    std::string val = trim(ln.substr(p + 1));

    if (key.empty()) { err = "empty header name"; return false; }
    for (char c : key) {
      if (!is_token_char(c)) { err = "invalid header name"; return false; }
    }

    out.headers[to_lower(key)] = val;
  }
  auto it = out.headers.find("content-length");
  if (it != out.headers.end()) {
    uint64_t cl = 0;
    if (!parse_content_length(it->second, cl)) {
      err = "bad content-length";
      return false;
    }
    out.content_length = cl;
  }

  return true;
}

std::string build_response_head(const HttpResponseHead& head) {
  std::ostringstream oss;
  oss << "HTTP/1.1 " << head.status << " " << head.reason << "\r\n";
  for (const auto& kv : head.headers) {
    oss << kv.first << ": " << kv.second << "\r\n";
  }
  oss << "\r\n";
  return oss.str();
}

}
