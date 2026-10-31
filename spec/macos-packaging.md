# macOSのインストーラとGitHub Actions

`master`からの自動公開・採番・対応ソース・公開承認は[統合リリース](multiplatform-release.md)を参照してください。以下の単体ビルド／検証CIとは分離しています。

## 独立した配布工程

Windowsは従来のCPack／IFW、macOSはApple標準の`pkgbuild`を使う。macOS用ツール依存は`scripts/package-macos.py`と`scripts/test-macos-installer.py`へ分離し、通常ビルドやWindows用スクリプトには追加しない。

生成する`.pkg`は `/Applications/AgentNodify.app` だけを導入する。Qt・ライセンス・サンプルはバンドル内の`Contents/Resources`等にまとめ、別の場所にユーザーデータやCLI認証情報を配置しない。偽CLI・テスト実行ファイル・履歴・ワークスペースは含めない。インストール先の探索による再配置は無効。導入・更新前にはアプリを明示的に終了する。

削除はアプリを終了してFinderでアプリをゴミ箱へ移動する。設定・内部フロー・履歴・外部プロジェクト・CLI認証情報は削除しない。IFWのMaintenance ToolやWindows専用AppData削除機能はmacOSのPKGには含めない。

## ローカル生成

macOS、Xcode Command Line Tools、CMake 3.24以上、Python 3.8以上、Qt 6.5以上のmacOSキット（`macdeployqt`含む）が必要。Python／JavaScriptノードを両方検証するにはNode.jsもPATHへ追加する。

```sh
Qt6_ROOT="/path/to/Qt/kit" python3 scripts/package-macos.py \
  --build-directory build-macos-package --version 0.1.0
```

Releaseビルド・通常CTest・新しいステージへのコピー・Qt依存収集・依存パス検査・ad-hoc署名検証・PKG生成を順に行う。既存インストーラの上書きは拒否する。`--reuse-build`では同じ版の既存Releaseビルドを要求し、CTestは省略しない。出力は次のとおり。

- `build-macos-package/installer/AgentNodify-<version>-macOS-<arm64|x86_64>-Setup.pkg`
- 同名の`.sha256`
- `package-validation.json`（版、CPU、SHA-256、署名・公証・公開承認の状態）

ステージ済みアプリも調査用に残す。Qtキット／Homebrew側のライブラリは変更しない。macdeployqtが終了コード0でもエラーを出す場合を検査し、同梱済みライブラリへの解決可能なrpath参照だけを修復する。未解決依存、バンドル外への参照・シンボリックリンクは拒否する。スモークテストは署名済みアプリの中へロックを書かず、一時領域を使う。

サンドボックスがAppleの`pkgbuild`を拒否する場合は、その生成コマンドの実行許可が必要。失敗を自動でサンドボックス外実行へ切り替えない。通常CTestは外部AIサービスに接続しない。

## 内容検査と実インストール

ローカルではホストに導入せず、PKGを一時ディレクトリへ展開して内容と署名を検査できる。

```sh
python3 scripts/test-macos-installer.py \
  --installer build-macos-package/installer/AgentNodify-0.1.0-macOS-arm64-Setup.pkg \
  --report build-macos-package/installer/installer-validation.json
```

`--install`は使い捨てGitHub Actions macOSランナー専用。既存のアプリや同一IDのインストール記録があれば拒否する。`sudo -n installer`で導入、記録の版確認、開発用Qt／PATHに依存しないCocoaスモーク起動、署名再検証、今回導入したアプリと記録だけの削除を行う。実ユーザーデータは読み書きしない。通常CTestから実インストールは呼び出さず、`macos_packaging_guards`では偽のツール応答による依存検査・境界逸脱・ホストへの導入拒否を確認する。

## GitHub Actions

`.github/workflows/macos-build.yml`は`develop`へのpush、`master`／`develop`向けPR、手動実行で動く。導入検証用の`ci/macos-installer`へのpushでも動く。単体CIはRelease・タグ・採番・公開承認変数を操作しない。`master`へのpushは重複ビルドせず、統合リリースの版付きmacOSジョブに任せる。

- `macos-15`でarm64、`macos-15-intel`でx86_64をネイティブビルドする。実CPUも検査する。
- Qt 6.12.0の公式macOSキットを、Windows CIと同じ固定コミットのaqtinstallで取得する。ダウンロードのハッシュ検証を無効化しない。
- Python 3.13を用意し、Node.jsはランナー付属のものを使う。actionsは固定コミットで参照する。
- ビルド・通常CTest・PKG生成・実導入／Cocoa起動／削除を実行する。権限は`contents: read`のみ。
- 成功時は`macos-<CPU>-installer-validation`へPKG・SHA-256・検証JSON・該当コミットのアプリソース・注意事項を保存する。失敗時も`macos-<CPU>-diagnostics`へログ・GUI確認画像を保存する。保持は7日間。
- 同一refの実行は直列化し、実行中の検証をキャンセルしない。Secrets、Developer ID、認証・推論用CLIは不要。

## 公開前に必要なこと

現在の成果物は**検証用**。アプリの署名はad-hoc、PKGはDeveloper ID Installer署名なし、公証なし。Gatekeeper回避や隔離属性の自動解除は行わない。公開リリースへのアップロードも行わない。

実際に使うmacOS Qtとその第三者依存のライセンス表示、対応ソース・パッチ・ビルド情報、Developer ID Application／Installer署名と公証、クリーン環境と更新時の確認を別途完了させる。Windows用`packaging/sources.json`やWindows公開承認を流用してmacOS配布の準備完了とは扱わない。Homebrew版と公式Qt版の依存集合も混同しない。

## 検証記録（2026-11-01 JST）

- ローカルmacOS 15.5／arm64／Homebrew Qt 6.12.0で通常CTest全18件成功、PKG生成・展開後の内容／署名検査が成功。ホストへの実インストールは行っていない。
- GitHub Actions [実行41214665093](https://github.com/Kiterai/AgentNodify/actions/runs/41214665093)で、`bf371db512c5e49343581b9559d54e98651b3b45`のコードを検証。以後の検証記録・資料のみのコミットではCIを再実行していない。
- `macos-15` arm64と`macos-15-intel` x86_64の両方で、公式Qt 6.12.0によるReleaseビルド、通常CTest全18件、PKG生成、実導入、Cocoaスモーク起動、署名再検証、アプリ／receiptの削除に成功。両CPUのインストーラと診断資料のアップロードも確認。
- 変更は`ci/macos-installer`ブランチで検証。master／developへのマージ、タグ・公開Releaseの作成、公開承認設定の変更は行っていない。Windowsでの今回の再実行と、公開用署名・公証・Gatekeeperの配布経路検証は対象外。
