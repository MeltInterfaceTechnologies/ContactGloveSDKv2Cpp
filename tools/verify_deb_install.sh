#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "usage: $0 path/to/contactglove-sdk-v2_<version>_<arch>.deb" >&2
    exit 2
fi

deb_path="$1"
if [ ! -f "$deb_path" ]; then
    echo "missing deb: $deb_path" >&2
    exit 2
fi

if command -v sudo >/dev/null 2>&1 && [ "$(id -u)" -ne 0 ]; then
    SUDO=(sudo)
else
    SUDO=()
fi

"${SUDO[@]}" dpkg -i "$deb_path"

package_name="contactglove-sdk-v2"
multiarch="$(dpkg-architecture -qDEB_HOST_MULTIARCH)"
libdir="/usr/lib/${multiarch}"
# CMake sets SOVERSION to PROJECT_VERSION_MAJOR. Use the DEB's version so
# this check follows the package being verified, including older releases.
package_version="$(dpkg-deb -f "$deb_path" Version)"
sdk_version="${package_version#*:}"
sdk_major="${sdk_version%%.*}"
if [[ ! "$sdk_major" =~ ^[0-9]+$ ]]; then
    echo "cannot determine SDK major version from DEB version: ${package_version}" >&2
    exit 1
fi
sdk_soname="libContactGloveSDKv2.so.${sdk_major}"

require_path() {
    local path="$1"
    if [ ! -e "$path" ]; then
        echo "missing expected installed path: $path" >&2
        echo "installed package contents:" >&2
        dpkg -L "${package_name}" >&2
        exit 1
    fi
}

require_package_entry() {
    local path="$1"
    if ! dpkg-deb -c "$deb_path" | grep -F "./${path}" >/dev/null; then
        echo "missing expected package entry: /${path}" >&2
        echo "deb package contents:" >&2
        dpkg-deb -c "$deb_path" >&2
        exit 1
    fi
}

require_path /usr/include/contactglove/ContactGloveSDKv2.hpp
require_path "${libdir}/libContactGloveSDKv2.so"
require_path "${libdir}/${sdk_soname}"
require_path "${libdir}/cmake/ContactGloveSDKv2/ContactGloveSDKv2Config.cmake"
require_path "${libdir}/cmake/ContactGloveSDKv2/ContactGloveSDKv2ConfigVersion.cmake"
require_path "${libdir}/cmake/ContactGloveSDKv2/ContactGloveSDKv2Targets.cmake"
require_path /usr/bin/contactglove-basic-usage
require_path /usr/share/ContactGloveSDKv2/examples/basic_usage.cpp
require_package_entry usr/share/doc/ContactGloveSDKv2/README.md
require_package_entry usr/share/doc/ContactGloveSDKv2/README_JA.md
require_package_entry usr/share/doc/ContactGloveSDKv2/LICENSE.md
require_package_entry usr/share/doc/ContactGloveSDKv2/NOTICE.md
require_package_entry usr/share/doc/ContactGloveSDKv2/THIRD_PARTY_NOTICES.md

workdir="$(mktemp -d)"
trap 'rm -rf "$workdir"' EXIT

cat >"${workdir}/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.20)
project(ContactGloveSDKv2DebConsumer LANGUAGES CXX)

find_package(ContactGloveSDKv2 CONFIG REQUIRED)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE ContactGloveSDKv2::ContactGloveSDKv2)
EOF

cat >"${workdir}/main.cpp" <<'EOF'
#include "contactglove/ContactGloveSDKv2.hpp"

int main() {
    ContactGloveSDKv2::SDK sdk;
    ContactGloveSDKv2::Host host;
    host.hostName = "localhost";
    host.ipAddress = "127.0.0.1";
    host.port = 25791;
    host.receivePort = 25788;
    return host.port == 25791 ? 0 : 1;
}
EOF

cmake -S "$workdir" -B "${workdir}/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "${workdir}/build" --config Release
ldd "${workdir}/build/consumer" | grep -F "${sdk_soname} =>" >/dev/null
"${workdir}/build/consumer"

echo "deb install and downstream CMake verification passed: ${deb_path}"
