# テスト結果（2026-10-26）

## 自動テスト

Windows 11 / MSVC 2022 / Qt 6.12.0 Release構成。
ノード内フォームへのGUI改修後にReleaseビルドを完了し、以下がすべて成功。

- コアのQtTest: 26 passed / 0 failed。
- GUIのQtTest: 9 passed / 0 failed。
- モデル一覧取得のQtTest: 22 passed / 0 failed。
- GUI起動・編集・読み取り専用グラフのスモークテスト: 成功。
- CTest: `core` / `gui` / `catalog` / `gui_smoke` の4件すべて成功。

GUIではフォーム編集・右ペイン同期・折り返しと改行による1〜8行の伸縮、
始端／終端の黄色背景と1行表示、ノード連番の非表示、フロー一覧、ウィジェット寿命を確認。
ノードと追加ボタンの説明ツールチップも確認した。
初期レイアウト確定前のサイズと比較していた縮小テストを修正し、
プレビュー1行に不要なスクロールバーが出る問題と右ペイン表示時の見切れも調整した。
オフスクリーンテストではWindowsのフォントディレクトリを明示指定する。
画像確認用の出力は`build/gui-editor.png`、詳細ログは`build/gui-results.txt` / `build/core-results.txt`。
モデル一覧の回帰テストには3つのCLIプロトコル、Codexのページ送り・重複除去・非表示モデル、
異常応答・出力上限・タイムアウト、キャッシュ、CLI変更、設定済みIDを変更しないUI更新を含む。
選択式ボックスについて、エージェント・モデル・分岐条件・接続先の候補リストの文字色・背景色・選択色と、選択操作を検証した。
画像確認用の出力は`build/gui-combo-popup.png`。

### 実CLIからのモデル一覧取得

サンドボックス内の専用プロファイルで、アプリと同じ`ModelCatalog`を使って確認した。
プロンプトやモデルへの推論リクエストは送っていない。

| エージェント | 結果 | 件数 |
|---|---|---|
| Codex | 成功 | 7 |
| Claude Code | 成功 | 13 |
| OpenCode CLI | 成功 | 25 |

件数は確認時点のCLI設定による。取得結果は`build/*-models-sandbox.json`、
自動テストの詳細は`build/catalog-results.txt`。認証情報・初期化応答全体は保存しない。

今回追加した確認:

- 完了済みノードがループの戻り辺から再発火する（カウンタ1→2→3）。
- ANDが2回発火し、各回で新しい全入力を消費する。
- 実行中ノードに追加の入力が届いても失わず、FIFOで再実行する。
- ANDの片側に先着した複数イベントを保持する。
- 前回のAND入力を再利用せず、次の片側入力だけでは再発火しない。
- 非選択分岐は実行せず、通常ノードの入力で合流できる。
- エージェントを含まない同期無限ループも停止できる。
- ループ内の不正な変数、および初回入力と戻り辺のスキーマ不整合を検出する。

## 実CLI / 実サービス

### 最新結果：サンドボックス内実行

`scripts/live-test.ps1`を使い、今回の実行はすべてsafe-shellの`sandboxed=true`で実施した。
サンドボックス外実行、権限の自動承認、認証情報のコピーや変更は行っていない。
追加の権限要求は不要だった。既存の専用プロファイルを利用し、通常ユーザーのサービスや設定は変更しない。

| エージェント / モデル | text | json | loop | and |
|---|---|---|---|---|
| Codex / `gpt-6-luna` | 成功 | 成功 | 成功 | 成功 |
| Claude Code / `claude-haiku-5-5` | 成功 | 成功 | 成功 | 成功 |
| OpenCode CLI / `openai/gpt-6-luna` | 成功 | 成功 | 成功 | 成功 |

全12ケース成功。`loop`は加算ノードの2回の実行、`and`は並列エージェント各2回と
ANDの2回の新しい入力待ち合わせを確認し、いずれも最終出力は`{"count":2}`。
OpenCodeの過去2回の専用サーバー起動失敗は今回再現しなかったが、原因特定や
上流の起動問題の修正を行ったという意味ではない。

設定と再実行方法は[サンドボックステスト手順](sandbox-tests.md)を参照。
最新の詳細結果は`build/{codex,claude,opencode}-{text,json,loop,and}-sandbox-live.json`。
CLIログや専用プロファイルは認証・セッション情報を含むため共有・配布しない。

### 従来の通常ユーザーでの結果（履歴）

アプリ本体と同じ`Runner`を使用する`agentnodify_live_test`で実施。
作業ディレクトリはテスト専用の一時ディレクトリ。
プロンプトではツール利用・ファイルの読み取り／変更を禁止し、モデルは明示指定。
権限の自動承認、認証情報の変更、サービスのインストールは行っていない。

| エージェント | テスト | 結果 |
|---|---|---|
| Codex / `gpt-6-luna` | テキスト | 成功: `AGENTNODIFY_OK` |
| Codex / `gpt-6-luna` | JSON Schema | 成功: `{"count":1}` |
| Codex / `gpt-6-luna` | ループ | 成功: 加算ノード2回、分岐2回、最終出力`{"count":2}` |
| Codex / `gpt-6-luna` | ANDを含むループ | 成功: 並列エージェント各2回、AND2回、最終出力`{"count":2}` |
| Claude Code 2.1.310 / `claude-haiku-5-5` | テキスト | 起動したが認証で停止: `Not logged in · Please run /login` |
| OpenCode CLI / `openai/gpt-6-luna` | テキスト | 成功: `AGENTNODIFY_OK` |
| OpenCode CLI / `openai/gpt-6-luna` | JSON出力＋スキーマ検証 | 成功: `{"count":1}` |
| OpenCode CLI / `openai/gpt-6-luna` | ループ | 成功: 加算ノード2回、分岐2回、最終出力`{"count":2}` |
| OpenCode CLI / `openai/gpt-6-luna` | ANDを含むループ | 成功: 並列エージェント各2回、AND2回、最終出力`{"count":2}` |

### 実環境から分かったこと

- 起動時のPATHには追加されたCLIが反映されていなかった。
  WindowsのレジストリPATHも検索するように修正し、Codexを解決できた。
- `gpt-6.0-luna`はモデル未対応エラー。モデル一覧で確認した`gpt-6-luna`で成功。
- サンドボックスでは別ユーザーとして実行され、Claudeの認証を利用できなかった。
  許可を得て通常ユーザーとして再実行したが、こちらも未ログインだった。
- 当初PATH上の`opencode`はGUI版を起動し、`opencode-cli`は未検出だった。
  ユーザーによるCLIのPATH追加後、`--program opencode-cli`で全4ケースが成功した。
  通常ユーザーで実行し、計12回のモデル応答を受信。CLIイベントは`step_start`と`text`のみで、
  ツール呼び出しはなかった。OpenCodeのJSON出力は生成時のスキーマ強制ではなく、
  プロンプトで形式を指定して受信後にJSON Schema検証する方式で確認した。

### 従来の認証問題について（履歴）

以下は通常ユーザーでの初期テスト時の条件。専用プロファイルでのログインは
すでにユーザー操作で完了し、上記の最新サンドボックステストではClaude Codeも成功した。

1. Claude Codeを通常ユーザーで起動し、`/login`を完了する。
2. CLIが使用できる許可モデルIDを設定し、`--case text`、`json`、`loop`、`and`を実行する。

```powershell
.\build\Release\agentnodify_live_test.exe --agent "Claude Code" --program C:/Users/User1/.local/bin/claude.exe --model claude-haiku-5-5 --case text --report build/claude-text-live.json
```

OpenCodeの再実行例（`--case`を変えると他の確認も実行できる）:

```powershell
.\build\Release\agentnodify_live_test.exe --agent OpenCode --program opencode-cli --model openai/gpt-6-luna --case and --report build/opencode-cli-and-live.json
```

個々の実行結果・イベント・ノード別実行回数・CLIログはローカルの`build/*host-live.json`に保存。
今回のOpenCodeの結果は`build/opencode-cli-{text,json,loop,and}-host-live.json`。
実行ログにはセッション情報等を含むため、この結果概要には生ログを転載していない。
