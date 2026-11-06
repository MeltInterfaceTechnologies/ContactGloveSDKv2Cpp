#!/usr/bin/env python3
import argparse
import socket
import struct
import subprocess
import sys
import threading
import time
from pathlib import Path


def osc_string(value: str) -> bytes:
    raw = value.encode("utf-8") + b"\0"
    return raw + (b"\0" * ((4 - len(raw) % 4) % 4))


def build_device_message() -> bytes:
    return b"".join(
        [
            osc_string("/DS2/HC/Device"),
            osc_string(",issiiiiiisi"),
            struct.pack(">i", 2),
            osc_string("CG3-LEFT-001"),
            osc_string("CG3-RIGHT-001"),
            struct.pack(">iiiiii", 88, 1, 1, 3, 5, 1),
            osc_string("ContactGlove3 Left"),
            struct.pack(">i", 0),
        ]
    )


def send_packets(stop: threading.Event, port: int) -> None:
    packet = build_device_message()
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        while not stop.is_set():
            sock.sendto(packet, ("127.0.0.1", port))
            time.sleep(0.05)
    finally:
        sock.close()


def main() -> int:
    parser = argparse.ArgumentParser(description="Smoke-test examples/basic_usage.")
    parser.add_argument("executable", type=Path)
    parser.add_argument("--port", type=int, default=25788)
    parser.add_argument("--timeout", type=float, default=12.0)
    args = parser.parse_args()

    if not args.executable.exists():
        print(f"missing executable: {args.executable}", file=sys.stderr)
        return 2

    stop = threading.Event()
    sender = threading.Thread(target=send_packets, args=(stop, args.port), daemon=True)
    sender.start()

    proc = subprocess.Popen(
        [str(args.executable)],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )

    try:
        output, _ = proc.communicate(timeout=args.timeout)
    except subprocess.TimeoutExpired:
        proc.kill()
        output, _ = proc.communicate()
        print(output)
        print("basic_usage timed out", file=sys.stderr)
        return 3
    finally:
        stop.set()
        sender.join(timeout=1.0)

    print(output)
    if proc.returncode != 0:
        print(f"basic_usage exited with {proc.returncode}", file=sys.stderr)
        return proc.returncode

    required = (
        "[callback] id=CG3-LEFT-001",
        "[poll] id=CG3-LEFT-001",
    )
    missing = [needle for needle in required if needle not in output]
    if missing:
        print(f"smoke output missing: {missing}", file=sys.stderr)
        return 4

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
