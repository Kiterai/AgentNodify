function Component() {
    installer.installationStarted.connect(this, Component.prototype.useSignedMaintenanceTool);
}

Component.prototype.useSignedMaintenanceTool = function() {
    if (installer.value("UseSignedMaintenanceTool") === "true"
            && (component.installationRequested() || component.updateRequested())) {
        // IFW copies this signed binary as MaintenanceTool after extraction.
        installer.setInstallerBaseBinary(installer.value("TargetDir") + "/maintenance/installerbase.exe");
    }
};

Component.prototype.createOperations = function() {
    component.createOperations();
    if (installer.value("os") === "win") {
        component.addOperation("CreateShortcut",
            "@TargetDir@/bin/AgentNodify.exe",
            "@StartMenuDir@/AgentNodify.lnk",
            "workingDirectory=@TargetDir@");
        component.addOperation("CreateShortcut",
            "@TargetDir@/MaintenanceTool.exe",
            "@StartMenuDir@/AgentNodify Maintenance.lnk");
        // Prompt rather than force-killing workflows during removal.
        component.addOperation("FakeStopProcessForUpdate", "AgentNodify.exe");
    }
};
