# テスト結果（2026-10-26〜2026-10-27）

## 自動テスト

Windows 11 / MSVC 2022 / Qt 6.12.0 Release構成。
ノード内フォームへのGUI改修後にReleaseビルドを完了し、以下がすべて成功。

- コアのQtTest: 30 passed / 0 failed。
- GUIのQtTest: 17 passed / 0 failed。
- モデル一覧取得のQtTest: 22 passed / 0 failed。
- 進捗表示のQtTest: 10 passed / 0 failed。
- GUI起動・編集・読み取り専用グラフのスモークテスト: 成功。
- CTest: `core` / `gui` / `catalog` / `progress` / `gui_smoke` の5件すべて成功。

### CLI進捗の整形表示

- Codexの公式[非対話実行](https://developers.openai.com/codex/noninteractive)と[SDKイベント定義](https://github.com/openai/codex/blob/main/sdk/typescript/src/items.ts)、Claude Codeの[CLI参照](https://code.claude.com/docs/en/cli-reference)と[streaming-output](https://platform.claude.com/docs/en/agent-sdk/streaming-output)、OpenCode V2の[CLI](https://opencode.ai/v2/docs/cli/commands/)と[OpenAPI](https://opencode.ai/v2/openapi.json)を確認した。
- 保存済みの実CLI応答を読み、CLIの`item.*`、`assistant/user/result`、`text/tool_use`の形を確認。実装した`ProgressParser`でもローカル再生し、結果を`build/progress-replay-results.txt`へ保存した（新たな推論は行っていない）。

| 再生した実CLIログ | 発言・最終応答 | ツール | ファイルアクセス |
|---|---:|---:|---:|
| Codexのファイル作成テスト | 2 | 0 | 0 |
| Claude Codeのファイル作成テスト | 1 | 1 | 1 |
| OpenCodeのファイル作成テスト | 1 | 1 | 1 |

Codexの保存済みテストはポリシー拒否で、構造化ツールイベントが出ていない。そのためコマンド・MCP・Web検索・ファイル変更の解釈は公式定義に基づくfixtureで確認し、拒否のstderrは簡易表示とした。

- Claude Codeはインストール済みCLIの`--help`でも`--include-partial-messages`対応を確認した。
- 3形式のツールIDによる開始・更新・結果の統合、Claudeの並列ツール結果の対応、テキスト／入力JSONの差分と完全メッセージの重複防止を検証。
- ツールの入力・出力が初期状態で独立して閉じており、更新でも開閉状態が残り、別の実行に切り替えると閉じることを確認。
- ファイル名と読取・書込・作成・変更・削除、失敗・中止の状態を検証。OpenCodeではV2の`state.content`と既存の`state.output`、ネストしたパッチメタデータも検証した。
- 600文字の文字列で500文字＋「...以下 100文字」、550個の絵文字で500個＋「...以下 50文字」、12要素の配列で8要素＋「...他 4件の項目」を確認。JSON文字列スカラーにも対応する。
- Markdownが`QTextBrowser`で太字・コードブロック等を表示し、リンクの自動遷移・画像のファイル取得が無効であることを確認。
- 未知イベント、壊れたJSON、行途中で分割された入力、改行なしの末尾テキスト、履歴の再生、ログ末尾追従とスクロール位置の維持を検証。
- 画像は`build/gui-progress-log.png`（ツール詳細を展開した例）、ログは`build/progress-results.txt`。`agentnodify_progress_probe`は保存済みのテストレポートだけを解釈し、CLI起動・ファイル操作・推論は行わない。

### 作業画面の選択枠・右ペイン余白

- 進行状況と出力の各領域の上余白が8pxであることを検証。
- ノード本体・フォーム内の選択で白い太枠になり、シーンの選択解除・背景クリック・ログや履歴へのフォーカス移動でも維持されることを検証。
- 別ノードへのクリックで枠が移り、旧ノードの通常枠が復元すること、始端・終端の黄色い塗りを変えないことを確認。
- 実行状態の更新と同一プロジェクトのタブ切替でも白枠が維持されることを確認。

### ノード単位の実行履歴

- 右ペインが縦スプリッターの3領域になり、履歴から実行1回分のログ・出力を選べることを確認。画像は`build/gui-node-history.png`。
- fixtureによる3回のループを2回実行し、エージェントの履歴6件がフロー単位の見出しなしに並ぶことを確認。開始・終了日時と各回で異なる出力、混在しないログ、再実行後も残る古い履歴、閲覧中の選択の維持、最新追従への復帰を検証。
- 始端・分岐・終端を含む全ノードでは2回のフローで16件の実行履歴を確認。実際の実行回数と履歴件数が一致し、実行前の検証失敗では履歴が作られないことを確認。
- 起動失敗・CLI異常終了・スキーマ不一致・タイムアウト・ユーザー中止の回も、それぞれ独立した履歴になることを確認。
- 一時プロファイルへ保存後に新しいWindowで復元し、プロジェクトID・日時・日本語ログ・出力・選択表示を確認。プロジェクト切替時の履歴混在がないことも検証。
- 保存記録の上書きが同じ実行IDに限定されること、壊れたJSONのスキップ、未完了記録の中断表示、開始日時の保持、IDのパストラバーサル拒否、待機／スキップ状態の保存拒否を検証。
- ノード削除後にフローを保存・復元しても、そのIDを新ノードで再利用しないことを確認。
- 実CLIの有料推論はこの機能の検証では実行せず、fixtureとサンドボックス内の保存テストで検証した。

### 2026-10-27: メニュー幅・外部アプリ・選択式モデル・ホイール

- ファイルメニューの固定最小幅を廃止し、項目幅とメニュー幅の差が30px未満、名前とショートカットを収める余白があることを確認。画像は`build/gui-file-menu.png`。
- プロジェクトの「…」がゴミ箱の左にあり、検出済みアプリとメニューの項目が一致することを確認。実在するネイティブ.exeのみを掲載する。テストではエクスプローラー・コマンドプロンプト・PowerShell・Git Bash・VS Code Insidersを検出し、通常版VS Codeは掲載されなかった。外部の対話アプリは自動起動していない。画像は`build/gui-project-open-menu.png`。
- 空白・日本語・`&`・括弧を含むパスについて起動引数を検証し、シェルコマンドへの埋め込みがないことと、存在しない起動先のエラーを確認。
- モデルは編集不可、空のモデルIDおよび「CLI既定」の項目なし。一覧更新で既存IDを保持し、候補外文字列を指定しても選択値は変化しないことを確認。未選択でのアプリ実行はモデル選択を促す。
- 80項目のモデル候補を埋め込みポップアップで開き、GraphicsViewにホイールイベントを送信。リストのスクロール位置が変わり拡大率は不変、末尾でも拡縮・キャンバスのスクロール位置は不変、背景上では拡縮することを確認。

### カレントディレクトリ・カーソル・右クリック削除・メニュー余白

- 実行元はリポジトリルート、プロジェクトは外側サンドボックスの一時領域に作った別ディレクトリ。Git管理外かつ空白・日本語を含む名前で検証した。
- `cwd`実CLIテストは、ツールが返す現在地と、相対パス`agentnodify-cwd-marker.txt`のランダム値を照合する。プロンプトには正解のディレクトリやランダム値を含めない。
- OpenCode / openai/gpt-6-lunaとClaude Code / claude-haiku-5-5は、両方の照合に成功。結果は`build/{opencode,claude}-cwd-sandbox-live.json`の`launcherCwd`・`project`・`actual`に保存。
- Codex / gpt-6-lunaは確認用コマンドもポリシー拒否となり実際の現在地は未確認（`build/codex-cwd-sandbox-live.json`）。fixtureでは`--cd`の引数と子プロセスの実際の現在地を検証した。
- 子プロセスのcwdと`PWD`をプロジェクトの正規化絶対パスに統一。わざと誤った`PWD`を親に設定しても3種のCLI fixtureが正しい場所で動作することを確認。
- OpenCodeのGUI側は以前共有サービス方式、実CLIテストは専用サーバー方式だった。通常ユーザーの共有サービスを使う再現試験は外側のサンドボックス境界を保つため実施していない。タスク実行はGUIも常に`--standalone`へ統一し、共有サービスに依存しないようにした。共有サービス自体の停止・変更はしていない。
- 日本語パスの初回OpenCode検証では、ツールのPowerShell出力が文字化けしたが相対ファイルの内容は一致した。検証プロンプトにPowerShellのUTF-8出力指定を加え、現在地と内容の両方の一致を確認。一般のPowerShell出力の文字コードをアプリが自動修復する変更ではない。
- GraphicsView上の選択式入力にマウスを移動し、実際のviewportのカーソルが矢印になることを確認。
- ノード本体の右クリックイベントから「削除」を実行し、ノード・接続辺の削除と始端の削除不可を確認。
- ファイルメニューの幅と名前・ショートカットの余白を確認。スクリーンショットは`build/gui-file-menu.png`。
- 全4種のCTestを2回ずつ実行して成功。サンドボックス外でのテスト・権限バイパスは行っていない。

### 一覧操作・作業ノード・実ファイル作成の追加確認

- フロー追加メニュー、右クリック設定、内部定義の削除、外部ファイルの保持を確認。
- テーマ付きのノード内プレビューについて2〜8行でスクロールバーなし・スクロール範囲0を確認。
- 作業画面のノード本体をGraphicsView経由でクリックし、右ペインへの選択反映を確認。フォーム内クリック・編集不可・実行中ログ・完了出力も確認。
- 思考量の保存・復元、3つのCLI引数変換、未指定時に余計な引数を渡さないことをfixtureで検証。
- OpenCodeの保存済み実行名が旧初期値`opencode`だったことを確認し、Windowsで`opencode-cli`に移行するよう修正。専用プロファイルのCLI一覧取得は25件成功。通常プロファイルを使う共有サービス経由の取得は今回のサンドボックスでは未検証。

実ファイルテストは外側のOSサンドボックス内、専用プロファイル、空の`build/file-check-<UUID>/`で実行。
依頼は`agentnodify-check.txt`の作成であり、エージェントの自己申告ではなく実ファイル内容を検証した。

| エージェント | 結果 |
|---|---|
| OpenCode / openai/gpt-6-luna | 既存権限設定で作成・内容確認成功（自動承認オプションなし） |
| Claude Code / claude-haiku-5-5 | 既定モードではWrite権限拒否。明示的なacceptEdits指定で作成・内容確認成功 |
| Codex / gpt-6-luna | 既定とworkspace-write指定の両方でコマンドがblocked by policyとなり未作成。環境のポリシー／Windowsサンドボックス設定の確認が必要。成功扱いにはしない |

レポートは`build/{codex,claude,opencode}-file-sandbox-live.json`（各エージェントの最終実行）。
書き込み許可はアプリのCLI設定で明示的に選ぶ場合だけ付与し、全権限バイパス・外側のサンドボックス外での再実行は行っていない。
CLIの正常終了はタスク達成を保証しないため、画面の完了メッセージにも出力確認の注意を追加した。

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
