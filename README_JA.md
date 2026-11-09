# ContactGlove C++/C/Rust SDK V2

[English](https://github.com/MeltInterfaceTechnologies/ContactGloveSDKv2Cpp/blob/main/README.md) | 日本語

DivingStationからContactGlove3 ProのデータをOSCで受信するSDKです。グローブ、コントローラー、エルゴノミクス、骨格、センサーのデータを取得またはコールバックで受信でき、振動を要求できます。C++はネイティブAPI、CとRustラッパーはC ABIを使用します。

## 必要環境

- ContactGlove3 Proと起動済みのDivingStation v2以降。接続を設定し、求められた場合は承認してください。
- Windows 11 x64、Ubuntu 22.04 / 24.04 x86_64、macOS 13以降（Apple Silicon / Intel）。WindowsはMicrosoft Visual C++ Redistributable（x64）が必要です。
- アプリのビルドにはC++17、C11、またはRust/Cargo 1.82以降と対応するネイティブSDKが必要です。CMake利用例は3.16以降を使用します。ビルド済みサンプルの実行にコンパイラは不要です。

## インストール

ビルド済みSDKは[最新のGitHub Release](https://github.com/MeltInterfaceTechnologies/ContactGloveSDKv2Cpp/releases/latest)からダウンロードできます。Windows・Linux・macOS向けアーカイブ、Ubuntu向けDEB、`SHA256SUMS.txt`を配布しています。Ubuntuでは、以下のAPTによるインストールも利用できます。

### Ubuntu 22.04 / 24.04、amd64: APT

`jammy` または `noble` 向けです。配布元から `contactglove-archive.asc` を取得し、保存したフォルダで主鍵のfingerprintを確認します。値が `DCBF 1BD7 8F31 3033 7506 DDF6 CC4A 11F6 725F 5132` と一致することを確認してから鍵を登録してください。詳しくは[SDKガイド](https://docs.melt-interface-technologies.com/ja/docs/contact/sdks/contact-glove-v2-native)を参照してください。

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

sourceを追加する前に、`VERSION_CODENAME` が `jammy` または `noble`、アーキテクチャが `amd64` であることを確認してください。

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

設定後の更新:

```sh
sudo apt-get update
sudo apt-get install --only-upgrade contactglove-sdk-v2
```

### アーカイブとDEBの直接インストール

[最新のGitHub Release](https://github.com/MeltInterfaceTechnologies/ContactGloveSDKv2Cpp/releases/latest)のAssetsから、OS・CPUに合うファイルを選んでください。

- Windows 11 x64: `ContactGloveSDKv2Cpp-<version>-Windows-AMD64.zip`
- Linux x86_64: `ContactGloveSDKv2Cpp-<version>-Linux-x86_64.tar.gz`
- macOS 13以降（Apple Silicon / Intel）: `ContactGloveSDKv2Cpp-<version>-macOS-universal.tar.gz`
- Ubuntu 22.04 amd64: `contactglove-sdk-v2_<version>-0jammy_amd64.deb`
- Ubuntu 24.04 amd64: `contactglove-sdk-v2_<version>-0noble_amd64.deb`

同じReleaseの`SHA256SUMS.txt`も取得し、インストール前にダウンロードしたファイルのSHA-256チェックサムを確認してください。GitHubが自動生成する **Source code** アーカイブには、ビルド済みSDKは含まれません。

`<version>`をダウンロードしたファイル名のバージョンに置き換えてください。ZIP/TGZは展開先をSDKルートとして使います。

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

対象Ubuntu向けDEBを直接インストールする場合（ワイルドカードが対象ファイルだけに一致することを確認してください）:

```sh
sudo apt install ./contactglove-sdk-v2_*.deb
```

<!-- BEGIN SOURCE-ONLY -->
SDKをソースからビルド・インストールするには[ビルドガイド](https://github.com/MeltInterfaceTechnologies/ContactGloveSDKv2Cpp/blob/main/docs/BUILD_JA.md)を参照してください。
<!-- END SOURCE-ONLY -->

## 同梱サンプルの実行

DivingStationを起動してグローブを接続・有効化し、求められた場合は接続を承認してください。同梱の `contactglove-basic-usage` は `127.0.0.1:25791` に接続し、`25788` で5秒間受信します。ZIP/TGZは展開先のSDKルートで実行します。

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

`[callback] id=...` や `[poll] id=...` が表示されれば受信できています。macOSの同梱実行ファイルには `@loader_path/../lib` のrpathが設定済みです。独自アプリのライブラリ検索は別途設定してください。

## アプリからの利用

C/C++のCMakeプロジェクトでリンクする例:

```cmake
find_package(ContactGloveSDKv2 CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE ContactGloveSDKv2::ContactGloveSDKv2)
```

利用側プロジェクトのディレクトリでconfigureします。ZIP/TGZは `CMAKE_PREFIX_PATH` にSDKの展開先を指定し、Ubuntu DEBは通常追加指定が不要です。

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/sdk
cmake --build build --config Release
```

実行時には、Windowsはアプリと同じ構成のSDK DLLを実行ファイルの隣にコピーするか、SDKの `bin/` を `PATH` に追加します。Linux TGZはSDKの `lib/` を `LD_LIBRARY_PATH` に追加し、macOSではアプリの `@rpath` またはライブラリ検索パスを設定します。ZIP/TGZはビルド済みライブラリであり、SDKソースの `CMakeLists.txt` は含みません。

Rustは同梱workspaceを書き込み可能な場所へコピーし、対応するネイティブライブラリを使用します。Cargoはビルド出力と `Cargo.lock` の両方を書き込む場合があるため、読み取り専用のインストール先で直接ビルドしないでください。

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

Ubuntu DEBではインストール先からworkspaceをコピーします。

```sh
export CONTACTGLOVE_SDK_ROOT=/usr
workdir="$(mktemp -d "${TMPDIR:-/tmp}/contactglove-rust.XXXXXXXX")"
cp -R /usr/share/ContactGloveSDKv2/rust "$workdir/rust"
chmod -R u+rwX "$workdir/rust"
cargo run --manifest-path "$workdir/rust/Cargo.toml" -p contactglove-sdk-v2 --example basic_usage
```
独自のCargoプロジェクトでは同梱の `contactglove-sdk-v2` ディレクトリを依存先に指定します。詳細は[Rustバインディングガイド](https://github.com/MeltInterfaceTechnologies/ContactGloveSDKv2Cpp/blob/main/bindings/rust/README.md)を参照してください。

同じ設定で受信する最小限のC++コード:

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

完全なサンプル: [C++](https://github.com/MeltInterfaceTechnologies/ContactGloveSDKv2Cpp/blob/main/examples/basic_usage.cpp)、[C](https://github.com/MeltInterfaceTechnologies/ContactGloveSDKv2Cpp/blob/main/examples/c_basic_usage.c)、[Rust](https://github.com/MeltInterfaceTechnologies/ContactGloveSDKv2Cpp/blob/main/bindings/rust/contactglove-sdk-v2/examples/basic_usage.rs)。接続成功は受信待ちの開始を意味し、LAN接続の承認完了を保証しません。受信コールバックからSDKの切断・破棄を行わず、Rustでは切断前にsubscriptionを解放してください。

## ドキュメント・問い合わせ

- [接続・組込み・トラブルシューティング](https://docs.melt-interface-technologies.com/ja/docs/contact/sdks/contact-glove-v2-native)、[APIリファレンス](https://docs.melt-interface-technologies.com/ja/docs/contact/sdks/contact-glove-v2-native/api)（アクセス権により閲覧できない場合があります）。
- [同梱APIリファレンス](https://github.com/MeltInterfaceTechnologies/ContactGloveSDKv2Cpp/blob/main/api.md)、[お問い合わせ](https://melt-interface-technologies.com/contact/)。
<!-- BEGIN SOURCE-ONLY -->
- [開発者向けガイド](https://github.com/MeltInterfaceTechnologies/ContactGloveSDKv2Cpp/blob/main/DEVELOPMENT_JA.md)。
<!-- END SOURCE-ONLY -->

## ライセンス

Melt Interface TechnologiesのSDK実装、ヘッダー、バインディング、サンプル、文書は [Apache License 2.0](https://github.com/MeltInterfaceTechnologies/ContactGloveSDKv2Cpp/blob/main/LICENSE.md) で提供します。[NOTICE](https://github.com/MeltInterfaceTechnologies/ContactGloveSDKv2Cpp/blob/main/NOTICE.md) と[サードパーティ通知](https://github.com/MeltInterfaceTechnologies/ContactGloveSDKv2Cpp/blob/main/THIRD_PARTY_NOTICES.md)も参照してください。第三者コンポーネントにはそれぞれのライセンスが適用されます。
