# plv-hw3-minihttpd


Small HTTP server implemented in C++.

## Requirements
- [X] Basic HTTP/1.1 request/response handling
- [ ] Methods: `GET`, `POST`, `DELETE`
- [ ] Status codes: `200`, `400`, `403`, `404`, `501`, `503` (HTML error pages for errors)
- [ ] Transfer big files (> 1GB) by streaming (no buffering into RAM)
- [ ] File storage API (upload/download/delete) under a server root directory
- [X] Configurable via config file (JSON/YAML/TOML; current plan: JSON). server IP, port, max clients, root directory, log file, keep-alive settings, etc.
- [ ] Keep-alive support (configurable)
- [ ] Multi-client handling (concurrency depends on system)
- [ ] Unit tests using PyTest (black-box, positive + negative)
- [ ] High Level Design + documentation (diagrams, configuration, logging, etc.)

## Build
- Requires CMake and a compiler

Build:
```bash
mkdir -p build
cd build
cmake ..
make
```