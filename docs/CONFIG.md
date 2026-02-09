# CONFIG.md

Example `config.json`:
```json
{
  "server_ip": "127.0.0.1",
  "port": 8080,
  "max_clients": 128,
  "root_dir": "./www",
  "log_file": "./server.log",
  "log_level": "INFO",
  "keep_alive": true,
  "keep_alive_timeout_sec": 10,
  "keep_alive_max_requests": 100,
  "read_header_max_bytes": 32768,
  "recv_chunk_size": 65536
}
```

## Fields
- `server_ip` (string): IPv4 address to bind to.
- `port` (int): 1..65535
- `max_clients` (int): max concurrent connections.
- `root_dir` (string): root directory for static hosting and `/files/*`.
- `log_file` (string): log output file path.
- `log_level` (string): FATAL/ERROR/WARN/INFO/DEBUG
- `keep_alive` (bool): enable/disable keep-alive.
- `keep_alive_timeout_sec` (int): socket timeouts.
- `keep_alive_max_requests` (int): cap requests per connection.
- `read_header_max_bytes` (int): max header size before 400.
- `recv_chunk_size` (int): chunk size for streaming and socket reads.
