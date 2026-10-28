NodePresentation endPresentation() {
    NodePresentation d(Kind::End);
    d.description = strings::EndDescription;
    d.background = colors::TerminalBackground;
    d.foreground = colors::TerminalForeground;
    d.accent = colors::TerminalAccent;
    d.specialBackground = true;
    d.portCaption = [](const Node &, bool, unsigned) { return kindName(Kind::End); };
    return d;
}
