# インストーラ

macOSのネイティブ`.pkg`とGitHub Actionsは[macOS配布・CIガイド](macos-packaging.md)を参照してください。以下は既存のIFW工程です。Windowsの生成・リリースは変更しません。
`master`の自動公開は[Windows／macOS統合リリース](multiplatform-release.md)へ移行し、同じ版の3インストーラがすべて検証済みになるまで公開しません。以下のWindows単体工程の説明より統合ジョブの構成を優先します。

## 方針

CMakeのinstall定義とCPackのIFWジェネレータを使う。通常ビルドでは無効で、`AGENTNODIFY_BUILD_INSTALLER=ON`の場合だけ有効になる。Qt Installer Framework（IFW）4.0以上を別途用意する。

初期構成は**オフライン専用**で、Maintenance Toolによるアンインストールを提供する。更新URL、オンラインリポジトリ、アプリ内更新確認はまだ設定していない。架空の配信URLや認証情報は埋め込まない。

インストール対象は、アプリ・Qt依存・MSVCランタイム・サンプル・ライセンスをまとめた単一の必須コンポーネント `org.agentnodify.application`。バージョンはCMakeの`project(VERSION)`に従う。`AGENTNODIFY_VERSION`キャッシュ変数（既定`0.1.0`）でビルド単位に上書きでき、アプリとインストーラに同じ版を使う。依存ライブラリのinstall規則や開発ディレクトリ全体をパッケージへ取り込まず、コンポーネントを明示する。

## Windowsでの生成

通常のビルド環境に加えて、QtのMaintenance Tool等でIFWを導入する。`binarycreator.exe`、`installerbase.exe`、`repogen.exe`を含むIFWのルートを指定する（`bin`ではない）。ローカル用スクリプトはツールを自動ダウンロードせず、開発者個人のパスは共有設定に含めない。GitHub Actionsでは後述の固定版を取得する。

```powershell
$qt = Read-Host 'Qtキットの絶対パス'
$ifw = Read-Host 'IFWの絶対パス（binの親）'
.\scripts\package.ps1 -QtPath $qt -IfwPath $ifw
```

`Qt6_ROOT`と`QTIFWDIR`による指定も可能。`-BuildDirectory`、`-Generator`、`-Architecture`はビルドスクリプト同様に指定できる。`-Version 0.1.1`でビルド・インストーラのバージョンを指定できる。既定ビルド先は`build`。Qtキットや生成器を変更するときは新しいビルド先を使う。

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

### AppData削除の検証（2026-10-30）

- ネットワーク・クリップボード・vGPU無効、入力読み取り専用／新規ログ出力のみ書き込み可のWindows Sandboxで確認。ゲスト実AppDataの既定保持46項目・明示削除40項目が成功し、入力マニフェストと両レポートのSetup.exe SHA-256一致を確認した。
- 実GUIでも確認ダイアログの既定が「いいえ」であること、「はい」でRoaming／Localのアプリ専用データが消えること、外部プロジェクトが残ること、Finish後のMaintenanceTool自己削除を確認。開始したSandboxは結果を回収してすべて終了した。
- IFWは標準入力も含めて対になったat記号間を変数展開するため、PowerShellコード内でat記号を使わない。終了コードだけでなく成功マーカーも検査する。起動失敗・非ゼロ終了・マーカー欠落、失敗／キャンセル時の削除拒否はオフラインテストで検証する。

既定のインストール先はユーザー単位。

| OS | 配置先 |
|---|---|
| Windows | `%LOCALAPPDATA%/Programs/AgentNodify` |
| macOS | `~/Applications/AgentNodify` |
| Linux | `~/.local/opt/AgentNodify` |

WindowsのLOCALAPPDATAはインストーラ実行時に解決する。ユーザーが明示指定したインストール先を優先する。WindowsではスタートメニューにアプリとMaintenance Toolのショートカットを登録する。

削除にはインストール先の`MaintenanceTool`を使う。Windowsでは「設定 → アプリ」からも開始できる。ファイルとショートカットはIFWの操作記録に従って削除する。未知のファイルを巻き込む再帰的なインストール先削除は無効にしているので、空のインストール先フォルダが残る場合がある。

**更新・削除前にトレイメニューまたはファイルメニューからアプリを終了する。** 閉じるボタンだけでは常駐が続く。Windowsの削除では実行プロセスの停止を促すIFW操作を登録し、フローを強制終了する操作は追加しない。他OSでは終了を手動確認する。

AppData配下の設定・内部フロー・履歴は既定で保持する。WindowsのGUIアンインストール開始時に、アプリデータも削除するか確認する（既定は「いいえ」）。CLIでは`MaintenanceTool.exe --confirm-command --default-answer purge RemoveAppData=true`で明示的に削除を選べる。通常の`purge`では保持する。

削除対象は実行ユーザーのWindows既知フォルダーにある`%APPDATA%/AgentNodify/AgentNodify`と`%LOCALAPPDATA%/AgentNodify/AgentNodify`の2ディレクトリのみ。正常なアンインストール完了後に実施し、失敗・キャンセル時には削除しない。PowerShellへ渡すコードは固定文字列で、利用者パスやインストーラー引数をコードに埋め込まない。組織ディレクトリまたは削除対象ツリーにジャンクション／シンボリックリンクがあれば削除全体を拒否し、手動確認を求める。親ディレクトリと他アプリのデータは残す。

ユーザー指定の外部フロー、外部プロジェクト、生成ファイル、CLIと認証情報はどちらの選択でも削除しない。インストール先の未知ファイル保持とMaintenanceToolの固定名自己削除は維持する。サンプルは編集前に作業場所へコピーする。無人の強制終了は追加しない。

## ライセンスと公開前の準備

AgentNodify本体のオリジナルコード・資料・サンプル・プロジェクト所有アセットには、ルートの`LICENSE`にあるMITライセンスを適用する。第三者のコードやライセンス文書をMITへ変更するものではない。著作権表示は`2026 AgentNodify contributors`とし、個々の貢献の権利を移転する意味ではない。

この構成で生成できるインストーラは、配布固有資料のレビューが完了するまでは**ローカル検証用**。生成成功や本体のMIT採用は、依存物のバイナリ配布条件を満たしたという意味ではない。

MIT本文をインストール先の`LICENSE`へ、依存の一覧・原文・出典を`licenses/`へ同梱する。QtNodes、nlohmann/json、JSON Schema Validatorの原文に加え、同梱Hedley・SMTPバリデータの表示、QtのLGPLv3／GPLv3本文、IFWのGPLv3＋Qt例外本文を含む。固定版依存3つのライセンスと、`packaging/distribution/APPLICATION-LICENSE.txt`のMIT本文は、構成時に元ファイルとの一致を検査する。原文を要約で置き換えない。

アプリの「ヘルプ → ライセンス・依存ソフトウェア…」では同じ原文をQtリソースからオフライン表示する。単体exe・ビルドツリーでも表示可能。`-LegalDirectory`配下の`.txt`／`.md`／`.rst`もビルド時に埋め込み、配布固有の表示・ソース提供案内を同じ画面で確認できる。秘密情報を含めない。文章は読み取り専用のプレーンテキストで、HTML解釈や外部取得を行わない。

Qt本体・IFWとそれぞれに含まれる第三者コードの**網羅的な表示と対応ソースの提供**は、実際に使ったバイナリの版・ビルドに合わせて別途整理する。IFW側の静的Qtと、アプリ側の動的Qtを混同しない。出典と適用範囲は`licenses/README.md`を参照する。

公開配布時は、例えば以下を含むディレクトリを作り、`-LegalDirectory <path>`（CMakeでは`AGENTNODIFY_LEGAL_DIRECTORY`）で渡す。内容は`licenses/distribution/`へコピーされる。未指定なら構成時に警告する。ディレクトリの存在だけでは条件適合を検証できないため、公開前レビューを省略しない。

- QtとIFWの、使用版に対応したライセンス・例外・著作権表示。
- QtとIFWに含まれる第三者コードの表示とライセンス。
- 適用条件に従った対応ソースの提供方法と、必要なソース・変更・ビルド情報。単なるプロジェクトトップへのリンクで済ませない。
- 商用ライセンスを使う場合の適用範囲の確認、MSVCランタイムの再配布条件の確認。

IFWオープンソース版のGPLv3＋例外と、QtのLGPL等は別に確認する。IFWを使うだけで独立したアプリ本体が必ずGPLになるわけではないが、インストーラ／Maintenance Tool側の義務はなくならない。最終判断は採用版の条文と必要に応じた法務確認による。

署名鍵・認証情報はリポジトリへ置かない。証明書を指定しない既定ビルドと現在のCIは未署名。Windowsの任意署名は後述の手順で実行できる。macOSの署名・公証は別途必要。

### 対応ソース・配布固有資料の生成

`packaging/sources.json`はWindowsリリース用の版・HTTPS取得先・SHA-256を固定する。アプリのQt 6.12.0（qtbase／qtsvg）と、IFW 4.7.0に静的リンクされたQt 6.6.0（qtbase／qtdeclarative／qttools／qt5compat）を別々に扱う。IFWソースは実行ファイルが報告するコミット`fad83b7a8ba6a9e3179236d11bba0ceb9b3de5b4`の版。IFWが報告するzlib 1.2.12、liblzma（XZ）5.2.5、bzip2 1.0.8のソースも含める。libarchive 3.7.1はIFWソースに含まれる。

Python 3.8以上の標準ライブラリだけで動く`scripts/distribution.py`が原文を収集する。アーカイブは展開・実行せずに読み、ライセンス本文、Qtの`qt_attribution.json`と参照先、第三者コードの先頭の著作権・ライセンスコメントを保持する。任意機能・未使用コードを含む保守的な上位集合なので、全項目が実バイナリへリンクされたという意味ではない。実ビルドの構成やベンダーパッチとの対応は別途レビューする。

まず公開せずに資料を収集し、レビューできる。出力先には新しい空ディレクトリを指定する。`--download`はネットワーク取得を明示許可するオプションで、省略時はキャッシュのみを使用する。

```powershell
python scripts/distribution.py collect --download `
  --cache build-source-cache --output build-distribution-review
```

生成物は`QT-NOTICES.txt`、`IFW-NOTICES.txt`、`APPLICATION-LICENSE.txt`、`SOURCE-MANIFEST.json`。この段階では`SOURCE-OFFER.txt`や承認済みファイルを作らない。対応ソースを含む原アーカイブは合計約160MB。原文の集約資料は約4.5MBで、生成物・大容量ソースをGitへコミットしない。

実リリースの`prepare`は、以下を追加で実施する。

1. 実際の`qmake -query QT_VERSION`と`installerbase --version`を確認し、Qt／IFW／内蔵Qt／IFWコミット／圧縮ライブラリの版がプロファイルと一致しなければ停止する。異なるQt版の資料を流用しない。
2. `git archive --format=zip`で作ったアプリソースのZIPコメントがリリースコミットSHAと一致することと、本体MIT本文を確認する。
3. 原アーカイブ、アプリソース、表示資料、版・ツールのSHA-256を含む`BUILD-INFO.json`を、`AgentNodify-<版>-Sources.zip`へまとめる。
4. 同じReleaseに置く実際のアセット名・URLを記した`SOURCE-OFFER.txt`と、ソースZIPのSHA-256を含む`distribution-release.json`を生成する。リンク先は公開前にはまだ存在しないため、**ソースZIPを含む全アセットのアップロード検証後にのみReleaseを公開する**。

この工程はGPLv3 6(d)のネットワーク上での同等アクセス方式を想定する。`SOURCE-OFFER.txt`という名前だけで、6(b)の3年間の書面オファー方式を採用したと扱わない。バイナリとソースを同じReleaseで提供し、必要な期間のアクセスを維持する。

ベンダーパッチや追加のビルド情報が必要な場合は、`--supplement-directory <dir>`でレビュー済みのファイルを渡す。直下の通常ファイルだけを受け付け、生成資料との名前衝突・シンボリックリンクを拒否する。ZIP内の`distribution/`、アプリの配布資料、ビルド記録に含める。CIでは`packaging/supplements/`が存在すれば同じ仕組みで取り込む。フォルダの準備だけでレビューを完了したとは扱わず、秘密情報を置かない。

手動実行の例（パス・版・リポジトリ・SHAは対象リリースの値を指定する）：

```powershell
git archive --format=zip --output=build-release/application-source.zip <commit-SHA>
python scripts/distribution.py prepare --download --cache build-source-cache `
  --output build-release/distribution --bundle-directory build-release/installer `
  --version <major.minor.patch> --repository <owner/repository> --commit <commit-SHA> `
  --qt-path <Qt-6.12.0-kit> --ifw-path <IFW-4.7.0-root> `
  --application-source build-release/application-source.zip
.\scripts\package.ps1 -QtPath <Qt-6.12.0-kit> -IfwPath <IFW-4.7.0-root> `
  -BuildDirectory build-release -Version <major.minor.patch> -LegalDirectory build-release/distribution
```

生成成功は法的承認ではない。上流のビルド済み配布物に必要なパッチ・設定・ビルド情報がある場合、その提供も含めて確認する。上流アーカイブだけで十分と自動判定しない。ソースアクセスを維持する責任は配布者にある。対応アセットを後から削除せず、保存・移転時も入手経路を維持する。Microsoftランタイム／コンパイラのソースをこのZIPに含めるとは表明しない。

### Windowsコード署名（任意）

証明書・署名サービスの契約や秘密鍵の作成は自動で行わない。Windows SDKのSignToolと、現在のユーザーの`My`証明書ストアからSignToolで利用できるコード署名証明書を別途用意する。ハードウェアトークン／プロバイダーの設定・PIN操作が必要な場合は手動で行う。クラウド署名サービスの独自CLIには、このヘルパーだけで対応するとは限らない。

```powershell
.\scripts\package.ps1 -QtPath <Qt-kit> -IfwPath <IFW-root> `
  -LegalDirectory <reviewed-distribution-directory> `
  -SigningCertificateThumbprint <40桁の証明書拇印> `
  -TimestampUrl <HTTPSのRFC3161タイムスタンプURL> `
  -SignToolPath <signtool.exe>
```

署名指定時は以下の順序を使う。

1. 元のSDKを変更せず、IFWの`installerbase.exe`をビルド先へコピーして署名・検証。
2. アプリをReleaseビルドして署名・検証し、通常テストを実行。
3. 署名済みベースをIFWのコンポーネント内の`maintenance/installerbase.exe`として同梱し、`setInstallerBaseBinary()`でMaintenance Tool生成に使う。Setupだけ署名してMaintenance Toolの署名も完了したと見なさない。
4. CPack生成完了後のSetup.exeを署名・検証。チェックサムは最終署名後のファイルから計算する。

`scripts/sign-windows.ps1`はSHA-256とRFC3161タイムスタンプを使い、`signtool verify /pa /all /v`で確認する。他の発行元の署名の置き換えは拒否し、第三者のQt DLL等を一括再署名しない。証明書の秘密鍵・PFX・PINをソースやログへ保存しない。拇印は秘密鍵ではないが、個人用の値を共有スクリプトへ固定しない。

公開用証明書は未提供。下記のSandboxで自己署名の処理と生成後Maintenance Toolの署名を確認したが、公開CAの証明書、RFC3161タイムスタンプ、証明書チェーン・失効検査、SmartScreenの挙動の検証には代えない。これらは公開用証明書の準備後に最終バイナリで確認する。署名後にバイナリを変更しない。CIにはまだ署名認証情報を設定せず、未署名であることをRelease本文へ明記する。

### 自己署名によるワークフロー検証（公開不可）

署名処理の入口は`scripts/sign-windows.ps1`へ集約する。通常は`package.ps1`の`-SigningCertificateThumbprint`、`-TimestampUrl`、`-SignToolPath`を変更するだけで証明書・RFC3161サービス・ツールを差し替えられる。現在はSignToolとCurrentUser/Myストアに対応し、独自CLIが必要なクラウド署名サービスには別途アダプターが必要。本番モードはHTTPSタイムスタンプ必須で、自己署名証明書を拒否する。

自己署名テストはWindows Sandbox専用。ホストの信頼ストアは変更しない。`scripts/prepare-signing-sandbox.ps1`で、CPackのステージの`config/`と`packages/`、必要なIFW／SignToolランタイム、検証スクリプトだけを読み取り専用入力へコピーする。SDKの依存マニフェストと入力全体のSHA-256を検査し、既存の出力先は拒否する。`-WhatIf`で準備計画を確認できる。

```powershell
.\scripts\prepare-signing-sandbox.ps1 `
  -PreparedPackageDirectory <CPackのIFWステージ> `
  -IfwPath <IFW-root> -SignToolPath <signtool.exe> `
  -OutputDirectory build-signing-sandbox
# 既存Sandboxを終了してから、生成したvalidate-signing.wsbを開く。
```

`run-signing-sandbox-guest.ps1`がゲスト内で以下を一括実行する。C++ビルドやCTestは行わない。

1. 入力をローカルAppDataへコピーし、SHA-256とSignToolの起動を確認する。
2. 有効期間7日・RSA/SHA-256・秘密鍵エクスポート不可の`AgentNodify TEST ONLY <乱数>`証明書をCurrentUser/Myに作成し、公開証明書だけをCurrentUser/Rootへ一時追加する。
3. 共通の`sign-windows.ps1 -TestOnlyNoTimestamp`で本体とMaintenance Tool用ベースに署名し、binarycreatorでオフラインSetupを再生成して最後に署名する。タイムスタンプ通信だけを省き、`signtool verify /pa /all /v`は省かない。
4. `test-installer.ps1 -RequireSigned -ExpectedSignerThumbprint <拇印>`で、3件の署名と発行元一致、導入・起動・削除、検証用ユーザーファイル保持を確認する。
5. 成否を問わず`finally`で今回のRoot／My証明書と秘密鍵だけを削除し、後始末結果を記録する。

ネットワーク・クリップボード・vGPUは無効。書き込み共有は新しい出力フォルダだけで、`guest-status.json`、`self-signing-test.json`、`result.json`とログだけを持ち出す。署名済みexe・秘密鍵・ホストの証明書ストアは共有しない。信頼追加・削除の確認ダイアログでは、ゲスト内のテスト証明書であることを利用者が確認する。強制終了で後始末が完走しない場合も、Sandboxを終了して環境を破棄する。拇印と進行状況は信頼追加前に出力する。

`release.mjs`は自己署名または自己署名判定のない有効署名の結果を公開に使うことを拒否する。自己署名は一般環境での信頼、SmartScreen、公開CA、失効確認、RFC3161の検証にはならない。

整理後のコードでもネットワーク無効のSandboxで3件の署名`Valid`・拇印一致、導入・削除・データ保持、証明書と秘密鍵の削除を確認済み。レポートを回収し、Sandboxの終了も確認した。通常CTestには署名引数、本番／テスト分離、公開拒否、WSB隔離、マニフェストと既存出力保護の検査を含める。実Sandboxの検証は明示実行とし、通常CTestでは証明書を作成しない。

## 検証

### 実インストール／アンインストール

`scripts/test-installer.ps1`は、実際のSetup.exeを変更せずにCLIで導入・起動・削除する。通常のCTestでは実行しない。既定ではビルド先の新しい作業ディレクトリを使い、既存ディレクトリの再利用・削除は拒否する。`-WhatIf`で副作用なしに計画を確認できる。

```powershell
.\scripts\test-installer.ps1 -Installer <Setup.exe> -WhatIf
.\scripts\test-installer.ps1 -Installer <Setup.exe> -WorkDirectory build-installer-roundtrip
```

検証用のProductUUID・製品名と、専用フォルダのショートカットを指定する。他のインストール登録や通常のスタートメニューを上書きしない。管理者昇格は要求せず、検証後はそのMaintenance Toolだけで削除する。失敗時にも生成済みの専用Maintenance Toolによる後始末を試みるが、無関係なフォルダ・レジストリの強制削除は行わない。

検査内容は、日本語・空白を含むパス、同梱ファイル、専用アンインストール登録・ショートカット、開発用PATHなしの`--smoke-test`、削除後の登録解除、未知のユーザー所有ファイル・外部プロジェクト／設定の検証用データの保持。`-AppDataMode Retain|Delete`は使い捨てWindows Sandboxの`WDAGUtilityAccount`だけで許可し、実際のゲストAppDataに新しい検証データを作り、既定保持／選択削除とCLIデータ領域・親の他ファイル保持を確認する。ホストの本物のユーザーデータを読み書きせず、実CLIのモデル取得・推論も行わない。

`result.json`には対象Setup.exeのSHA-256、成功／失敗、検査項目、インストーラ・アプリ・生成後Maintenance ToolのAuthenticode状態を記録する。`-RequireSigned`で3つすべての有効な署名を要求し、`-ExpectedSignerThumbprint`を指定した場合も有効性と発行元一致を必須にする。署名エラーを未署名扱いに読み替えない。

### クリーンなWindows Sandbox用の準備

```powershell
.\scripts\prepare-installer-sandbox.ps1 -Installer <Setup.exe> -OutputDirectory build-installer-sandbox
```

生成した`validate-installer.wsb`を、Windows Sandboxが利用可能なPCで手動で開く。先に開いたSandboxがある場合は終了してから起動する（多重起動エラーの場合、ゲストの検証は開始されない）。スクリプトはWindowsの機能を有効化せず、VMも自動起動しない。ホストへ実行ポリシーの変更を行わず、ゲストのテストプロセスだけ`RemoteSigned`で起動する。

共有する入力はSetup.exeと検証スクリプトだけで読み取り専用、出力は新しいログ専用フォルダだけを書き込み可能にする。ネットワーク・クリップボード・vGPUは無効。リポジトリ全体、ホームディレクトリ、認証情報を共有しない。ゲストランナーは入力のコピーとSHA-256一致を確認し、ゲストのローカルAppData内で保持／削除の2回の導入・削除を行う。共有出力へインストールせず、レポートとログだけを出力へコピーする。結果はホスト側の`output/run/Retain/result.json`と`output/run/Delete/result.json`、進行状況は`output/run/guest-status.json`に出る。生成時の`input-manifest.json`と結果に記録したインストーラのSHA-256も照合する。再実行には新しい出力ディレクトリを使う。

署名済みパッケージの検査では生成時に`-RequireSigned -ExpectedSignerThumbprint <拇印>`を追加できる。ゲストが必要な証明書チェーン・失効情報を持たず署名を検証できない場合は失敗として扱い、ネットワークや証明書検査を勝手に緩和しない。必要なオンライン確認は所有者が別途管理する検証環境で実施する。

IFW 4.7.0の自己削除はVBScriptに依存するため、エンジンがないWindowsでも削除できるよう、成功した削除後だけ固定名のMaintenanceTool.exeを遅延削除する。GUIでは完了画面のFinishまたは閉じる操作まで待つ。ユーザーパスは作業ディレクトリとして渡し、コマンド文字列へ埋め込まず、ディレクトリやワイルドカードを削除しない。

修正版は通常CTest全19件とホスト上の導入・起動・削除・登録解除・検証用データ保持を確認済み。同じ未署名インストーラについて、ネットワーク無効のWindows Sandboxでも日本語・空白を含むゲストローカルパスへの導入、開発用PATHなしのスモーク起動、MaintenanceTool.exeを含む削除、登録・ショートカット解除、ユーザー所有の検証ファイル4件の保持が成功した。入力マニフェスト・ホスト結果・ゲスト結果のインストーラSHA-256一致を確認した。実GUIでの手動操作や公開用署名の検証まで完了とは扱わない。公開・ローカルともQt 6.12.0へ統一する。公開対象の版・署名が変われば、その最終バイナリで再検証する。

`packaging_layout`テストは新しい一時ステージへ対象コンポーネントだけをinstallし、本体・Qt・MSVCランタイム・サンプル・ライセンスの存在と、開発物／ユーザーデータの非混入を確認する。Windowsでは開発用PATHとQtプラグイン環境変数に依存しないステージ済みexeの`--smoke-test`も実行する。検証ステージは調査用にビルド先へ残す。

生成物のメタデータとスクリプトの読み込みは、インストールせずに確認できる。キャッシュの保存先を作業ディレクトリ内へ明示する。

```powershell
& '<生成したSetup.exe>' --no-proxy --cache-path ./build/ifw-cache search
```

WindowsでIFW 4.7.0を使い、Releaseビルド・CTest・CPackによる未署名exe生成・上記のコンポーネント検索を確認済み。ソース取得は固定した10個の原アーカイブのSHA-256検証と資料収集を確認した。開発ホストで日本語・空白を含むパスへの実インストール、開発用PATHなしの起動、実アンインストール、登録・ショートカット解除、ユーザー所有の検証ファイル保持を確認した。リリース公開のテストは偽のAPI応答を使用し、実際のGitHub公開や署名は行っていない。Sandboxの実証範囲と未完了項目は上記のとおり。

公開前には開発環境のないVM等でも以下を確認する。

1. 日本語・空白を含むパスへの非管理者インストール。
2. スタートメニューからの起動と、CLI未導入時の動作。
3. 既存設定がない状態からの保存、終了、再起動。
4. 常駐中・フロー実行中の削除操作で、予期せぬデータ損失がないこと。
5. Maintenance Tool／Windows設定からの削除、ショートカット解除。
6. 設定・履歴・外部プロジェクトと、インストール先の未知ファイルが保持されること。

## GitHub Actionsでの自動リリース

### 初回の公開準備（所有者の作業）

既存のローカル検証用パッケージを、資料未整備のまま公開しないよう必須チェックを設ける。上記の`collect`による資料、固定した`sources.json`、実際のバイナリ／ソースの対応、パッチ・ビルド情報、MSVC再配布条件をレビューしてから、Settings → Secrets and variables → Actions → Variablesで`RELEASE_LEGAL_APPROVED`を文字列`true`に設定する。これは所有者の公開承認であり、準備スクリプトは自動で設定しない。未承認時、版不一致、ソース取得・検証失敗時は停止し、タグ・公開Releaseは発行しない。ファイルの存在・非空チェックは法的適合を保証しない。

| ファイル | 準備する内容 |
|---|---|
| `APPLICATION-LICENSE.txt` | ルートの`LICENSE`から生成するMIT本文 |
| `QT-NOTICES.txt` | Qt 6.12.0の原アーカイブから収集する本文・第三者表示 |
| `IFW-NOTICES.txt` | IFW 4.7.0・Qt 6.6.0・圧縮ライブラリの原アーカイブから収集する本文・第三者表示 |
| `SOURCE-OFFER.txt` | 実際に生成するソースZIP名・同じReleaseでの入手先・提供範囲・注意事項 |

これらは`build-release/distribution/`へ生成し、インストーラ内の`licenses/distribution/`とライセンス画面へ取り込む。静的なダミーのソース提供先をコミットしてチェックを通さない。公開前レビューにはクリーンVMでの確認も含める。現在のCIで公開するインストーラは未署名で、SmartScreen等が警告する可能性がある。

### 自動処理と採番

`master`へのマージに伴うpushで通常ビルドが成功した後、専用ジョブがQt 6.12.0・IFW 4.7.0（`qt.tools.ifw.47`）をaqtinstallの固定上流コミット`076e1659807d0b362a3ed684d54c2e9c775eb9c7`で取得する（3.3.0は新しいQtリポジトリ構成に未対応）。`scripts/release.mjs prepare`が版を計算し、`distribution.py prepare`がソースZIP・資料を準備する。`scripts/package.ps1 -Version <版>`で全通常テストと`packaging_layout`検査、CPack生成を行う。成功後にのみ`publish`がタグ・下書きReleaseを作り、ソースZIP、配布記録、インストーラ、`SHA256SUMS.txt`をアップロードする。各応答の名前・サイズ・GitHub報告SHA-256を検証してから正式Releaseへ切り替える。当面は未署名で配布し、署名状態の要約は残すが、署名に関する追加の注意書きは付けない。署名サービスの採用は未定とし、任意署名・署名検証・自己署名公開拒否のコードは維持する。CLI別途導入・料金・スクリプト実行権限の注意は本文へ明記する。`develop`ではタグ・Releaseを作らない。

`publish`自体も`LEGAL_APPROVED=true`を要求する。ソースZIPの版・コミット・リポジトリ・SHA-256と、`LEGAL_DIRECTORY`で渡した資料のファイル集合・内容が準備時の記録に一致しなければ、Release作成前に停止する。準備後に資料を編集した場合は、ソースZIPも含めて新しいディレクトリで作り直す。

CIはパッケージ生成後に実インストール／削除を行い、`installer-validation.json`を作る。`publish`は成功結果と対象Setup.exeのSHA-256一致を必須とし、レポート欠落・失敗・インストーラの変更・署名検証エラーがあればRelease作成前に停止する。結果レポートもチェックサム付きのReleaseアセットに含める。`REQUIRE_SIGNED=true`または`EXPECTED_SIGNER_THUMBPRINT`で署名を必須化できる。現在のCIは署名を設定していないため、3ファイルが未署名であることを記録する。CIランナーには開発ツールがあるため、PATHを除去したテストだけでクリーンVM検証まで完了とは扱わない。

- 版は安定版のSemVer `MAJOR.MINOR.PATCH`、タグは`vMAJOR.MINOR.PATCH`。初回はCMake既定`0.1.0`のパッチを上げた`v0.1.1`となる。
- 全安定版タグ・下書きを含むReleaseとCMakeの基準版を数値比較し、最大の版のパッチを1つ上げる。プレリリース、ビルドメタデータ、不正形式のタグは採番対象外。
- メジャー／マイナーは自動推測しない。変更する場合はCMakeの基準版をPRで上げる（例: `0.2.0`へ変更すると次回は`v0.2.1`）。破壊的変更かどうかのSemVer判断は所有者が行う。
- CMakeソースをリリースごとに自動コミットしない。タグのコミットのソースと、Releaseに記録した版を使って`-Version`で再現する。
- 同じコミットの再実行ではRelease本文のコミット識別子を検証し、未完成の下書きは同じ版で再開、公開済みなら採番・再公開をスキップする。公開済みのアセット・タグは上書きしない。
- 公開途中で失敗した場合、下書き・タグが残ることがある。自動削除せず、同じ実行を再実行する。タグの付け替え・二重のコミット識別子・並行した手動リリース等で版が衝突した場合は失敗させる。別の発行者による操作と自動公開を同時に行わない。

トークンは組み込みの`GITHUB_TOKEN`を使い、リリースジョブだけ`contents: write`を持つ。PAT・署名鍵・追加Secretsは不要。同一ブランチの直列実行と待機キューの上限については[開発ガイド](development.md#github-actionswindows)を参照。`master`への直接push禁止は引き続きブランチ保護で管理する。GitHub Releaseでの配布は、Maintenance Toolの自動更新リポジトリの整備とは別である。

## 将来の更新配信

更新URLと運用方針が決まった後、OS・CPUごとのHTTPSリポジトリ、バージョン管理、Maintenance Tool自身の更新、署名・配信元保護を追加する。IFW 4.11等のオフライン／hybrid動作とCPackの対応を採用版で確認する。リポジトリを追加するだけで、現在のオフライン専用インストールに自動更新が有効になると仮定しない。

アプリ内の更新通知と、安全な保存・終了・再起動も別実装。ユーザーデータの形式移行はインストーラのファイル更新とは別に管理する。
