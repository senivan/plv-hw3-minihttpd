import socket
import subprocess
import tempfile
import time
from pathlib import Path
import json
import pytest
import os

def wait_port(host: str, port: int, timeout=5.0):
    t0 = time.time()
    while time.time() - t0 < timeout:
        try:
            with socket.create_connection((host, port), timeout=0.2):
                return
        except OSError:
            time.sleep(0.05)
    raise RuntimeError(f"server did not open {host}:{port}")

@pytest.fixture(scope="session")
def server_bin():
    env = os.environ.get("MINIHTTPD_BIN")
    if env:
        return env
    root = Path(__file__).resolve().parents[1]
    build = root / "build"
    exe = build / "minihttpd"
    if not exe.exists():
        subprocess.check_call(["cmake", "-S", str(root), "-B", str(build), "-DCMAKE_BUILD_TYPE=Release"])
        subprocess.check_call(["cmake", "--build", str(build), "-j"])
    return str(exe)

@pytest.fixture()
def running_server(server_bin):
    with tempfile.TemporaryDirectory() as td:
        td = Path(td)
        www = td / "www"
        www.mkdir()
        (www / "index.html").write_text("<h1>hi</h1>", encoding="utf-8")

        s = socket.socket()
        s.bind(("127.0.0.1", 0))
        port = s.getsockname()[1]
        s.close()

        cfg = {
            "server_ip": "127.0.0.1",
            "port": port,
            "max_clients": 32,
            "root_dir": str(www),
            "log_file": str(td / "server.log"),
            "log_level": "DEBUG",
            "keep_alive": True,
            "keep_alive_timeout_sec": 2,
            "keep_alive_max_requests": 10,
            "read_header_max_bytes": 32768,
            "recv_chunk_size": 65536
        }

        cfg_path = td / "config.json"
        cfg_path.write_text(json.dumps(cfg, indent=2), encoding="utf-8")

        proc = subprocess.Popen(
            [server_bin, str(cfg_path)],
            cwd=str(td),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True
        )

        try:
            wait_port("127.0.0.1", port, timeout=5.0)
            yield ("127.0.0.1", port, www, proc)
        finally:
            proc.kill()
            try:
                proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                proc.terminate()
