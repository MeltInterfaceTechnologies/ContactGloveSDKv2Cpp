# Build, tests, CI and distribution

English | [日本語](BUILD_JA.md) · [Development](../DEVELOPMENT.md)

## Build

CMake 3.20+, a C++17 compiler (MSVC 2019+, GCC 9+ or Clang 9+) and Git are required. The first configure fetches oscpack at the revision pinned in `CMakeLists.txt` and applies the local socket patch. Windows needs Visual Studio's desktop C++ workload; macOS needs Xcode Command Line Tools. API generation additionally needs Doxygen 1.9+, Moxygen 2+ (Node.js/npm) and Python.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --config Release --prefix dist/install
```

`CMAKE_BUILD_TYPE=Release` selects Release for single-config generators; `--config Release` selects it for multi-config generators. For multi-config Windows generators, build Debug as well for Debug consumers. C++ applications must use the matching SDK configuration because the API passes C++ objects across the DLL boundary.

### Static library

Add `-DCONTACTGLOVE_BUILD_STATIC=ON` to build `ContactGloveSDKv2` as a static library (`libContactGloveSDKv2.a`, or `ContactGloveSDKv2.lib` / `ContactGloveSDKv2d.lib` on Windows) for hosts that cannot load an SDK DLL from the application directory, such as plugins. oscpack is compiled into the library. The default stays a shared library, and distribution packages and Rust bindings use the shared build.

```sh
cmake -S . -B build-static -DCMAKE_BUILD_TYPE=Release -DCONTACTGLOVE_BUILD_STATIC=ON
cmake --build build-static --config Release
cmake --install build-static --config Release --prefix dist/static-install
```

`find_package(ContactGloveSDKv2)` consumers get the required settings from the imported target. Projects that do not use CMake, such as Visual Studio projects, must define `CONTACTGLOVE_SDK_STATIC`, add the `include` directory, and link the SDK library together with `ws2_32.lib` and `winmm.lib` on Windows or `pthread` on Linux/macOS. On Windows, link `ContactGloveSDKv2d.lib` from Debug and `ContactGloveSDKv2.lib` from Release; both use the DLL C runtime (`/MDd`, `/MD`). `python tools/test_static_consumer.py dist/static-install --config Release` builds and smokes the installed samples against the static install.

## Tests and CI

```sh
cmake --build build --config Release --target runtime_regression protocol_v2_validation c_sample_regression sender_port_integration
ctest --test-dir build -C Release --output-on-failure
python tools/test_sender_port.py <path-to-sender_port_integration>
python tools/test_sender_port.py <path-to-sender_port_integration> --send-port 0
```

See [tests/README.md](../tests/README.md) for coverage and port usage. Tests use loopback and do not establish physical-device or LAN approval. CI (`.github/workflows/ci.yml`) builds and tests Windows, macOS and Linux, validates Rust with an installed native SDK, and checks packages. For local Rust setup and generated bindings see [Rust README](../bindings/rust/README.md).

## Distribution

The root READMEs are the source for install and archive guides. Installation and packaging render links for each layout; source-only development links are omitted. Reinstall removes an unchanged known legacy `README_EN.md`; an unfamiliar file is preserved and must be moved or removed before retrying.

These commands build local distribution files. For installing the SDK via the Ubuntu APT repository, see the [user guide](https://docs.melt-interface-technologies.com/en/docs/contact/sdks/contact-glove-v2-native); registering or publishing to that repository is a separate release operation.

```sh
cmake --build build --config Release --target package
```

Windows multi-config ZIP requires both configurations and `cpack -C "Debug;Release"` from the build directory. macOS Universal TGZ: configure with `-DCONTACTGLOVE_MACOS_UNIVERSAL=ON`. Linux defaults to TGZ; to build an Ubuntu DEB in the target environment, add `-DCONTACTGLOVE_PACKAGE_DEB=ON -DCONTACTGLOVE_DEB_DISTRIBUTION=noble` (or `jammy`) and install `dpkg-dev` and `file`. Verify the actual archive or install using `tools/verify_distribution_licenses.py` and the platform-specific `tools/verify_*package*` / `tools/verify_deb_install.sh` scripts.

## API reference

```sh
cmake -S . -B build -DBUILD_DOCS=ON
cmake --build build --config Release --target docs
moxygen build/docs/xml --output build/docs/md/api.md --noindex
```

The output is Markdown, not HTML. Review and copy the generated content to root `api.md`; preserve generated content except whitespace. Doxygen reads the C++ public header, including its C ABI and Rust notes.
