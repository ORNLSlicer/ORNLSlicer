#pragma once

#include <QCommandLineParser>
#include <qdir.h>
#include <qsharedpointer.h>

#include "managers/settings/settings_manager.h"

namespace ORNL {

/*!
 * \class CommandLineConverter
 * \brief Handles setup of QCommandLineParser and conversion to appropriate data structs.
 */
class CommandLineConverter {

  public:
    //! \brief Constructor
    CommandLineConverter();

    //! \brief Set parser with available options
    //! \param parser: parser to set options for
    void setupCommandLineParser(QCommandLineParser& parser);

    //! \brief Set options with chosen command line options and parse global's into GSM
    //! \param parser: parser with values
    //! \param options: valid command line options
    bool convertOptions(QCommandLineParser& parser, QSharedPointer<SettingsBase> options);

  private:
    //! \brief Convert/Check options that are required for processing
    //! These include either a project file or STL/GlobalSettings pair and an output location.
    //! \param parser: parser with values
    //! \param options: valid command line options
    bool checkRequiredSettings(QCommandLineParser& parser, QSharedPointer<SettingsBase> options);

    //! \brief Convert/Check options that are optional: parts and preferences.
    //! These include LocalSettings, transforms, and shifting.
    //! \param parser: parser with values
    //! \param options: valid command line options
    bool checkOptionalPartSettingsAndPreferences(QCommandLineParser& parser, QSharedPointer<SettingsBase> options);

    //! \brief Convert/Check options that are optional: Gcode export control.
    //! These include overwriting, auxiliary files, project copy, file bundling, and header information
    //! (operator/description) \param parser: parser with values \param options: valid command line options
    bool checkOptionalExportOptions(QCommandLineParser& parser, QSharedPointer<SettingsBase> options);

    //! \brief Convert/Check options that are optional: real-time mode.
    //! These include slice bounds, real-time mode, real-time communication mode, and real-time network address.
    //! \param parser: parser with values
    //! \param options: valid command line options
    bool checkAdvancedOptions(QCommandLineParser& parser, QSharedPointer<SettingsBase> options);

    //! \brief Check if file path is valid and of the correct type
    //! \param path: path of file
    //! \param suffix: necessary file extension
    bool isValid(QString path, QString suffix);

    //! \brief Copy of master json for access to individual settings
    QSharedPointer<SettingsBase> m_master;
};
} // namespace ORNL
