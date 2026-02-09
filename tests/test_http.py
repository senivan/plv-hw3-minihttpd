import socket
import http.client
from pathlib import Path

def raw_request(host, port, data: bytes) -> bytes:
    with socket.create_connection((host, port), timeout=2) as s:
        s.sendall(data)
        chunks = []
        s.settimeout(0.5)
        while True:
            try:
                b = s.recv(65536)
            except socket.timeout:
                break
            if not b:
                break
            chunks.append(b)
        return b"".join(chunks)

def test_get_root_200(running_server):
    host, port, www, proc = running_server
    conn = http.client.HTTPConnection(host, port, timeout=2)
    conn.request("GET", "/")
    resp = conn.getresponse()
    body = resp.read()
    assert resp.status == 200
    assert b"hi" in body
    conn.close()

def test_404_returns_html(running_server):
    host, port, www, proc = running_server
    conn = http.client.HTTPConnection(host, port, timeout=2)
    conn.request("GET", "/nope.html")
    resp = conn.getresponse()
    body = resp.read()
    assert resp.status == 404
    assert b"<!doctype html" in body.lower()
    conn.close()

def test_501_for_unsupported_method(running_server):
    host, port, www, proc = running_server
    resp = raw_request(host, port, b"PUT / HTTP/1.1\r\nHost: x\r\nContent-Length: 0\r\n\r\n")
    assert b"501" in resp.split(b"\r\n", 1)[0]

def test_400_for_malformed_request(running_server):
    host, port, www, proc = running_server
    resp = raw_request(host, port, b"GET / HTTP/1.1\r\nHost x\r\n\r\n")
    assert b"400" in resp.split(b"\r\n", 1)[0]

def test_storage_upload_get_delete(running_server):
    host, port, www, proc = running_server
    data = b"hello world\n" * 1000

    conn = http.client.HTTPConnection(host, port, timeout=2)
    conn.request("POST", "/files/uploads/a.txt", body=data, headers={"Content-Length": str(len(data))})
    resp = conn.getresponse()
    _ = resp.read()
    assert resp.status == 200
    conn.close()

    conn = http.client.HTTPConnection(host, port, timeout=2)
    conn.request("GET", "/files/uploads/a.txt")
    resp = conn.getresponse()
    body = resp.read()
    assert resp.status == 200
    assert body == data
    conn.close()

    conn = http.client.HTTPConnection(host, port, timeout=2)
    conn.request("DELETE", "/files/uploads/a.txt")
    resp = conn.getresponse()
    _ = resp.read()
    assert resp.status == 200
    conn.close()

    conn = http.client.HTTPConnection(host, port, timeout=2)
    conn.request("GET", "/files/uploads/a.txt")
    resp = conn.getresponse()
    _ = resp.read()
    assert resp.status == 404
    conn.close()

def test_forbidden_path_traversal(running_server):
    host, port, www, proc = running_server
    conn = http.client.HTTPConnection(host, port, timeout=2)
    conn.request("GET", "/files/../secret.txt")
    resp = conn.getresponse()
    _ = resp.read()
    assert resp.status in (403, 400)
    conn.close()

def test_keep_alive_two_requests_same_socket(running_server):
    host, port, www, proc = running_server
    s = socket.create_connection((host, port), timeout=2)
    try:
        req1 = b"GET / HTTP/1.1\r\nHost: x\r\nConnection: keep-alive\r\n\r\n"
        req2 = b"GET /nope.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n"
        s.sendall(req1 + req2)

        data = b""
        s.settimeout(0.5)
        while True:
            try:
                b = s.recv(65536)
            except socket.timeout:
                break
            if not b:
                break
            data += b

        assert b"HTTP/1.1 200" in data
        assert b"HTTP/1.1 404" in data
    finally:
        s.close()

def test_big_file_sparse_download_headers(running_server):
    host, port, www, proc = running_server
    big = Path(www) / "big.bin"
    size = 1024 * 1024 * 1024 + 123
    with open(big, "wb") as f:
        f.seek(size - 1)
        f.write(b"\0")

    s = socket.create_connection((host, port), timeout=2)
    try:
        s.sendall(b"GET /big.bin HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n")
        head = b""
        while b"\r\n\r\n" not in head and len(head) < 65536:
            chunk = s.recv(4096)
            if not chunk:
                break
            head += chunk
        assert b"HTTP/1.1 200" in head.split(b"\r\n", 1)[0]
        assert f"Content-Length: {size}".encode() in head
    finally:
        s.close()
