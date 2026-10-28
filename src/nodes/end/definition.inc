NodeDefinition NodeRegistry::end() {
    NodeDefinition d;
    d.kind = Kind::End;
    d.key = strings::End;
    d.name = strings::EndName;
    d.description = strings::EndDescription;
    d.background = colors::TerminalBackground;
    d.foreground = colors::TerminalForeground;
    d.accent = colors::TerminalAccent;
    d.specialBackground = true;
    d.terminal = true;
    d.outputs = [](const Node &) { return 0u; };
    d.structured = [](const Node &) { return false; };
    d.execute = &Runner::executeImmediate;
    d.portCaption = [](const Node &, bool, unsigned) { return kindName(Kind::End); };
    return d;
}
