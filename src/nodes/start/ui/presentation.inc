NodePresentation startPresentation() {
    NodePresentation d(Kind::Start);
    d.description = strings::StartDescription;
    d.background = colors::TerminalBackground;
    d.foreground = colors::TerminalForeground;
    d.accent = colors::TerminalAccent;
    d.specialBackground = true;
    d.portCaption = [](const Node &, bool, unsigned) { return kindName(Kind::Start); };
    return d;
}
