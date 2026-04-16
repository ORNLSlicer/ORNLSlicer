#pragma once

#include <QDir>
#include <QMap>
#include <QObject>
#include <QSharedPointer>
#include <QVector>
#include <qcontainerfwd.h>
#include <qhash.h>
#include <qtmetamacros.h>
#include <qtypes.h>

#include "configs/settings_base.h"
#include "configs/settings_range.h"
#include "part/part.h"
#include "utilities/qt_json_conversion.h"

namespace ORNL {
class SettingsBase;

//! \brief Define for easy access to this singleton.
#define GSM SettingsManager::getInstance()

/*!
 *  \class SettingsManager
 *  \brief Singleton manager class that contains all currently active settings.
 *  \todo This class is in need of a refactor / a possible merge with the SessionManager.
 */
class SettingsManager : public QObject {
    Q_OBJECT
  public:
    //! \brief Get the singleton instance of this object.
    static QSharedPointer<SettingsManager> getInstance();

    // ---- Master Configuration ----

    //! \brief Obtains the master configuration as a SettingsBase. The master settings contain all information
    //!        for display purposes.
    QSharedPointer<SettingsBase> getMaster() const;

    // ---- Global Configuration ----

    bool loadAllGlobals(QString path);

    //! \brief Adds layer bar templates into global
    //! \param newTemplateSaved checks if template has been created or edited from dialog
    //! \return true if successful.
    bool loadGlobalLayerBarTemplate(QString path, bool newTemplateSaved);

    //! \brief Adds layer bar templates into global
    //! \param path: template layer bar file path to be loaded.
    //! \return true if successful.
    bool loadLayerBarTemplate(QString path);

    //! \brief Add a json to the global.
    //! \note This function 'layers' the new settings on top of the active settings.
    bool loadGlobalJson(QString path);

    //! \brief Add a json to the global.
    //! \note This function 'layers' the new settings on top of the active settings.
    bool loadGlobalJson(const fifojson& j);

    //! \brief Retrieve the global configuration from the manager.
    QSharedPointer<SettingsBase> getGlobal() const;

    QMap<QString, QMap<QString, QSharedPointer<SettingsBase>>> getAllGlobals() const;

    //! \brief Retrieve all global layer bar template.
    //! \return Map containing all layer bar templates.
    QMap<QString, QVector<SettingsRange>> getAllLayerBarTemplates() const;

    //! \brief Retrieve current console settings
    //! \return Current console settings
    QSharedPointer<SettingsBase> getConsoleSettings();

    //! \brief Construct active global setting object
    //! \param settingTab major division of settings to load
    //! \param settingFile which file to load
    void constructActiveGlobal(QString settingTab, QString settingFile);

    //! \brief Construct active global layer bar setting object
    //! \param settingTab major division of settings to load
    //! \param settingFile which file to load
    void constructLayerBarTemplate(QString settingTab, QString settingFile);

    //! \brief Construct active global setting object
    //! \param settingTab hash containing all major divisions to construct global from
    void constructActiveGlobal(QHash<QString, QString> settingTabAndFile);

    //! \brief Construct active layer bar global setting object
    //! \param settingTab hash containing all major divisions to construct global from
    void constructLayerBarTemplate(QHash<QString, QString> settingTabAndFile);

    //! \brief Construct active global setting object
    //! \param path: settings file to construct and immediately load as global
    void consoleConstructActiveGlobal(QString path);

    //! \brief Remove settings from active global setting object
    //! //! \param settingTab major division of settings to remove
    //! \param settingFile which file to remove
    void removeCurrentSettings(QString settingTab, QString settingFile);

    //! \brief Remove suffixes if in Version 1.0 format. Place items into json array
    //! \param j: json containing all settings keys and values. May or may not have suffixes.
    //! \return json array with all key suffixes removed.
    fifojson removeSuffixes(fifojson& j);

    //! \brief Load vector of settings ranges
    //! \param path: template file path to be loaded.
    bool loadLayerSettings(QString path);

    //! \brief Load new layer bar template.
    //! \param layerBarTemplate: template to be added.
    //! \return True if successful.
    bool loadLayerSettingsFromTemplate(QVector<SettingsRange> layerBarTemplate);

    //! \brief Returns m_settings_ranges- Vector of ranges.
    QVector<SettingsRange> getLayerSettings();

    //! \brief Generates a json object from the global SettingsBase.
    fifojson globalJson() const;

    //! \brief Reset the global settings to null values.
    void clearGlobal();

    //! \brief The currently seleted template
    //! \param currentTemplate: name of currently selected template from drop down
    void setCurrentTemplate(QString currentTemplate);

    //! \brief Returns currently selected template
    QString getCurrentTemplate();

    //! \brief Clears previous template if <no template selected>.
    void clearTemplate();

    //! \brief Check settings file version
    //! \param path: path of file
    //! \param settings_data: settings to validate
    //! \param gui: whether or not its the GUI or command line requesting (for differing output)
    //! \return 0 - nothing to update, 1 - something to update and successfully rolled forward,
    //! -1 something to update and prevented from rolling forward
    int checkVersion(QString filename, fifojson& settings_data, bool gui);

    // ---- Template Configurations ----

    //! \brief Save a list of keys as a template configuration.
    //! \param keys: keys whose value to save
    //! \param path: path to save output to
    //! \param name: Optional name for header information
    void saveTemplate(const QStringList& keys, QString path, QString name);

    //! \brief Set copy of console settings separate from global.
    //! \param sb: Settings provided to console
    void setConsoleSettings(QSharedPointer<SettingsBase> sb);

  signals:
    //! \brief Signal that something was loaded into the global.
    //! \param name Name of new settings file to use
    void globalLoaded(QString name);

    //! \brief Signal that something was loaded into the ranges.
    void rangesLoaded();

    //! \brief Signal that new layer bar tempalate was saved.
    void newLayerBarTemplateSaved();

  private:
    //! \brief Constructor
    SettingsManager();

    //! \brief Singleton pointer.
    static QSharedPointer<SettingsManager> m_singleton;

    //! \brief Global Settings.
    QSharedPointer<SettingsBase> m_global;

    QMap<QString, QMap<QString, QSharedPointer<SettingsBase>>> m_allGlobals;
    QVector<QString> globalNames;

    //! \brief Ranges. Ranges are keyed off of the cantor pair of the layers.
    QMap<QSharedPointer<Part>, QMap<uint, QSharedPointer<SettingsRange>>> m_ranges;

    //! \brief Master settings.
    QSharedPointer<SettingsBase> m_master;

    //! \brief Valid file suffixes for settings files
    QVector<QString> m_validSuffixes;

    //! \brief Valid file suffixes for layer bar settings files
    QVector<QString> m_validLayerSuffixes;

    //! \brief Current master version as pulled from ORNLSlicer config
    double m_current_master_version;

    //! \brief Tracking for if user requests all settings be rolled forward
    bool m_yes_to_all_update;

    //! \brief Currently selected template
    QString m_current_template;

    //! \brief Settings specified to console using command line interface
    QSharedPointer<SettingsBase> m_console_settings;

    //! \brief Template of settings ranges and their settings bases
    QVector<SettingsRange> m_settings_ranges;

    //! \brief Displayed file names and their corresponding layer bar templates
    QMap<QString, QVector<SettingsRange>> m_all_layer_bar_templates;
};
} // namespace ORNL
