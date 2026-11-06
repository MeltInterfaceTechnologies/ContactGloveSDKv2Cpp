#!/usr/bin/env python3
"""Build and smoke the installed samples against a static SDK install."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

CONSUMER_CMAKE = """\
cmake_minimum_required(VERSION 3.20)
project(contactglove_static_consumer LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    add_compile_options(/utf-8)
endif()
find_package(ContactGloveSDKv2 CONFIG REQUIRED)
get_target_property(_type ContactGloveSDKv2::ContactGloveSDKv2 TYPE)
if(NOT _type STREQUAL "STATIC_LIBRARY")
    message(FATAL_ERROR "Expected a static ContactGloveSDKv2, found ${_type}")
endif()
set(_examples "${ContactGloveSDKv2_DIR}/../../../share/ContactGloveSDKv2/examples")
add_executable(cpp_consumer "${_examples}/basic_usage.cpp")
target_link_libraries(cpp_consumer PRIVATE ContactGloveSDKv2::ContactGloveSDKv2)
add_executable(c_consumer "${_examples}/c_basic_usage.c")
target_link_libraries(c_consumer PRIVATE ContactGloveSDKv2::ContactGloveSDKv2)
"""

SHARED_SUFFIXES = (".dll", ".so", ".dylib")


def run(command):
    print("+", " ".join(str(part) for part in command), flush=True)
    subprocess.run([str(part) for part in command], check=True)


def find_executable(build, config, name):
    for candidate in (build / config / f"{name}.exe", build / f"{name}.exe", build / name):
        if candidate.is_file():
            return candidate
    raise FileNotFoundError(f"{name} was not built in {build}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("prefix", type=Path, help="cmake --install prefix of a static SDK build")
    parser.add_argument("--config", default="Release")
    args = parser.parse_args()
    prefix = args.prefix.resolve()

    shared = [path for path in prefix.rglob("*ContactGloveSDKv2*")
              if path.name.endswith(SHARED_SUFFIXES) or ".so." in path.name]
    if shared:
        raise RuntimeError(f"static install contains shared SDK libraries: {shared}")

    tools = Path(__file__).resolve().parent
    with tempfile.TemporaryDirectory(prefix="contactglove-static-consumer-") as temporary:
        source = Path(temporary) / "src"
        build = Path(temporary) / "build"
        source.mkdir()
        (source / "CMakeLists.txt").write_text(CONSUMER_CMAKE, encoding="utf-8")
        run(["cmake", "-S", source, "-B", build, f"-DCMAKE_BUILD_TYPE={args.config}",
             f"-DCMAKE_PREFIX_PATH={prefix}"])
        run(["cmake", "--build", build, "--config", args.config])
        for name in ("cpp_consumer", "c_consumer"):
            run([sys.executable, tools / "smoke_basic_usage.py",
                 find_executable(build, args.config, name)])
    print("static consumer OK")


if __name__ == "__main__":
    main()
