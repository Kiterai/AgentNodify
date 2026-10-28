#pragma once
#include "colors.h"
#include <array>
#include <cstddef>
namespace an::strings {
inline constexpr char ApplicationIconResource[] = ":/icons/agentnodify.ico";
inline constexpr char16_t EllipsisCharacter = u'…';
inline constexpr char16_t LineSeparator = u'\u2028', ParagraphSeparator = u'\u2029';
namespace detail {
template <std::size_t... N> constexpr auto join(const char (&...pieces)[N]) {
    std::array<char, (N + ...) - sizeof...(N) + 1> result{};
    std::size_t offset = 0;
    auto append = [&](const auto &piece) {
        for (std::size_t i = 0; i + 1 < sizeof(piece); ++i)
            result[offset++] = piece[i];
    };
    (append(pieces), ...);
    return result;
}
} // namespace detail
// UI text, diagnostics, serialization keys and protocol literals. Equal values
// intentionally share a constant; keep portable JSON/CLI values unchanged.
inline constexpr char UserEnvironmentRegistry[] = "HKEY_CURRENT_USER\\Environment";
inline constexpr char MachineEnvironmentRegistry[] = "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment";
inline constexpr char Path[] = "Path";
inline constexpr char EnvironmentVariablePattern[] = "%([^%]+)%";
inline constexpr char TargetCancelled[] = "target_cancelled";
inline constexpr char ExecutionCancelled[] = "実行を中止しました";
inline constexpr char TargetFailed[] = "target_failed";
inline constexpr char TerminalNotReached[] = "terminal_not_reached";
inline constexpr char FlowNotFound[] = "flow_not_found";
inline constexpr char InvalidPath[] = "invalid_path";
inline constexpr char VariablePathPattern[] = R"((?:^|\.)([A-Za-z_\p{L}][\w\p{L}-]*)|\[(\d+)\])";
inline constexpr char InvalidVariablePath[] = "不正な変数パス: ";
inline constexpr char And[] = "and";
inline constexpr char All[] = "all";
inline constexpr char UnknownNodeType[] = "不明なノード種別: ";
inline constexpr char Variable[] = "variable";
inline constexpr char Value[] = "value";
inline constexpr char Equals[] = "equals";
inline constexpr char Id[] = "id";
inline constexpr char Kind[] = "kind";
inline constexpr char Title[] = "title";
inline constexpr char X[] = "x";
inline constexpr char Y[] = "y";
inline constexpr char Agent[] = "agent";
inline constexpr char Model[] = "model";
inline constexpr char Effort[] = "effort";
inline constexpr char Prompt[] = "prompt";
inline constexpr char Schema[] = "schema";
inline constexpr char RetainContext[] = "retainContext";
inline constexpr char Inputs[] = "inputs";
inline constexpr char Rules[] = "rules";
inline constexpr char ScriptLanguage[] = "scriptLanguage";
inline constexpr char ScriptPath[] = "scriptPath";
inline constexpr char ScriptDirectory[] = "scriptDirectory";
inline constexpr char ShellCommand[] = "shellCommand";
inline constexpr char ShellDirectory[] = "shellDirectory";
inline constexpr char ShellOutput[] = "shellOutput";
inline constexpr char TargetFlow[] = "targetFlow";
inline constexpr char ProjectPath[] = "projectPath";
inline constexpr char UserChoices[] = "userChoices";
inline constexpr char WaitDuration[] = "waitDuration";
inline constexpr char WaitUnit[] = "waitUnit";
inline constexpr char ScheduleMonth[] = "scheduleMonth";
inline constexpr char ScheduleDay[] = "scheduleDay";
inline constexpr char ScheduleHour[] = "scheduleHour";
inline constexpr char ScheduleMinute[] = "scheduleMinute";
inline constexpr char ScheduleWeekdays[] = "scheduleWeekdays";
inline constexpr char From[] = "from";
inline constexpr char To[] = "to";
inline constexpr char Output[] = "output";
inline constexpr char Input[] = "input";
inline constexpr char Version[] = "version";
inline constexpr char Name[] = "name";
inline constexpr char Nodes[] = "nodes";
inline constexpr char Edges[] = "edges";
inline constexpr char NextNodeId[] = "nextNodeId";
inline constexpr char UnsupportedFlowVersion[] = "未対応のフローファイル形式です";
inline constexpr char Codex[] = "Codex";
inline constexpr char Empty[] = "";
inline constexpr char PythonLanguage[] = "python";
inline constexpr char JsonFormat[] = "json";
inline constexpr char InvalidWaitDuration[] = "待機時間は0〜%1の整数を指定してください";
inline constexpr char Seconds[] = "seconds";
inline constexpr char Wildcard[] = "*";
inline constexpr char UserChoicesMustBeArray[] = "ユーザー選択肢は配列で指定してください";
inline constexpr char InputCountTooLarge[] = "入力数は%1以下にしてください";
inline constexpr char DuplicateNodeId[] = "ノードIDが重複しています";
inline constexpr char WholeInput[] = "#";
inline constexpr char MissingArrayElement[] = "配列要素が存在しません: ";
inline constexpr char MissingVariable[] = "変数が存在しません: ";
inline constexpr char InterpolationPattern[] = R"(\$\{([^}]+)\})";
inline constexpr char SchemaDialect[] = "$schema";
inline constexpr char Draft7Http[] = "http://json-schema.org/draft-07/schema#";
inline constexpr char Draft7Https[] = "https://json-schema.org/draft-07/schema#";
inline constexpr char HttpJsonSchemaOrgDraft07Schema[] = "http://json-schema.org/draft-07/schema";
inline constexpr char HttpsJsonSchemaOrgDraft07Schema[] = "https://json-schema.org/draft-07/schema";
inline constexpr char UnsupportedSchemaDialect[] = "対応するJSON SchemaはDraft 7です";
inline constexpr char AnUnknown[] = "_anUnknown";
inline constexpr char AnSources[] = "_anSources";
inline constexpr char SchemaReference[] = "$ref";
inline constexpr char UnresolvedSchemaReference[] = "解決できないスキーマ参照: ";
inline constexpr char Items[] = "items";
inline constexpr char MissingSchemaItems[] = "スキーマに配列要素がありません: ";
inline constexpr char SchemaIndexOutOfRange[] = "スキーマの配列範囲外: ";
inline constexpr char Properties[] = "properties";
inline constexpr char MissingSchemaVariable[] = "前段スキーマに変数がありません: ";
inline constexpr char MissingStart[] = "始端ノードは1つ必要です";
inline constexpr char TooManyEnds[] = "終端ノードは0個または1個にしてください";
inline constexpr char MissingConnectionNode[] = "接続先ノードが存在しません";
inline constexpr char InvalidConnectionPort[] = "接続ポートが範囲外です";
inline constexpr char DuplicateConnection[] = "同じ接続が重複しています";
inline constexpr char NodeErrorPrefix[] = "[%1] ";
inline constexpr char UnreachableFromStart[] = "始端から到達できません";
inline constexpr char UnreachableEnd[] = "終端に到達できません";
inline constexpr char UnconnectedInput[] = "入力%1が未接続です";
inline constexpr char UnconnectedBranchOutput[] = "分岐出力%1が未接続です";
inline constexpr char UnconnectedChoiceOutput[] = "選択肢出力%1が未接続です";
inline constexpr char Ok[] = "OK";
inline constexpr char NewFlowName[] = "新しいフロー";
inline constexpr char RunningFrameFirst[] = "◐";
inline constexpr char RunningFrameSecond[] = "◓";
inline constexpr char RunningFrameThird[] = "◑";
inline constexpr char RunningFrameFourth[] = "◒";
inline constexpr char SuccessSymbol[] = "✓";
inline constexpr char WarningSymbol[] = "!";
inline constexpr char IdleSymbol[] = "·";
inline constexpr char Space[] = " ";
inline constexpr char StateOpeningBracket[] = " [";
inline constexpr char ClosingBracket[] = "]";
inline constexpr char JSONText[] = "JSON / text";
inline constexpr char Payload[] = "payload";
inline constexpr char RunningValue[] = "running";
inline constexpr char SucceededValue[] = "succeeded";
inline constexpr char Failed[] = "failed";
inline constexpr char CancelledValue[] = "cancelled";
inline constexpr char UnexecutedNodeHistory[] = "実行されていないノードは履歴に保存できません";
inline constexpr char UnknownHistoryState[] = "不明な実行履歴の状態です";
inline constexpr char InvalidHistoryIdentity[] = "実行履歴のIDまたは日時が不正です";
inline constexpr char CreateHistoryDirectoryFailed[] = "実行履歴の保存先を作成できません";
inline constexpr char Node[] = "node";
inline constexpr char ExecutionNumber[] = "executionNumber";
inline constexpr char Directory[] = "directory";
inline constexpr char StartedAt[] = "startedAt";
inline constexpr char FinishedAt[] = "finishedAt";
inline constexpr char State[] = "state";
inline constexpr char Log[] = "log";
inline constexpr char Sequence[] = "sequence";
inline constexpr char StructuredOutput[] = "structuredOutput";
inline constexpr char ContextResumed[] = "contextResumed";
inline constexpr char JsonFileExtension[] = ".json";
inline constexpr char SaveHistoryFailed[] = "実行履歴を保存できません: ";
inline constexpr char JsonFilePattern[] = "*.json";
inline constexpr char UnsupportedHistoryVersion[] = "未対応の実行履歴の形式です";
inline constexpr char InvalidHistoryId[] = "実行履歴のIDが不正です";
inline constexpr char InvalidHistoryTime[] = "実行履歴の日時が不正です";
inline constexpr char InterruptedHistoryNotice[] = "\n[前回の実行は終了を確認できなかったため、中断として表示します]\n";
inline constexpr char ColonSpace[] = ": ";
inline constexpr char ApplicationName[] = "AgentNodify";
inline constexpr char SmokeArgument[] = "--smoke-test";
inline constexpr char SmokeInstanceDirectory[] = "/smoke-instance";
inline constexpr char CreateInstanceDirectoryFailed[] = "起動確認用のディレクトリを作成できません。アプリを終了します。";
inline constexpr char InstanceLock[] = "instance.lock";
inline constexpr char AlreadyRunning[] = "AgentNodifyはすでに起動しています。タスクトレイのアイコンからウィンドウを表示してください。";
inline constexpr char AcquireInstanceLockFailed[] = "起動確認用のロックを取得できません。アプリを終了します。";
inline constexpr char ClaudeCode[] = "Claude Code";
inline constexpr char OpenCode[] = "OpenCode";
inline constexpr char CatalogCliNotFound[] = "CLIが見つかりません。ツール → CLI設定を確認してください。";
inline constexpr char CmdExtension[] = ".cmd";
inline constexpr char BatExtension[] = ".bat";
inline constexpr char NativeCliRequired[] = "ネイティブのCLI実行ファイルを指定してください（.cmd/.batは非対応）。";
inline constexpr char AppServer[] = "app-server";
inline constexpr char PrintArgument[] = "--print";
inline constexpr char InputFormatArgument[] = "--input-format";
inline constexpr char StreamJson[] = "stream-json";
inline constexpr char OutputFormatArgument[] = "--output-format";
inline constexpr char VerboseArgument[] = "--verbose";
inline constexpr char NoSessionPersistenceArgument[] = "--no-session-persistence";
inline constexpr char SettingsArgument[] = "--settings";
inline constexpr char DisableHooksSettings[] = "{\"disableAllHooks\":true}";
inline constexpr char StrictMcpArgument[] = "--strict-mcp-config";
inline constexpr char McpConfigArgument[] = "--mcp-config";
inline constexpr char EmptyMcpSettings[] = "{\"mcpServers\":{}}";
inline constexpr char Models[] = "models";
inline constexpr char StandaloneArgument[] = "--standalone";
inline constexpr char Method[] = "method";
inline constexpr char Initialize[] = "initialize";
inline constexpr char Params[] = "params";
inline constexpr char ClientInfo[] = "clientInfo";
inline constexpr char ApplicationVersion[] = "0.1.0";
inline constexpr char Type[] = "type";
inline constexpr char ControlRequest[] = "control_request";
inline constexpr char RequestId[] = "request_id";
inline constexpr char CatalogInitialize[] = "catalog-initialize";
inline constexpr char Request[] = "request";
inline constexpr char Subtype[] = "subtype";
inline constexpr char CatalogStartFailed[] = "CLIを起動できません: ";
inline constexpr char CatalogTimeout[] = "モデル一覧の取得がタイムアウトしました。";
inline constexpr char CatalogResponseTooLarge[] = "モデル一覧の応答が%1MiBを超えました。";
inline constexpr char InvalidCatalogResponse[] = "モデル一覧の応答形式が不正です。";
inline constexpr char InvalidCatalogModel[] = "モデル情報の応答形式が不正です。";
inline constexpr char Hidden[] = "hidden";
inline constexpr char InvalidModelId[] = "モデルIDが不正です。";
inline constexpr char DisplayName[] = "displayName";
inline constexpr char Description[] = "description";
inline constexpr char ResolvedModel[] = "resolvedModel";
inline constexpr char Newline[] = "\n";
inline constexpr char SupportedReasoningEfforts[] = "supportedReasoningEfforts";
inline constexpr char SupportedEffortLevels[] = "supportedEffortLevels";
inline constexpr char ReasoningEffort[] = "reasoningEffort";
inline constexpr char TooManyModels[] = "モデル一覧が%1件を超えました。";
inline constexpr char ErrorValue[] = "error";
inline constexpr char Code[] = "code";
inline constexpr char Message[] = "message";
inline constexpr char CatalogOnlyClient[] = "Catalog-only client";
inline constexpr char CodexCatalogRejected[] = "Codexがモデル一覧要求を拒否しました。";
inline constexpr char Result[] = "result";
inline constexpr char Initialized[] = "initialized";
inline constexpr char ModelList[] = "model/list";
inline constexpr char Limit[] = "limit";
inline constexpr char Data[] = "data";
inline constexpr char NextCursor[] = "nextCursor";
inline constexpr char CatalogPaginationLimit[] = "モデル一覧のページ送りが終了しません。";
inline constexpr char Cursor[] = "cursor";
inline constexpr char ControlResponse[] = "control_response";
inline constexpr char Response[] = "response";
inline constexpr char Success[] = "success";
inline constexpr char ClaudeInitializationFailed[] = "Claude Codeの初期化に失敗しました。";
inline constexpr char InvalidCatalogJson[] = "CLIのモデル一覧応答が正しいJSONではありません。";
inline constexpr char CatalogAbnormalExit[] = "CLIが異常終了しました（exit %1）。認証とCLI設定を確認してください。";
inline constexpr char CatalogEndedEarly[] = "モデル一覧を受信する前にCLIが終了しました。";
inline constexpr char AnsiEscapePattern[] = "\x1b\\[[0-9;]*[A-Za-z]";
inline constexpr char OpenCodeModelPattern[] = "^[A-Za-z0-9_.:-]+/[A-Za-z0-9_./:+@-]+$";
inline constexpr char InvalidOpenCodeCatalog[] = "OpenCodeのモデル一覧の応答形式が不正です。GUI版ではなくCLI版を指定してください。";
inline constexpr char CodeFont[] = "Consolas";
inline constexpr char TextPreviewTooltip[] = "クリック、またはEnterで右ペインの編集UIを開きます。";
inline constexpr char NodeForm[] = "nodeForm";
inline constexpr auto NodeFormStyleStorage = detail::join(
    "\n        QWidget#nodeForm { background: ", colors::TransparentCss, "; color:", colors::InputForeground,
    "; }\n        QWidget#nodeForm QWidget { background: ", colors::TransparentCss,
    "; }\n        QWidget#nodeForm QLabel { background: ", colors::TransparentCss, "; color:", colors::FormLabel,
    "; }\n        QWidget#nodeForm QCheckBox, QWidget#nodeForm QCheckBox:disabled { color:", colors::InputForeground,
    "; spacing:6px; }\n        QWidget#nodeForm QCheckBox::indicator { width:16px; height:16px; }\n        QWidget#nodeForm QLineEdit, "
    "QWidget#nodeForm QComboBox, QWidget#nodeForm QSpinBox,\n        QWidget#nodeForm QPlainTextEdit { background:",
    colors::InputBackground, "; color:", colors::InputForeground, "; border:1px solid ", colors::InputBorder,
    "; border-radius:4px; padding:5px; selection-background-color:", colors::SelectionBackground,
    "; }\n        QWidget#nodeForm QPushButton { background:", colors::FormButtonBackground, "; color:", colors::Text,
    "; border:1px solid ", colors::FormButtonBorder,
    "; border-radius:4px; padding:4px 8px; }\n        QWidget#nodeForm QPushButton:hover { background:", colors::FormButtonHover,
    "; }\n    ");
inline constexpr const char *NodeFormStyle = NodeFormStyleStorage.data();
inline constexpr char NodeFormForegroundStyle[] = "QWidget#nodeForm, QWidget#nodeForm QLabel { color:%1; }";
inline constexpr char NodeTitle[] = "nodeTitle";
inline constexpr char NameLabel[] = "名前";
inline constexpr char RemoveChoice[] = "removeChoice";
inline constexpr char UnregisteredSuffix[] = "（未登録）";
inline constexpr char UnsupportedSuffix[] = "（未対応）";
inline constexpr char UnknownNodeKind[] = "Unknown node kind";
inline constexpr char AgentName[] = "エージェント";
inline constexpr char AgentDescription[] =
    "選択したエージェントへ1タスクを渡します。入力全体は ${#}、個別の値は ${name}、${obj.key}、${items[0]} "
    "で参照できます。${#}はスキーマ未指定の文字列入力でも使用できます。入力を受けるたびに実行します。出力スキーマは省略可能です。";
inline constexpr char String[] = "string";
inline constexpr char EmptyPrompt[] = "プロンプトが空です";
inline constexpr char UnknownAgent[] = "不明なエージェントです";
inline constexpr char InvalidJsonSchema[] = "不正なJSON Schema: ";
inline constexpr char InputLabel[] = "入力";
inline constexpr char OutputLabel[] = "出力";
inline constexpr char Preview[] = "Preview";
inline constexpr char PromptPreviewPlaceholder[] = "クリックして入力プロンプトを編集";
inline constexpr char ShellPreviewPlaceholder[] = "クリックしてシェルコマンドを編集";
inline constexpr char SchemaPreviewPlaceholder[] = "任意 · クリックしてJSON Schemaを編集";
inline constexpr char NodeAgent[] = "nodeAgent";
inline constexpr char NodeModel[] = "nodeModel";
inline constexpr char SelectModel[] = "モデルを選択";
inline constexpr char RefreshSymbol[] = "↻";
inline constexpr char RefreshModels[] = "refreshModels";
inline constexpr char ModelCatalogStatus[] = "modelCatalogStatus";
inline constexpr char ModelLabel[] = "モデル";
inline constexpr char NodeEffort[] = "nodeEffort";
inline constexpr char DefaultEffortLabel[] = "既定（指定なし）";
inline constexpr char EffortTooltip[] =
    "モデルが対応する思考量を選んでください。未指定ならCLI・モデルの既定値を使います。OpenCodeではモデルの指定が必要です。";
inline constexpr char EffortLabel[] = "思考量";
inline constexpr char RetainContextLabel[] = "前回のコンテキストを引き継ぐ";
inline constexpr char RetainContextTooltip[] = "同じフロー実行内で、このノードの前回のCLIセッションを再利用します。別ノードや次回のフロー実"
                                               "行とは共有しません。実行履歴は毎回個別に記録します。";
inline constexpr char InputPromptLabel[] = "入力プロンプト";
inline constexpr char OutputSchemaLabel[] = "出力スキーマ（任意）";
inline constexpr char CurrentSettingSuffix[] = "（現在の設定）";
inline constexpr char CatalogNotLoaded[] = "未取得";
inline constexpr char SelectCatalogModelHint[] = "取得した候補からモデルを選択してください。";
inline constexpr char CatalogLoading[] = "取得中…";
inline constexpr char CatalogFailed[] = "取得失敗";
inline constexpr char CatalogPreviousModels[] = "取得失敗 · 前回の候補を表示";
inline constexpr char RefreshModelsTooltip[] = "モデル一覧を再取得\n";
inline constexpr char Low[] = "low";
inline constexpr char Medium[] = "medium";
inline constexpr char High[] = "high";
inline constexpr char Xhigh[] = "xhigh";
inline constexpr char Max[] = "max";
inline constexpr char CreateTemporaryDirectoryFailed[] = "一時ディレクトリを作成できません";
inline constexpr char ResponseTxt[] = "response.txt";
inline constexpr char Exec[] = "exec";
inline constexpr char JsonArgument[] = "--json";
inline constexpr char SkipGitCheckArgument[] = "--skip-git-repo-check";
inline constexpr char LastMessageArgument[] = "--output-last-message";
inline constexpr char WorkingDirectoryArgument[] = "--cd";
inline constexpr char SandboxArgument[] = "--sandbox";
inline constexpr char WorkspaceWrite[] = "workspace-write";
inline constexpr char SchemaJson[] = "schema.json";
inline constexpr char WriteSchemaFailed[] = "スキーマファイルを書き込めません";
inline constexpr char OutputSchemaArgument[] = "--output-schema";
inline constexpr char ModelArgument[] = "--model";
inline constexpr char ConfigArgument[] = "-c";
inline constexpr char ReasoningEffortAssignment[] = "model_reasoning_effort=";
inline constexpr char Resume[] = "resume";
inline constexpr char Dash[] = "-";
inline constexpr char PromptArgument[] = "-p";
inline constexpr char PartialMessagesArgument[] = "--include-partial-messages";
inline constexpr char PermissionModeArgument[] = "--permission-mode";
inline constexpr char AcceptEdits[] = "acceptEdits";
inline constexpr char EffortArgument[] = "--effort";
inline constexpr char JsonSchemaArgument[] = "--json-schema";
inline constexpr char ResumeArgument[] = "--resume";
inline constexpr char Run[] = "run";
inline constexpr char FormatArgument[] = "--format";
inline constexpr char OpenCodeEffortRequiresModel[] = "OpenCodeの思考量を指定する場合はモデルを選択してください";
inline constexpr char SessionArgument[] = "--session";
inline constexpr char JsonSchemaPromptSuffix[] = "\nReturn only JSON (no Markdown) matching this JSON Schema:\n";
inline constexpr char EndOfOptions[] = "--";
inline constexpr char AgentnodifyPrompt[] = "agentnodify.prompt";
inline constexpr char ReadFinalResponseFailed[] = "最終応答ファイルを開けません";
inline constexpr char MissingSessionId[] = "コンテキストを引き継ぐためのCLIセッションIDを取得できません";
inline constexpr char SessionMismatch[] = "CLIの再開先セッションIDが前回と一致しません";
inline constexpr char AllName[] = "All";
inline constexpr char AllDescription[] =
    "全入力の発火を待ち、入力ポート番号順の配列を出力します。発火後は待ち合わせ状態をリセットし、再び新しい全入力を待ちます。先着した入力は"
    "FIFOで保持します。入力数を減らすと、削除した入力端への接続だけを消します。";
inline constexpr char Array[] = "array";
inline constexpr char InvalidInputCount[] = "入力数は1〜%1にしてください";
inline constexpr char IndexedInputLabel[] = "入力 %1";
inline constexpr char Any[] = "any";
inline constexpr char AnyName[] = "Any";
inline constexpr char AnyDescription[] =
    "いずれかの入力にイベントが届くたびに発火します。出力はevent（入力の0始まりインデックス）とinput（その入力データ）です。複数イベントは"
    "到着順に1件ずつ処理します。入力数を減らすと、削除した入力端への接続だけを消します。";
inline constexpr char Object[] = "object";
inline constexpr char Event[] = "event";
inline constexpr char Integer[] = "integer";
inline constexpr char Minimum[] = "minimum";
inline constexpr char Maximum[] = "maximum";
inline constexpr char Required[] = "required";
inline constexpr char AdditionalProperties[] = "additionalProperties";
inline constexpr char Branch[] = "branch";
inline constexpr char BranchName[] = "分岐";
inline constexpr char BranchDescription[] = "上から順に条件を評価し、最初に一致した出力へ伝播します。一致しない場合はelseへ進みます。比較は"
                                            "文字列で行います。条件の追加・削除でも残った条件と「それ以外」への接続を維持します。";
inline constexpr char OkValue[] = "ok";
inline constexpr char ElseLabel[] = "それ以外";
inline constexpr char IndexedRuleLabel[] = "条件%1";
inline constexpr char NodeRules[] = "nodeRules";
inline constexpr char AddRule[] = "＋ 条件";
inline constexpr char AddRuleValue[] = "addRule";
inline constexpr char RemoveSymbol[] = "×";
inline constexpr char RemoveRule[] = "removeRule";
inline constexpr char RemoveRuleTooltip[] = "この条件を削除";
inline constexpr char RuleVariable[] = "ruleVariable";
inline constexpr char RuleVariablePlaceholder[] = "前段の変数名";
inline constexpr char IfLabel[] = "もしも";
inline constexpr char SubjectParticle[] = "が";
inline constexpr char RuleOperator[] = "ruleOperator";
inline constexpr char EqualsLabel[] = "等しい";
inline constexpr char NotEqualsLabel[] = "異なる";
inline constexpr char RuleValue[] = "ruleValue";
inline constexpr char RuleValuePlaceholder[] = "比較する値";
inline constexpr char ComparisonParticle[] = "と";
inline constexpr char ThenLabel[] = "ならば";
inline constexpr char NodeInputs[] = "nodeInputs";
inline constexpr char InputCountTooltip[] = "残った入力端と出力端への接続は維持し、削除した入力端への接続だけを消します。";
inline constexpr char InputCountLabel[] = "入力ポート数";
inline constexpr char SelectFlow[] = "フローを選択";
inline constexpr char ProjectPathPlaceholder[] = "例: ../projects/${name}";
inline constexpr char ProjectPathTooltip[] = "絶対パス、または実行元プロジェクトからの相対パス。${name}等で前段の値を参照できます。";
inline constexpr char FlowLabel[] = "フロー";
inline constexpr char ProjectPathLabel[] = "プロジェクトパス";
inline constexpr char HostUnavailable[] = "host_unavailable";
inline constexpr char Delay[] = "delay";
inline constexpr char Waiting[] = "待機";
inline constexpr char DelayDescription[] =
    "指定した日・時間・分・秒だけ非同期で待機し、受け取ったデータをそのまま出力します。待機中も他ノードと画面操作は継続します。";
inline constexpr char DayLabel[] = "日";
inline constexpr char Days[] = "days";
inline constexpr char HoursLabel[] = "時間";
inline constexpr char Hours[] = "hours";
inline constexpr char MinuteLabel[] = "分";
inline constexpr char Minutes[] = "minutes";
inline constexpr char SecondLabel[] = "秒";
inline constexpr char WaitDurationLabel[] = "待機時間";
inline constexpr char UnitLabel[] = "単位";
inline constexpr char DelayLogMessage[] = "待機: %1 %2\n";
inline constexpr char End[] = "end";
inline constexpr char EndName[] = "終端";
inline constexpr char EndDescription[] = "フローの処理の終わりを表します。前段から受け取ったデータを最終出力にします。";
inline constexpr char FlowRun[] = "flow_run";
inline constexpr char FlowRunName[] = "AgentNodify フロー実行";
inline constexpr char FlowRunDescription[] = "指定フロー・プロジェクトを非同期に開始します。出力はresult: started / already / "
                                             "failedです。相対パスは実行元プロジェクト基準で、${name}等を使用できます。";
inline constexpr char MissingTargetFlow[] = "フローを選択してください";
inline constexpr char EmptyProjectPath[] = "プロジェクトパスが空です";
inline constexpr char ProjectNotFound[] = "project_not_found";
inline constexpr char Already[] = "already";
inline constexpr char Started[] = "started";
inline constexpr char FlowWait[] = "flow_wait";
inline constexpr char FlowWaitName[] = "AgentNodify フロー終了";
inline constexpr char FlowWaitDescription[] =
    "指定フロー・プロジェクトの実行終了を待ち、終端の入力を出力します。完了済みなら直ちに返します。未開始・失敗・中止・循環待機はエラーにな"
    "ります。相対パスは実行元プロジェクト基準で、${name}等を使用できます。";
inline constexpr char ExecutionNotStarted[] = "execution_not_started";
inline constexpr char WaitCycle[] = "wait_cycle";
inline constexpr char ProjectCreate[] = "project_create";
inline constexpr char ProjectCreateName[] = "AgentNodify プロジェクト作成";
inline constexpr char ProjectCreateDescription[] =
    "指定フローにプロジェクトを登録し、必要なディレクトリを作成します。出力はresult: success / "
    "failedとpathまたはreasonです。同じフロー・パスの重複は失敗します。相対パスは実行元プロジェクト基準で、${name}等を使用できます。";
inline constexpr char PathValue[] = "path";
inline constexpr char Reason[] = "reason";
inline constexpr char ProjectAlreadyExists[] = "project_already_exists";
inline constexpr char PathNotDirectory[] = "path_not_directory";
inline constexpr char DirectoryCreateFailed[] = "directory_create_failed";
inline constexpr char Schedule[] = "schedule";
inline constexpr char ScheduleName[] = "スケジュール";
inline constexpr char ScheduleDescription[] =
    "ローカル時刻で次に月・日・時・分・曜日の全条件が一致する分境界まで待機します。空欄／*は任意、カンマ区切りは複数値、*/"
    "nはnの倍数です。曜日のチェックなしは任意の曜日です。データはそのまま出力します。";
inline constexpr char AnyScheduleValuePlaceholder[] = "*（任意）";
inline constexpr char ScheduleRangeTooltip[] = "%1〜%2。カンマ区切り、*／空欄、*/n（nの倍数）を指定できます。";
inline constexpr char MonthLabel[] = "月";
inline constexpr char HourLabel[] = "時";
inline constexpr char TuesdayShort[] = "火";
inline constexpr char WednesdayShort[] = "水";
inline constexpr char ThursdayShort[] = "木";
inline constexpr char FridayShort[] = "金";
inline constexpr char SaturdayShort[] = "土";
inline constexpr char ScheduleWeekdayObjectName[] = "scheduleWeekday%1";
inline constexpr char WeekdayLabel[] = "曜日";
inline constexpr char ScheduleWeekdayLabelObjectName[] = "scheduleWeekdayLabel%1";
inline constexpr auto ActiveWeekdayStyleStorage = detail::join("color:", colors::TimerForeground, ";");
inline constexpr const char *ActiveWeekdayStyle = ActiveWeekdayStyleStorage.data();
inline constexpr auto InactiveWeekdayStyleStorage = detail::join("color:", colors::WeekdayInactive, ";");
inline constexpr const char *InactiveWeekdayStyle = InactiveWeekdayStyleStorage.data();
inline constexpr char ScheduleTargetLogMessage[] = "次の予定: ";
inline constexpr char Script[] = "script";
inline constexpr char ScriptName[] = "スクリプト";
inline constexpr char ScriptDescription[] = "PythonまたはJavaScriptファイルをシステムのPython／Node.jsで実行します。前段データをUTF-"
                                            "8のJSONで標準入力へ渡し、標準出力を次段へ渡します（JSON、または文字列）。相対パスはプロジェク"
                                            "ト基準です。実行ディレクトリの省略時はスクリプトのある場所を使います。";
inline constexpr char JavaScriptLanguage[] = "javascript";
inline constexpr char InvalidScriptLanguage[] = "スクリプト言語はPythonまたはJavaScriptを指定してください";
inline constexpr char EmptyScriptPath[] = "スクリプトファイルのパスが空です";
inline constexpr char Python[] = "Python";
inline constexpr char NodeJs[] = "Node.js";
inline constexpr char JavaScriptDisplayName[] = "JavaScript (Node.js)";
inline constexpr char ScriptPathPlaceholder[] = "例: scripts/transform.py";
inline constexpr char ScriptDirectoryPlaceholder[] = "省略時: スクリプトのあるディレクトリ";
inline constexpr char ScriptPathTooltip[] = "スクリプトファイルの絶対パス、またはプロジェクトからの相対パス";
inline constexpr char ScriptDirectoryTooltip[] = "絶対パス、またはプロジェクトからの相対パス。空ならスクリプトのあるディレクトリ";
inline constexpr char Browse[] = "参照…";
inline constexpr char ScriptFileBrowse[] = "scriptFileBrowse";
inline constexpr char SelectScriptFile[] = "スクリプトファイルを選択";
inline constexpr char ScriptDirectoryBrowse[] = "scriptDirectoryBrowse";
inline constexpr char SelectWorkingDirectory[] = "実行ディレクトリを選択";
inline constexpr char LanguageLabel[] = "言語";
inline constexpr char FilePathLabel[] = "ファイルパス";
inline constexpr char WorkingDirectoryLabel[] = "実行ディレクトリ";
inline constexpr char ScriptFileDialog[] = "scriptFileDialog";
inline constexpr char JavaScriptFileFilter[] = "JavaScript (*.js *.mjs *.cjs);;すべてのファイル (*)";
inline constexpr char PythonFileFilter[] = "Python (*.py *.pyw);;すべてのファイル (*)";
inline constexpr char ScriptDirectoryDialog[] = "scriptDirectoryDialog";
inline constexpr char ReadScriptFailed[] = "スクリプトファイルを読み取れません: ";
inline constexpr char MissingScriptDirectory[] = "スクリプトの実行ディレクトリが存在しません: ";
inline constexpr char Python3[] = "python3";
inline constexpr char PythonNotFound[] = "システムのPythonが見つかりません。PythonをインストールしPATHを設定してください";
inline constexpr char PythonUnbufferedArgument[] = "-u";
inline constexpr char NodeNotFound[] = "システムのNode.jsが見つかりません。Node.jsをインストールしPATHを設定してください";
inline constexpr char ScriptOutputMustBeUtf8[] = "スクリプトの標準出力はUTF-8で出力してください";
inline constexpr char Shell[] = "shell";
inline constexpr char ShellName[] = "シェル実行";
inline constexpr char ShellDescription[] = "シェルコマンドを実行します（Windows: PowerShell、その他: /bin/sh）。前段の値を ${name} "
                                           "等でそのまま置換します。出力は exit_code・stdout・stderr "
                                           "を持つJSON、またはstdoutのみのテキストを選択できます。実行ディレクトリの省略時・相対パスの基準"
                                           "はフローの保存先です。信頼できる入力だけを使用してください。";
inline constexpr char TextFormat[] = "text";
inline constexpr char ExitCode[] = "exit_code";
inline constexpr char Stdout[] = "stdout";
inline constexpr char Stderr[] = "stderr";
inline constexpr char EmptyShellCommand[] = "シェルコマンドが空です";
inline constexpr char InvalidShellOutput[] = "シェルの出力形式が不正です";
inline constexpr char PowerShell[] = "PowerShell";
inline constexpr char Sh[] = "sh";
inline constexpr char JsonShellOutputLabel[] = "JSON（終了コード・stdout・stderr）";
inline constexpr char TextShellOutputLabel[] = "テキスト（stdoutのみ）";
inline constexpr char OutputFormatLabel[] = "出力形式";
inline constexpr char ShellOutputTooltip[] = "JSONはexit_code・stdout・stderrを出力します。テキストはstdoutを文字列のまま出力し、後段では${"
                                             "#}で参照できます。stderrはどちらも進行ログに残ります。";
inline constexpr char ShellDirectoryPlaceholder[] = "省略時: フローの保存ディレクトリ";
inline constexpr char ShellDirectoryTooltip[] = "絶対パス、またはフローの保存先からの相対パス";
inline constexpr char ShellDirectoryBrowse[] = "shellDirectoryBrowse";
inline constexpr char SelectShellDirectory[] = "シェルの実行ディレクトリを選択";
inline constexpr char ShellCommandLabel[] = "コマンド内容";
inline constexpr char ShellDirectoryDialog[] = "shellDirectoryDialog";
inline constexpr char MissingFlowDirectory[] = "シェル実行の既定／相対ディレクトリを解決できません。フローの保存先を指定してください";
inline constexpr char MissingShellDirectory[] = "シェルの実行ディレクトリが存在しません: ";
inline constexpr char Powershell[] = "powershell";
inline constexpr char Pwsh[] = "pwsh";
inline constexpr char PowerShellNotFound[] = "PowerShellが見つかりません";
inline constexpr char PowerShellUtf8Preamble[] = "[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false); $OutputEncoding = "
                                                 "[Console]::OutputEncoding; $ProgressPreference = 'SilentlyContinue';\n";
inline constexpr char NoLogoArgument[] = "-NoLogo";
inline constexpr char NoProfileArgument[] = "-NoProfile";
inline constexpr char NonInteractiveArgument[] = "-NonInteractive";
inline constexpr char PowerShellOutputFormatArgument[] = "-OutputFormat";
inline constexpr char Text[] = "Text";
inline constexpr char PowerShellCommandArgument[] = "-Command";
inline constexpr char PosixShellExecutable[] = "/bin/sh";
inline constexpr char Start[] = "start";
inline constexpr char StartName[] = "始端";
inline constexpr char StartDescription[] = "フローの処理を開始します。開始時に空のオブジェクト {} を1回出力します。";
inline constexpr char UserInput[] = "user_input";
inline constexpr char UserInputName[] = "ユーザ入力";
inline constexpr char UserInputDescription[] =
    "作業画面の中央下部でユーザー入力を待ちます。前段データをメッセージとして表示し、選択したボタンの出力端だけへ入力文字列を渡します。選択"
    "肢は最低1個です。選択肢の追加・削除でも残った端への接続を維持します。";
inline constexpr char MissingUserChoices[] = "ユーザー選択肢は1個以上必要です";
inline constexpr char EmptyChoiceName[] = "選択肢の名前が空です";
inline constexpr char UserChoicesLabel[] = "ユーザー選択肢";
inline constexpr char AddChoice[] = "＋ 選択肢";
inline constexpr char AddChoiceValue[] = "addChoice";
inline constexpr char IndexedChoiceLabel[] = "選択肢 %1";
inline constexpr char ChoiceName[] = "choiceName";
inline constexpr char ChoiceNameTooltip[] = "出力 %1 の表示名とボタンの文字列";
inline constexpr char Completed[] = "completed";
inline constexpr char Canceled[] = "canceled";
inline constexpr char Add[] = "add";
inline constexpr char Added[] = "added";
inline constexpr char Create[] = "create";
inline constexpr char FileCreatedLabel[] = "作成";
inline constexpr char Delete[] = "delete";
inline constexpr char Deleted[] = "deleted";
inline constexpr char Remove[] = "remove";
inline constexpr char FileDeletedLabel[] = "削除";
inline constexpr char Rename[] = "rename";
inline constexpr char Renamed[] = "renamed";
inline constexpr char Move[] = "move";
inline constexpr char FileMovedLabel[] = "移動";
inline constexpr char FileChangedLabel[] = "変更";
inline constexpr char DisplayDepthLimit[] = "display depth limit";
inline constexpr char Succeeded[] = "完了";
inline constexpr char FailedLabel[] = "失敗";
inline constexpr char Cancelled[] = "中止";
inline constexpr char Unknown[] = "unknown";
inline constexpr char UnknownResultLabel[] = "結果不明";
inline constexpr char Running[] = "実行中";
inline constexpr char FinalResponseLabel[] = "最終応答";
inline constexpr char FinalResponse[] = "final-response";
inline constexpr char InputPrompt[] = "input-prompt";
inline constexpr char PlainEventKey[] = "plain:%1";
inline constexpr char UnrecognizedEvent[] = "解釈できないイベント";
inline constexpr char Item[] = "item";
inline constexpr char SimpleItem[] = "simple:item:";
inline constexpr char Part[] = "part";
inline constexpr char PartID[] = "partID";
inline constexpr char SimplePart[] = "simple:part:";
inline constexpr char StreamEvent[] = "stream_event";
inline constexpr char Index[] = "index";
inline constexpr char SimpleStream[] = "simple:stream:";
inline constexpr char Colon[] = ":";
inline constexpr char EventKey[] = "event:%1:%2";
inline constexpr char EventLabel[] = "イベント";
inline constexpr char UrlSchemeSeparator[] = "://";
inline constexpr char FileEventSeparator[] = ":file:";
inline constexpr char ToolNamespaceSeparator[] = "__";
inline constexpr char Read[] = "read";
inline constexpr char ReadFile[] = "read_file";
inline constexpr char FileReadLabel[] = "読取";
inline constexpr char Write[] = "write";
inline constexpr char WriteFile[] = "write_file";
inline constexpr char FileWriteLabel[] = "書込";
inline constexpr char Edit[] = "edit";
inline constexpr char Multiedit[] = "multiedit";
inline constexpr char Notebookedit[] = "notebookedit";
inline constexpr char File[] = "_file";
inline constexpr char FilePath[] = "file_path";
inline constexpr char FilePathValue[] = "filePath";
inline constexpr char NotebookPath[] = "notebook_path";
inline constexpr char Patch[] = "patch";
inline constexpr char ApplyPatch[] = "apply_patch";
inline constexpr char PatchText[] = "patchText";
inline constexpr char PatchAddPrefix[] = "*** Add File: ";
inline constexpr char PatchDeletePrefix[] = "*** Delete File: ";
inline constexpr char PatchUpdatePrefix[] = "*** Update File: ";
inline constexpr char PatchMovePrefix[] = "*** Move to: ";
inline constexpr char PatchMoveAlternativePrefix[] = "*** Move To: ";
inline constexpr char ToolLabel[] = "ツール";
inline constexpr char CodexValue[] = "codex:";
inline constexpr char CommandExecution[] = "command_execution";
inline constexpr char ClaudeTool[] = "claude:tool:";
inline constexpr char Opencode[] = "opencode:";
inline constexpr char Glob[] = "glob";
inline constexpr char Pattern[] = "pattern";
inline constexpr char Command[] = "command";
inline constexpr char WriteValue[] = "Write";
inline constexpr char TurnStarted[] = "turn.started";
inline constexpr char TurnStart[] = "turn.start";
inline constexpr char TurnCompleted[] = "turn.completed";
inline constexpr char StepStart[] = "step_start";
inline constexpr char StepFinish[] = "step_finish";
inline constexpr char StepStartValue[] = "step-start";
inline constexpr char StepFinishValue[] = "step-finish";
inline constexpr char RateLimitEvent[] = "rate_limit_event";
inline constexpr char ThreadStarted[] = "thread.started";
inline constexpr char System[] = "system";
inline constexpr char ThinkingTokens[] = "thinking_tokens";
inline constexpr char Init[] = "init";
inline constexpr char Assistant[] = "assistant";
inline constexpr char User[] = "user";
inline constexpr char ItemValue[] = "item.";
inline constexpr char Status[] = "status";
inline constexpr char ItemCompleted[] = "item.completed";
inline constexpr char AgentMessage[] = "agent_message";
inline constexpr char AggregatedOutput[] = "aggregated_output";
inline constexpr char McpToolCall[] = "mcp_tool_call";
inline constexpr char Server[] = "server";
inline constexpr char Slash[] = "/";
inline constexpr char Tool[] = "tool";
inline constexpr char Arguments[] = "arguments";
inline constexpr char WebSearch[] = "web_search";
inline constexpr char Query[] = "query";
inline constexpr char FileChange[] = "file_change";
inline constexpr char Changes[] = "changes";
inline constexpr char ToolUse[] = "tool_use";
inline constexpr char Content[] = "content";
inline constexpr char Metadata[] = "metadata";
inline constexpr char Files[] = "files";
inline constexpr char FileValue[] = "file";
inline constexpr char ClaudeThinking[] = "claude:thinking:";
inline constexpr char ThinkingLabel[] = "思考中";
inline constexpr char MessageStart[] = "message_start";
inline constexpr char ContentBlockStart[] = "content_block_start";
inline constexpr char ContentBlock[] = "content_block";
inline constexpr char ClaudeText[] = "claude:text:";
inline constexpr char Thinking[] = "thinking";
inline constexpr char RedactedThinking[] = "redacted_thinking";
inline constexpr char ContentBlockDelta[] = "content_block_delta";
inline constexpr char Delta[] = "delta";
inline constexpr char ThinkingDelta[] = "thinking_delta";
inline constexpr char SignatureDelta[] = "signature_delta";
inline constexpr char TextDelta[] = "text_delta";
inline constexpr char InputJsonDelta[] = "input_json_delta";
inline constexpr char PartialJson[] = "partial_json";
inline constexpr char ContentBlockStop[] = "content_block_stop";
inline constexpr char MessageStop[] = "message_stop";
inline constexpr char MessageDelta[] = "message_delta";
inline constexpr char Uuid[] = "uuid";
inline constexpr char ToolResult[] = "tool_result";
inline constexpr char ToolUseId[] = "tool_use_id";
inline constexpr char IsError[] = "is_error";
inline constexpr char StructuredOutputValue[] = "structured_output";
inline constexpr char TruncatedStringSuffix[] = "...以下 %1文字";
inline constexpr char ObjectSummary[] = "{%1項目}";
inline constexpr char ArraySummary[] = "[%1件]";
inline constexpr char DisplayLimitNotice[] = "...表示上限";
inline constexpr char ArrayIndexLabel[] = "[%1]";
inline constexpr char TruncatedItemsSuffix[] = "...他 %1件の項目";
inline constexpr char JsonPointerEscape[] = "~";
inline constexpr char JsonPointerEscapedTilde[] = "~0";
inline constexpr char JsonPointerEscapedSlash[] = "~1";
inline constexpr char RowHovered[] = "rowHovered";
inline constexpr char Color[] = "color:";
inline constexpr char CssTerminator[] = ";";
inline constexpr char CommandPreview[] = "commandPreview";
inline constexpr auto SecondaryTextStyleStorage = detail::join("color:", colors::SecondaryText, ";");
inline constexpr const char *SecondaryTextStyle = SecondaryTextStyleStorage.data();
inline constexpr char Ellipsis[] = "...";
inline constexpr char FallbackHeader[] = "fallbackHeader";
inline constexpr char ContentView[] = "contentView";
inline constexpr auto ContentViewStyleStorage = detail::join(
    "QWidget#contentView { background:", colors::TransparentCss, "; color:", colors::Text,
    "; } QTextBrowser, QTreeWidget { background:", colors::ContentBackground, "; color:", colors::Text, "; border:1px solid ",
    colors::ContentBorder, "; padding:4px; selection-background-color:", colors::SelectionBackground,
    "; selection-color:", colors::WhiteCss,
    "; } QWidget#progressCard QTextBrowser, QWidget#progressCard QTreeWidget { background:", colors::TransparentCss,
    "; border:0; padding:0; } QTreeWidget::item:selected { background:", colors::SelectionBackground, "; color:", colors::WhiteCss, "; }");
inline constexpr const char *ContentViewStyle = ContentViewStyleStorage.data();
inline constexpr char MarkdownContent[] = "markdownContent";
inline constexpr char JsonContent[] = "jsonContent";
inline constexpr auto UserInputMessageStyleStorage = detail::join(
    "QWidget#userInputMessage, QTextBrowser, QTreeWidget { background:", colors::TransparentCss, "; color:", colors::Text,
    "; border:0; padding:0; } QTreeWidget::item:selected { background:", colors::SelectionBackground, "; color:", colors::WhiteCss, "; }");
inline constexpr const char *UserInputMessageStyle = UserInputMessageStyleStorage.data();
inline constexpr char ProgressCard[] = "progressCard";
inline constexpr auto ProgressCardStyleStorage =
    detail::join("QWidget#progressCard { background:", colors::TransparentCss, "; border:0; } QLabel { color:", colors::Text,
                 "; background:", colors::TransparentCss, "; border:none; } QToolButton { color:", colors::DisclosureText,
                 "; background:", colors::TransparentCss, "; border:0; padding:0; } QToolButton:hover { color:", colors::White,
                 "; } QToolButton:focus { color:", colors::FocusText, "; }");
inline constexpr const char *ProgressCardStyle = ProgressCardStyleStorage.data();
inline constexpr char ProgressHeading[] = "progressHeading";
inline constexpr char AgentInputPrompt[] = "agentInputPrompt";
inline constexpr char InputPromptSeparator[] = "inputPromptSeparator";
inline constexpr auto PromptSeparatorStyleStorage = detail::join("background:", colors::InputBorder, "; border:0;");
inline constexpr const char *PromptSeparatorStyle = PromptSeparatorStyleStorage.data();
inline constexpr char CommandHeader[] = "commandHeader";
inline constexpr char ToolStatus[] = "toolStatus";
inline constexpr char CommandToggle[] = "commandToggle";
inline constexpr char ExpandCommandTooltip[] = "コマンド全体を表示";
inline constexpr char CommandFull[] = "commandFull";
inline constexpr auto CommandDetailStyleStorage = detail::join("color:", colors::SecondaryText, "; margin-left:12px;");
inline constexpr const char *CommandDetailStyle = CommandDetailStyleStorage.data();
inline constexpr char ToolInputToggle[] = "toolInputToggle";
inline constexpr char ToolOutputToggle[] = "toolOutputToggle";
inline constexpr char FileStatus[] = "fileStatus";
inline constexpr char DetailsLabel[] = "詳細";
inline constexpr char EventDetailToggle[] = "eventDetailToggle";
inline constexpr char ExpandPatternTooltip[] = "パターン全体を表示";
inline constexpr char ItemStatusSeparator[] = " · ";
inline constexpr char QtRichTextStart[] = "<qt>";
inline constexpr char QtRichTextEnd[] = "</qt>";
inline constexpr char ProgressTimeline[] = "progressTimeline";
inline constexpr auto ProgressTimelineStyleStorage = detail::join(
    "QScrollArea#progressTimeline, QWidget#progressItems { background:", colors::WindowBackground, "; color:", colors::Text, "; }");
inline constexpr const char *ProgressTimelineStyle = ProgressTimelineStyleStorage.data();
inline constexpr char ProgressItems[] = "progressItems";
inline constexpr char CmdDisableAutoRunArgument[] = "/D";
inline constexpr char CmdKeepOpenArgument[] = "/K";
inline constexpr char PowerShellKeepOpenArgument[] = "-NoExit";
inline constexpr char BashDirectoryArgument[] = "--cd=";
inline constexpr char EditorNewWindowArgument[] = "--new-window";
inline constexpr char LOCALAPPDATA[] = "LOCALAPPDATA";
inline constexpr char WINDIR[] = "WINDIR";
inline constexpr char ProgramW6432[] = "ProgramW6432";
inline constexpr char ProgramFiles[] = "ProgramFiles";
inline constexpr char ProgramFilesX86Environment[] = "ProgramFiles(x86)";
inline constexpr char LocalProgramsSuffix[] = "/Programs";
inline constexpr char HKEYCURRENTUSER[] = "HKEY_CURRENT_USER";
inline constexpr char HKEYLOCALMACHINE[] = "HKEY_LOCAL_MACHINE";
inline constexpr char ApplicationPathsRegistry[] = "\\Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\";
inline constexpr char Dot[] = ".";
inline constexpr char Exe[] = "exe";
inline constexpr char ExplorerAction[] = "エクスプローラーで開く";
inline constexpr char ExplorerExe[] = "explorer.exe";
inline constexpr char ExplorerExecutableSuffix[] = "/explorer.exe";
inline constexpr char CommandPromptAction[] = "コマンドプロンプトで開く";
inline constexpr char CmdExe[] = "cmd.exe";
inline constexpr char CmdExecutableSuffix[] = "/System32/cmd.exe";
inline constexpr char PowerShellCoreSuffix[] = "/PowerShell/7/pwsh.exe";
inline constexpr char GitBashSuffix[] = "/Git/git-bash.exe";
inline constexpr char VsCodeSuffix[] = "/Microsoft VS Code/Code.exe";
inline constexpr char VsCodeInsidersSuffix[] = "/Microsoft VS Code Insiders/Code - Insiders.exe";
inline constexpr char PwshExe[] = "pwsh.exe";
inline constexpr char WindowsPowerShellSuffix[] = "/System32/WindowsPowerShell/v1.0/powershell.exe";
inline constexpr char GitRegistry[] = "\\Software\\GitForWindows";
inline constexpr char InstallPath[] = "InstallPath";
inline constexpr char GitBashRootSuffix[] = "/git-bash.exe";
inline constexpr char GitExe[] = "git.exe";
inline constexpr char GitBashParentPath[] = "../git-bash.exe";
inline constexpr char GitBashAncestorPath[] = "../../git-bash.exe";
inline constexpr char CodeCmd[] = "code.cmd";
inline constexpr char VsCodeParentPath[] = "../Code.exe";
inline constexpr char CodeInsidersCmd[] = "code-insiders.cmd";
inline constexpr char CodeInsiders[] = "code-insiders";
inline constexpr char VsCodeInsidersParentPath[] = "../Code - Insiders.exe";
inline constexpr char PowerShellAction[] = "PowerShellで開く";
inline constexpr char PowershellExe[] = "powershell.exe";
inline constexpr char GitBashAction[] = "Git Bashで開く";
inline constexpr char GitBashExe[] = "git-bash.exe";
inline constexpr char VsCodeAction[] = "VS Codeで開く";
inline constexpr char CodeExe[] = "Code.exe";
inline constexpr char VsCodeInsidersAction[] = "VS Code Insidersで開く";
inline constexpr char CodeInsidersExe[] = "Code - Insiders.exe";
inline constexpr char ProjectApplicationNotFound[] = "ディレクトリまたはアプリが見つかりません。";
inline constexpr char PWD[] = "PWD";
inline constexpr char Skipped[] = "スキップ";
inline constexpr char Error[] = "エラー";
inline constexpr char InvalidCliJson[] = "CLIから不正なJSONイベントを受信しました";
inline constexpr char MissingAgentResponse[] = "エージェントの最終応答がありません";
inline constexpr char UuidPattern[] = "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$";
inline constexpr char OpenCodeSessionPattern[] = "^ses_[A-Za-z0-9_-]{1,128}$";
inline constexpr char ThreadId[] = "thread_id";
inline constexpr char SessionId[] = "session_id";
inline constexpr char SessionID[] = "sessionID";
inline constexpr char InvalidSessionId[] = "CLIセッションIDの形式が不正です";
inline constexpr char ConflictingSessionIds[] = "CLI応答に異なるセッションIDが含まれています";
inline constexpr char MissingProjectDirectory[] = "プロジェクトディレクトリが存在しません";
inline constexpr char TruncatedLogNotice[] = "[古いログを省略]\n";
inline constexpr char IncompleteAllInputs[] = "実行が停止しました（Allの全入力が揃わず、発火可能なノードがありません）";
inline constexpr char EndNotReached[] = "終端に到達せず実行が停止しました";
inline constexpr char FlowCompleted[] = "フローの実行が完了しました（タスクの達成・ファイル作成結果は各ノードの出力を確認してください）";
inline constexpr char ExecutableNotFound[] = "実行ファイルが見つかりません。CLI設定を確認してください: ";
inline constexpr char UnsupportedBatchExecutable[] = ".cmd/.batは非対応です。CLIのネイティブ.exeを指定してください";
inline constexpr char PYTHONUTF8[] = "PYTHONUTF8";
inline constexpr char EnvironmentEnabled[] = "1";
inline constexpr char PYTHONIOENCODING[] = "PYTHONIOENCODING";
inline constexpr char Utf8[] = "utf-8";
inline constexpr char StdoutTooLarge[] = "標準出力が%1MiBを超えました";
inline constexpr char StderrTooLarge[] = "標準エラーが%1MiBを超えました";
inline constexpr char ProcessAbnormalExit[] = "%1が異常終了しました (exit %2)";
inline constexpr char CodexExecutable[] = "codex";
inline constexpr char ClaudeExecutable[] = "claude";
inline constexpr char OpenCodeWindowsExecutable[] = "opencode-cli";
inline constexpr char OpenCodeExecutable[] = "opencode";
inline constexpr char ScheduleNumberPattern[] = "^[0-9]+$";
inline constexpr char StepPrefix[] = "*/";
inline constexpr char InvalidScheduleStep[] = ": */nのnは正の整数を指定してください";
inline constexpr char InvalidScheduleField[] = ": %1〜%2の数値、カンマ区切り、*、*/nを指定してください";
inline constexpr char EmptyScheduleField[] = ": 一致する値がありません";
inline constexpr char InvalidWaitUnitOrDuration[] = "待機時間は0〜%1の整数、単位は日・時間・分・秒を指定してください";
inline constexpr char InvalidWeekday[] = "曜日は月曜日〜日曜日を指定してください";
inline constexpr char InvalidScheduleBase[] = "スケジュールの基準日時が不正です";
inline constexpr char NoMatchingScheduleDate[] = "スケジュール条件に一致する日時がありません（月・日・曜日の組み合わせを確認してください）";
inline constexpr auto ComboPopupStyleStorage = detail::join(
    "\n        QAbstractItemView { background-color:", colors::InputBackground, "; color:", colors::InputForeground,
    ";\n            border:1px solid ", colors::InputBorder, "; selection-background-color:", colors::SelectionBackground,
    ";\n            selection-color:", colors::White, "; outline:0; }\n        QAbstractItemView::item { color:", colors::InputForeground,
    "; padding:5px 8px; min-height:20px; }\n        QAbstractItemView::item:hover, QAbstractItemView::item:selected {\n            "
    "background-color:",
    colors::SelectionBackground, "; color:", colors::White,
    "; }\n        QAbstractItemView:disabled, QAbstractItemView::item:disabled { color:", colors::MutedText, "; }\n    ");
inline constexpr const char *ComboPopupStyle = ComboPopupStyleStorage.data();
inline constexpr char Positions[] = "positions";
inline constexpr char MoveNodeCommand[] = "ノードを移動";
inline constexpr char AddNodeCommand[] = "ノードを追加";
inline constexpr char DeleteNodeCommand[] = "ノードを削除";
inline constexpr char EditConnectionCommand[] = "接続・ポートを編集";
inline constexpr char EditNodeCommand[] = "ノードを編集";
inline constexpr char JsonStringPattern[] = R"("(?:[^"\\]|\\.)*")";
inline constexpr char JsonLiteralPattern[] = R"(\b(?:true|false|null|\d+)\b)";
inline constexpr char VariableHighlightPattern[] = R"(\$\{[^}]+\})";
inline constexpr char FlowFileExtension[] = ".flow.json";
inline constexpr char MissingModel[] = "モデルを選択してください: ";
inline constexpr char FlowDirectorySuffix[] = "/flows";
inline constexpr char RunningFlowTitle[] = "実行中のフロー";
inline constexpr char RunningFlowEditWarning[] =
    "このフローはプロジェクトで実行中のため編集できません。停止してから編集してください。ノードの移動は可能です。";
inline constexpr char WaitingForUserInput[] = "ユーザー入力を待機しています";
inline constexpr char ExecutionResultTitle[] = "実行結果";
inline constexpr char StopProjectTooltip[] = "選択中のプロジェクトの実行を停止します";
inline constexpr char AddNodeEnd[] = "addNode_end";
inline constexpr char RunningProjectTitle[] = "実行中のプロジェクト";
inline constexpr char StopProjectBeforeOperation[] = "対象のプロジェクトでフローを実行中です。対象の実行を中止して操作を続けますか？";
inline constexpr char SystemTrayIcon[] = "systemTrayIcon";
inline constexpr char TrayMenu[] = "trayMenu";
inline constexpr char Show[] = "表示";
inline constexpr char TrayShow[] = "trayShow";
inline constexpr char Quit[] = "終了";
inline constexpr char TrayQuit[] = "trayQuit";
inline constexpr char FileMenuLabel[] = "ファイル";
inline constexpr char FileMenu[] = "fileMenu";
inline constexpr char OpenAction[] = "開く…";
inline constexpr char SaveAction[] = "保存";
inline constexpr char SaveAsAction[] = "名前を付けて保存…";
inline constexpr char EditMenuLabel[] = "編集";
inline constexpr char UndoAction[] = "元に戻す";
inline constexpr char UndoFlowEdit[] = "undoFlowEdit";
inline constexpr char RedoAction[] = "やり直す";
inline constexpr char RedoFlowEdit[] = "redoFlowEdit";
inline constexpr char ValidateAction[] = "フローを検証";
inline constexpr char F7[] = "F7";
inline constexpr char CliSettingsAction[] = "CLI設定…";
inline constexpr char RefreshModelsAction[] = "モデル一覧を更新";
inline constexpr char HelpMenuLabel[] = "ヘルプ";
inline constexpr char UsageAction[] = "操作方法";
inline constexpr char UsageHelp[] = "左のボタンでノードを追加し、丸いポートをドラッグして接続します。\nパラメータはノード内のフォームで編集"
                                    "します。複数行欄をクリックすると右ペインで編集できます。\nノードや追加ボタンにカーソルを置くと機能説明"
                                    "を表示します。Deleteで選択要素を削除できます。\nAll／Anyの入力は0-"
                                    "indexed、分岐は上から順・最後が「それ以外」です。\n端の増減では残った端への接続を維持し、削除した端の"
                                    "接続だけを消します。\nフロー一覧の右クリックで名前変更・一覧から削除、＋から新規作成・既存追加ができま"
                                    "す。\nフローを保存し、プロジェクト作業タブでディレクトリを追加して実行します。\n作業ノードをクリックす"
                                    "ると右ペインに状態・CLIログ・出力を表示します。\nCLIの認証と権限設定は各CLIで事前に行ってください。";
inline constexpr char FlowScroll[] = "flowScroll";
inline constexpr char FlowRail[] = "flowRail";
inline constexpr char MainTabs[] = "mainTabs";
inline constexpr char AddNodePrefix[] = "＋ ";
inline constexpr char CannotAddNode[] = "ノードを追加できません";
inline constexpr char AddNode[] = "addNode_";
inline constexpr auto NodePaletteButtonStyleStorage = detail::join(
    "QPushButton { background:%1; color:%2; border:1px solid %3; } QPushButton:hover { background:%4; } QPushButton:disabled { background:",
    colors::DisabledNodeButton, "; color:", colors::DisabledNodeText, "; border-color:", colors::DisabledNodeBorder, "; }");
inline constexpr const char *NodePaletteButtonStyle = NodePaletteButtonStyleStorage.data();
inline constexpr char NodePalette[] = "nodePalette";
inline constexpr char FlowEditorView[] = "flowEditorView";
inline constexpr char ZoomToFitTooltip[] = "背景にフォーカスがあるとき、Spaceでフロー全体を画面に収めます。";
inline constexpr char NodeContextMenu[] = "nodeContextMenu";
inline constexpr char TextEditorPane[] = "textEditorPane";
inline constexpr char CloseAction[] = "閉じる";
inline constexpr char ParameterEditor[] = "parameterEditor";
inline constexpr auto ValidationErrorStyleStorage = detail::join("color:", colors::ValidationError);
inline constexpr const char *ValidationErrorStyle = ValidationErrorStyleStorage.data();
inline constexpr char FlowEditorTab[] = "フロー編集";
inline constexpr char ProjectLabel[] = "プロジェクト";
inline constexpr char AddProjectDirectoryAction[] = "＋ ディレクトリを追加…";
inline constexpr char SelectProjectHint[] = "プロジェクトを選択してください";
inline constexpr char RunAction[] = "▶ 実行";
inline constexpr char StopAction[] = "■ 停止";
inline constexpr char StopFlowTitle[] = "フローを停止";
inline constexpr char StopFlowConfirmation[] = "選択中のプロジェクトのフロー実行を停止しますか？";
inline constexpr char WorkInputSplitter[] = "workInputSplitter";
inline constexpr char UserInputPanel[] = "userInputPanel";
inline constexpr char UserInputSelector[] = "userInputSelector";
inline constexpr char UserInputMessage[] = "userInputMessage";
inline constexpr char UserInputText[] = "userInputText";
inline constexpr char UserMessagePlaceholder[] = "メッセージを入力し、下の選択肢を押してください";
inline constexpr char InputShortcutTooltip[] = "Ctrl+Enterで最初の選択肢を送信します。Enterのみでは改行します。";
inline constexpr char ResizeInputPaneTooltip[] = "ドラッグしてフロー図と入力フォームの高さを調整します";
inline constexpr char SelectNodeHint[] = "ノードを選択してください";
inline constexpr char WorkDetailsSplitter[] = "workDetailsSplitter";
inline constexpr char ExecutionHistoryTitle[] = "実行履歴";
inline constexpr char FollowLatestExecution[] = "followLatestExecution";
inline constexpr char FollowLatestAction[] = "最新を追従";
inline constexpr char FollowLatestTooltip[] = "最新のノード実行に自動で切り替えます。履歴を選択すると追従を停止します。";
inline constexpr char NodeExecutionHistory[] = "nodeExecutionHistory";
inline constexpr char ExecutionHistoryTooltip[] = "選択したノードの実行を日時順（新しい順）に表示します。ノード未選択時は表示しません。";
inline constexpr char ProgressLogTitle[] = "進行状況 / CLIログ";
inline constexpr char NextOutputTitle[] = "次段への出力";
inline constexpr char ProjectWorkTab[] = "プロジェクト作業";
inline constexpr auto ApplicationStyleStorage = detail::join(
    "\n        QMainWindow, QWidget { background:", colors::WindowBackground, "; color:", colors::Text,
    "; }\n        QMenuBar, QMenu, QTabBar::tab { background:", colors::MenuBackground,
    "; }\n        QMenu { padding:6px; }\n        QMenu::item { padding:8px 36px 8px 20px; }\n        QMenu::item:selected { background:",
    colors::SelectionBackground, "; }\n        QPushButton { background:", colors::ButtonBackground, "; border:1px solid ",
    colors::ButtonBorder, "; border-radius:5px; padding:7px 9px; }\n        QPushButton:hover { background:", colors::ButtonHover,
    "; } QPushButton:disabled { color:", colors::DisabledText,
    "; }\n        QLineEdit, QPlainTextEdit, QTextBrowser, QTreeWidget, QListWidget, QComboBox, QSpinBox, QTableWidget { background:",
    colors::ContentBackground, "; border:1px solid ", colors::ContentBorder,
    "; padding:4px; selection-background-color:", colors::SelectionBackground,
    "; }\n        QTabBar::tab { padding:12px 24px; } QTabBar::tab:selected { color:", colors::FocusText, "; border-bottom:2px solid ",
    colors::TabUnderline, "; }\n        QSplitter::handle { background:", colors::Splitter,
    "; width:5px; } QScrollArea { border:0; }\n        QWidget#flowRail { background:", colors::FlowRailBackground, "; border:1px solid ",
    colors::ContentBorder, "; border-radius:14px; }\n        QScrollArea#flowScroll { background:", colors::TransparentCss,
    "; border:0; }\n        QToolButton[flowTile=\"true\"] { background:", colors::FlowTileBackground, "; color:", colors::FlowTileText,
    "; border:2px solid ", colors::TransparentCss,
    "; border-radius:14px; padding:0; font-size:17px; font-weight:bold; }\n        QToolButton[flowTile=\"true\"]:hover { background:",
    colors::FlowTileHover, "; border-color:", colors::FlowTileHoverBorder,
    "; }\n        QToolButton[flowTile=\"true\"]:checked { background:", colors::FlowTileSelected, "; border:3px solid ",
    colors::FlowTileSelectedBorder,
    "; }\n        QToolButton[flowTile=\"true\"]:checked:hover { background:", colors::FlowTileSelectedHover,
    "; border-color:", colors::FlowTileSelectedHoverBorder, "; }\n        QToolButton#addFlowTile { background:", colors::AddFlowBackground,
    "; color:", colors::AddFlowText, "; border:2px dashed ", colors::AddFlowBorder,
    "; font-size:28px; }\n        QToolButton#addFlowTile:hover { background:", colors::AddFlowHover,
    "; border-color:", colors::AddFlowHoverBorder, "; color:", colors::WhiteCss, "; }\n    ");
inline constexpr const char *ApplicationStyle = ApplicationStyleStorage.data();
inline constexpr char CreateOrOpenFlowHint[] = "フローを作成するか、既存のJSONフローを開いてください";
inline constexpr char WindowTitlePrefix[] = "AgentNodify — ";
inline constexpr char DirtyMarker[] = " *";
inline constexpr char UndoPrefix[] = "元に戻す: ";
inline constexpr char RedoPrefix[] = "やり直す: ";
inline constexpr char DeleteNodesConfirmation[] = "%1個のノードと接続を削除しますか？";
inline constexpr char CannotDuplicateStart[] = "始端は複製できません";
inline constexpr char TooManyEndNodes[] = "終端は1個までです";
inline constexpr char ApplicationQtNodesGraph[] = "application/qt-nodes-graph";
inline constexpr char FlowTile[] = "flowTile";
inline constexpr char RenameFlowAction[] = "名前を変更";
inline constexpr char FlowNameLabel[] = "フロー名";
inline constexpr char RemoveFromListAction[] = "一覧から削除";
inline constexpr char AddSymbol[] = "＋";
inline constexpr char AddFlowAction[] = "フローを追加";
inline constexpr char AddFlowTile[] = "addFlowTile";
inline constexpr char NewFlowAction[] = "新規フロー";
inline constexpr char AddExistingFlowAction[] = "既存フローの追加";
inline constexpr char SchemaEditorTitle[] = "出力スキーマ · JSON Schema Draft 7";
inline constexpr char ShellEditorTitle[] = "シェルコマンド";
inline constexpr char IndexedFlowName[] = "フロー %1";
inline constexpr char RemoveInternalFlowConfirmation[] =
    "このフローはアプリ内部にのみ保存されています。フロー定義のデータも削除します。元に戻せません。\n一覧から削除しますか？";
inline constexpr char RemoveExternalFlowConfirmation[] = "一覧から削除しますか？外部のフロー定義ファイルは削除しません。";
inline constexpr char RemoveFlowTitle[] = "フローを一覧から削除";
inline constexpr char RemoveFlowDefinitionFailed[] = "フロー定義を削除できません: ";
inline constexpr char UnsavedChangesTitle[] = "未保存の変更";
inline constexpr char RemoveUnsavedFlowConfirmation[] = "未保存の変更は失われます。外部ファイルは変更せず一覧から削除しますか？";
inline constexpr char OpenFlowTitle[] = "フローを開く";
inline constexpr char FlowFileFilter[] = "JSON flow (*.json)";
inline constexpr char InvalidDrawingConnection[] = "描画できない接続があります（存在しないノードまたはポート）";
inline constexpr char SaveFlowTitle[] = "フローを保存";
inline constexpr char SavedFlowMessage[] = "保存しました: ";
inline constexpr char ValidateFlowTitle[] = "フロー検証";
inline constexpr char NoValidationErrors[] = "エラーはありません";
inline constexpr char ProjectRow[] = "projectRow";
inline constexpr auto ProjectRowStyleStorage = detail::join("QWidget#projectRow, QLabel { background:", colors::TransparentCss, "; }");
inline constexpr const char *ProjectRowStyle = ProjectRowStyleStorage.data();
inline constexpr char ProjectName[] = "projectName";
inline constexpr char ProjectRunningIcon[] = "projectRunningIcon";
inline constexpr char ProjectInputWaitingIcon[] = "projectInputWaitingIcon";
inline constexpr auto ActivityIconStyleStorage = detail::join("background:", colors::TransparentCss, "; border:0;");
inline constexpr const char *ActivityIconStyle = ActivityIconStyleStorage.data();
inline constexpr char MoreActionsSymbol[] = "…";
inline constexpr char ProjectOpenMenu[] = "projectOpenMenu";
inline constexpr char OpenDirectoryTooltip[] = "ディレクトリを開く";
inline constexpr char OpenProjectDirectoryTooltip[] = "プロジェクトディレクトリを開く";
inline constexpr char RemoveProject[] = "removeProject";
inline constexpr char RemoveProjectTooltip[] = "一覧から削除（ディレクトリは削除しません）";
inline constexpr char RemoveProjectTitle[] = "プロジェクトを一覧から削除";
inline constexpr char AddProjectHint[] = "プロジェクトディレクトリを追加してください";
inline constexpr char Active[] = "active";
inline constexpr char PendingInputCount[] = "ユーザー入力待ち（%1件）";
inline constexpr char ProjectRunningHint[] = "フローを実行中";
inline constexpr char ProjectIdleHint[] = "実行していません";
inline constexpr char StatusSeparator[] = " / ";
inline constexpr char ProjectTooltipSeparator[] = " — ";
inline constexpr char ExecutionNumberText[] = "実行 %1";
inline constexpr char InternalExecution[] = "内部処理";
inline constexpr char DefaultEffort[] = "Default";
inline constexpr char MissingExecutionMetadata[] = "不明（記録なし）";
inline constexpr char ContinuedContextSuffix[] = " 継続コンテキスト";
inline constexpr auto ContinuedContextHtmlStorage =
    detail::join(" <span style=\"color:", colors::ContextAnnotation, "\">継続コンテキスト</span>");
inline constexpr const char *ContinuedContextHtml = ContinuedContextHtmlStorage.data();
inline constexpr char HtmlLineBreak[] = "<br>";
inline constexpr char ExecutionStartedPrefix[] = "開始: ";
inline constexpr char ExecutionFinishedPrefix[] = "\n終了: ";
inline constexpr char InterruptedExecution[] = "不明（中断）";
inline constexpr char ExecutionStatePrefix[] = "\n状態: ";
inline constexpr char CurrentExecutionKey[] = "current:%1:%2";
inline constexpr char YyyyMMDdHHMmSsZzz[] = "yyyy/MM/dd HH:mm:ss.zzz";
inline constexpr char WaitingSuffix[] = " — 待機";
inline constexpr char ExecutionCountSuffix[] = "（実行 %1 回）";
inline constexpr char UserInputChoice[] = "userInputChoice";
inline constexpr char ChoiceIndex[] = "choiceIndex";
inline constexpr char InputShortcutPrefix[] = "+Enterで ";
inline constexpr char UserInputShortcutHint[] = "userInputShortcutHint";
inline constexpr auto InputShortcutStyleStorage = detail::join("color:", colors::ContextAnnotation, ";");
inline constexpr const char *InputShortcutStyle = InputShortcutStyleStorage.data();
inline constexpr char HistoryDirectorySuffix[] = "/history/";
inline constexpr char ProjectDirectoryTitle[] = "プロジェクトディレクトリ";
inline constexpr char ProjectAlreadyRunning[] = "このプロジェクトは既に実行中です";
inline constexpr char CannotStartFlow[] = "フローを開始できません";
inline constexpr char CliSettingsTitle[] = "CLI設定";
inline constexpr char CliExecutableTitle[] = "CLI実行ファイル";
inline constexpr char ExecutableFileFilter[] = "Executable (*.exe);;All files (*)";
inline constexpr char CliPermissionsHint[] =
    "Codexはworkspace-write、Claude "
    "CodeはacceptEditsで書き込みを許可します。\n既存の拒否ルールは維持します。OpenCodeは既存CLIの権限設定に従います。\n認証は各CLIで事前に"
    "行い、Windowsでは.cmdではなくネイティブ.exeを指定してください。\nOpenCodeのJSON出力はスキーマ指示＋受信後検証です。";
inline constexpr char Flow[] = "flow";
inline constexpr char HistoryId[] = "historyId";
inline constexpr char Flows[] = "flows";
inline constexpr char Projects[] = "projects";
inline constexpr char WorkspaceFileSuffix[] = "/workspace.json";
inline constexpr char SaveWorkspaceFailed[] = "ワークスペース設定を保存できません: ";
inline constexpr char InvalidConnectionTitle[] = "不正な接続";
inline constexpr char LoadHistoryFailed[] = "一部の実行履歴を読み込めませんでした:\n";
inline constexpr char LoadSettingsFailed[] = "設定の読み込みに失敗: ";
inline constexpr char TrayUnavailableNotice[] =
    "タスクトレイが利用できないためウィンドウを隠せません。終了する場合は「ファイル → 終了」を選択してください。";
inline constexpr char QuitRunningFlowsConfirmation[] = "実行中のフローがあります。呼び出し先を含むすべてのフローを中止して終了しますか？";
inline constexpr char SaveHistoryTitle[] = "実行履歴の保存";
inline constexpr char QuitUnsavedHistoryConfirmation[] = "実行履歴を保存できていません。未保存の履歴を失っても終了しますか？";
inline constexpr char SayHello[] = "Say hello";
} // namespace an::strings
