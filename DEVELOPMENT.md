# Development

English | [日本語](DEVELOPMENT_JA.md)

For SDK maintainers working from source. For usage, see [README](README.md).

- [Build, tests, CI and distribution](docs/BUILD.md)
- [Rust binding maintenance](bindings/rust/README.md)

`api.md` is generated from Doxygen comments in `include/contactglove/ContactGloveSDKv2.hpp`; do not edit it by hand. Generate XML with the `docs` target, which runs `tools/prepare_doxygen_xml.py`, and convert it with Moxygen as described in the build guide. The generated C ABI bindings are checked in; regenerate them after changing the C header and review the result. Consumer builds do not require libclang.

