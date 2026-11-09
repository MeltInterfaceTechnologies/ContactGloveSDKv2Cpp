# ContactGlove C++/C/Rust SDK V2

English | [日本語](https://github.com/MeltInterfaceTechnologies/ContactGloveSDKv2Cpp/blob/main/README_JA.md)

Receive ContactGlove3 Pro data from DivingStation over OSC. The SDK offers polling and callbacks for glove, controller, ergonomics, skeleton and sensor data, and haptic requests. C++ uses the native API; C and the Rust wrapper use its C ABI.

## Requirements

- ContactGlove3 Pro and a running DivingStation v2 or later; configure and approve the connection there if prompted.
- Windows 11 x64, Ubuntu 22.04 / 24.04 x86_64, or macOS 13+ (Apple Silicon / Intel). Windows requires the Microsoft Visual C++ Redistributable (x64).
- To build an application: C++17 for C++, C11 for C, or Rust/Cargo 1.82+ and the matching native SDK for Rust. The CMake consumer example requires CMake 3.16+. Prebuilt samples do not require a compiler.

## Installation

Download prebuilt SDK packages from the [latest GitHub Release](https://github.com/MeltInterfaceTechnologies/ContactGloveSDKv2Cpp/releases/latest). The release includes Windows, Linux and macOS archives, Ubuntu DEB packages, and `SHA256SUMS.txt`. Ubuntu users can also install through APT as described below.

### Ubuntu 22.04 / 24.04, amd64: APT

For `jammy` or `noble`, obtain `contactglove-archive.asc` from the distributor. In the directory containing it, inspect the primary key fingerprint and confirm it is `DCBF 1BD7 8F31 3033 7506 DDF6 CC4A 11F6 725F 5132` before registering the key. Follow the [SDK guide](https://docs.melt-interface-technologies.com/en/docs/contact/sdks/contact-glove-v2-native) for the complete setup.

```sh
sudo apt-get update
sudo apt-get install ca-certificates gnupg
gpg --show-keys --with-fingerprint ./contactglove-archive.asc
```

```sh
sudo install -d -m 0755 /etc/apt/keyrings
gpg --dearmor --output contactglove-archive.gpg ./contactglove-archive.asc
sudo install -m 0644 contactglove-archive.gpg /etc/apt/keyrings/contactglove-archive.gpg
. /etc/os-release
echo "$VERSION_CODENAME"
dpkg --print-architecture
```

Before adding the source, confirm `VERSION_CODENAME` is `jammy` or `noble` and the architecture is `amd64`.

```sh
cat <<EOF | sudo tee /etc/apt/sources.list.d/contactglove.sources > /dev/null
Types: deb
URIs: https://apt.melt-interface-technologies.com/ubuntu
Suites: ${VERSION_CODENAME}
Components: main
Architectures: amd64
Signed-By: /etc/apt/keyrings/contactglove-archive.gpg
EOF
sudo apt-get update
sudo apt-get install contactglove-sdk-v2
```

To upgrade after setup:

```sh
sudo apt-get update
sudo apt-get install --only-upgrade contactglove-sdk-v2
```

### Archives and direct DEB

Open the [latest GitHub Release](https://github.com/MeltInterfaceTechnologies/ContactGloveSDKv2Cpp/releases/latest) and choose an asset for your OS and CPU:

- Windows 11 x64: `ContactGloveSDKv2Cpp-<version>-Windows-AMD64.zip`
- Linux x86_64: `ContactGloveSDKv2Cpp-<version>-Linux-x86_64.tar.gz`
- macOS 13+ (Apple Silicon / Intel): `ContactGloveSDKv2Cpp-<version>-macOS-universal.tar.gz`
- Ubuntu 22.04 amd64: `contactglove-sdk-v2_<version>-0jammy_amd64.deb`
- Ubuntu 24.04 amd64: `contactglove-sdk-v2_<version>-0noble_amd64.deb`

Download `SHA256SUMS.txt` from the same release and verify the selected file's SHA-256 checksum before installing. GitHub's automatically generated **Source code** archives do not contain the prebuilt SDK.

Replace `<version>` with the downloaded filename's version. Extract ZIP/TGZ and use the extracted directory as the SDK root.

Windows x64 ZIP:

```powershell
Expand-Archive .\ContactGloveSDKv2Cpp-<version>-Windows-AMD64.zip -DestinationPath .
```

Linux x86_64 TGZ:

```sh
tar -xzf ContactGloveSDKv2Cpp-<version>-Linux-x86_64.tar.gz
```

macOS Universal TGZ:

```sh
tar -xzf ContactGloveSDKv2Cpp-<version>-macOS-universal.tar.gz
```

For a matching Ubuntu DEB, install directly (ensure the wildcard selects only the intended file):

```sh
sudo apt install ./contactglove-sdk-v2_*.deb
```

<!-- BEGIN SOURCE-ONLY -->
To build and install the SDK from source, see the [build guide](https://github.com/MeltInterfaceTechnologies/ContactGloveSDKv2Cpp/blob/main/docs/BUILD.md).
<!-- END SOURCE-ONLY -->

## Run the bundled sample

Start DivingStation, connect and enable the glove, and approve the connection if prompted. The bundled `contactglove-basic-usage` connects to `127.0.0.1:25791` and listens on `25788` for five seconds. For ZIP/TGZ run from the extracted SDK root:

```powershell
# Windows ZIP
.\bin\contactglove-basic-usage.exe
```

```sh
# Linux TGZ
LD_LIBRARY_PATH="$PWD/lib" ./bin/contactglove-basic-usage
# macOS Universal TGZ
./bin/contactglove-basic-usage
# Ubuntu APT or DEB
contactglove-basic-usage
```

`[callback] id=...` or `[poll] id=...` confirms that data is being received. The bundled macOS executables have `@loader_path/../lib` as their rpath; configure library lookup separately for your application.

## Use in an application

For C/C++ consumers using CMake:

```cmake
find_package(ContactGloveSDKv2 CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE ContactGloveSDKv2::ContactGloveSDKv2)
```

Configure the consumer project from its own directory. For ZIP/TGZ, set `CMAKE_PREFIX_PATH` to the extracted SDK root; an Ubuntu DEB normally needs no extra path.

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/sdk
cmake --build build --config Release
```

At runtime, copy the matching SDK DLL beside the Windows executable or add the SDK `bin/` to `PATH`; Debug and Release DLLs must match the application configuration. For Linux TGZ add the SDK `lib/` to `LD_LIBRARY_PATH`; on macOS configure the application's `@rpath` or library search path. ZIP/TGZ contains prebuilt libraries, not the SDK source `CMakeLists.txt`.

For Rust, copy the bundled workspace to a writable directory and use the matching native library. Cargo may need to write both build output and `Cargo.lock`, so do not build directly in a read-only SDK installation:

```powershell
# Windows ZIP: set the SDK root to the extracted directory
$env:CONTACTGLOVE_SDK_ROOT = "C:\path\to\ContactGloveSDKv2Cpp-<version>-Windows-AMD64"
$env:PATH = "$env:CONTACTGLOVE_SDK_ROOT\bin;$env:PATH"
$work = Join-Path $env:TEMP ("contactglove-rust-" + [guid]::NewGuid())
New-Item -ItemType Directory -Path $work | Out-Null
Copy-Item -Recurse "$env:CONTACTGLOVE_SDK_ROOT\rust" "$work\rust"
Get-ChildItem -Recurse -File "$work\rust" | ForEach-Object { $_.IsReadOnly = $false }
cargo run --manifest-path "$work\rust\Cargo.toml" -p contactglove-sdk-v2 --example basic_usage
```

```sh
# Linux or macOS TGZ: from the SDK root
export CONTACTGLOVE_SDK_ROOT="$PWD"
workdir="$(mktemp -d "${TMPDIR:-/tmp}/contactglove-rust.XXXXXXXX")"
cp -R "$CONTACTGLOVE_SDK_ROOT/rust" "$workdir/rust"
chmod -R u+rwX "$workdir/rust"
# Linux
export LD_LIBRARY_PATH="$CONTACTGLOVE_SDK_ROOT/lib:${LD_LIBRARY_PATH:-}"
# macOS (instead of the Linux line)
export DYLD_LIBRARY_PATH="$CONTACTGLOVE_SDK_ROOT/lib:${DYLD_LIBRARY_PATH:-}"
cargo run --manifest-path "$workdir/rust/Cargo.toml" -p contactglove-sdk-v2 --example basic_usage
```

For an Ubuntu DEB, copy the workspace from its installed location:

```sh
export CONTACTGLOVE_SDK_ROOT=/usr
workdir="$(mktemp -d "${TMPDIR:-/tmp}/contactglove-rust.XXXXXXXX")"
cp -R /usr/share/ContactGloveSDKv2/rust "$workdir/rust"
chmod -R u+rwX "$workdir/rust"
cargo run --manifest-path "$workdir/rust/Cargo.toml" -p contactglove-sdk-v2 --example basic_usage
```
For your own Cargo project, point the dependency to the bundled `contactglove-sdk-v2` directory. See the [Rust binding guide](https://github.com/MeltInterfaceTechnologies/ContactGloveSDKv2Cpp/blob/main/bindings/rust/README.md) for details.

A minimal C++ receiver using the same defaults:

```cpp
#include <chrono>
#include <iostream>
#include <thread>
#include "contactglove/ContactGloveSDKv2.hpp"

int main() {
    ContactGloveSDKv2::SDK sdk;
    ContactGloveSDKv2::Host host{"localhost", "127.0.0.1", 25791, 25788};
    sdk.RegisterCallbackForGloveDataStream([](const ContactGloveSDKv2::GloveData& data) {
        std::cout << "battery=" << data.batteryPercentage << "%\n";
    });
    if (sdk.ConnectToHost(host) != ContactGloveSDKv2::SDKReturnCode::Success) return 1;
    std::this_thread::sleep_for(std::chrono::seconds(5));
    sdk.Disconnect(host);
}
```

Complete examples: [C++](https://github.com/MeltInterfaceTechnologies/ContactGloveSDKv2Cpp/blob/main/examples/basic_usage.cpp), [C](https://github.com/MeltInterfaceTechnologies/ContactGloveSDKv2Cpp/blob/main/examples/c_basic_usage.c), and [Rust](https://github.com/MeltInterfaceTechnologies/ContactGloveSDKv2Cpp/blob/main/bindings/rust/contactglove-sdk-v2/examples/basic_usage.rs). A successful connect starts the listener; it does not confirm LAN approval. Do not disconnect or destroy the SDK from a receive callback; in Rust, drop subscriptions before disconnecting.

## Documentation and support

- [Setup, integration and troubleshooting](https://docs.melt-interface-technologies.com/en/docs/contact/sdks/contact-glove-v2-native) and [API reference](https://docs.melt-interface-technologies.com/en/docs/contact/sdks/contact-glove-v2-native/api) (availability may depend on access).
- [Bundled API reference](https://github.com/MeltInterfaceTechnologies/ContactGloveSDKv2Cpp/blob/main/api.md) and [contact support](https://melt-interface-technologies.com/en/contact/).
<!-- BEGIN SOURCE-ONLY -->
- [Development guide](https://github.com/MeltInterfaceTechnologies/ContactGloveSDKv2Cpp/blob/main/DEVELOPMENT.md).
<!-- END SOURCE-ONLY -->

## License

Melt Interface Technologies SDK code, headers, bindings, examples and documentation are licensed under [Apache License 2.0](https://github.com/MeltInterfaceTechnologies/ContactGloveSDKv2Cpp/blob/main/LICENSE.md). See [NOTICE](https://github.com/MeltInterfaceTechnologies/ContactGloveSDKv2Cpp/blob/main/NOTICE.md) and [third-party notices](https://github.com/MeltInterfaceTechnologies/ContactGloveSDKv2Cpp/blob/main/THIRD_PARTY_NOTICES.md). Third-party components retain their own licenses.
