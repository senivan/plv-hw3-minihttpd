#include "utils.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <system_error>

namespace minihttpd {

std::string trim(const std::string& s) {
  size_t b = 0;
  while (b < s.size() && std::isspace((unsigned char)s[b])) b++;
  size_t e = s.size();
  while (e > b && std::isspace((unsigned char)s[e - 1])) e--;
  return s.substr(b, e - b);
}

std::string to_lower(const std::string& s) {
  std::string out = s;
  for (auto& c : out) c = (char)std::tolower((unsigned char)c);
  return out;
}

static int hexval(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  return -1;
}

std::string url_decode(const std::string& s) {
  std::string out;
  out.reserve(s.size());

  for (size_t i = 0; i < s.size(); i++) {
    char c = s[i];
    if (c == '%' && i + 2 < s.size()) {
      int hi = hexval(s[i + 1]);
      int lo = hexval(s[i + 2]);
      if (hi >= 0 && lo >= 0) {
        out.push_back((char)((hi << 4) | lo));
        i += 2;
      } else {
        out.push_back(c);
      }
    } else if (c == '+') {
      out.push_back(' ');
    } else {
      out.push_back(c);
    }
  }

  return out;
}

std::string html_escape(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default: out.push_back(c); break;
    }
  }
  return out;
}

std::string error_page_html(int status, const std::string& title, const std::string& detail) {
  std::ostringstream oss;
  oss << "<!doctype html><html><head><meta charset=\"utf-8\"/>"
      << "<title>" << status << " " << html_escape(title) << "</title>"
      << "</head><body style=\"font-family: sans-serif;\">"
      << "<h1>" << status << " " << html_escape(title) << "</h1>"
      << "<p>" << html_escape(detail) << "</p>"
      << "<hr/><p><small>minihttpd</small></p>"
      << "</body></html>";
  return oss.str();
}

static std::filesystem::path weakly_canon(const std::filesystem::path& p) {
  std::error_code ec;
  auto r = std::filesystem::weakly_canonical(p, ec);
  if (ec) {
    return p.lexically_normal();
  }
  return r.lexically_normal();
}

bool is_within_root(const std::filesystem::path& root, const std::filesystem::path& candidate) {
  auto r = weakly_canon(root);
  auto c = weakly_canon(candidate);

  auto rit = r.begin();
  auto cit = c.begin();
  for (; rit != r.end(); ++rit, ++cit) {
    if (cit == c.end()) return false;
    if (*rit != *cit) return false;
  }
  return true;
}

std::filesystem::path safe_join_under_root(const std::filesystem::path& root, const std::string& rel, bool& ok) {
  ok = false;

  if (rel.find('\0') != std::string::npos) return {};

  std::string cleaned = rel;
  while (!cleaned.empty() && (cleaned.front() == '/' || cleaned.front() == '\\')) {
    cleaned.erase(cleaned.begin());
  }

  std::filesystem::path relp(cleaned);

  for (const auto& part : relp) {
    if (part == "..") return {};
    if (part == ".") continue;
  }

  auto joined = root / relp;

  if (!is_within_root(root, joined)) return {};

  ok = true;
  return joined;
}

} 
