#pragma once
#include <string>
#include <filesystem>

namespace minihttpd {

std::string trim(const std::string& s);
std::string to_lower(const std::string& s);

std::string url_decode(const std::string& s);

std::string html_escape(const std::string& s);
std::string error_page_html(int status, const std::string& title, const std::string& detail);


bool is_within_root(const std::filesystem::path& root, const std::filesystem::path& candidate);

std::filesystem::path safe_join_under_root(
  const std::filesystem::path& root,
  const std::string& rel,
  bool& ok
);

} 
