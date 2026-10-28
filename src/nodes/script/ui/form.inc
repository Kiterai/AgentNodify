class ScriptForm final : public NodeParameterForm {
public:
    ScriptForm(NodeForm &owner, QFormLayout *form);
private:
    QComboBox *scriptLanguage_ = nullptr;
    QLineEdit *scriptPath_ = nullptr, *scriptDirectory_ = nullptr;
    QPushButton *scriptFileBrowse_ = nullptr, *scriptDirectoryBrowse_ = nullptr;
};

ScriptForm::ScriptForm(NodeForm &owner, QFormLayout *form) : NodeParameterForm(owner) {
    const auto &node = node_;
    scriptLanguage_ = new QComboBox;
    scriptLanguage_->setObjectName(strings::ScriptLanguage);
    scriptLanguage_->addItem(strings::Python, strings::PythonLanguage);
    scriptLanguage_->addItem(strings::JavaScriptDisplayName, strings::JavaScriptLanguage);
    styleComboBoxPopup(scriptLanguage_);
    if (scriptLanguage_->findData(node.scriptLanguage) < 0)
        scriptLanguage_->addItem(node.scriptLanguage + strings::UnsupportedSuffix, node.scriptLanguage);
    scriptLanguage_->setCurrentIndex(scriptLanguage_->findData(node.scriptLanguage));
    scriptPath_ = new QLineEdit(node.scriptPath);
    scriptPath_->setObjectName(strings::ScriptPath);
    scriptPath_->setPlaceholderText(strings::ScriptPathPlaceholder);
    scriptDirectory_ = new QLineEdit(node.scriptDirectory);
    scriptDirectory_->setObjectName(strings::ScriptDirectory);
    scriptDirectory_->setPlaceholderText(strings::ScriptDirectoryPlaceholder);
    scriptPath_->setToolTip(strings::ScriptPathTooltip);
    scriptDirectory_->setToolTip(strings::ScriptDirectoryTooltip);
    auto pathRow = [](QLineEdit *edit, QPushButton *browse) {
        auto *row = new QWidget;
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(metrics::SpaceSmall);
        edit->setMinimumWidth(0);
        layout->addWidget(edit, 1);
        layout->addWidget(browse);
        return row;
    };
    scriptFileBrowse_ = new QPushButton(strings::Browse);
    scriptFileBrowse_->setObjectName(strings::ScriptFileBrowse);
    scriptFileBrowse_->setToolTip(strings::SelectScriptFile);
    scriptFileBrowse_->setAccessibleName(strings::SelectScriptFile);
    scriptDirectoryBrowse_ = new QPushButton(strings::Browse);
    scriptDirectoryBrowse_->setObjectName(strings::ScriptDirectoryBrowse);
    scriptDirectoryBrowse_->setToolTip(strings::SelectWorkingDirectory);
    scriptDirectoryBrowse_->setAccessibleName(strings::SelectWorkingDirectory);
    form->addRow(strings::LanguageLabel, scriptLanguage_);
    form->addRow(strings::FilePathLabel, pathRow(scriptPath_, scriptFileBrowse_));
    form->addRow(strings::WorkingDirectoryLabel, pathRow(scriptDirectory_, scriptDirectoryBrowse_));
    connect(scriptLanguage_, &QComboBox::currentIndexChanged, this, [this] {
        if (!updating_) {
            node_.scriptLanguage = scriptLanguage_->currentData().toString();
            publish();
        }
    });
    connect(scriptPath_, &QLineEdit::textEdited, this, [this](const QString &text) {
        node_.scriptPath = text;
        publish();
    });
    connect(scriptDirectory_, &QLineEdit::textEdited, this, [this](const QString &text) {
        node_.scriptDirectory = text;
        publish();
    });
    connect(scriptFileBrowse_, &QPushButton::clicked, this, [this] {
        if (readOnly_)
            return;
        QFileDialog dialog(dialogOwner(&owner_), strings::SelectScriptFile);
        dialog.setObjectName(strings::ScriptFileDialog);
        dialog.setFileMode(QFileDialog::ExistingFile);
        dialog.setAcceptMode(QFileDialog::AcceptOpen);
        dialog.setNameFilter(node_.scriptLanguage == strings::JavaScriptLanguage ? strings::JavaScriptFileFilter
                                                                                 : strings::PythonFileFilter);
        if (!node_.scriptPath.isEmpty() && QFileInfo(node_.scriptPath).exists())
            dialog.selectFile(node_.scriptPath);
        if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty())
            return;
        node_.scriptPath = dialog.selectedFiles().first();
        scriptPath_->setText(node_.scriptPath);
        publish();
    });
    connect(scriptDirectoryBrowse_, &QPushButton::clicked, this, [this] {
        if (readOnly_)
            return;
        QFileDialog dialog(dialogOwner(&owner_), strings::SelectWorkingDirectory);
        dialog.setObjectName(strings::ScriptDirectoryDialog);
        dialog.setFileMode(QFileDialog::Directory);
        dialog.setOption(QFileDialog::ShowDirsOnly);
        dialog.setAcceptMode(QFileDialog::AcceptOpen);
        if (!node_.scriptDirectory.isEmpty() && QFileInfo(node_.scriptDirectory).isDir())
            dialog.setDirectory(node_.scriptDirectory);
        else if (QFileInfo(node_.scriptPath).isFile())
            dialog.setDirectory(QFileInfo(node_.scriptPath).absolutePath());
        if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty())
            return;
        node_.scriptDirectory = dialog.selectedFiles().first();
        scriptDirectory_->setText(node_.scriptDirectory);
        publish();
    });
    syncParameters_ = [this](const Node &) {
        const auto &node = node_;
        if (scriptLanguage_) {
            if (scriptLanguage_->findData(node.scriptLanguage) < 0)
                scriptLanguage_->addItem(node.scriptLanguage + strings::UnsupportedSuffix, node.scriptLanguage);
            scriptLanguage_->setCurrentIndex(scriptLanguage_->findData(node.scriptLanguage));
        }
        if (scriptPath_ && scriptPath_->text() != node.scriptPath)
            scriptPath_->setText(node.scriptPath);
        if (scriptDirectory_ && scriptDirectory_->text() != node.scriptDirectory)
            scriptDirectory_->setText(node.scriptDirectory);
    };
    readOnlyParameters_ = [this](bool readOnly) {
        scriptFileBrowse_->setEnabled(!readOnly);
        scriptDirectoryBrowse_->setEnabled(!readOnly);
    };
}
