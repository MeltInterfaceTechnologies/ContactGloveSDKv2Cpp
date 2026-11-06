# 開発者向けガイド

[English](DEVELOPMENT.md) | 日本語

ソースからSDKを開発する方向けの案内です。利用方法は[README](README_JA.md)を参照してください。

- [ビルド・テスト・CI・配布](docs/BUILD_JA.md)
- [Rustバインディングの保守](bindings/rust/README.md)

`api.md` は `include/contactglove/ContactGloveSDKv2.hpp` のDoxygenコメントから生成します。手編集せず、ビルドガイドに従って `docs` ターゲットでXMLを生成し、Moxygenで変換してください。`docs` ターゲットは `tools/prepare_doxygen_xml.py` も実行します。C ABIの生成済みRustバインディングはリポジトリに含まれます。Cヘッダーを変更したら再生成して差分を確認してください。利用者のビルドにlibclangは不要です。

