# ビルド・テスト・CI・配布

[English](BUILD.md) | 日本語 · [開発者向けガイド](../DEVELOPMENT_JA.md)

## ビルド

CMake 3.20以上、C++17対応コンパイラ（MSVC 2019以降、GCC 9以降、Clang 9以降）、Gitが必要です。初回の構成時に `CMakeLists.txt` で固定したrevisionのoscpackを取得し、ローカルのソケットパッチを適用します。WindowsはVisual StudioのC++デスクトップ開発、macOSはXcode Command Line Toolsが必要です。API生成にはDoxygen 1.9以上、Moxygen 2以上（Node.js/npm）、Pythonも必要です。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --config Release --prefix dist/install
```

single-config generatorでは `CMAKE_BUILD_TYPE=Release`、multi-config generatorでは `--config Release` でReleaseを選びます。Windowsのmulti-config generatorではDebug利用者向けにDebugもビルドします。C++ APIはDLL境界を越えてC++オブジェクトを渡すため、アプリとSDKの構成を揃えてください。

### 静的ライブラリ

プラグインなど、アプリのディレクトリからSDKのDLLを読み込めないホスト向けに、`-DCONTACTGLOVE_BUILD_STATIC=ON` を付けると `ContactGloveSDKv2` を静的ライブラリ（`libContactGloveSDKv2.a`、Windowsでは `ContactGloveSDKv2.lib` / `ContactGloveSDKv2d.lib`）としてビルドします。oscpackはライブラリ内に組み込まれます。既定は共有ライブラリのままで、配布パッケージとRustバインディングは共有ライブラリ版を使います。

```sh
cmake -S . -B build-static -DCMAKE_BUILD_TYPE=Release -DCONTACTGLOVE_BUILD_STATIC=ON
cmake --build build-static --config Release
cmake --install build-static --config Release --prefix dist/static-install
```

`find_package(ContactGloveSDKv2)` を使う場合、必要な設定はimported targetから渡されます。Visual StudioプロジェクトなどCMakeを使わない場合は、`CONTACTGLOVE_SDK_STATIC` を定義し、`include` ディレクトリを追加して、SDKライブラリと一緒にWindowsでは `ws2_32.lib` と `winmm.lib`、Linux/macOSでは `pthread` をリンクしてください。WindowsではDebugで `ContactGloveSDKv2d.lib`、Releaseで `ContactGloveSDKv2.lib` をリンクします。どちらもDLL版Cランタイム（`/MDd`、`/MD`）を使います。`python tools/test_static_consumer.py dist/static-install --config Release` で、静的ライブラリのinstall先に対して同梱サンプルをビルドし、動作を確認できます。

## テスト・CI

```sh
cmake --build build --config Release --target runtime_regression protocol_v2_validation c_sample_regression sender_port_integration
ctest --test-dir build -C Release --output-on-failure
python tools/test_sender_port.py <sender_port_integrationのパス>
python tools/test_sender_port.py <sender_port_integrationのパス> --send-port 0
```

検証範囲とポートは [tests/README.md](../tests/README.md) を参照してください。テストはループバックを使い、実機やLAN承認を検証しません。CI（`.github/workflows/ci.yml`）はWindows、macOS、Linuxのビルド・テスト、インストールしたネイティブSDKを用いたRust検証、パッケージ検証を行います。Rustの設定と生成手順は [Rust README](../bindings/rust/README.md) を参照してください。

## 配布物

ルートのREADME日英を正本として、通常installとアーカイブ内の配置に合わせたリンクを生成します。同梱しない開発者向けリンクは除きます。再install時、既知の未編集の旧 `README_EN.md` は削除します。それ以外の同名ファイルは保護し、移動または削除してから再実行します。

以下はローカルで配布ファイルを生成する手順です。UbuntuのAPTリポジトリからのインストールは[利用者向けガイド](https://docs.melt-interface-technologies.com/ja/docs/contact/sdks/contact-glove-v2-native)を参照してください。リポジトリへの登録・公開は別のリリース作業です。

```sh
cmake --build build --config Release --target package
```

Windowsのmulti-config ZIPは両構成をビルドし、ビルドディレクトリで `cpack -C "Debug;Release"` を実行します。macOS Universal TGZは `-DCONTACTGLOVE_MACOS_UNIVERSAL=ON` で構成します。Linuxの既定はTGZです。Ubuntu DEBは対象環境で `-DCONTACTGLOVE_PACKAGE_DEB=ON -DCONTACTGLOVE_DEB_DISTRIBUTION=noble`（または `jammy`）を指定し、`dpkg-dev` と `file` を導入します。実際のアーカイブまたはインストール結果を `tools/verify_distribution_licenses.py` と各OS向け `tools/verify_*package*` / `tools/verify_deb_install.sh` で確認します。

## APIリファレンス

```sh
cmake -S . -B build -DBUILD_DOCS=ON
cmake --build build --config Release --target docs
moxygen build/docs/xml --output build/docs/md/api.md --noindex
```

出力はHTMLではなくMarkdownです。生成結果を確認してルートの `api.md` に反映し、生成本文は空白以外を編集しません。DoxygenはC++公開ヘッダーを入力とし、そこにあるC ABIとRustの補足も生成します。
