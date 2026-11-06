#!/usr/bin/env python3
"""Verify the Windows ZIP ships Debug and Release SDK libraries.

A fresh CMake consumer is built in Debug and Release with the Visual Studio
generator. Each configuration must resolve its matching SDK DLL, and the
C++ and C samples (which call ConnectToHost) must run in both.
"""

import argparse
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile

EXPECTED_FILES = (
    "bin/ContactGloveSDKv2.dll",
    "bin/ContactGloveSDKv2d.dll",
    "bin/ContactGloveSDKv2d.pdb",
    "lib/ContactGloveSDKv2.lib",
    "lib/ContactGloveSDKv2d.lib",
    "lib/cmake/ContactGloveSDKv2/ContactGloveSDKv2Targets-release.cmake",
    "lib/cmake/ContactGloveSDKv2/ContactGloveSDKv2Targets-debug.cmake",
    "bin/contactglove-basic-usage.exe",
    "bin/contactglove-c-basic-usage.exe",
)
UNEXPECTED_FILES = (
    "bin/contactglove-basic-usaged.exe",
    "bin/contactglove-c-basic-usaged.exe",
)
SDK_DLLS = {"Debug": "ContactGloveSDKv2d.dll", "Release": "ContactGloveSDKv2.dll"}


def run(command, **kwargs):
    print("Running:", " ".join(str(argument) for argument in command), flush=True)
    return subprocess.run([str(argument) for argument in command], check=True, **kwargs)


def verify(archive):
    smoke = Path(__file__).resolve().with_name("smoke_basic_usage.py")
    with tempfile.TemporaryDirectory(prefix="contactglove-windows-package-") as temporary:
        temporary = Path(temporary)
        with zipfile.ZipFile(archive) as package:
            package.extractall(temporary)
        roots = list(temporary.glob("ContactGloveSDKv2Cpp-*-Windows-*"))
        if len(roots) != 1:
            raise ValueError("expected exactly one Windows SDK package root")
        sdk = roots[0]
        missing = [name for name in EXPECTED_FILES if not (sdk / name).is_file()]
        if missing:
            raise ValueError(f"missing package files: {missing}")
        extra = [name for name in UNEXPECTED_FILES if (sdk / name).exists()]
        if extra:
            raise ValueError(f"unexpected Debug samples in package: {extra}")

        consumer = temporary / "consumer"
        consumer.mkdir()
        examples = sdk / "share" / "ContactGloveSDKv2" / "examples"
        example = (examples / "basic_usage.cpp").as_posix()
        c_example = (examples / "c_basic_usage.c").as_posix()
        (consumer / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.20)\n"
            "project(SDKPackageConsumer LANGUAGES C CXX)\n"
            "set(CMAKE_C_STANDARD 11)\n"
            "set(CMAKE_CXX_STANDARD 17)\n"
            "find_package(ContactGloveSDKv2 CONFIG REQUIRED)\n"
            f'add_executable(cpp_consumer "{example}")\n'
            f'add_executable(c_consumer "{c_example}")\n'
            "foreach(consumer cpp_consumer c_consumer)\n"
            "target_link_libraries(${consumer} PRIVATE ContactGloveSDKv2::ContactGloveSDKv2)\n"
            "add_custom_command(TARGET ${consumer} POST_BUILD\n"
            "    COMMAND ${CMAKE_COMMAND} -E copy_if_different\n"
            "        $<TARGET_FILE:ContactGloveSDKv2::ContactGloveSDKv2>\n"
            "        $<TARGET_FILE_DIR:${consumer}>\n"
            "    VERBATIM)\n"
            "endforeach()\n",
            encoding="utf-8",
        )
        build = temporary / "consumer-build"
        configure = run(
            ["cmake", "-S", consumer, "-B", build, f"-DCMAKE_PREFIX_PATH={sdk}"],
            stderr=subprocess.PIPE,
            text=True,
        )
        sys.stderr.write(configure.stderr)
        if "ContactGloveSDKv2 was found without a Debug library" in configure.stderr:
            raise ValueError("package config warned that the Debug library is missing")

        for config, dll in SDK_DLLS.items():
            run(["cmake", "--build", build, "--config", config])
            output = build / config
            copied = sorted(path.name for path in output.glob("ContactGloveSDKv2*.dll"))
            if copied != [dll]:
                raise ValueError(f"{config} consumer resolved {copied}, expected [{dll}]")
            # Keep the SDK bin/ off PATH so only the copied DLL can be loaded.
            for consumer_name in ("cpp_consumer", "c_consumer"):
                run([sys.executable, smoke, output / f"{consumer_name}.exe"])
    print("Windows package Debug/Release downstream consumers passed:", archive)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    args = parser.parse_args()
    verify(args.archive.resolve())
