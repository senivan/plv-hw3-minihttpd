#pragma once
#include <string>
#include <mutex>
#include <fstream>

namespace minihttpd {

enum class LogLevel { FATAL=0, ERROR=1, WARN=2, INFO=3, DEBUG=4 };

LogLevel parse_level(const std::string& s);
std::string level_to_string(LogLevel lvl);

class Logger {
public:
  static Logger& instance();
  void configure(const std::string& file_path, LogLevel level);
  void log(LogLevel lvl, const std::string& msg);

  LogLevel level() const;
private:
  Logger() = default;

  mutable std::mutex mu_;
  std::ofstream file_;
  LogLevel level_{LogLevel::INFO};
};

}

#define LOG_FATAL(msg) ::minihttpd::Logger::instance().log(::minihttpd::LogLevel::FATAL, (msg))
#define LOG_ERROR(msg) ::minihttpd::Logger::instance().log(::minihttpd::LogLevel::ERROR, (msg))
#define LOG_WARN(msg)  ::minihttpd::Logger::instance().log(::minihttpd::LogLevel::WARN,  (msg))
#define LOG_INFO(msg)  ::minihttpd::Logger::instance().log(::minihttpd::LogLevel::INFO,  (msg))
#define LOG_DEBUG(msg) ::minihttpd::Logger::instance().log(::minihttpd::LogLevel::DEBUG, (msg))
