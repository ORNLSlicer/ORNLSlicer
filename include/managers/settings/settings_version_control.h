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

    //! \brief Rolls settings after removed slicing modes and positional enum changes to version 4.0
    //! \param version: current version in settings file
    //! \param settings: settings to alter
    static void pre_4_0To4_0(double& version, fifojson& settings);

    //! \brief Rolls settings after removed concentric fill option to version 5.0
    //! \param version: current version in settings file
    //! \param settings: settings to alter
    static void pre_5_0To5_0(double& version, fifojson& settings);

    //! \brief Rolls settings after removed G-code syntax and positional enum changes to version 6.0
    //! \param version: current version in settings file
    //! \param settings: settings to alter
    static void pre_6_0To6_0(double& version, fifojson& settings);

    //! \brief Rolls settings after removing the radial testbed syntax to version 7.0
    //! \param version: current version in settings file
    //! \param settings: settings to alter
    static void pre_7_0To7_0(double& version, fifojson& settings);

    //! \brief Rolls settings after replacing radial/helical slicing modes with cylindrical path pattern to version 8.0
    //! \param version: current version in settings file
    //! \param settings: settings to alter
    static void pre_8_0To8_0(double& version, fifojson& settings);

    //! \brief Rolls settings after reordering Cylindrical before Image slicing mode to version 9.0
    //! \param version: current version in settings file
    //! \param settings: settings to alter
    static void pre_9_0To9_0(double& version, fifojson& settings);

    //! \brief Rolls renamed slicing setting keys to version 10.0
    //! \param version: current version in settings file
    //! \param settings: settings to alter
    static void pre_10_0To10_0(double& version, fifojson& settings);
};
} // namespace ORNL
