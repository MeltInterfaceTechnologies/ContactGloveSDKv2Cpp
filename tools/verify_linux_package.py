#!/usr/bin/env python3
"""Verify the shipped TGZ's samples and a fresh CMake consumer on Linux."""

import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def run(command, env=None):
    print("Running:", " ".join(str(argument) for argument in command), flush=True)
    subprocess.run([str(argument) for argument in command], env=env, check=True)


def verify(archive):
    smoke = Path(__file__).resolve().with_name("smoke_basic_usage.py")
    with tempfile.TemporaryDirectory(prefix="contactglove-linux-package-") as temporary:
        temporary = Path(temporary)
        run(["tar", "-xzf", archive, "-C", temporary])
        roots = list(temporary.glob("ContactGloveSDKv2Cpp-*-Linux-x86_64"))
        if len(roots) != 1:
            raise ValueError("expected exactly one Linux SDK package root")
        sdk = roots[0]
        env = os.environ.copy()
        env["LD_LIBRARY_PATH"] = str(sdk / "lib") + (
            ":" + env["LD_LIBRARY_PATH"] if env.get("LD_LIBRARY_PATH") else ""
        )
        for sample in ("contactglove-basic-usage", "contactglove-c-basic-usage"):
            run([sys.executable, smoke, sdk / "bin" / sample], env)

        consumer = temporary / "consumer"
        consumer.mkdir()
        examples = sdk / "share" / "ContactGloveSDKv2" / "examples"
        (consumer / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.20)\n"
            "project(SDKPackageConsumer LANGUAGES C CXX)\n"
            "set(CMAKE_CXX_STANDARD 17)\n"
            "set(CMAKE_C_STANDARD 11)\n"
            "find_package(ContactGloveSDKv2 CONFIG REQUIRED)\n"
            f'add_executable(cpp_consumer "{examples / "basic_usage.cpp"}")\n'
            f'add_executable(c_consumer "{examples / "c_basic_usage.c"}")\n'
            "target_link_libraries(cpp_consumer PRIVATE ContactGloveSDKv2::ContactGloveSDKv2)\n"
            "target_link_libraries(c_consumer PRIVATE ContactGloveSDKv2::ContactGloveSDKv2)\n",
            encoding="utf-8",
        )
        build = temporary / "consumer-build"
        run(["cmake", "-S", consumer, "-B", build, "-DCMAKE_BUILD_TYPE=Release",
             f"-DCMAKE_PREFIX_PATH={sdk}"])
        run(["cmake", "--build", build, "--parallel", "2"])
        for sample in ("cpp_consumer", "c_consumer"):
            run([sys.executable, smoke, build / sample], env)
    print("Linux package samples and downstream C/C++ consumers passed:", archive)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    args = parser.parse_args()
    verify(args.archive.resolve())
