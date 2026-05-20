#pragma once

#include "utilities/qt_json_conversion.h"

namespace ORNL {
/*!
 *  \class SettingsVersionControl
 *  \brief Static class that contains functions for rolling forward settings files
 *         if/when existing settings are altered.
 */
class SettingsVersionControl {
  public:
    //! \brief Public interface: receives current version and settings for alteration
    //! \param version: current version in settings file
    //! \param settings: settings to alter
    static void rollSettingsForward(double& version, fifojson& settings);

    //! \brief Apply appropriate header to settings for saving to file
    //! \param version: current version to set
    //! \param settings: settings to modify
    static void formatSettings(double version, fifojson& settings);

  private:
    //! \brief Rolls initial settings templates without a version to version 1.0
    //! \param version: current version in settings file
    //! \param settings: settings to alter
    static void pre_1_0To1_0(double& version, fifojson& settings);

    //! \brief Rolls initial settings templates without a version to version 2.0
    //! \param version: current version in settings file
    //! \param settings: settings to alter
    static void pre_2_0To2_0(double& version, fifojson& settings);

    //! \brief Rolls settings after deleted, added, and positional enum changes to version 3.0
    //! \param version: current version in settings file
    //! \param settings: settings to alter
    static void pre_3_0To3_0(double& version, fifojson& settings);
};
} // namespace ORNL
