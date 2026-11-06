#!/usr/bin/env python3
"""Verify stable UDP endpoints and delayed approval across connection retries."""

import argparse
import socket
import struct
import subprocess
import sys
import time
from pathlib import Path


REMOTE_PORT = 25811
RECEIVE_PORT = 25812
SEND_PORT = 25813


def osc_string(value: str) -> bytes:
    raw = value.encode("utf-8") + b"\0"
    return raw + (b"\0" * ((4 - len(raw) % 4) % 4))


def osc_address(packet: bytes) -> str:
    try:
        end = packet.index(b"\0")
        return packet[:end].decode("utf-8")
    except (UnicodeDecodeError, ValueError):
        return ""


def device_message() -> bytes:
    return b"".join(
        [
            osc_string("/DS2/HC/Device"),
            osc_string(",issiiiiiisi"),
            struct.pack(">i", 2),
            osc_string("sender-port-test"),
            osc_string(""),
            struct.pack(">iiiiii", 80, 1, 1, 3, 5, 1),
            osc_string("Sender Port Test"),
            struct.pack(">i", 0),
        ]
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--send-port", type=int, default=SEND_PORT)
    args = parser.parse_args()
    if not args.executable.exists():
        print(f"missing executable: {args.executable}", file=sys.stderr)
        return 2

    received: list[tuple[str, tuple[str, int]]] = []
    server = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    server.bind(("127.0.0.1", REMOTE_PORT))
    server.settimeout(0.05)
    proc = subprocess.Popen([str(args.executable), str(args.send_port)])
    connect_count = 0
    deadline = time.monotonic() + 10.0

    try:
        while time.monotonic() < deadline:
            try:
                packet, sender = server.recvfrom(4096)
            except socket.timeout:
                if proc.poll() is not None:
                    break
                continue

            address = osc_address(packet)
            received.append((address, sender))
            if address == "/DS2/HC/Connect":
                connect_count += 1
                if connect_count >= 6:
                    server.sendto(device_message(), ("127.0.0.1", RECEIVE_PORT))

        returncode = proc.wait(timeout=1.0)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()
        print("sender-port integration executable timed out", file=sys.stderr)
        return 3
    finally:
        server.close()

    if returncode != 0:
        print(f"sender-port integration executable exited with {returncode}", file=sys.stderr)
        return returncode

    expected = {
        "/DS2/HC/Connect",
        "/DS2/HC/sender-port-test/Haptics/Body",
        "/DS2/HC/Disconnect",
    }
    addresses = {address for address, _ in received}
    missing = expected - addresses
    if missing:
        print(f"missing OSC messages: {sorted(missing)}; received: {received}", file=sys.stderr)
        return 4

    source_ports = {sender[1] for _, sender in received if _ in expected}
    if len(source_ports) != 1 or (args.send_port and source_ports != {args.send_port}):
        print(f"expected stable source port ({args.send_port}), got {sorted(source_ports)}", file=sys.stderr)
        return 5
    if connect_count != 6 or received[-1][0] != "/DS2/HC/Disconnect":
        print(f"unexpected retry/disconnect sequence: {received}", file=sys.stderr)
        return 6
    if any(address == "/DS2/HC/Disconnect" for address, _ in received[:-1]):
        print(f"approval revoked during retry: {received}", file=sys.stderr)
        return 7
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
