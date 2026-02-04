#include "config.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>
#include <limits>

namespace minihttpd {

using json = nlohmann::json;

static uint64_t get_u64(const json& j, const char* key, uint64_t def) {
  if (!j.contains(key)) return def;
  if (!j.at(key).is_number_integer()) {
    throw std::runtime_error(std::string("config key must be integer: ") + key);
  }
  auto v = j.at(key).get<long long>();
  if (v < 0) throw std::runtime_error(std::string("config key must be non-negative: ") + key);
  return static_cast<uint64_t>(v);
}

static std::string get_str(const json& j, const char* key, const std::string& def) {
  if (!j.contains(key)) return def;
  if (!j.at(key).is_string()) {
    throw std::runtime_error(std::string("config key must be string: ") + key);
  }
  return j.at(key).get<std::string>();
}

static bool get_bool(const json& j, const char* key, bool def) {
  if (!j.contains(key)) return def;
  if (!j.at(key).is_boolean()) {
    throw std::runtime_error(std::string("config key must be boolean: ") + key);
  }
  return j.at(key).get<bool>();
}

ServerConfig load_config_json(const std::string& path) {
  std::ifstream f(path);
  if (!f) throw std::runtime_error("cannot open config file: " + path);

  json j;
  try {
    f >> j;
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("invalid JSON: ") + e.what());
  }

  if (!j.is_object()) throw std::runtime_error("config root must be a JSON object");

  ServerConfig cfg;

  cfg.server_ip = get_str(j, "server_ip", cfg.server_ip);

  {
    auto p = get_u64(j, "port", cfg.port);
    if (p == 0 || p > 65535) throw std::runtime_error("port must be 1..65535");
    cfg.port = static_cast<uint16_t>(p);
  }

  {
    auto mc = get_u64(j, "max_clients", cfg.max_clients);
    if (mc == 0 || mc > std::numeric_limits<uint32_t>::max()) {
      throw std::runtime_error("max_clients must be 1..4294967295");
    }
    cfg.max_clients = static_cast<uint32_t>(mc);
  }

  cfg.root_dir = get_str(j, "root_dir", cfg.root_dir);

  cfg.log_file = get_str(j, "log_file", cfg.log_file);
  cfg.log_level = get_str(j, "log_level", cfg.log_level);

  cfg.keep_alive = get_bool(j, "keep_alive", cfg.keep_alive);

  {
    auto t = get_u64(j, "keep_alive_timeout_sec", cfg.keep_alive_timeout_sec);
    if (cfg.keep_alive && t == 0) throw std::runtime_error("keep_alive_timeout_sec must be > 0 when keep_alive=true");
    if (t > std::numeric_limits<uint32_t>::max()) throw std::runtime_error("keep_alive_timeout_sec too large");
    cfg.keep_alive_timeout_sec = static_cast<uint32_t>(t);
  }

  {
    auto m = get_u64(j, "keep_alive_max_requests", cfg.keep_alive_max_requests);
    if (cfg.keep_alive && m == 0) throw std::runtime_error("keep_alive_max_requests must be > 0 when keep_alive=true");
    if (m > std::numeric_limits<uint32_t>::max()) throw std::runtime_error("keep_alive_max_requests too large");
    cfg.keep_alive_max_requests = static_cast<uint32_t>(m);
  }

  {
    auto v = get_u64(j, "read_header_max_bytes", cfg.read_header_max_bytes);
    if (v < 1024) throw std::runtime_error("read_header_max_bytes too small (min 1024)");
    if (v > std::numeric_limits<uint32_t>::max()) throw std::runtime_error("read_header_max_bytes too large");
    cfg.read_header_max_bytes = static_cast<uint32_t>(v);
  }

  {
    auto v = get_u64(j, "recv_chunk_size", cfg.recv_chunk_size);
    if (v < 1024) throw std::runtime_error("recv_chunk_size too small (min 1024)");
    if (v > std::numeric_limits<uint32_t>::max()) throw std::runtime_error("recv_chunk_size too large");
    cfg.recv_chunk_size = static_cast<uint32_t>(v);
  }

  if (cfg.server_ip.empty()) throw std::runtime_error("server_ip must not be empty");
  if (cfg.root_dir.empty()) throw std::runtime_error("root_dir must not be empty");

  return cfg;
}

}
