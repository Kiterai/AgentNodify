function Controller() {
    this.uninstallSucceeded = false;
    this.maintenanceCleanupScheduled = false;
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
    if (this.uninstallSucceeded && installer.isCommandLineInstance()) {
        this.cleanupMaintenanceTool();
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
