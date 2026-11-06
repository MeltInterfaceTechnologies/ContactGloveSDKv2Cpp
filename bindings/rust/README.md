# ContactGlove C++/C/Rust SDK V2 Rust binding

`contactglove-sdk-v2` is the official thin Rust wrapper over the SDK's C ABI.
It is distributed with the matching C++ SDK.
For distribution setup and CMake/C/Rust integration, see the
[SDK guide](https://docs.melt-interface-technologies.com/en/docs/contact/sdks/contact-glove-v2-native).
The binding maintenance procedure is documented below.

The Rust bindings are licensed under Apache-2.0, like the SDK's C/C++ code.
The full license and notices are in the repository root, or in
`share/doc/ContactGloveSDKv2/` within a distribution or installation prefix.
The native SDK includes oscpack under its own license; see
`THIRD_PARTY_NOTICES.md` in that documentation directory.

Copy the bundled workspace to a writable directory before running its example.
Cargo may need to write both its build output and `Cargo.lock`; a workspace in
`/usr/share` or another read-only installation directory must not be used as
Cargo's working workspace. Keep `CONTACTGLOVE_SDK_ROOT` pointing to the
matching native SDK.

From a ZIP/TGZ distribution on Windows:

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


From a Linux/macOS TGZ, use the extracted SDK root and a writable temporary
workspace. Set the appropriate dynamic library search path:

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

For an Ubuntu DEB:

```sh
export CONTACTGLOVE_SDK_ROOT=/usr
workdir="$(mktemp -d "${TMPDIR:-/tmp}/contactglove-rust.XXXXXXXX")"
cp -R /usr/share/ContactGloveSDKv2/rust "$workdir/rust"
chmod -R u+rwX "$workdir/rust"
cargo run --manifest-path "$workdir/rust/Cargo.toml" -p contactglove-sdk-v2 --example basic_usage
```
For your own Cargo project, a path dependency on the bundled
`contactglove-sdk-v2` crate lets the project keep its own lockfile and output.

`Sdk` and `Subscription` are intentionally not `Send` or `Sync`.  Stream
callbacks run on the SDK receiver thread; copy or send the owned callback
values to another thread when needed.  Drop each `Subscription` before
disconnecting or dropping `Sdk`.

## Regenerating the low-level binding

The `contactglove-sdk-v2-sys/src/bindings.rs` file is checked in so consumer
builds do not need libclang.  Maintainers regenerate it after changing
`ContactGloveSDKv2C.h`, review the diff, then run the workspace tests.

```powershell
 # Install once: cargo install bindgen-cli --locked
 # Set this to the directory containing libclang.dll (for example, LLVM\bin).
 .\generate_bindings.ps1 -LibClangPath "C:\Program Files\LLVM\bin"
cargo fmt --all --manifest-path .\Cargo.toml --check
```

`LIBCLANG_PATH` may be set instead of passing `-LibClangPath` each time.
