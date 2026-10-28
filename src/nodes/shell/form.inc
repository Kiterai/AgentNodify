void NodeForm::buildShell(QVBoxLayout *layout, QFormLayout *form) {
    const auto &node = node_;
    shellOutput_ = new QComboBox;
    shellOutput_->setObjectName(strings::ShellOutput);
    styleComboBoxPopup(shellOutput_);
    shellOutput_->setMinimumWidth(0);
    shellOutput_->setMinimumContentsLength(metrics::ComboMinimumCharacters);
    shellOutput_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    shellOutput_->addItem(strings::JsonShellOutputLabel, strings::JsonFormat);
    shellOutput_->addItem(strings::TextShellOutputLabel, strings::TextFormat);
    if (shellOutput_->findData(node.shellOutput) < 0)
        shellOutput_->addItem(node.shellOutput + strings::UnsupportedSuffix, node.shellOutput);
    shellOutput_->setCurrentIndex(shellOutput_->findData(node.shellOutput));
    form->addRow(strings::OutputFormatLabel, shellOutput_);
    shellOutput_->setToolTip(strings::ShellOutputTooltip);
    connect(shellOutput_, &QComboBox::currentIndexChanged, this, [this] {
        if (!updating_ && !readOnly_) {
            node_.shellOutput = shellOutput_->currentData().toString();
            publish();
        }
    });
    shellDirectory_ = new QLineEdit(node.shellDirectory);
    shellDirectory_->setObjectName(strings::ShellDirectory);
    shellDirectory_->setMinimumWidth(0);
    shellDirectory_->setPlaceholderText(strings::ShellDirectoryPlaceholder);
    shellDirectory_->setToolTip(strings::ShellDirectoryTooltip);
    shellDirectoryBrowse_ = new QPushButton(strings::Browse);
    shellDirectoryBrowse_->setObjectName(strings::ShellDirectoryBrowse);
    shellDirectoryBrowse_->setToolTip(strings::SelectShellDirectory);
    auto *row = new QWidget;
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(metrics::SpaceSmall);
    rowLayout->addWidget(shellDirectory_, 1);
    rowLayout->addWidget(shellDirectoryBrowse_);
    form->addRow(strings::WorkingDirectoryLabel, row);
    shellCommand_ = addPreview(layout, strings::ShellCommandLabel, strings::ShellCommand, node.shellCommand, strings::ShellPreviewPlaceholder);
    connect(shellDirectory_, &QLineEdit::textEdited, this, [this](const QString &text) {
        node_.shellDirectory = text;
        publish();
    });
    connect(shellDirectoryBrowse_, &QPushButton::clicked, this, [this] {
        if (readOnly_)
            return;
        QFileDialog dialog(dialogOwner(this), strings::SelectShellDirectory);
        dialog.setObjectName(strings::ShellDirectoryDialog);
        dialog.setFileMode(QFileDialog::Directory);
        dialog.setOption(QFileDialog::ShowDirsOnly);
        dialog.setAcceptMode(QFileDialog::AcceptOpen);
        if (!node_.shellDirectory.isEmpty() && QFileInfo(node_.shellDirectory).isDir())
            dialog.setDirectory(node_.shellDirectory);
        if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty())
            return;
        node_.shellDirectory = dialog.selectedFiles().first();
        shellDirectory_->setText(node_.shellDirectory);
        publish();
    });
    syncParameters_ = [this](const Node &) {
        const auto &node = node_;
        if (shellDirectory_ && shellDirectory_->text() != node.shellDirectory)
            shellDirectory_->setText(node.shellDirectory);
        if (shellOutput_) {
            if (shellOutput_->findData(node.shellOutput) < 0)
                shellOutput_->addItem(node.shellOutput + strings::UnsupportedSuffix, node.shellOutput);
            shellOutput_->setCurrentIndex(shellOutput_->findData(node.shellOutput));
        }
        if (shellCommand_ && shellCommand_->toPlainText() != node.shellCommand)
            shellCommand_->setPlainText(node.shellCommand);
    };
    readOnlyParameters_ = [this](bool readOnly) { shellDirectoryBrowse_->setEnabled(!readOnly); };
}
