"""
ContactGlove C++/C/Rust SDK V2 - OSC test server

Sends data in the /DS2/HC/Device format to verify the SDK's behavior.

Usage:
    uv run main.py
    uv run main.py --port 25788 --interval 1.0
"""

import argparse
import math
import socket
import time

from pythonosc.osc_message_builder import OscMessageBuilder

PROTOCOL_VERSION = 2


def build_device_message(devices: list[dict]) -> bytes:
    """
    /DS2/HC/Device format:
      int version
      (string id, string pair_id, int battery, int is_haptics, int is_left,
       int device_type, int color, int connected_state, string name,
       int is_external_power_present) * N
    """
    builder = OscMessageBuilder(address="/DS2/HC/Device")
    builder.add_arg(PROTOCOL_VERSION)

    for d in devices:
        builder.add_arg(d["id"])
        builder.add_arg(d["pair_id"])
        builder.add_arg(int(d["battery"]))
        builder.add_arg(1 if d["is_haptics"] else 0)
        builder.add_arg(1 if d["is_left"] else 0)
        builder.add_arg(int(d["family_type"]))
        builder.add_arg(int(d["color"]))
        builder.add_arg(int(d["connected_state"]))
        builder.add_arg(d["name"])
        builder.add_arg(1 if d["is_external_power_present"] else 0)

    return builder.build().dgram


def main() -> None:
    parser = argparse.ArgumentParser(description="ContactGlove C++/C/Rust SDK V2 OSC test server")
    parser.add_argument("--host",     default="127.0.0.1")
    parser.add_argument("--port",     type=int,   default=25788)
    parser.add_argument("--interval", type=float, default=1.0,
                        help="Seconds between sends (default: 1.0)")
    args = parser.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    # DeviceFamilyType: ContactGlove3 = 3
    # DeviceColor:         Cyan = 5
    # DeviceConnectedState: Connected = 1

    base_devices = [
        {
            "id":               "CG3-LEFT-001",
            "pair_id":          "CG3-RIGHT-001",
            "is_haptics":       True,
            "is_left":          True,
            "family_type":      3,
            "color":            5,
            "connected_state":  1,
            "name":             "ContactGlove3 Left",
            "is_external_power_present": False,
        },
        {
            "id":               "CG3-RIGHT-001",
            "pair_id":          "CG3-LEFT-001",
            "is_haptics":       True,
            "is_left":          False,
            "family_type":      3,
            "color":            5,
            "connected_state":  1,
            "name":             "ContactGlove3 Right",
            "is_external_power_present": False,
        },
    ]

    print(f"Sending /DS2/HC/Device to {args.host}:{args.port} every {args.interval}s")
    print("Press Ctrl+C to stop.\n")

    t = 0
    while True:
        # Battery oscillates slowly to make updates visible
        devices = [
            {**d,
             "battery":   80 + int(10 * math.sin(t * 0.2)) if d["is_left"]
                          else 70 + int(10 * math.cos(t * 0.2))}
            for d in base_devices
        ]

        dgram = build_device_message(devices)
        sock.sendto(dgram, (args.host, args.port))

        left  = next(d for d in devices if d["is_left"])
        right = next(d for d in devices if not d["is_left"])
        print(
            f"[{t:4d}] LEFT  battery={left['battery']:3d}%  "
            f"RIGHT battery={right['battery']:3d}%"
        )

        t += 1
        time.sleep(args.interval)


if __name__ == "__main__":
    main()
