#include "managers/settings/settings_manager.h"

#include "QDirIterator"
#include "QFile"
#include "QMessageBox"
#include "QRegularExpression"
#include "QStandardPaths"
#include "managers/session_manager.h"
#include "managers/settings/settings_version_control.h"
#include "nlohmann/json.hpp"

namespace ORNL {
QSharedPointer<SettingsManager> SettingsManager::m_singleton = QSharedPointer<SettingsManager>();

QSharedPointer<SettingsManager> SettingsManager::getInstance() {
    if (m_singleton.isNull()) {
        m_singleton.reset(new SettingsManager());
    }
    return m_singleton;
}

SettingsManager::SettingsManager() : m_global(new SettingsBase()), m_master(new SettingsBase()) {
    // Load the master configuration. With QRC, this should always be available as it is embeded in the executable.
    QFile master_file(":/configs/master.conf");
    master_file.open(QIODevice::ReadOnly);

    QString master_data = master_file.readAll();
    m_master->json(json::parse(master_data.toStdString()));
    for (auto& el : m_master->json().items()) {
        if (!m_allGlobals.contains(QString::fromStdString(el.value()[Constants::Settings::Master::kMajor]))) {
            m_allGlobals.insert(QString::fromStdString(el.value()[Constants::Settings::Master::kMajor]),
                                QMap<QString, QSharedPointer<SettingsBase>>());
        }
        // populate global with all master's defaults
        m_global->json()[0][el.key()] = el.value()[Constants::Settings::Master::kDefault];
    }
    m_validSuffixes.append("s2c");
    m_validLayerSuffixes.append("s2l");

    QFile versions(":/configs/versions.conf");
    versions.open(QIODevice::ReadOnly);
    QString version_string = versions.readAll();
    fifojson version_data = json::parse(version_string.toStdString());
    m_current_master_version = version_data["master_version"];
    m_yes_to_all_update = false;
    m_current_template = "";
}

QSharedPointer<SettingsBase> SettingsManager::getMaster() const { return m_master; }

bool SettingsManager::loadGlobalJson(QString path) {

    if (path.isEmpty())
        return false;
    QFile conf_file(path);

    // Attempt to open the file.
    if (!conf_file.exists() || !conf_file.open(QIODevice::ReadOnly)) {
        qWarning("Error occured when opening configuration file '%s'", qUtf8Printable(path));
        return false;
    }

    // Get file details to later use name as key into map
    QFileInfo fileInfo(conf_file);

    // TODO: if same name, clear settings for name, load master defaults, then populate

    // Load the data and parse it.
    QString settings_data = conf_file.readAll();
    conf_file.close();
    fifojson j = json::parse(settings_data.toStdString());
    int result = checkVersion(fileInfo.completeBaseName(), j, true);
    if (result == 1) {
        conf_file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
        conf_file.write(j.dump(4).c_str());
        conf_file.close();
    }
    else if (result == -1) {
        // break;
    }

    // if key found in master
    for (auto& array : j[Constants::SettingFileStrings::kSettings].items()) {
        for (auto& el : array.value().items()) {
            QString key = QString::fromStdString(el.key());
            if (m_master->json().find(key.toStdString()) != m_master->json().end()) {
                // create tab for it
                QString displayedTab =
                    QString::fromStdString(m_master->json()[key.toStdString()][Constants::Settings::Master::kMajor]);
                if (!m_allGlobals[displayedTab].contains(fileInfo.completeBaseName())) {
                    // creates new global settings from filename if filename not found
                    QSharedPointer<SettingsBase> sb = QSharedPointer<SettingsBase>(new SettingsBase());
                    m_allGlobals[displayedTab][fileInfo.completeBaseName()] = sb;
                }
                m_allGlobals[displayedTab][fileInfo.completeBaseName()]->setSetting(key, el.value());
            }
        }
    }
    return true;
}

bool SettingsManager::loadAllGlobals(QString path) {
    if (path.isEmpty())
        return false;

    // resource profiles
    QDirIterator it(path, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString nextFile = it.next();
        QFileInfo fileInfo(nextFile);
        if (m_validSuffixes.contains(fileInfo.suffix())) {
            loadGlobalJson(nextFile);
        }
    }
    return true;
}

bool SettingsManager::loadLayerBarTemplate(QString path) {
    if (path.isEmpty())
        return false;

    // resource profiles
    QDirIterator it(path, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString nextFile = it.next();
        QFileInfo fileInfo(nextFile);
        if (m_validLayerSuffixes.contains(fileInfo.suffix())) {
            loadGlobalLayerBarTemplate(nextFile, false);
        }
    }
    return true;
}

bool SettingsManager::loadGlobalLayerBarTemplate(QString path, bool newTemplateSaved) {
    if (path.isEmpty())
        return false;
    QFile conf_file(path);
    QFileInfo fileInfo(conf_file);
    QVector<SettingsRange> new_range;
    fifojson j_array = fifojson::array({});

    if (!conf_file.exists() || !conf_file.open(QIODevice::ReadOnly)) {
        qWarning("Error occured when opening configuration file '%s'", qUtf8Printable(path));
        return false;
    }

    // Load the data and parse it.
    QString settings_data = conf_file.readAll();
    conf_file.close();
    fifojson j_arr = json::parse(settings_data.toStdString());
    for (auto& item : j_arr.items()) {
        fifojson j = item.value()["settings"];
        QSharedPointer<SettingsBase> sb = QSharedPointer<SettingsBase>::create();
        sb->populate(j);
        SettingsRange sr(item.value()["low"], item.value()["high"], "", sb);
        new_range.push_back(sr);
    }
    m_all_layer_bar_templates[fileInfo.completeBaseName()] = new_range;
    // After new template added, emit signal that it has been saved so it is added to templates list.
    if (newTemplateSaved) {
        emit newLayerBarTemplateSaved();
    }
    return true;
}

void SettingsManager::constructActiveGlobal(QString settingTab, QString settingFile) {
    if (m_allGlobals[settingTab].contains(settingFile)) {
        m_global->populate(m_allGlobals[settingTab][settingFile]->json());
    }
    else {
        m_global->populate(m_allGlobals[settingTab]["LFAM_03in"]->json());
        CSM->setMostRecentSettingHistory(settingTab, "LFAM_03in");
    }
}

void SettingsManager::constructLayerBarTemplate(QString settingTab, QString settingFile) {
    if (m_allGlobals[settingTab].contains(settingFile)) {
        m_global->populate(m_allGlobals[settingTab][settingFile]->json());
    }
    else {
        m_global->populate(m_allGlobals[settingTab]["LFAM_03in"]->json());
        CSM->setMostRecentSettingHistory(settingTab, "LFAM_03in");
    }
}

void SettingsManager::constructActiveGlobal(QHash<QString, QString> settingTabAndFile) {
    QHashIterator<QString, QString> i(settingTabAndFile);
    while (i.hasNext()) {
        i.next();
        constructActiveGlobal(i.key(), i.value());
    }
}

void SettingsManager::constructLayerBarTemplate(QHash<QString, QString> settingTabAndFile) {
    QHashIterator<QString, QString> i(settingTabAndFile);
    while (i.hasNext()) {
        i.next();
        constructLayerBarTemplate(i.key(), i.value());
    }
}

int SettingsManager::checkVersion(QString filename, fifojson& settings_data, bool gui) {
    fifojson header;
    double version = 0;
    auto item = settings_data.find(Constants::SettingFileStrings::kHeader);
    bool header_found = item != settings_data.end() && !item.value().is_null();

    if (header_found) {
        header = settings_data[Constants::SettingFileStrings::kHeader];
        auto item2 = header.find(Constants::SettingFileStrings::kVersion);
        if (item2 != header.end() && !item2.value().is_null())
            version = header[Constants::SettingFileStrings::kVersion];
    }

    if (version < m_current_master_version) {
        if (gui) {
            int ret = m_yes_to_all_update;
            if (!ret)
                ret = QMessageBox::warning(
                    nullptr, "ORNL Slicer-2",
                    filename + "is outdated. Do you want to update this template to the newest compatible version?  "
                               "Failure to do so may result in program instability.",
                    QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::No);

            if (ret == QMessageBox::YesToAll)
                m_yes_to_all_update = true;

            if (ret == QMessageBox::Yes || m_yes_to_all_update) {
                SettingsVersionControl::rollSettingsForward(version, settings_data);
                return 1;
            }
            else
                return -1;
        }
        else {
            qInfo() << filename + " is outdated. Failure to update may result in program instability. Do you want to "
                                  "update this template to the newest compatible version? (Y/N)";
            std::string response;
            std::cin >> response;
            if (QString::fromStdString(response).toUpper() == "Y") {
                SettingsVersionControl::rollSettingsForward(version, settings_data);
                return 1;
            }
            else
                return -1;
        }
    }
    return 0;
}

void SettingsManager::consoleConstructActiveGlobal(QString path) {
    for (auto& el : m_master->json().items())
        m_global->json()[0][el.key()] = el.value()[Constants::Settings::Master::kDefault];

    QFile conf_file(path);
    QFileInfo fileInfo(conf_file);
    conf_file.open(QIODevice::ReadOnly);
    QString settings_data = conf_file.readAll();
    conf_file.close();
    fifojson j = json::parse(settings_data.toStdString());
    int result = checkVersion(fileInfo.completeBaseName(), j, false);
    if (result == 1) {
        conf_file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
        conf_file.write(j.dump(4).c_str());
        conf_file.close();
    }
    else if (result == -1) {
        // break;
    }

    m_global->populate(j[Constants::SettingFileStrings::kSettings]);
}

void SettingsManager::removeCurrentSettings(QString settingTab, QString settingFile) {
    fifojson j_array = fifojson::array({});
    j_array = m_allGlobals[settingTab][settingFile]->json();
    for (auto& array : j_array.items()) {
        for (auto& el : array.value().items()) {
            // reset current settings to default
            m_global->json()[0][el.key()] = m_master->json()[el.key()][Constants::Settings::Master::kDefault];
        }
    }
}

fifojson SettingsManager::removeSuffixes(fifojson& j) {
    fifojson settings_array = fifojson::array({});
    fifojson current_index_settings;
    int index = 0; // Last index denotes suffix
    for (auto& el : j[Constants::SettingFileStrings::kSettings].items()) {
        QString key_root = QString::fromStdString(el.key());
        int last_index = key_root.lastIndexOf(QRegularExpression("_\\d+")); // index of suffix
        if (last_index >= 0) {
            key_root.chop(key_root.size() - last_index);                               // remove suffix
            int key_suffix = key_root.right(key_root.size() - last_index - 1).toInt(); // get suffix
            // If suffix matches index, add to json
            if (key_suffix == index)
                current_index_settings[key_root.toStdString()] = el.value();
            // otherwise increment index and add current json to json array
            else {
                index++;
                settings_array.push_back(current_index_settings);
                // Empty current json to be filled again with next index items
                for (auto it : current_index_settings.items()) {
                    current_index_settings.erase(current_index_settings.begin(), current_index_settings.end());
                }
                current_index_settings[key_root.toStdString()] = el.value();
            }
        }
        else {
            current_index_settings[key_root.toStdString()] = el.value();
        }
    }
    settings_array.push_back(current_index_settings);
    return settings_array;
}

bool SettingsManager::loadGlobalJson(const fifojson& j) {

    // tabs must already exist by virtue of the settings manager having loaded already
    QStringList tabs {Constants::Settings::SettingTab::kPrinter, Constants::Settings::SettingTab::kMaterial,
                      Constants::Settings::SettingTab::kProfile, Constants::Settings::SettingTab::kExperimental};

    // either create or reset last session globals as necessary
    QString name = "_LastSession";
    for (QString tab : tabs) {
        if (!m_allGlobals[tab].contains(name)) {
            QSharedPointer<SettingsBase> sb = QSharedPointer<SettingsBase>(new SettingsBase());
            m_allGlobals[tab][name] = sb;
        }
        else {
            m_allGlobals[tab][name]->reset();
        }
    }

    // set all the globals that contain the last session values
    for (auto& el : j[Constants::SettingFileStrings::kSettings].items()) {
        if (m_master->json().find(el.key()) != m_master->json().end()) {
            QString displayedTab =
                QString::fromStdString(m_master->json()[el.key()][Constants::Settings::Master::kMajor]);

            m_allGlobals[displayedTab][name]->setSetting(QString::fromStdString(el.key()), el.value());
        }
    }

    // set default values to newly active global then overlay with loaded values
    m_global->reset();
    for (auto& el : m_master->json().items()) {
        m_global->json()[0][el.key()] = el.value()[Constants::Settings::Master::kDefault];
    }
    m_global->populate(j[Constants::SettingFileStrings::kSettings]);

    emit globalLoaded(name);
    return true;
}

bool SettingsManager::loadLayerSettings(QString path) {
    if (path.isEmpty())
        return false;
    QFile conf_file(path);

    fifojson j_array = fifojson::array({});

    if (!conf_file.exists() || !conf_file.open(QIODevice::ReadOnly)) {
        qWarning("Error occured when opening configuration file '%s'", qUtf8Printable(path));
        return false;
    }

    // Load the data and parse it.
    QString settings_data = conf_file.readAll();
    conf_file.close();
    fifojson j_arr = json::parse(settings_data.toStdString());
    // Loop through json array to create settings ranges and add to settings range array.
    for (auto& item : j_arr.items()) {
        fifojson j = item.value()["settings"];
        QSharedPointer<SettingsBase> sb = QSharedPointer<SettingsBase>::create();
        sb->populate(j);
        SettingsRange sr(item.value()["low"], item.value()["high"], "", sb);
        m_settings_ranges.push_back(sr);
    }
    return true;
}

bool SettingsManager::loadLayerSettingsFromTemplate(QVector<SettingsRange> layerBarTemplate) {
    if (layerBarTemplate.size() == 0)
        return false;
    m_settings_ranges.clear();
    m_settings_ranges = layerBarTemplate;
    return true;
}

QVector<SettingsRange> SettingsManager::getLayerSettings() { return m_settings_ranges; }

QSharedPointer<SettingsBase> SettingsManager::getGlobal() const { return m_global; }

QMap<QString, QMap<QString, QSharedPointer<SettingsBase>>> SettingsManager::getAllGlobals() const {
    return m_allGlobals;
}

QMap<QString, QVector<SettingsRange>> SettingsManager::getAllLayerBarTemplates() const {
    return m_all_layer_bar_templates;
}

void SettingsManager::setCurrentTemplate(QString currentTemplate) { m_current_template = currentTemplate; }

QString SettingsManager::getCurrentTemplate() { return m_current_template; }

void SettingsManager::clearTemplate() { m_settings_ranges.clear(); }

fifojson SettingsManager::globalJson() const {
    fifojson formattedJson = m_global->json();
    SettingsVersionControl::formatSettings(m_current_master_version, formattedJson);
    return formattedJson;
}

void SettingsManager::clearGlobal() { m_global.reset(new SettingsBase()); }

void SettingsManager::saveTemplate(const QStringList& keys, QString path, QString name) {
    if (path.isEmpty())
        return;
    QFile conf_file(path);

    // Attempt to open the file.
    if (!conf_file.open(QIODevice::WriteOnly)) {
        qWarning("Error occured when writing template '%s'", qUtf8Printable(path));
        return;
    }

    // Collect the keys specified.
    fifojson tj = fifojson::object();
    for (QString key : keys) {
        int index = 0;
        bool found = true;
        // save all suffixed versions of the key
        // assumes that suffixes are sequential, ie that if there is no _3, there is not _4, _5,...
        while (found) {
            if (m_global->json().find(key.toStdString()) == m_global->json().end()) {
                found = false;
                // didn't find any suffixed version, use the un-suffixed if it exists
                if (index == 0 && !m_global->json()[0][key.toStdString()].is_null()) {
                    // output unsuffixed value with suffixed key bc all things should be suffixed moving forward
                    tj[key.toStdString()] = m_global->json()[0][key.toStdString()];
                }
            }
            else {
                tj[key.toStdString()] = m_global->json()[0][key.toStdString()];
            }
            ++index;
        }
    }

    SettingsVersionControl::formatSettings(m_current_master_version, tj);
    fifojson whole_file;                  // json including header and settings
    fifojson j_array = fifojson::array(); // array for settings template
    j_array[0] = tj[Constants::SettingFileStrings::kSettings];
    whole_file[Constants::SettingFileStrings::kHeader] = tj[Constants::SettingFileStrings::kHeader];
    whole_file[Constants::SettingFileStrings::kSettings] = j_array;
    if (!name.isEmpty())
        whole_file[Constants::SettingFileStrings::kHeader][Constants::SettingFileStrings::kCreatedBy] =
            name.toStdString();

    conf_file.write(whole_file.dump(4).c_str());
    conf_file.close();
}

void SettingsManager::setConsoleSettings(QSharedPointer<SettingsBase> sb) { m_console_settings = sb; }

QSharedPointer<SettingsBase> SettingsManager::getConsoleSettings() { return m_console_settings; }
} // namespace ORNL
