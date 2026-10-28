function Component() {
}

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
