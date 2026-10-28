# AgentNodify

Codex・Claude Code・OpenCodeなどのエージェントをノードでつなぎ、ワークフローを作成・実行するデスクトップアプリです。Qt 6 / QtNodesを使用しています。

## ビルド方法

Windows向けの手順です。以下をインストールしてください。

- Git、CMake 3.24以上、PowerShell 5.1以上
- Visual Studio 2022またはBuild Tools（「C++によるデスクトップ開発」とWindows SDK）
- Qt 6.5以上のMSVC x64キット（Widgets / Test、`windeployqt`を含む）

cloneしたリポジトリのルートで、Qtキットの場所を指定して実行します。

```powershell
$qt = Read-Host 'Qtキットの絶対パス'
.\scripts\build.ps1 -QtPath $qt
```

Releaseビルド・自動テスト・Qt DLL配置を行い、`build/Release/AgentNodify.exe`を生成します。
`-QtPath`の代わりに環境変数`Qt6_ROOT`も使えます。初回は依存ライブラリの取得にネットワーク接続が必要です。
配布先にはVisual C++ 2022ランタイムも必要です。

手動ビルドや環境設定の詳細は[開発ガイド](spec/development.md)を参照してください。

## ソース構成

| パス | 内容 |
|---|---|
| `src/app/` | アプリの起動・多重起動制御 |
| `src/core/` | データモデル・検証・実行・CLI連携・履歴 |
| `src/ui/` | 編集画面・フォーム・進行状況の表示 |
| `src/nodes/` | 種類別のノード実装と登録 |
| `src/shared/` | 共通の文字列・色・設定値 |
| `tests/` | 自動テストと統合テスト用プログラム |
| `scripts/` | ビルド・実CLIテスト用スクリプト |
| `examples/` | サンプルフローとスクリプト |
| `spec/` | [仕様](spec/spec.md)・開発資料 |

## 依存ライブラリ

| ライブラリ | バージョン | ライセンス |
|---|---|---|
| Qt | 6.5以上 | 利用するディストリビューションに従う |
| QtNodes | 3.0.12 | BSD-3-Clause |
| nlohmann/json | 3.11.3 | MIT |
| nlohmann/json-schema-validator | 2.3.0 | MIT |

Qt以外はCMakeが固定バージョン・SHA-256検証付きで取得します。ライセンス本文は各ソースツリーに含まれます。
