function Controller() {
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
