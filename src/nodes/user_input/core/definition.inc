NodeDefinition NodeRegistry::userInput() {
    NodeDefinition d;
    d.kind = Kind::UserInput;
    d.key = strings::UserInput;
    d.name = strings::UserInputName;
    d.editableOutputs = true;
    d.outputConnectionsRequired = true;
    d.trailingOutput = false;
    d.unconnectedOutputMessage = strings::UnconnectedChoiceOutput;
    d.editableOutputCount = [](const Node &n) { return unsigned(n.userChoices.size()); };
    d.outputs = [](const Node &n) { return unsigned(n.userChoices.size()); };
    d.sameOutput = [](const Node &a, unsigned i, const Node &b, unsigned j) { return a.userChoices[i] == b.userChoices[j]; };
    d.schemaUsesInputs = false;
    d.schema = [](const Node &, const SchemaInputs &) -> Json { return {{strings::Type, strings::String}}; };
    d.validate = [](const Node &n) {
        QStringList errors;
        if (n.userChoices.isEmpty())
            errors << strings::MissingUserChoices;
        for (const auto &choice : n.userChoices)
            if (choice.trimmed().isEmpty())
                errors << strings::EmptyChoiceName;
        return errors;
    };
    d.structured = [](const Node &) { return false; };
    d.execute = &Runner::executeUserInput;
    return d;
}
