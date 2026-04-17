#include "windows/gcode_export.h"

#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QGroupBox>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QStringBuilder>
#include <qboxlayout.h>
#include <qdebug.h>
#include <qfiledevice.h>
#include <qfileinfo.h>
#include <qgridlayout.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qsize.h>
#include <qstringliteral.h>
#include <qtextedit.h>
#include <qwidget.h>

#include "gcode/gcode_meta.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "threading/gcode_adamantine_saver.h"
#include "threading/gcode_amcm_saver.h"
#include "threading/gcode_aml3d_saver.h"
#include "threading/gcode_marlin_saver.h"
#include "threading/gcode_meld_saver.h"
#include "threading/gcode_rpbf_saver.h"
#include "threading/gcode_sandia_saver.h"
#include "threading/gcode_simulation_output.h"
#include "threading/gcode_tormach_saver.h"
#include "units/unit.h"
#include "utilities/constants.h"

namespace ORNL {

GcodeExport::GcodeExport(QWidget* parent) {
    setWindowTitle(QApplication::applicationDisplayName() + ": G-Code/Project Export");

    QIcon icon;
    icon.addFile(QStringLiteral(":/icons/ornlslicer_logo.png"), QSize(), QIcon::Normal, QIcon::Off);
    setWindowIcon(icon);

    m_layout = new QVBoxLayout();

    QGroupBox* headerBox = new QGroupBox("Optional G-Code Header Information");
    QGridLayout* headerGrid = new QGridLayout();

    m_operator_input = new QLineEdit();
    m_description_input = new QTextEdit();

    headerBox->setLayout(headerGrid);
    headerGrid->addWidget(new QLabel("Operator (Sliced by):"), 0, 0);
    headerGrid->addWidget(m_operator_input, 0, 1);
    headerGrid->addWidget(new QLabel("Description (Additional header notes):"), 1, 0);
    headerGrid->addWidget(m_description_input, 1, 1);

    m_layout->addWidget(headerBox);

    QGroupBox* optionsBox = new QGroupBox("Export Options");
    QVBoxLayout* optionsGrid = new QVBoxLayout();
    m_gcode_file_checkbox = new QCheckBox("Save G-Code file(s)");
    m_gcode_file_checkbox->setChecked(true);
    m_auxiliary_file_checkbox = new QCheckBox("Save Auxiliary files (if applicable)");
    m_auxiliary_file_checkbox->setChecked(true);
    m_project_file_checkbox = new QCheckBox("Save Project file");
    m_bundle_files_checkbox = new QCheckBox("Create subdirectory to bundle files");

    optionsGrid->addWidget(m_gcode_file_checkbox);
    optionsGrid->addWidget(m_auxiliary_file_checkbox);
    optionsGrid->addWidget(m_project_file_checkbox);
    optionsGrid->addWidget(m_bundle_files_checkbox);
    optionsBox->setLayout(optionsGrid);

    m_layout->addWidget(optionsBox);

    QPushButton* m_export_gcode_button = new QPushButton("Export");
    connect(m_export_gcode_button, &QPushButton::pressed, [this]() { this->exportGcode(); });
    m_layout->addWidget(m_export_gcode_button);

    this->setLayout(m_layout);
    this->setFixedSize(this->sizeHint());
}

GcodeExport::~GcodeExport() {}

void GcodeExport::setDefaultName(QString name) { m_default_name = name; }

void GcodeExport::updateOutputInformation(QString tempLocation, GcodeMeta meta) {
    m_location = tempLocation;
    m_most_recent_meta = meta;
}

void GcodeExport::closeEvent(QCloseEvent* event) {
    m_operator_input->clear();
    m_description_input->clear();
    m_gcode_file_checkbox->setChecked(true);
    m_auxiliary_file_checkbox->setChecked(true);
    m_project_file_checkbox->setChecked(false);
    m_bundle_files_checkbox->setChecked(false);
}

void GcodeExport::exportGcode() {
    QString filepath;
    QFileDialog exportDialog;
    QString filter = "G-Code File (*" % m_most_recent_meta.m_file_suffix % ")";
    if (m_bundle_files_checkbox->isChecked()) {
        filepath = exportDialog.getSaveFileName(this, "Export G-Code",
                                                CSM->getMostRecentGcodeLocation() % '/' % m_default_name %
                                                    m_most_recent_meta.m_file_suffix,
                                                filter, &filter, QFileDialog::DontConfirmOverwrite);
    }
    else {
        filepath = exportDialog.getSaveFileName(this, "Export G-Code",
                                                CSM->getMostRecentGcodeLocation() % '/' % m_default_name %
                                                    m_most_recent_meta.m_file_suffix,
                                                filter, &filter);
    }

    if (filepath != QString()) {
        QFileInfo info(filepath);
        QString partName;
        QString fullName = info.fileName();
        filepath = info.absolutePath();

        CSM->setMostRecentGcodeLocation(info.absolutePath());

        // remove file suffix from part name if it exists - keeps periods in the file name intact
        if (fullName.endsWith(m_most_recent_meta.m_file_suffix)) {
            partName = fullName.left(fullName.length() - m_most_recent_meta.m_file_suffix.length());
        }
        else {
            partName = fullName;
        }

        // if bundling files, create folder based on name
        if (m_bundle_files_checkbox->isChecked()) {
            filepath = filepath % '/' % partName;
            QDir dir(filepath);
            dir.mkdir(filepath);
        }
        QString gcodeFileName;
        if (m_most_recent_meta == GcodeMetaList::AdamantineMeta) {
            gcodeFileName = filepath % '/' % partName % "_scan_path" % m_most_recent_meta.m_file_suffix;
        }
        else {
            gcodeFileName = filepath % '/' % partName % m_most_recent_meta.m_file_suffix;
        }
        if (QFile::exists(gcodeFileName))
            QFile::remove(gcodeFileName);

        QString projectFileName = filepath % '/' % partName % ".s2p";

        if (m_project_file_checkbox->isChecked()) {
            if (QFile::exists(projectFileName)) {
                QMessageBox::StandardButton reply =
                    QMessageBox::question(this, "Warning", "Project file already exists.  Do you wish to overwrite?",
                                          QMessageBox::Yes | QMessageBox::No);

                if (reply == QMessageBox::Yes) {
                    QFile::remove(projectFileName);
                    CSM->saveSession(projectFileName, false);
                }
            }
            else {
                CSM->saveSession(projectFileName, false);
            }
        }

        QString text;
        QFile inputFile(m_location);
        if (inputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&inputFile);
            text = in.readAll();
            inputFile.close();
        }

        // Insert comment start/end characters to the description
        QString description = m_description_input->toPlainText();
        int lineEnd = 0;
        while (lineEnd > -1) {
            if (lineEnd + 1 <
                description.length()) // Check to make sure lineEnd + 1 exists before potentially trying to access it
            {
                lineEnd = description.indexOf("\n", lineEnd);
                if (lineEnd > -1) // "\n" is found, -1 means not found
                {
                    description.insert(lineEnd, m_most_recent_meta.m_comment_ending_delimiter);
                    lineEnd = description.indexOf("\n", lineEnd);
                    description.insert(lineEnd + 1, m_most_recent_meta.m_comment_starting_delimiter);
                    lineEnd++; // Increment lineEnd so that the next search doesn't find the same result
                }
            }
            else {
                break;
            }
        }

        // Add header information to the gcode file
        text.prepend(m_most_recent_meta.m_comment_starting_delimiter % "Sliced by: " % m_operator_input->text() %
                     m_most_recent_meta.m_comment_ending_delimiter % "\n" %
                     m_most_recent_meta.m_comment_starting_delimiter % "Slicing notes: " % description %
                     m_most_recent_meta.m_comment_ending_delimiter % "\n");

        if (m_auxiliary_file_checkbox->isChecked()) {
            if (CSM->sensorFilesGenerated()) {
                bool overwrite = false;
                bool hasAnswered = false;
                text.replace("#TEMPFILENAME#", partName + "-");

                QFileInfo fi(m_location);
                QDir tempDir = fi.absoluteDir();
                QFile sensorFile(tempDir.absolutePath() % "/scan_output-0.dat");
                if (sensorFile.exists()) {
                    QString outputLoc = filepath % '/' % partName % "-0.dat";

                    if (QFile::exists(outputLoc)) {
                        QMessageBox::StandardButton reply = QMessageBox::question(
                            this, "Warning", "Sensor file(s) already exists.  Do you wish to overwrite?",
                            QMessageBox::Yes | QMessageBox::No);

                        if (reply == QMessageBox::Yes) {
                            QFile::remove(outputLoc);
                            QFile::copy(sensorFile.fileName(), outputLoc);
                            overwrite = true;
                        }
                        hasAnswered = true;
                    }
                    else
                        QFile::copy(sensorFile.fileName(), outputLoc);
                }

                int i = 1;
                bool moreFound = true;
                while (moreFound) {
                    moreFound = false;
                    QFile sensorFile(tempDir.absolutePath() % "/scan_output-" % QString::number(i) % ".dat");
                    if (sensorFile.exists()) {
                        QString outputLoc = filepath % '/' % partName % "-" % QString::number(i) % ".dat";

                        if (!hasAnswered && QFile::exists(outputLoc)) {
                            QMessageBox::StandardButton reply = QMessageBox::question(
                                this, "Warning", "Sensor file(s) already exists.  Do you wish to overwrite?",
                                QMessageBox::Yes | QMessageBox::No);

                            if (reply == QMessageBox::Yes) {
                                QFile::remove(outputLoc);
                                QFile::copy(sensorFile.fileName(), outputLoc);
                                overwrite = true;
                            }
                            hasAnswered = true;
                        }
                        else if (hasAnswered && QFile::exists(outputLoc)) {
                            if (overwrite) {
                                QFile::remove(outputLoc);
                                QFile::copy(sensorFile.fileName(), outputLoc);
                            }
                            else
                                continue;
                        }
                        else
                            QFile::copy(sensorFile.fileName(), outputLoc);

                        moreFound = true;
                    }
                    ++i;
                }
            }
        }

        if (m_most_recent_meta == GcodeMetaList::RPBFMeta) {
            Angle clockAngle = GSM->getGlobal()->setting<Angle>(ES::RPBFSlicing::kClockingAngle);

            bool use_sector_offsetting = GSM->getGlobal()->setting<bool>(ES::RPBFSlicing::kSectorOffsettingEnable);
            Angle sector_width = GSM->getGlobal()->setting<Angle>(ES::RPBFSlicing::kSectorSize);

            GCodeRPBFSaver* saver = new GCodeRPBFSaver(m_location, filepath, gcodeFileName, text, m_most_recent_meta,
                                                       clockAngle(), use_sector_offsetting, sector_width);
            connect(saver, &GCodeRPBFSaver::finished, saver, &GCodeRPBFSaver::deleteLater);
            connect(saver, &GCodeRPBFSaver::finished, this,
                    [this, filepath, partName]() { showComplete(filepath, partName); });
            saver->start();
        }
        else if ((m_most_recent_meta == GcodeMetaList::MarlinMeta ||
                  m_most_recent_meta == GcodeMetaList::CincinnatiMeta) &&
                 GSM->getGlobal()->setting<bool>(ES::FileOutput::kSimulationOutput)) {
            GCodeSimulationOutput* saver =
                new GCodeSimulationOutput(m_location, filepath, gcodeFileName, text, m_most_recent_meta);
            connect(saver, &GCodeSimulationOutput::finished, saver, &GCodeSimulationOutput::deleteLater);
            connect(saver, &GCodeSimulationOutput::finished, this,
                    [this, filepath, partName]() { showComplete(filepath, partName); });
            saver->start();
        }
        else if (m_most_recent_meta == GcodeMetaList::MeldMeta &&
                 GSM->getGlobal()->setting<bool>(ES::FileOutput::kMeldCompanionOutput)) {
            GCodeMeldSaver* saver = new GCodeMeldSaver(m_location, filepath, gcodeFileName, text, m_most_recent_meta);
            connect(saver, &GCodeMeldSaver::finished, saver, &GCodeMeldSaver::deleteLater);
            connect(saver, &GCodeMeldSaver::finished, this,
                    [this, filepath, partName]() { showComplete(filepath, partName); });
            saver->start();
        }
        else if (m_most_recent_meta == GcodeMetaList::TormachMeta &&
                 GSM->getGlobal()->setting<bool>(ES::FileOutput::kTormachOutput)) {
            GCodeTormachSaver* saver =
                new GCodeTormachSaver(m_location, filepath, gcodeFileName, text, m_most_recent_meta);
            connect(saver, &GCodeTormachSaver::finished, saver, &GCodeTormachSaver::deleteLater);
            connect(saver, &GCodeTormachSaver::finished, this,
                    [this, filepath, partName]() { showComplete(filepath, partName); });
            saver->start();
        }
        else if (m_most_recent_meta == GcodeMetaList::AML3DMeta &&
                 GSM->getGlobal()->setting<bool>(ES::FileOutput::kAML3DOutput)) {
            GCodeAML3DSaver* saver = new GCodeAML3DSaver(m_location, filepath, gcodeFileName, text, m_most_recent_meta);
            connect(saver, &GCodeAML3DSaver::finished, saver, &GCodeAML3DSaver::deleteLater);
            connect(saver, &GCodeAML3DSaver::finished, this,
                    [this, filepath, partName]() { showComplete(filepath, partName); });
            saver->start();
        }
        else if (m_most_recent_meta == GcodeMetaList::SandiaMeta &&
                 GSM->getGlobal()->setting<bool>(ES::FileOutput::kSandiaOutput)) {
            GCodeSandiaSaver* saver =
                new GCodeSandiaSaver(m_location, filepath, gcodeFileName, text, m_most_recent_meta);
            connect(saver, &GCodeSandiaSaver::finished, saver, &GCodeSandiaSaver::deleteLater);
            connect(saver, &GCodeSandiaSaver::finished, this,
                    [this, filepath, partName]() { showComplete(filepath, partName); });
            saver->start();
        }
        else if (m_most_recent_meta == GcodeMetaList::MarlinMeta &&
                 GSM->getGlobal()->setting<bool>(ES::FileOutput::kMarlinOutput)) {
            GCodeMarlinSaver* saver =
                new GCodeMarlinSaver(m_location, filepath, gcodeFileName, text, m_most_recent_meta);
            connect(saver, &GCodeMarlinSaver::finished, saver, &GCodeMarlinSaver::deleteLater);
            connect(saver, &GCodeMarlinSaver::finished, this,
                    [this, filepath, partName]() { showComplete(filepath, partName); });
            saver->start();
        }
        else if ((m_most_recent_meta == GcodeMetaList::ORNLMeta ||
                  m_most_recent_meta == GcodeMetaList::ORNLMetricMeta) &&
                 GSM->getGlobal()->setting<bool>(ES::FileOutput::kAMCMOutput)) {
            GCodeAMCMSaver* saver = new GCodeAMCMSaver(m_location, filepath, gcodeFileName, text, m_most_recent_meta);
            connect(saver, &GCodeAMCMSaver::finished, saver, &GCodeAMCMSaver::deleteLater);
            connect(saver, &GCodeAMCMSaver::finished, this,
                    [this, filepath, partName]() { showComplete(filepath, partName); });
            saver->start();
        }
        else if (m_most_recent_meta == GcodeMetaList::AdamantineMeta) {
            GCodeAdamantineSaver* saver =
                new GCodeAdamantineSaver(m_location, filepath, gcodeFileName, text, m_most_recent_meta);
            connect(saver, &GCodeAdamantineSaver::finished, saver, &GCodeAdamantineSaver::deleteLater);
            connect(saver, &GCodeAdamantineSaver::finished, this,
                    [this, filepath, partName]() { showComplete(filepath, partName); });
            saver->start();
        }
        else {
            QFile tempFile(m_location % "temp");
            if (tempFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                QTextStream out(&tempFile);
                out << text;
                tempFile.close();

                QFile::rename(tempFile.fileName(), gcodeFileName);
                showComplete(filepath, partName);
            }
        }
    }
}

void GcodeExport::showComplete(QString path, QString filename) {
    QMessageBox::information(this, "File Export", filename % " has been succesfully exported to " % path);
}
} // namespace ORNL
