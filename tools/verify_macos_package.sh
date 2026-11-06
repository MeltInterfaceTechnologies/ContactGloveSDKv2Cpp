#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "usage: $0 path/to/ContactGloveSDKv2Cpp-<version>-macOS-universal.tar.gz" >&2
    exit 2
fi

archive_path="$1"
if [ ! -f "$archive_path" ]; then
    echo "missing macOS package: ${archive_path}" >&2
    exit 2
fi

case "$archive_path" in
    *-macOS-universal.tar.gz) ;;
    *)
        echo "unexpected macOS package name: ${archive_path}" >&2
        exit 2
        ;;
esac

require_file() {
    local file_path="$1"
    if [ ! -f "$file_path" ]; then
        echo "missing expected package file: ${file_path}" >&2
        exit 1
    fi
}

require_universal_binary() {
    local binary_path="$1"
    local architectures
    architectures="$(lipo -archs "$binary_path")"
    case " ${architectures} " in
        *" arm64 "*) ;;
        *)
            echo "missing arm64 architecture: ${binary_path} (${architectures})" >&2
            exit 1
            ;;
    esac
    case " ${architectures} " in
        *" x86_64 "*) ;;
        *)
            echo "missing x86_64 architecture: ${binary_path} (${architectures})" >&2
            exit 1
            ;;
    esac
}

require_macos_13() {
    local binary_path="$1"
    if ! otool -l "$binary_path" | awk '
        $1 == "cmd" && $2 == "LC_BUILD_VERSION" { in_build_version = 1; next }
        in_build_version && $1 == "minos" && $2 == "13.0" { found = 1 }
        END { exit(found ? 0 : 1) }
    '; then
        echo "missing macOS 13.0 deployment target: ${binary_path}" >&2
        exit 1
    fi
}

require_sample_rpath() {
    local binary_path="$1"
    if ! otool -l "$binary_path" | awk '
        $1 == "cmd" && $2 == "LC_RPATH" { in_rpath = 1; next }
        in_rpath && $1 == "path" {
            if ($2 == "@loader_path/../lib") {
                found = 1
            }
            in_rpath = 0
        }
        END { exit(found ? 0 : 1) }
    '; then
        echo "missing sample rpath @loader_path/../lib: ${binary_path}" >&2
        exit 1
    fi
}

script_dir="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(CDPATH= cd -- "${script_dir}/.." && pwd)"
workdir="$(mktemp -d)"
trap 'rm -rf "$workdir"' EXIT

tar -xzf "$archive_path" -C "$workdir"
package_roots=("${workdir}"/ContactGloveSDKv2Cpp-*-macOS-universal)
if [ "${#package_roots[@]}" -ne 1 ] || [ ! -d "${package_roots[0]}" ]; then
    echo "archive must contain one macOS Universal SDK root" >&2
    exit 1
fi
package_root="${package_roots[0]}"

library_path="${package_root}/lib/libContactGloveSDKv2.dylib"
cpp_sample="${package_root}/bin/contactglove-basic-usage"
c_sample="${package_root}/bin/contactglove-c-basic-usage"

require_file "$library_path"
require_file "$cpp_sample"
require_file "$c_sample"
require_file "${package_root}/include/contactglove/ContactGloveSDKv2.hpp"
require_file "${package_root}/include/contactglove/ContactGloveSDKv2C.h"
require_file "${package_root}/lib/cmake/ContactGloveSDKv2/ContactGloveSDKv2Config.cmake"
require_file "${package_root}/rust/Cargo.toml"
require_file "${package_root}/share/doc/ContactGloveSDKv2/README.md"
require_file "${package_root}/share/doc/ContactGloveSDKv2/README_JA.md"
require_file "${package_root}/README.md"
require_file "${package_root}/README_JA.md"

for binary_path in "$library_path" "$cpp_sample" "$c_sample"; do
    require_universal_binary "$binary_path"
    require_macos_13 "$binary_path"
done

for sample_path in "$cpp_sample" "$c_sample"; do
    require_sample_rpath "$sample_path"
done

if ! otool -D "$library_path" | grep -E '^@rpath/libContactGloveSDKv2\.[0-9]+\.dylib$' >/dev/null; then
    echo "SDK library must retain an @rpath install name: ${library_path}" >&2
    exit 1
fi

python3 "${repo_root}/tools/smoke_basic_usage.py" "$cpp_sample"
python3 "${repo_root}/tools/smoke_basic_usage.py" "$c_sample"

consumer_dir="${workdir}/consumer"
mkdir -p "$consumer_dir"
cat >"${consumer_dir}/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.20)
project(ContactGloveSDKv2MacConsumer LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)

find_package(ContactGloveSDKv2 CONFIG REQUIRED)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE ContactGloveSDKv2::ContactGloveSDKv2)
add_executable(c_consumer c_basic_usage.c)
target_link_libraries(c_consumer PRIVATE ContactGloveSDKv2::ContactGloveSDKv2)
EOF

cat >"${consumer_dir}/main.cpp" <<'EOF'
#include "contactglove/ContactGloveSDKv2.hpp"

int main() {
    ContactGloveSDKv2::SDK sdk;
    return 0;
}
EOF

cp "${package_root}/share/ContactGloveSDKv2/examples/c_basic_usage.c" "${consumer_dir}/c_basic_usage.c"
cmake -S "$consumer_dir" -B "${consumer_dir}/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$package_root"
cmake --build "${consumer_dir}/build" --config Release

echo "macOS Universal package verification passed: ${archive_path}"
