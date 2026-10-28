# 開発ガイド

製品の動作仕様は[仕様書](spec.md)、最短のビルド手順は[README](../README.md#ビルド方法)を参照してください。

## 設計と実装の配置

依存方向は `app → ui → core → shared` です。`agentcore` はQt Coreだけを使用し、Qt GUI／WidgetsやQtNodesをリンクしません。

- `src/core/flow.*`: データモデル、JSON保存、グラフ・スキーマ・変数の検証。
- `src/core/runner.*`: 非同期実行、分岐・合流、CLIプロセスとプロトコル。
- `src/core/coordinator.*`: プロジェクトごとの実行と別フロー連携。
- `src/core/history.*`: ノード1実行ごとの履歴のアトミック保存・復元。
- `src/core/progress.*` / `src/ui/progressview.*`: CLIイベントの解析とMarkdown・JSON・タイムライン表示。
- `src/core/modelcatalog.*`: 推論を行わないモデル一覧取得とキャッシュ。
- `src/core/cli.*`: CLI実行ファイルの解決。
- `src/ui/graph.*`: QtNodesのAbstractGraphModelアダプター。
- `src/ui/window.*`: 編集画面・プロジェクト画面。

### ノードの追加・変更

種類ごとの内部定義・実行処理は `src/nodes/<種類>/core/`、表示定義・フォームは `src/nodes/<種類>/ui/` に配置します。他の種類の実装には依存させません。
内部の `NodeDefinition` は検証・スキーマ・実行を、UIの `NodePresentation` は色・説明・フォーム生成を担当します。
`.inc` はC++実装断片で、唯一の登録入口 `src/nodes/registry.cpp` から読み込みます。登録ファイルはコア用とUI用に別々にコンパイルします。
保存形式との互換性のため、`Node` の既存フィールドを維持してください。

`NodeForm` は名前欄・編集通知・入力制御・サイズ調整の共通の外枠です。
種類別の状態と補助処理は `ui/form.inc` の `NodeParameterForm` 派生クラスが保持し、必要な種類だけ生成します。
All／Anyの入力ポート数や別フロー連携のフロー・パス指定には `src/ui/nodecommon/` の共通フォームを再利用します。

文字列は `src/shared/strings.h`、固定色は `colors.h`、設定値は `constants.h` に集約します。共有部分にウィジェットや描画処理を置かないでください。

アプリアイコンの共通アセットは `assets/agentnodify.ico` です（16〜256px）。Qtリソースとしてウィンドウ・タスクバー・トレイに使用し、Windowsでは `assets/agentnodify.rc` で実行ファイルにも埋め込みます。アイコン変更時はこのファイルを更新してください。

本体のライセンスはルートの`LICENSE`（MIT）。依存ライセンスの原文・出典は`licenses/`に置き、一覧は`packaging/THIRD-PARTY-NOTICES.txt`へ集約します。`cmake/Licenses.cmake`で固定版依存の原文との一致を検査し、通常ビルドでもQtリソースへ埋め込みます。`src/ui/licensedialog.*`が「ヘルプ → ライセンス・依存ソフトウェア…」のオフライン表示を担当します。GUIテストは全資料の読み込み・原文表示・ダイアログの再表示を検証します。配布固有資料と対応ソースの提供は[配布ガイド](packaging.md#ライセンスと公開前の準備)に従い、公開承認チェックを迂回しないでください。

## ビルド環境の詳細

C++20を使用します。Qtキットのコンパイラ・ABI・アーキテクチャをビルド環境に合わせてください。
`scripts/build.ps1` は構成・ビルド・CTest・Qt DLL配置を行い、終了時にはPATHを復元します。

| 指定 | 内容 |
|---|---|
| `-QtPath` / `Qt6_ROOT` | Qtキットの場所。どちらも未指定ならエラー |
| `-Configuration` | `Release`（既定）または`Debug` |
| `-BuildDirectory` | ビルド先。既定は`build` |
| `-Generator` | 既定は`Visual Studio 17 2022` |
| `-Architecture` | Visual Studioの対象。既定は`x64` |

Qt・生成器・アーキテクチャを変更するときは新しいビルドディレクトリを指定します。
Ninja等を使う場合は対応コンパイラを利用できる開発者シェルで実行してください。
PowerShellの実行ポリシーで拒否された場合は組織のポリシーに従うか、以下の手動手順を使用します。

```powershell
$qt = Read-Host 'Qtキットの絶対パス'
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 "-DCMAKE_PREFIX_PATH=$qt"
cmake --build build --config Release --parallel
$env:PATH = "$qt/bin;$env:PATH"
ctest --test-dir build -C Release --output-on-failure
& "$qt/bin/windeployqt.exe" build/Release/AgentNodify.exe --no-translations --no-compiler-runtime
```

手動手順は現在のシェルのPATHを変更します。必要なら専用のターミナルで実行してください。
ネットワーク制限下ではCMakeの `FETCHCONTENT_SOURCE_DIR_JSON`、`FETCHCONTENT_SOURCE_DIR_JSON_VALIDATOR`、`FETCHCONTENT_SOURCE_DIR_QTNODES` に、それぞれ指定バージョンの展開済みソースを指定できます。

## GitHub Actions（Windows）

`.github/workflows/windows-build.yml` は`master`・`develop`ブランチへのpush（PRマージを含む）で、GitHubホストの`windows-2022`上にMSVC 2022 x64／Qt 6.8.3を用意します。PR作成・他ブランチへのpush・手動実行では起動しません。既存の`scripts/build.ps1`でReleaseビルド・通常のCTest・Qt DLL配置を実行します。Python・Node.jsはランナー付属のものを使用し、不在なら失敗させます。Qtは固定版のaqtinstallで取得し、個人のQtパスや認証情報は使いません。

`master`をPR経由でのみ更新できるよう保護し、マージに伴う`push`でビルドを起動する運用です。マージコミット・Squash・Rebaseのいずれでも更新後のコミットをビルドします。ブランチ保護を迂回した直接pushが許可されている場合はそれでも起動するため、直接更新の禁止はGitHubのブランチ保護で管理してください。マージ前の必須チェックには使えません。`develop`は作成後、そのブランチにもワークフローファイルを含めてください。

同一ブランチのワークフローは直列実行し、`queue: max`で最大100件の待機を保持します。実行中の採番・公開をキャンセルしません。キュー上限超過・失敗時は対象コミットの実行をActionsから再実行してください。再実行で同じコミットに別バージョンは割り当てません。

Actions画面の`Windows build`から結果を確認できます。CTestログ・CMake構成ログ・テスト画像は、失敗時も含めて`windows-x64-release-diagnostics`アーティファクトに7日間保存します。ビルド自体のログは各実行のステップにあります。

`develop`はビルド検証だけです。`master`ではビルド成功後、[配布ガイド](packaging.md#github-actionsでの自動リリース)の公開準備チェックを通過した場合に限り、バージョン採番・IFWインストーラ生成・通常のCTestと配布レイアウト検査・GitHub Release公開を実施します。実CLIの認証・モデル取得・推論、署名は行いません。

リポジトリのSettings → Actions → GeneralでActionsと使用する`actions/checkout`・`actions/upload-artifact`を許可してください。通常ビルドのトークンは`contents: read`のみ、`master`のリリースジョブだけ`contents: write`を使用します。追加Secrets・PATは不要ですが、組織のポリシーでActionsからのタグ作成・Release公開が許可されている必要があります。タグ保護に拒否された場合はワークフローが失敗し、保護設定を自動で迂回しません。非公開リポジトリではWindowsランナーの利用枠・料金を確認してください。`pull_request_target`は使用しません。

採番・再実行判定のオフラインテストは`node --test tests/release.test.mjs`で実行できます。GitHubやエージェントCLIへの接続はありません。

ワークフローを登録する認証には対象リポジトリのContentsとWorkflowsの書き込み権限が必要です（classic PATなら`repo`と`workflow`、fine-grained PAT／GitHub Appなら`Contents: Read and write`と`Workflows: Read and write`）。PR作成にはさらにPull requestsの書き込み権限が必要です。実行結果のAPI確認にはActionsの読み取り、手動起動・再実行にはActionsの書き込みが必要です。トークンをソースやチャットへ貼り付けないでください。

## テスト

IFWインストーラの生成・配布ファイル・公開前の確認は[配布ガイド](packaging.md)を参照してください。通常ビルドにIFWは不要です。

通常のビルド・CTestにエージェントCLI、アカウント、APIキーは不要です。CLI連携の自動テストは偽CLIを使用します。
既存ビルドでテストだけを実行する場合も、Qt DLLを検出できるPATHを設定してください。

- `core`: 変数・スキーマ・保存・接続検証、CLIイベント、並列実行、分岐・合流・ループ、FIFO、失敗とキャンセル。
- `script` / `shell`: 外部ランタイム実行、標準入出力、実行ディレクトリ。
- `catalog`: モデル取得の3プロトコル、ページ送り、重複・非表示候補、キャッシュ、異常応答、出力上限、タイムアウト、CLIパス変更時の更新。
- `progress`: ストリーミング更新、折りたたみ、JSON省略、履歴再生、安全なMarkdown表示。
- `gui` / `gui_smoke`: フォームと右ペインの同期、プレビュー上限、ノード表示・接続、ウィジェットのライフサイクル。CTestからオフスクリーンで実行。
- `instance` / `userinput` / `context` / `coordinator` / `timing`: 多重起動制御、入力待ち、コンテキスト継続、別フロー連携、待機・スケジュール。
- `node_registry`: 登録の完全性、共有上限、ポート再対応付け、ノード実装の依存境界、文字列の集約。
- `layering`: UIなしのフロー実行、内部モデル・共有部分からUIへの依存禁止、全ソースのCMake／インクルード経由の参照。

Python実行にはPython 3（`python`または`python3`）、JavaScript実行にはNode.js（`node`）をPATHに追加します。未導入ランタイムの該当ケースはスキップされるため、テスト結果のスキップも確認してください。
Windowsのシェル実行にはPowerShell、その他の環境では`/bin/sh`を使用します。
GUIテストの確認画像はビルドディレクトリに出力します。

### 実CLIでの確認

実CLIの認証・モデル一覧取得・推論テストは通常のCTestとは別です。明示的に実施する場合は[設定・認証手順](sandbox-tests.md)に従ってください。
`agentnodify_catalog_probe` はモデル一覧取得、`agentnodify_live_test` は推論を伴う確認、`agentnodify_progress_probe` は保存済みレポートの進行イベント再生に使用します。
`scripts/live-test.ps1` は分離プロファイルを使いますが、OSサンドボックスは作りません。
`-Models`は推論プロンプトを送らずモデル一覧だけを取得します。`-Run`は外部通信・課金を伴い得ます。
使用するモデルは仕様の許可範囲に限り、利用不可の場合は別モデルへ自動変更しません。

## ローカル設定と機密情報

個人のQt／CLIパス、認証情報、サンドボックス権限は共有しません。`build/`、`build-*/`はローカル用の無視対象です。
実CLIを使う場合だけ、各提供元の手順で導入・認証し、アプリのCLI設定にネイティブ`.exe`を指定します。Windowsの`.cmd` / `.bat`ラッパーは非対応です。
CLIの全権限自動承認やサンドボックス無効化は追加せず、既存の拒否ルールを維持します。
スクリプト・シェルノードはユーザー権限で実行され、エージェントCLIの権限制限は適用されません。
CLI出力やプロンプトを含む履歴・レポートは平文で機密情報を含む可能性があるため、コミット・配布しないでください。
