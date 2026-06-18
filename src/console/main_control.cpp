#include "console/main_control.h"

#include <iostream>

#include <qdebug.h>
#include <qdir.h>
#include <qfiledevice.h>
#include <qfileinfo.h>
#include <qobject.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>

#include "configs/settings_base.h"
#include "gcode/gcode_meta.h"
#include "managers/preferences_manager.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "threading/gcode_loader.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "utilities/qt_json_conversion.h"

namespace ORNL {

MainControl::MainControl(QSharedPointer<SettingsBase> options) : QObject() {
    GSM->setConsoleSettings(options);
    m_options = options;
    continueStartup();
}

void MainControl::continueStartup() {
    if (m_options->contains(Constants::ConsoleOptionStrings::kInputGlobalSettings))
        GSM->consoleConstructActiveGlobal(
            m_options->setting<QString>(Constants::ConsoleOptionStrings::kInputGlobalSettings));

    connect(CSM.get(), &SessionManager::partAdded, this, &MainControl::loadComplete);
    connect(CSM.get(), &SessionManager::forwardSliceComplete, this, &MainControl::sliceComplete);
    connect(CSM.get(), &SessionManager::updateDialog, this, &MainControl::displayProgress);
}

void MainControl::run() {

    if (m_options->contains(Constants::ConsoleOptionStrings::kShiftPartsOnLoad)) {
        if (m_options->setting<bool>(Constants::ConsoleOptionStrings::kShiftPartsOnLoad)) {
            PreferencesManager::getInstance()->setFileShiftPreference(PreferenceChoice::kPerformAutomatically);
        }
        else {
            PreferencesManager::getInstance()->setFileShiftPreference(PreferenceChoice::kSkipAutomatically);
        }
    }
    if (m_options->contains(Constants::ConsoleOptionStrings::kAlignParts)) {
        if (m_options->setting<bool>(Constants::ConsoleOptionStrings::kAlignParts)) {
            PreferencesManager::getInstance()->setAlignPreference(PreferenceChoice::kPerformAutomatically);
        }
        else {
            PreferencesManager::getInstance()->setAlignPreference(PreferenceChoice::kSkipAutomatically);
        }
    }
    if (m_options->contains(Constants::ConsoleOptionStrings::kUseImplicitTransforms)) {
        PreferencesManager::getInstance()->setUseImplicitTransforms(
            m_options->setting<bool>(Constants::ConsoleOptionStrings::kUseImplicitTransforms));
    }

    if (static_cast<SlicerType>(GSM->getGlobal()->setting<int>(PS::Slicing::kSlicerType)) == SlicerType::kImageSlice)
        CSM->setDefaultGcodeDir(m_options->setting<QString>(Constants::ConsoleOptionStrings::kOutputLocation));

    int stlCount = m_options->setting<int>(Constants::ConsoleOptionStrings::kInputStlCount);
    int supportStlCount = m_options->setting<int>(Constants::ConsoleOptionStrings::kInputSupportStlCount);
    if (stlCount > 0) {
        m_parts_to_load = stlCount + supportStlCount;
        for (int i = 0; i < stlCount; ++i) {
            if (m_options->contains(Constants::ConsoleOptionStrings::kInputSTLTransform)) {
                QFile file(m_options->setting<QString>(Constants::ConsoleOptionStrings::kInputSTLTransform));
                file.open(QIODevice::ReadOnly);
                QTextStream in(&file);
                QString transforms = in.readAll();
                file.close();
                fifojson j = fifojson::parse(transforms.toStdString());

                for (auto it : j[Constants::Settings::Session::kParts].items())
                    CSM->loadModel(m_options->setting<QString>(Constants::ConsoleOptionStrings::kInputStlFiles + "_" +
                                                               QString::number(i)),
                                   false, MeshType::kBuild, true);

                CSM->loadPartsJson(j);
            }
            else
                CSM->loadModel(m_options->setting<QString>(Constants::ConsoleOptionStrings::kInputStlFiles + "_" +
                                                           QString::number(i)),
                               false, MeshType::kBuild, true);
        }

        if (supportStlCount > 0)
            for (int i = 0; i < supportStlCount; ++i)
                CSM->loadModel(m_options->setting<QString>(Constants::ConsoleOptionStrings::kInputSupportStlFiles +
                                                           "_" + QString::number(i)),
                               false, MeshType::kSupport, true);
    }
    else {
        connect(CSM.get(), &SessionManager::totalPartsInProject, this, &MainControl::partsInProject);
        CSM->loadSession(false, m_options->setting<QString>(Constants::ConsoleOptionStrings::kInputProjectFile));
    }
}

void MainControl::partsInProject(int total) { m_parts_to_load = total; }

void MainControl::loadComplete() {
    --m_parts_to_load;

    if (m_parts_to_load == 0 && !CSM->doSlice())
        emit finished();
}

void MainControl::sliceComplete(QString filepath, bool alterFile) {
    if (static_cast<SlicerType>(GSM->getGlobal()->setting<int>(PS::Slicing::kSlicerType)) != SlicerType::kImageSlice) {
        GCodeLoader* loader = new GCodeLoader(filepath, alterFile);
        connect(loader, &GCodeLoader::finished, loader, &GCodeLoader::deleteLater);
        connect(loader, &GCodeLoader::forwardInfoToBuildExportWindow, this, &MainControl::updateOutputInformation);
        connect(loader, &GCodeLoader::finished, this, &MainControl::gcodeParseComplete);
        connect(loader, &GCodeLoader::updateDialog, this, &MainControl::displayProgress);
        loader->start();
    }
    else {
        emit finished();
    }
}

void MainControl::updateOutputInformation(QString tempLocation, GcodeMeta meta) {
    m_temp_location = tempLocation;
    m_selected_meta = meta;
}

void MainControl::gcodeParseComplete() {
    QFileInfo info(m_options->setting<QString>(Constants::ConsoleOptionStrings::kOutputLocation));
    QString partName = info.baseName();
    QString filepath = info.absolutePath();

    QString gcodeFileName = filepath % '/' % partName % m_selected_meta.m_file_suffix;
    if (QFile::exists(gcodeFileName))
        QFile::remove(gcodeFileName);

    QString projectFileName = filepath % '/' % partName % ".s2p";

    QString text;
    QFile inputFile(m_temp_location);
    if (inputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&inputFile);
        text = in.readAll();
        inputFile.close();
    }

    QFile tempFile(m_temp_location % "temp");
    if (tempFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream out(&tempFile);
        out << text;
        tempFile.close();

        QFile::rename(tempFile.fileName(), gcodeFileName);

        emit finished();
    }
}

void MainControl::displayProgress(StatusUpdateStepType type, int percentage) {
    if (m_last_step_type != type)
        m_last_step_type = type;
    else
        std::cout << "\r";

    std::cout << toString(type).toStdString() << " " << percentage;

    if (percentage == 100)
        std::cout << "\n";
}
} // namespace ORNL
