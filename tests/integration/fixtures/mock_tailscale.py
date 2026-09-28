#!/usr/bin/env python3
"""
Mock Tailscale LocalAPI server for integration testing.
Binds to a Unix socket and replies to any HTTP GET with a
canned /localapi/v0/status response that reports BackendState: Running.
"""

import socket
import sys
import os

STATUS_JSON = b"""{
  "BackendState": "Running",
  "Self": {
    "ID": "test-self",
    "HostName": "test-host",
    "DNSName": "test-host.tailscale.ts.net.",
    "TailscaleIPs": ["100.64.0.1"],
    "ExitNode": false
  },
  "Peer": {
    "peer-abc123": {
      "ID": "peer-abc123",
      "HostName": "peer1",
      "DNSName": "peer1.tailscale.ts.net.",
      "TailscaleIPs": ["100.64.0.2"],
      "ExitNode": true
    },
    "peer-def456": {
      "ID": "peer-def456",
      "HostName": "peer2",
      "DNSName": "peer2.tailscale.ts.net.",
      "TailscaleIPs": ["100.64.0.3"],
      "ExitNode": false
    }
  },
  "CurrentTailnet": {
    "Name": "test-net",
    "MagicDNSSuffix": "tailscale.ts.net"
  }
}
"""

RESPONSE = (
    b"HTTP/1.0 200 OK\r\n"
    b"Content-Type: application/json\r\n"
    b"Content-Length: " + str(len(STATUS_JSON)).encode() + b"\r\n"
    b"\r\n"
    + STATUS_JSON
)

def main():
    if len(sys.argv) < 2:
        print("Usage: mock_tailscale.py <unix-socket-path>", file=sys.stderr)
        sys.exit(1)

    sock_path = sys.argv[1]

    # Remove stale socket
    if os.path.exists(sock_path):
        os.unlink(sock_path)

    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.bind(sock_path)
    s.listen(1)

    # Parent process can detect readiness via socket existence
    print(f"Mock Tailscale listening on {sock_path}", flush=True)

    while True:
        conn, _ = s.accept()
        try:
            data = conn.recv(4096)
            if data:
                conn.sendall(RESPONSE)
        finally:
            conn.close()

if __name__ == "__main__":
    main()
