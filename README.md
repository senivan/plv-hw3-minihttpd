# minihttpd

A small HTTP file server.

- Methods: GET, POST, DELETE
- Status codes: 200, 400, 403, 404, 501, 503 
- Streams big files (>1GB)
- Static file hosting from a configurable root directory
- Storage API under `/files/*` for upload/download/delete
- Configurable via JSON config
- Optional keep-alive
- Handles multiple clients

## Build

Requirements:
- CMake 3.16+
- C++20 compiler

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Run

```bash
./build/minihttpd ./config.json
```

You should see a log line like:
`Listening on 127.0.0.1:8080`

## Usage

### Static files
Static files are served from `root_dir`.

- `/` maps to `/index.html`
- directories do not list contents (403) unless `index.html` exists

Examples:
```bash
curl -v http://127.0.0.1:8080/
curl -v http://127.0.0.1:8080/index.html
```

### Storage API: `/files/*`

Upload:
```bash
curl -X POST --data-binary @local.bin http://127.0.0.1:8080/files/uploads/local.bin
```

Download:
```bash
curl -o downloaded.bin http://127.0.0.1:8080/files/uploads/local.bin
```

Delete:
```bash
curl -X DELETE http://127.0.0.1:8080/files/uploads/local.bin
```

Notes:
- `POST` requires `Content-Length`

## Tests

Run:
```bash
pytest -q
```

## Docs

See `docs/`:
- `docs/HLD.md` High-level design and diagrams
- `docs/API.md` HTTP routes and behavior
- `docs/CONFIG.md` Configuration reference
