#pragma once
#include "config.hpp"

namespace minihttpd {

class HttpServer {
public:
  explicit HttpServer(ServerConfig cfg);
  int run();

private:
  ServerConfig cfg_;
};

} 
