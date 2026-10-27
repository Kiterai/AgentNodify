# 実CLIテストをサンドボックス内で実行する

## 現状と境界

対象はsafe-shellが作る**外側のOSサンドボックス**です。各CLIのツール権限とは別です。
`sandboxed=false`やCLIの全権限自動承認・sandbox無効化は使いません。限定的な書き込みモードも明示的に選んだテストだけで使います。

2026-10-26の確認では、実行ユーザーは`srt-sandbox`、HOME / USERPROFILEはUser1、
APPDATA / LOCALAPPDATAはsrt-sandboxでした。PATHからのCLI検出も失敗しました。
Codex / Claudeの絶対パスによる`--version`は成功しましたが、両方の認証状態は未ログインでした。
当初のSRT設定の通信許可にはモデルAPIがありませんでした。以下の承認要求で追加しました。

`scripts/live-test.ps1`はCLIを絶対パスで起動し、HOME・AppData・XDG・CLI設定を
`build/live-profile/`に統一します。終了時には呼び出し元の環境変数を戻します。
`TEMP` / `TMP`は上書きせず、サンドボックスが用意する既定の一時領域を使います。
実行テストのプロジェクトは`build/file-check-<UUID>/`に通常のディレクトリ作成APIで作り、ファイル確認用に残します。
専用プロファイル配下では通常のディレクトリ作成は成功するものの、Qtの`QTemporaryDir`は
アクセス拒否となったためです。作業ディレクトリ配下への追加権限は不要でした。
通常ユーザーの認証ファイルのコピー、全ホームへの書き込み許可、外側への自動再試行は行いません。
独立したプロファイルでも、CLIや認証方式がWindows資格情報ストアに依存する場合は
別ユーザーから利用できない可能性があります。その場合はテスト専用API資格情報等を検討してください。

## 1. safe-shellの承認要求で権限を追加

エージェントが`request_file_permission` / `request_network_permission`を1件ずつ呼び、
ユーザーが許可対象と保存範囲を確認します。設定ファイルを直接編集する運用にはしません。
現在の合成結果は`show_permissions`の`effective_sandbox_settings`で確認できます。
既存の拒否設定は維持し、必要な実行ファイル・ホストだけを許可します。

今回、以下を承認済みです。

| 対象 | 権限 | 保存範囲 |
|---|---|---|
| `C:/Users/User1/AppData/Local/Programs/@opencode-aidesktop/resources` | 読み取り | グローバル |
| `api.openai.com` | 通信 | グローバル |
| `chatgpt.com` | 通信 | このディレクトリ配下 |
| `auth.openai.com` | 通信 | このディレクトリ配下 |
| `api.anthropic.com` | 通信 | このディレクトリ配下 |

グローバル許可は他のプロジェクトにも適用されます。
追加のアクセス拒否が出た場合も、対象を確認してから承認要求します。
`scripts/srt-settings.live.example.jsonc`は参考用の未検証例で、適用不要です。
認証・APIキー設定はユーザーが手動で行い、認証情報のコピーや値の表示はしません。

## 2. テスト専用認証（初回のみ・ユーザー操作）

通常の対話可能なPowerShellで、使うCLIについて実行してください。
これは認証のための初回操作であり、モデルテストそのものはサンドボックス内で行います。

```powershell
powershell -ExecutionPolicy Bypass -File scripts/live-test.ps1 -Agent Codex -Login
powershell -ExecutionPolicy Bypass -File scripts/live-test.ps1 -Agent 'Claude Code' -Login
powershell -ExecutionPolicy Bypass -File scripts/live-test.ps1 -Agent OpenCode -Login
```

`-Program C:/path/to/cli.exe`でインストール先を変更できます。
Codex / OpenCodeのOpenAIログインはユーザー操作で完了し、サンドボックス内でも利用できました。
Claude CodeもユーザーがProプランへの変更とログインを完了し、サンドボックス内で全4ケース成功しました。
プラン変更・ログインはユーザー操作で行い、エージェントによる認証情報の変更は行っていません。
APIキーを使う場合は各CLIが対応する環境変数を安全な方法で設定してください。
キーをコマンド文字列・設定例・レポート・Gitに書かないでください。
`build/`はGit除外済みですが、認証情報を含むため共有・配布・成果物のアップロードは禁止です。
ログイン後もトークン更新に書き込みが必要なため、専用プロファイルは書き込み可能にします。

## 3. サンドボックス内で確認・実行

まずsafe-shellの`sandboxed=true`、`shell="powershell"`で、課金のない起動確認を行います。

```powershell
./scripts/live-test.ps1 -Agent Codex
./scripts/live-test.ps1 -Agent 'Claude Code'
./scripts/live-test.ps1 -Agent OpenCode
```

その後、明示的に`-Run`を付けた場合のみ実モデルを呼び出します（課金あり）。

```powershell
./scripts/live-test.ps1 -Agent Codex -Case text -Run
./scripts/live-test.ps1 -Agent 'Claude Code' -Case json -Run
./scripts/live-test.ps1 -Agent OpenCode -Case loop -Run
```

`-Case`は`text` / `json` / `loop` / `and` / `file` / `cwd`。モデルは仕様で許可されたLuna / Haikuに固定しています。
Qtが異なる場合は`-QtPath`を指定します。CTestには登録しません。

カレントディレクトリの検証は`-Run -Case cwd`です。既定ではサンドボックスの一時領域に日本語・空白入りの
ディレクトリを作成し、現在地と相対ファイルのランダム値を照合します。任意の検証先には`-ProjectDirectory`を指定できます。
既存の同名マーカーファイルは上書きしません。検証先に対する読み書き権限が必要です。
OpenCodeのタスク実行はGUI・テストとも常に専用サーバーの`--standalone`を使用します。

実ファイル作成の確認は`-Run -Case file`です。Codex / Claude Codeに書き込み権限を明示的に与える検証だけ、
`-WorkspaceWrite`を追加します（Codex: workspace-write、Claude Code: acceptEdits。全権限バイパスではありません）。
このオプションを外すと既存CLI設定に従います。OpenCodeの自動承認オプションは追加しません。
2026-10-26の追加確認ではOpenCodeとacceptEditsのClaude Codeは実ファイル作成に成功し、
Codexはworkspace-writeでも環境ポリシーに拒否されて未作成でした。詳細は`test-results.md`を参照してください。

モデル一覧の取得確認（推論リクエストなし）は`-Models`で行えます。`-Run` / `-Login`とは併用しません。

```powershell
./scripts/live-test.ps1 -Agent Codex -Models
./scripts/live-test.ps1 -Agent 'Claude Code' -Models
./scripts/live-test.ps1 -Agent OpenCode -Models
```

アプリと同じ`ModelCatalog`を使う`agentnodify_catalog_probe`で取得し、
結果は`build/{codex,claude,opencode}-models-sandbox.json`にモデル情報だけを保存します。
CLI初期化応答のアカウント情報・認証情報・生ログはレポートに保存しません。
OpenCodeは分離したXDG領域を使い、実テストでは`run --standalone`で専用サーバーを起動します。
通常ユーザーの既存サービスとのポート衝突を避けるための設定で、権限自動承認ではありません。
通常ユーザーの既存サービスへ明示接続する設定や
資格情報・パスの環境変数による上書きがある場合は、実行前に取り除いてください。
バックグラウンドサービスの起動可否もサンドボックス内での確認対象です。

CodexはWindows既定TLSで`workspace routing discovery failed`となりました。
`CODEX_CA_CERTIFICATE`にGit同梱のCAバンドルを指定すると成功したため、
既存の`CODEX_CA_CERTIFICATE` / `SSL_CERT_FILE`がない場合に限り、スクリプトが
ユーザー単位またはProgram Files配下のGitからCAバンドルを検出して設定します。
`-CodexCaCertificate C:/path/to/ca.pem`で明示指定もできます。
証明書検証は無効化せず、プロセス終了後は環境変数を復元します。

結果は`build/{codex,claude,opencode}-{case}-sandbox-live.json`。
成功判定は、サンドボックス内で全ケースがPASSとなることです。
失敗時は認証・ファイルアクセス・通信・サービス起動のどの段階かを確認し、
サンドボックス外実行で代替して成功扱いにはしません。

既存の外実行許可は追加・使用していません。
`.agent-config/rules.jsonc`には承認ツールがサンドボックス内の通信許可を追加しています。

## 今回の検証結果

承認ツールによる許可追加後も、実行はすべて`sandboxed=true`で確認しました。

| 確認 | 結果 |
|---|---|
| PowerShell構文解析 | 成功 |
| 専用プロファイルでCodex `--version` | 成功（0.164.0） |
| 専用プロファイルでClaude `--version` | 成功（2.1.310） |
| Codex起動後のHOME復元 | 成功 |
| 専用プロファイルでOpenCode `--version` | 読み取り許可追加後に成功（2.1.4） |
| OpenAI API / ChatGPT Codex API / Anthropic APIへの未認証HTTPS | HTTP 401（通信成功、認証は未確認） |
| OpenAIのOpenID設定エンドポイントへのHTTPS | HTTP 200 |
| Codex実モデル text / json / loop / and | 全4ケース成功（`gpt-6-luna`） |
| OpenCode実モデル text / json / loop | 成功（`openai/gpt-6-luna`） |
| OpenCode実モデル and | 最新の再確認で成功。並列ノード各2回、ANDの新しい全入力の待ち合わせ2回を確認 |
| Claude Code実モデル text / json / loop / and | 全4ケース成功（`claude-haiku-5-5`、Proログイン後） |
| CTest core / gui_smoke | 成功 |
| CTest gui | GUI改修後の再ビルド・テスト修正を行い成功（QtTest 57 passed / 0 failed、プロジェクト別フォーム・下書き／選択復元・境界ドラッグ・ノードズーム・選択中のみ停止・入力待ち中の操作・編集と実行の分離・並列手動開始・既存GUI機能を含む） |
| モデル一覧：Codex | 成功（7件、app-serverの`model/list`） |
| モデル一覧：Claude Code | 成功（13件、制御プロトコル初期化応答の`models`） |
| モデル一覧：OpenCode | 成功（25件、`models --standalone`） |
| CTest catalog | 成功（QtTest 22 passed / 0 failed） |
| CTest progress | 成功（QtTest 10 passed / 0 failed、実CLI応答に基づくfixture・Markdown/JSON UI） |
| CTest script | 成功（QtTest 36 passed / 0 failed / 0 skipped、システムPython 3.15.1／Node.js 24.22.1、stdin/stdout・cwd・ループ・AND・エラー・中止） |
| CTest shell | 成功（QtTest 23 passed / 0 failed / 0 skipped、システムPowerShell、変数・JSON結果・保存先基準cwd・ループ・AND・出力上限） |
| CTest coordinator | 成功（QtTest 27 passed / 0 failed / 0 skipped、fake_agent使用、プロジェクト作成・非同期開始・待機・並列・循環検出・中止） |
| CTest instance | 成功（QtTest 6 passed / 0 failed、別プロセスによる重複拒否・通常解放・強制終了後の回復・ファイルエラー） |
| CTest context | 成功（QtTest 34 passed / 0 failed、3エージェントの引き継ぎオン／オフ・ループ個別履歴・ノード／Runner間分離・中止／エラー・再実行時のリセット。fake_agent使用、5回連続成功も確認） |
| CTest userinput | 成功（QtTest 12 passed / 0 failed、ユーザー待機・選択分岐・文字列出力・ループ／FIFO／並列・個別履歴・中止／エラー・子プロジェクトへの回答） |

最新のサンドボックス内実テストは3エージェント×4ケースの全12件が成功。
以前のOpenCodeの並列起動2回の失敗（`Standalone server exited before reporting readiness`）は
今回再現していない。原因特定や上流問題の修正を行ったわけではないため、再発時は起動ログを確認する。
GUIの縮小テストは初期レイアウト確定前の値との比較を修正し、Windowsのオフスクリーン描画に
必要なフォントディレクトリを指定した。詳細ログは`build/gui-results.txt`。
