# IFWインストーラ

## 方針

CMakeのinstall定義とCPackのIFWジェネレータを使う。通常ビルドでは無効で、`AGENTNODIFY_BUILD_INSTALLER=ON`の場合だけ有効になる。Qt Installer Framework（IFW）4.0以上を別途用意する。

初期構成は**オフライン専用**で、Maintenance Toolによるアンインストールを提供する。更新URL、オンラインリポジトリ、アプリ内更新確認はまだ設定していない。架空の配信URLや認証情報は埋め込まない。

インストール対象は、アプリ・Qt依存・MSVCランタイム・サンプル・ライセンスをまとめた単一の必須コンポーネント `org.agentnodify.application`。バージョンはCMakeの`project(VERSION)`に従う。依存ライブラリのinstall規則や開発ディレクトリ全体をパッケージへ取り込まず、コンポーネントを明示する。

## Windowsでの生成

通常のビルド環境に加えて、QtのMaintenance Tool等でIFWを導入する。`binarycreator.exe`、`installerbase.exe`、`repogen.exe`を含むIFWのルートを指定する（`bin`ではない）。ツールを自動ダウンロードするスクリプトや、開発者個人のパスは共有設定に含めない。

```powershell
$qt = Read-Host 'Qtキットの絶対パス'
$ifw = Read-Host 'IFWの絶対パス（binの親）'
.\scripts\package.ps1 -QtPath $qt -IfwPath $ifw
```

`Qt6_ROOT`と`QTIFWDIR`による指定も可能。`-BuildDirectory`、`-Generator`、`-Architecture`はビルドスクリプト同様に指定できる。既定ビルド先は`build`。Qtキットや生成器を変更するときは新しいビルド先を使う。

スクリプトはRelease構成、ビルド、通常のCTestと配布レイアウト検査、CPack実行を順に行い、失敗時は中止する。実CLIの認証・モデル取得・課金テストは実施しない。出力先は `<build>/installer/AgentNodify-<version>-<OS>-<CPU>-Setup.exe`。

Qt依存は`qt_generate_deploy_app_script()`でinstall時に収集する。MSVCのReleaseランタイムは`InstallRequiredSystemLibraries`でアプリローカル配置するため、別のVC++インストーラや管理者昇格を通常必要としない。再配布権は使用するVisual Studioの契約を確認する。

`build/Release`の丸ごとコピーはしない。偽CLI、テストexe、静的ライブラリ、ユーザー設定、実行履歴は配布しない。

### 手動実行・他OS

```sh
cmake -S . -B build-package -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="<Qt-kit>" \
  -DAGENTNODIFY_BUILD_INSTALLER=ON -DCPACK_IFW_ROOT="<IFW-root>"
cmake --build build-package --config Release --parallel
ctest --test-dir build-package -C Release --output-on-failure
cpack --config build-package/CPackConfig.cmake -C Release -G IFW
```

マルチ構成生成器でも必ず`-C Release`を指定する。macOSでは`.app`バンドルを作成し、LinuxではQtの依存収集に対応するQt 6.7以上を要求する。各OSでネイティブにビルド・生成する。macOS／Linuxの構成は用意しているが、現時点で実機検証しているのはWindowsのみ。Linuxでのglibc互換性、macOSの署名・公証、OS別のアーキテクチャ対応は別途検証する。

## インストールと削除

既定のインストール先はユーザー単位。

| OS | 配置先 |
|---|---|
| Windows | `%LOCALAPPDATA%/Programs/AgentNodify` |
| macOS | `~/Applications/AgentNodify` |
| Linux | `~/.local/opt/AgentNodify` |

WindowsのLOCALAPPDATAはインストーラ実行時に解決する。ユーザーが明示指定したインストール先を優先する。WindowsではスタートメニューにアプリとMaintenance Toolのショートカットを登録する。

削除にはインストール先の`MaintenanceTool`を使う。Windowsでは「設定 → アプリ」からも開始できる。ファイルとショートカットはIFWの操作記録に従って削除する。未知のファイルを巻き込む再帰的なインストール先削除は無効にしているので、空のインストール先フォルダが残る場合がある。

**更新・削除前にトレイメニューまたはファイルメニューからアプリを終了する。** 閉じるボタンだけでは常駐が続く。Windowsの削除では実行プロセスの停止を促すIFW操作を登録し、フローを強制終了する操作は追加しない。他OSでは終了を手動確認する。

AppData配下の設定・フロー・履歴、ユーザー指定のフロー、外部プロジェクト、生成ファイル、CLIと認証情報は削除しない。サンプルは編集前に作業場所へコピーする。アプリ内のユーザーデータ削除機能や無人の強制終了は今回追加しない。

## ライセンスと公開前の準備

この構成で生成できるインストーラは**ローカル検証用**。生成成功は公開配布の法的・安全性要件を満たしたという意味ではない。アプリ自体の配布ライセンスも所有者が決める必要があり、この変更では新しい製品ライセンスを設定しない。

QtNodes、nlohmann/json、JSON Schema Validatorのライセンス本文と、依存の案内を同梱する。Qt本体・IFWとそれぞれに含まれる第三者コードの配布条件は、実際に使ったバイナリの版・ビルドに合わせて別途整理する。IFW側の静的Qtと、アプリ側の動的Qtを混同しない。

公開配布時は、例えば以下を含むディレクトリを作り、`-LegalDirectory <path>`（CMakeでは`AGENTNODIFY_LEGAL_DIRECTORY`）で渡す。内容は`licenses/distribution/`へコピーされる。未指定なら構成時に警告する。ディレクトリの存在だけでは条件適合を検証できないため、公開前レビューを省略しない。

- QtとIFWの、使用版に対応したライセンス・例外・著作権表示。
- QtとIFWに含まれる第三者コードの表示とライセンス。
- 適用条件に従った対応ソースの提供方法と、必要なソース・変更・ビルド情報。単なるプロジェクトトップへのリンクで済ませない。
- 商用ライセンスを使う場合の適用範囲の確認、MSVCランタイムの再配布条件の確認。

IFWオープンソース版のGPLv3＋例外と、QtのLGPL等は別に確認する。IFWを使うだけで独立したアプリ本体が必ずGPLになるわけではないが、インストーラ／Maintenance Tool側の義務はなくならない。最終判断は採用版の条文と必要に応じた法務確認による。

署名鍵・認証情報はリポジトリへ置かない。今回のインストーラは未署名。Windowsのコード署名、macOSの署名・公証はリリース工程へ別途組み込む。

## 検証

`packaging_layout`テストは新しい一時ステージへ対象コンポーネントだけをinstallし、本体・Qt・MSVCランタイム・サンプル・ライセンスの存在と、開発物／ユーザーデータの非混入を確認する。Windowsでは開発用PATHとQtプラグイン環境変数に依存しないステージ済みexeの`--smoke-test`も実行する。検証ステージは調査用にビルド先へ残す。

生成物のメタデータとスクリプトの読み込みは、インストールせずに確認できる。キャッシュの保存先を作業ディレクトリ内へ明示する。

```powershell
& '<生成したSetup.exe>' --no-proxy --cache-path ./build/ifw-cache search
```

WindowsでIFW 4.7.0を使い、Releaseビルド・全15件のCTest・CPackによるexe生成・上記のコンポーネント検索を確認済み。Debug指定のCPack生成が拒否されることも確認した。実際のOSへのインストール／アンインストールや、クリーンVMでの検証は別途必要。

公開前には開発環境のないVM等でも以下を確認する。

1. 日本語・空白を含むパスへの非管理者インストール。
2. スタートメニューからの起動と、CLI未導入時の動作。
3. 既存設定がない状態からの保存、終了、再起動。
4. 常駐中・フロー実行中の削除操作で、予期せぬデータ損失がないこと。
5. Maintenance Tool／Windows設定からの削除、ショートカット解除。
6. 設定・履歴・外部プロジェクトと、インストール先の未知ファイルが保持されること。

## 将来の更新配信

更新URLと運用方針が決まった後、OS・CPUごとのHTTPSリポジトリ、バージョン管理、Maintenance Tool自身の更新、署名・配信元保護を追加する。IFW 4.11等のオフライン／hybrid動作とCPackの対応を採用版で確認する。リポジトリを追加するだけで、現在のオフライン専用インストールに自動更新が有効になると仮定しない。

アプリ内の更新通知と、安全な保存・終了・再起動も別実装。ユーザーデータの形式移行はインストーラのファイル更新とは別に管理する。
