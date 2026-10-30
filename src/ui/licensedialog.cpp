#include "licensedialog.h"
#include "shared/constants.h"
#include "shared/strings.h"
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QVBoxLayout>

// A static library needs an explicit resource reference, outside the namespace.
static void initializeApplicationLicenses() { Q_INIT_RESOURCE(application_licenses); }

namespace an {
LicenseDialog::LicenseDialog(QWidget *parent) : QDialog(parent) {
    initializeApplicationLicenses();
    setObjectName(strings::LicenseDialog);
    setWindowTitle(strings::LicensesAction);
    resize(metrics::LicenseDialogWidth, metrics::LicenseDialogHeight);
    auto *layout = new QVBoxLayout(this);
    auto *versions = new QLabel(QString(strings::LicenseVersionSummary)
        .arg(strings::ApplicationVersion, QT_VERSION_STR, qVersion()));
    versions->setObjectName(strings::LicenseVersions);
    versions->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(versions);
    auto *hint = new QLabel(strings::LicenseScopeHint); hint->setWordWrap(true);
    layout->addWidget(hint);
    auto *splitter = new QSplitter(this);
    auto *list = new QListWidget(splitter); list->setObjectName(strings::LicenseList);
    auto *text = new QPlainTextEdit(splitter); text->setObjectName(strings::LicenseText);
    text->setReadOnly(true);
    splitter->setStretchFactor(1, 1);
    layout->addWidget(splitter, 1);
    for (const auto &document : strings::LicenseDocuments) {
        auto *item = new QListWidgetItem(QString::fromUtf8(document.title), list);
        item->setData(Qt::UserRole, QString::fromUtf8(document.resource));
    }
    const QDir distribution(strings::DistributionLicenseResourceDirectory);
    QDirIterator files(distribution.path(), QDir::Files, QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const auto path = files.next();
        auto *item = new QListWidgetItem(QString(strings::DistributionLicenseLabel)
            .arg(distribution.relativeFilePath(path)), list);
        item->setData(Qt::UserRole, path);
    }
    connect(list, &QListWidget::currentItemChanged, this, [text](QListWidgetItem *item) {
        if (!item) { text->clear(); return; }
        QFile file(item->data(Qt::UserRole).toString());
        // License files are plain text: no HTML interpretation or network access.
        text->setPlainText(file.open(QIODevice::ReadOnly)
            ? QString::fromUtf8(file.readAll()) : QString(strings::LicenseReadFailed));
    });
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    list->setCurrentRow(0);
}
}
