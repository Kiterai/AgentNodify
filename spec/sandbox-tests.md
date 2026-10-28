# 実CLIテストの設定・認証

通常のCTestは偽CLIを使い、認証や課金は不要です。以下は外部サービスへ接続する任意の統合テストです。
ビルド環境は[README](../README.md#ビルドと起動)を参照してください。

## 前提と安全上の境界

- 利用するCLIのネイティブ`.exe`を導入してください。`.cmd` / `.bat`は非対応です。
- `scripts/live-test.ps1`の`-Program`に実際の絶対パスを指定します。省略時はPATH上の`codex.exe`、`claude.exe`、`opencode-cli.exe`を検索します。個人のインストール先は推測しません。
- Qtは`-QtPath`または`Qt6_ROOT`で指定します。省略する場合、テスト実行ファイルからQt DLLが見つかるようPATHまたはDLL配置を設定してください。
- スクリプトはOSサンドボックスを作りません。必要なら自身の実行環境で用意し、CLI・Qtの読み取り、ビルド／テスト領域の書き込み、認証方式に必要な通信先だけを許可してください。包括的な権限許可や拒否設定の解除は不要です。
- アプリと同様、Codexでは`workspace-write`、Claude Codeでは`acceptEdits`を指定します。OpenCodeは自身の権限設定に従います。`-WorkspaceWrite`は互換用であり、権限を切り替えません。

## 分離プロファイル

スクリプトはHOME / USERPROFILE / APPDATA / LOCALAPPDATA、CODEX_HOME、CLAUDE_CONFIG_DIR、XDG設定を
`build/live-profile/`に切り替え、終了時に呼び出し元の環境変数を戻します。TEMP / TMPは変更しません。
通常ユーザーの認証ファイルはコピーせず、専用プロファイルで別途ログインします。
環境変数から継承されるAPIキーやOS資格情報ストアまで隔離するものではありません。
実行ユーザーや資格情報ストアに依存する認証は、各CLIの公式手順に従って設定してください。

`build/live-profile/`には認証情報が含まれる場合があります。レポート、ログ、生成プロジェクトにも機密情報が含まれ得るため、コミット・配布しないでください。

## 手順（Windows PowerShell）

リポジトリルートで、まずREADMEの手順により`build/`へビルドします。別ディレクトリへビルドした場合、このスクリプトではなくテスト実行ファイルを直接指定してください。

```powershell
$qt = Read-Host 'Qtキットの絶対パス'
$cli = Read-Host 'Codexのネイティブ .exeの絶対パス'

# バージョン確認のみ（既定動作）
.\scripts\live-test.ps1 -Agent Codex -Program $cli -QtPath $qt

# ユーザーの対話ターミナルで初回ログイン
.\scripts\live-test.ps1 -Agent Codex -Program $cli -QtPath $qt -Login

# モデル一覧の取得のみ（推論プロンプトを送らない）
.\scripts\live-test.ps1 -Agent Codex -Program $cli -QtPath $qt -Models

# 明示実行のみ。外部通信・課金が発生し得る
.\scripts\live-test.ps1 -Agent Codex -Program $cli -QtPath $qt -Model gpt-6-luna -Case text -Run
```

Claude Codeは`-Agent 'Claude Code'`、OpenCodeは`-Agent OpenCode`とし、対応するCLIパスを指定します。
`-Login` / `-Models` / `-Run`は同時指定できません。`-Configuration Debug`でDebug版を使用できます。
`-Run`には`-Model`が必須です。仕様で許可するLuna / HaikuのIDだけが受理されます。OpenCodeでは必要に応じてプロバイダ接頭辞を付けます。
モデルの利用可否は契約・CLI・プロバイダで確認してください。失敗時に別モデルへ自動変更しません。

`-Case`は`text` / `json` / `loop` / `and` / `file` / `cwd`です。
`file`はテストファイルを書き込み、`cwd`はテスト用マーカーを読み取ります。
`-ProjectDirectory`を指定する場合は、既存の重要ファイルがないテスト専用ディレクトリを使用してください。
生成プロジェクトとレポートは検証用に保持されます。不要になったデータは各自で削除してください。

## 証明書・通信エラー

CLIが利用する証明書ストアと、組織のプロキシ／CA設定を確認してください。
Codexで明示的なCAバンドルが必要な場合だけ`-CodexCaCertificate`にそのファイルを指定します。
既存の`CODEX_CA_CERTIFICATE` / `SSL_CERT_FILE`は、明示指定しない限り変更しません。
Git等のインストール先から証明書を推測したり、証明書検証を無効にしたり、サンドボックス外で自動再試行したりはしません。
