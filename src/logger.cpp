#include "logger.hpp"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <cctype>

namespace minihttpd {

static std::string ts_now() {
  using clock = std::chrono::system_clock;
  auto now = clock::now();
  std::time_t t = clock::to_time_t(now);
  std::tm tm{};
#if defined(__unix__) || defined(__APPLE__)
  localtime_r(&t, &tm);
#else
  tm = *std::localtime(&t);
#endif
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
  return oss.str();
}

Logger& Logger::instance() {
  static Logger inst;
  return inst;
}

LogLevel parse_level(const std::string& s) {
  std::string u = s;
  for (auto& c : u) c = (char)std::toupper((unsigned char)c);

  if (u == "FATAL") return LogLevel::FATAL;
  if (u == "ERROR") return LogLevel::ERROR;
  if (u == "WARN" || u == "WARNING") return LogLevel::WARN;
  if (u == "INFO") return LogLevel::INFO;
  if (u == "DEBUG") return LogLevel::DEBUG;
  return LogLevel::INFO;
}

std::string level_to_string(LogLevel lvl) {
  switch (lvl) {
    case LogLevel::FATAL: return "FATAL";
    case LogLevel::ERROR: return "ERROR";
    case LogLevel::WARN:  return "WARN";
    case LogLevel::INFO:  return "INFO";
    case LogLevel::DEBUG: return "DEBUG";
  }
  return "INFO";
}

void Logger::configure(const std::string& file_path, LogLevel level) {
  std::lock_guard<std::mutex> lk(mu_);
  level_ = level;

  file_.open(file_path, std::ios::app);
  if (!file_) {
    std::cerr << "Warning, could not open log file: " << file_path << "\n";
  }
}

LogLevel Logger::level() const {
  std::lock_guard<std::mutex> lk(mu_);
  return level_;
}

void Logger::log(LogLevel lvl, const std::string& msg) {
  std::lock_guard<std::mutex> lk(mu_);
  if ((int)lvl > (int)level_) return;

  std::ostringstream line;
  line << ts_now()
       << " [" << level_to_string(lvl) << "]"
       << " [tid=" << std::this_thread::get_id() << "] "
       << msg << "\n";

  std::cout << line.str();
  std::cout.flush();

  if (file_) {
    file_ << line.str();
    file_.flush();
  }
}

}
