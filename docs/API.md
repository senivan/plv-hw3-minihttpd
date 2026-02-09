# API.md

## Static files (GET)
Served from `root_dir`.

Rules:
- `/` maps to `/index.html`
- Directories are not listed
  - if directory contains `index.html`, it is served
  - otherwise returns 403

Examples:
```bash
curl -v http://127.0.0.1:8080/
curl -v http://127.0.0.1:8080/index.html
```

## Storage API (`/files/*`)

### POST /files/<path> (upload)
Writes request body to `<root_dir>/<path>`.

Requirements:
- Must include `Content-Length` (chunked not supported).
- Intermediate directories are created if needed.

Example:
```bash
curl -X POST --data-binary @local.bin http://127.0.0.1:8080/files/uploads/local.bin
```

Responses:
- 200 OK: uploaded
- 400 Bad Request: missing/invalid Content-Length or invalid path
- 403 Forbidden: path forbidden
- 503 Service Unavailable: socket/IO error

### GET /files/<path> (download)
Streams file from `<root_dir>/<path>`.

Example:
```bash
curl -o out.bin http://127.0.0.1:8080/files/uploads/local.bin
```

Responses:
- 200 OK: file returned with Content-Length
- 404 Not Found: missing
- 403 Forbidden: forbidden path

### DELETE /files/<path>
Deletes `<root_dir>/<path>`.

Example:
```bash
curl -X DELETE http://127.0.0.1:8080/files/uploads/local.bin
```

Responses:
- 200 OK: deleted
- 404 Not Found: missing
- 403 Forbidden: delete forbidden or invalid path

## Keep-alive
Behavior:
- HTTP/1.1: keep-alive default unless `Connection: close`
- HTTP/1.0: close default unless `Connection: keep-alive`

Config:
- `keep_alive`
- `keep_alive_timeout_sec`
- `keep_alive_max_requests`
