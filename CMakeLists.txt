cmake_minimum_required(VERSION 3.16)
project(minihttpd LANGUAGES CXX)

if (WIN32)
  message(FATAL_ERROR "Currently only *nix systems are supported.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(minihttpd
  src/main.cpp
  src/logger.cpp
  src/config.cpp
)

target_include_directories(minihttpd PRIVATE include third_party)
target_compile_options(minihttpd PRIVATE -Wall -Wextra -Wpedantic)
