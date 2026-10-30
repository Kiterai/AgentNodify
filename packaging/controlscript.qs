function Controller() {
    this.uninstallSucceeded = false;
    this.maintenanceCleanupScheduled = false;
    this.removeAppData = false;
    if (installer.isInstaller()) { installer.setValue("RemoveAppData", "false"); }
    installer.uninstallationStarted.connect(this, Controller.prototype.chooseAppDataRemoval);
    installer.uninstallationFinished.connect(this, Controller.prototype.uninstallationFinished);
    installer.finishButtonClicked.connect(this, Controller.prototype.cleanupMaintenanceTool);
    // QDialog::rejected also covers closing the completed wizard with its X button.
    if (typeof gui !== "undefined" && gui.rejected) {
        gui.rejected.connect(this, Controller.prototype.cleanupMaintenanceTool);
    }
    installer.setValue("UseSignedMaintenanceTool",
        typeof signedInstallerBaseEnabled !== "undefined" && signedInstallerBaseEnabled ? "true" : "false");
    if (installer.isInstaller() && installer.value("os") === "win") {
        var fallback = installer.value("HomeDir") + "/AppData/Local/Programs/AgentNodify";
        var target = installer.fromNativeSeparators(installer.value("TargetDir"));
        var localData = installer.environmentVariable("LOCALAPPDATA");
        // Preserve an explicitly supplied TargetDir and existing installations.
        if (localData && target === installer.fromNativeSeparators(fallback)) {
            installer.setValue("TargetDir",
                installer.fromNativeSeparators(localData) + "/Programs/AgentNodify");
        }
    }
}

Controller.prototype.uninstallationFinished = function() {
    // IFW Success is zero. Never remove the tool after a failed/cancelled action.
    this.uninstallSucceeded = installer.isUninstaller() && installer.status === 0;
    var cleanupError = null;
    if (this.uninstallSucceeded && this.removeAppData) {
        try { this.cleanupAppData(); } catch (error) { cleanupError = error; }
    }
    if (this.uninstallSucceeded && installer.isCommandLineInstance()) {
        this.cleanupMaintenanceTool();
    }
    if (cleanupError) {
        if (!installer.isCommandLineInstance()) {
            QMessageBox.warning("AppDataCleanupFailed", "AgentNodify", String(cleanupError), QMessageBox.Ok);
        }
        throw cleanupError;
    }
};

Controller.prototype.chooseAppDataRemoval = function() {
    if (!installer.isUninstaller() || installer.value("os") !== "win") { return; }
    // Command-line removal is opt-in on this invocation; GUI always asks with No as default.
    if (installer.isCommandLineInstance()) {
        this.removeAppData = installer.value("RemoveAppData") === "true";
    } else {
        this.removeAppData = QMessageBox.question("RemoveAppData", "AgentNodify",
            "AppData内のAgentNodifyデータ（設定・内部フロー・実行履歴）も削除しますか？\n外部のプロジェクト・フロー、CLIの認証情報は削除しません。",
            QMessageBox.Yes | QMessageBox.No, QMessageBox.No) === QMessageBox.Yes;
    }
};

Controller.prototype.cleanupAppData = function() {
    // Constant command text: no installer value or user path is interpolated into code.
    // Validate the whole tree before deleting anything; never follow junctions/symlinks.
    // Do not use PowerShell at-sign array syntax: IFW expands paired at-signs
    // as installer variables, even in stdin. Require a success marker as well as exit 0.
    var command = "$ErrorActionPreference='Stop'; try { "
        + "function AssertPlain($p) { $i=Get-Item -LiteralPath $p -Force; "
        + "if ($i.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'AppData cleanup refuses reparse points' }; "
        + "if ($i.PSIsContainer) { Get-ChildItem -LiteralPath $p -Force | ForEach-Object { AssertPlain $_.FullName } } }; "
        + "$targets=New-Object 'System.Collections.Generic.List[string]'; foreach ($kind in ('ApplicationData','LocalApplicationData')) { "
        + "$base=[Environment]::GetFolderPath($kind); if ([string]::IsNullOrWhiteSpace($base)) { throw 'Missing AppData folder' }; "
        + "$org=Join-Path $base 'AgentNodify'; $path=Join-Path $org 'AgentNodify'; "
        + "if (Test-Path -LiteralPath $org) { "
        + "if ((Get-Item -LiteralPath $org -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'AppData cleanup refuses redirected organization folder' }; "
        + "if (Test-Path -LiteralPath $path) { AssertPlain $path; $targets.Add($path) } } }; "
        + "foreach ($path in $targets) { Remove-Item -LiteralPath $path -Recurse -Force } "
        + "Write-Output 'AgentNodifyAppDataRemoved' "
        + "} catch { [Console]::Out.WriteLine($_.Exception.Message); exit 1 }\n";
    var result = installer.execute(installer.environmentVariable("SystemRoot") + "/System32/WindowsPowerShell/v1.0/powershell.exe",
        ["-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-Command", "-"], command, "UTF-8", "UTF-8");
    if (result.length !== 2 || Number(result[1]) !== 0 || String(result[0]).indexOf("AgentNodifyAppDataRemoved") < 0) {
        throw new Error("AppData cleanup failed (exit " + result[1] + "); retained data may require manual removal. " + result[0]);
    }
};

Controller.prototype.cleanupMaintenanceTool = function() {
    if (!this.uninstallSucceeded || this.maintenanceCleanupScheduled || installer.value("os") !== "win") {
        return;
    }
    // IFW 4.7 uses uninstall.vbs for self-removal. New Windows images can lack
    // that optional engine. Use the OS command processor without enabling VBS.
    // All command text is constant ASCII: user paths go ONLY in workingDirectory.
    // Delete just our fixed filename, never a directory or wildcard. Loopback
    // ping provides a bounded delay until this process releases its executable.
    var command = "for /L %i in (1,1,60) do @if exist MaintenanceTool.exe (del /Q MaintenanceTool.exe >nul 2>&1 & if exist MaintenanceTool.exe %SystemRoot%\\System32\\ping.exe -n 2 127.0.0.1 >nul 2>&1)";
    var started = installer.executeDetached(installer.environmentVariable("SystemRoot") + "/System32/cmd.exe",
        ["/D", "/Q", "/V:OFF", "/S", "/C", command], installer.value("TargetDir"));
    if (!started) { throw new Error("Cannot schedule MaintenanceTool.exe cleanup."); }
    this.maintenanceCleanupScheduled = true;
};
