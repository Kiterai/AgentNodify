# Windows／macOSの統合リリース

## 処理の順序

`master`へのPRマージに伴うpushで、`.github/workflows/windows-build.yml`（表示名`Build and release`）が起動する。既存のWindows採番規則・同一コミットの再実行判定を維持し、次の順で処理する。

1. Windows通常ReleaseビルドとCTest。
2. `release`ジョブでバージョンを一度だけ決定。Windows用の配布資料・対応ソースを準備し、同じ版のIFWインストーラを生成、実導入・起動・削除を検証する。まだReleaseやタグは作らない。
3. `.github/workflows/macos-release.yml`を呼び出し、同じ版・同じコミットでmacOS arm64／x86_64をそれぞれネイティブビルドする。通常CTest、PKG生成、実導入、Cocoaスモーク、署名再検証、削除を行う。
4. 最終ジョブは同じworkflow runの3系統の成果物を明示名で取得する。別runの成果物や単体CIの固定版PKGをリリースへ流用しない。
5. 全インストーラ、検証JSON、対応ソースZIP、配布資料の版・コミット・CPU・SHA-256を確認する。1系統でも欠落・失敗・変更があれば公開しない。
6. 1つの下書きReleaseへ全アセットをアップロードし、GitHubが報告する名前・サイズ・SHA-256の一致を確認してから公開する。公開済みReleaseは変更せず、途中失敗は同じコミット・版の下書きで再開する。

統合ワークフロー内では`develop`は従来のWindows通常ビルドのみ。`ci/multiplatform-release`は全パッケージと最終集約まで検証するが、`release.mjs verify`で終了し、GitHubへの変更・公開は行わない。単体macOSワークフローは引き続き`develop`／PR等の早期検証に使い、`master`の重複ビルドは行わない。

## 公開承認

公開ジョブには以下のリポジトリActions Variablesが両方必要。

| 変数 | 必要な値・範囲 |
|---|---|
| `RELEASE_LEGAL_APPROVED` | `true`。従来のWindows／IFW配布資料・対応ソース・ランタイム等のレビュー |
| `MACOS_RELEASE_LEGAL_APPROVED` | `true`。公式Qt macOSキットとmacOS用対応ソース・配布資料の独立したレビュー |

設定先はGitHubのSettings → Secrets and variables → Actions → Variables。準備スクリプト・エージェント・ワークフローは承認変数を自動設定しない。Windowsの既存承認をmacOSへ流用しない。テストブランチの`verify`はmacOS承認がなくても入力の整合性を確認できるが、承認にはならず、`publish`の承認チェックは省略しない。

## macOSの対応ソース

`packaging/macos/sources.json`は公式Qt 6.12.0のqtbase／qtsvg原アーカイブとSHA-256を固定する。`scripts/distribution-macos.py`は実qmakeのQt版とアプリのgit archiveのコミット・MIT本文を確認し、原文のライセンス／第三者表示・ソース・ビルド情報をCPU別にまとめる。IFWはmacOS PKGに含めない。

- `AgentNodify-<版>-macOS-<CPU>-Sources.zip`
- `macos-<CPU>-distribution-release.json`
- `macos-<CPU>-installer-validation.json`
- `macos-<CPU>-package-validation.json`
- `AgentNodify-<版>-macOS-<CPU>-Setup.pkg`

Windowsの既存4アセットにmacOS各CPUの5アセットを加え、全14ファイルの`SHA256SUMS.txt`とともに同じReleaseで提供する。macOS用の`SOURCE-OFFER.txt`はその実際のソースZIP名・同一ReleaseのURLを使い、PKG内のライセンス資料とアプリのライセンス画面にも取り込む。アセット数はチェックサムを含め15件。

資料の生成成功やハッシュ一致は法的承認ではない。公式バイナリとソースの対応、必要なベンダーパッチ・設定・ビルド情報、利用条件の適用を所有者がレビューする。Homebrew Qtのローカル検証結果は公式キットの公開承認に使わない。

## 署名・公証と検証範囲

現在のmacOSアプリはad-hoc署名、PKGはDeveloper ID Installer署名なし、公証なし。この状態をRelease本文に明示する。Gatekeeperの無効化・隔離属性削除は自動実行しない。Developer ID署名・公証の完了を偽らない。Windowsの署名必須ポリシー・自己署名公開拒否は維持する。

通常CTestは偽のAPI・アーカイブを使い、外部AIサービスへ接続せず、GitHubへ公開しない。統合ワークフローのインストール検証は使い捨てランナーのみ。公開前にはクリーン環境と実配布経路での確認も別途行う。

## 統合検証記録（2026-11-01 JST）

- [実行41217199738](https://github.com/Kiterai/AgentNodify/actions/runs/41217199738)で`451eee3cc3ed7502efdcb81ac064a4b08b5a4381`を検証。候補版は`0.1.3`。
- Windows通常ビルド、版付きWindows IFWインストーラ、macOS arm64／x86_64の資料付きPKG、3系統の実導入・起動・削除、対応ソース・検証記録の最終集約が成功した。初回のWindowsソース取得は接続タイムアウトで失敗し、ハッシュ検証を維持した同一コミットの失敗ジョブ再実行で成功した。
- 最終`verify`は全入力の整合性確認に成功したが、`MACOS_RELEASE_LEGAL_APPROVED`は未設定（false）だった。承認を自動設定せず、[PR #7](https://github.com/Kiterai/AgentNodify/pull/7)のマージ・実公開は所有者のレビュー・承認待ち。既存の公開済み`v0.1.2`は変更していない。
- ローカルでは通常CTest全18件、公開プロトコルのNodeテスト24件、配布資料のPythonテスト11件が成功。追補の回帰テスト・資料・表示文言はCI再実行を省略したコミットへまとめているため、マージ後の本番ワークフローでも最終コミットを再ビルド・再検証する。
