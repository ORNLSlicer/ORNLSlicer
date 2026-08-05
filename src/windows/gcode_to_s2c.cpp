#include "windows/gcode_to_s2c.h"

#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QMessageBox>
#include <QStringBuilder>
#include <qcontainerfwd.h>

#include "gcode/gcode_settings_importer.h"
#include "utilities/constants.h"

namespace ORNL {
namespace {
QString errorPreview(const QStringList& errors) {
    constexpr int kMaxErrors = 10;
    QStringList preview = errors.mid(0, kMaxErrors);
    if (errors.size() > kMaxErrors)
        preview.append(QString("...and %1 more.").arg(errors.size() - kMaxErrors));
    return preview.join("\n");
}

QString comparableFilePath(const QString& path) {
    QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? info.absoluteFilePath() : canonical;
}

bool pathsReferToSameFile(const QString& first, const QString& second) {
    if (first.isEmpty() || second.isEmpty())
        return false;

    const QString first_path = comparableFilePath(first);
    const QString second_path = comparableFilePath(second);
#ifdef Q_OS_WIN
    return first_path.compare(second_path, Qt::CaseInsensitive) == 0;
#else
    return first_path == second_path;
#endif
}
} // namespace

GcodeToS2CDialog::GcodeToS2CDialog(QWidget* parent) : QDialog(parent) { setupUi(); }

QString GcodeToS2CDialog::outputFilePath() const { return m_output_edit->text().trimmed(); }

void GcodeToS2CDialog::setupUi() {
    setWindowTitle("G-Code to S2C");
    setMinimumWidth(620);

    m_layout = new QGridLayout(this);

    QLabel* gcode_label = new QLabel("G-Code file:", this);
    m_gcode_edit = new QLineEdit(this);
    m_gcode_edit->setReadOnly(true);
    m_gcode_browse = new QPushButton("Browse...", this);

    QLabel* output_label = new QLabel("Settings file:", this);
    m_output_edit = new QLineEdit(this);
    m_output_edit->setReadOnly(true);
    m_output_browse = new QPushButton("Browse...", this);

    m_use_defaults = new QCheckBox("Use default values for missing settings", this);
    m_use_defaults->setChecked(true);

    m_status_label = new QLabel(this);
    m_status_label->setWordWrap(true);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_buttons->button(QDialogButtonBox::Ok)->setText("Create");
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);

    m_layout->addWidget(gcode_label, 0, 0);
    m_layout->addWidget(m_gcode_edit, 0, 1);
    m_layout->addWidget(m_gcode_browse, 0, 2);
    m_layout->addWidget(output_label, 1, 0);
    m_layout->addWidget(m_output_edit, 1, 1);
    m_layout->addWidget(m_output_browse, 1, 2);
    m_layout->addWidget(m_use_defaults, 2, 1, 1, 2);
    m_layout->addWidget(m_status_label, 3, 0, 1, 3);
    m_layout->addWidget(m_buttons, 4, 0, 1, 3);

    connect(m_gcode_browse, &QPushButton::clicked, this, &GcodeToS2CDialog::browseGcodeFile);
    connect(m_output_browse, &QPushButton::clicked, this, &GcodeToS2CDialog::browseOutputFile);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &GcodeToS2CDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &GcodeToS2CDialog::reject);
    connect(m_gcode_edit, &QLineEdit::textChanged, this, &GcodeToS2CDialog::refreshAcceptState);
    connect(m_output_edit, &QLineEdit::textChanged, this, &GcodeToS2CDialog::refreshAcceptState);
}

void GcodeToS2CDialog::browseGcodeFile() {
    QFileDialog dialog(this);
    dialog.setWindowTitle("Select G-Code File");
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setNameFilters(QStringList() << "G-Code Files (*.gcode *.nc *.mpf *.eia *.txt)" << "Any Files (*)");

    if (!dialog.exec())
        return;

    m_gcode_edit->setText(dialog.selectedFiles().first());
    setDefaultOutputPath();
}

void GcodeToS2CDialog::browseOutputFile() {
    QFileDialog dialog(this);
    dialog.setWindowTitle("Save Settings File");
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setNameFilters(QStringList() << "ORNLSlicer Configuration/Template File (*.s2c)" << "Any Files (*)");
    dialog.setDefaultSuffix("s2c");

    if (!m_output_edit->text().isEmpty())
        dialog.selectFile(m_output_edit->text());

    if (!dialog.exec())
        return;

    m_output_edit->setText(dialog.selectedFiles().first());
}

void GcodeToS2CDialog::refreshAcceptState() {
    const bool ready = !m_gcode_edit->text().trimmed().isEmpty() && !m_output_edit->text().trimmed().isEmpty();
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(ready);
}

void GcodeToS2CDialog::accept() {
    m_status_label->setText("Creating settings file...");
    QApplication::processEvents();

    const QString gcode_path = m_gcode_edit->text().trimmed();
    const QString output_path = outputFilePath();
    if (pathsReferToSameFile(gcode_path, output_path)) {
        m_status_label->setText("Choose a different settings file path.");
        QMessageBox::critical(this, "G-Code to S2C", "The settings file cannot overwrite the selected G-Code file.");
        return;
    }

    auto missing_callback = [this](const QString& key, const fifojson& master_entry) {
        return promptForMissingValue(key, master_entry);
    };

    GcodeSettingsImporter::ImportResult result =
        GcodeSettingsImporter::importFile(gcode_path, m_use_defaults->isChecked(), missing_callback);

    if (!result.errors.isEmpty()) {
        m_status_label->setText("Could not create settings file.");
        QMessageBox::critical(this, "G-Code to S2C", errorPreview(result.errors));
        return;
    }

    QFile output_file(output_path);
    if (!output_file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        m_status_label->setText("Could not write settings file.");
        QMessageBox::critical(this, "G-Code to S2C", "Could not write settings file:\n" + output_path);
        return;
    }

    output_file.write(result.settings_file.dump(4).c_str());
    output_file.close();

    QString message = QString("Created %1 from %2 footer values.")
                          .arg(QFileInfo(output_path).fileName())
                          .arg(result.imported_keys.size());
    if (!result.defaulted_keys.isEmpty())
        message += QString(" Used defaults for %1 missing settings.").arg(result.defaulted_keys.size());
    if (!result.prompted_keys.isEmpty())
        message += QString(" Prompted for %1 missing settings.").arg(result.prompted_keys.size());
    if (!result.warnings.isEmpty())
        message += "\n\nWarnings:\n" + errorPreview(result.warnings);

    QMessageBox::information(this, "G-Code to S2C", message);
    QDialog::accept();
}

void GcodeToS2CDialog::setDefaultOutputPath() {
    if (m_gcode_edit->text().isEmpty() || !m_output_edit->text().isEmpty())
        return;

    QFileInfo info(m_gcode_edit->text());
    m_output_edit->setText(info.absolutePath() % "/" % info.completeBaseName() % ".s2c");
}

std::optional<fifojson> GcodeToS2CDialog::promptForMissingValue(const QString& key, const fifojson& master_entry) {
    const QString display = GcodeSettingsImporter::displayName(master_entry);
    const QString label = display + " (" + key + ") is missing.\nEnter the raw setting value:";
    const QString default_text = QString::fromStdString(master_entry.at(Constants::Settings::Master::kDefault).dump());

    while (true) {
        bool ok = false;
        const QString text =
            QInputDialog::getText(this, "Missing Setting Value", label, QLineEdit::Normal, default_text, &ok);
        if (!ok)
            return std::nullopt;

        fifojson candidate;
        try {
            candidate = fifojson::parse(text.toStdString());
        } catch (...) { candidate = text.toStdString(); }

        fifojson normalized;
        QString error;
        if (GcodeSettingsImporter::validateValue(key, master_entry, candidate, normalized, error, false))
            return normalized;

        QMessageBox::warning(this, "Invalid Setting Value", error);
    }
}

} // namespace ORNL
