#pragma once

#include <qcontainerfwd.h>
#include <qhashfunctions.h>

#include "common_parser.h"
#include "gcode/gcode_meta.h"

namespace ORNL {
/*!
 * \class AdamantineParser
 * \brief This class implements the GCode parsing configuration for the
 * Adamantine 3D printer(s). \note The current commands that this class
 * implements are:
 *          - G Commands:
 *              - G28, G92
 *          - M Commands:
 *              - M83
 */
class AdamantineParser : public CommonParser {
  public:
    //! \brief Standard constructor that specifies meta type and whether or not
    //! to alter layers for minimal layer time
    //! \param meta GcodeMeta struct that includes information about units and
    //! delimiters
    //! \param allowLayerAlter bool to determinw whether or not to alter layers to
    //! adhere to minimal layer times
    //! \param lines Original lines of the gcode file
    //! \param upperLines Uppercase lines of the original used for ease of parsing/comparison
    AdamantineParser(GcodeMeta meta, bool allowLayerAlter, QStringList& lines, QStringList& upperLines);

    //! \brief Function to initialize syntax specific handlers
    virtual void config();

  protected:
    //! \brief Handler for 'G28' command for homing
    //! \param params Accepted by function for formatting check, but are not
    //! used for the command
    virtual void G28Handler(QVector<QString> params);

    //! \brief Handler for 'G92' command for filament reset to 0
    //! \param params Single parameter of interest: E, passed to common parser handler
    virtual void G92Handler(QVector<QString> params);

    //! \brief Handler for 'M83' command for switching extruder to relative mode
    //! \param params Accepted by function for formatting check, but are not
    //! used for the command
    virtual void M83Handler(QVector<QString> params);

    //! \brief Handler for 'M605' command to change dual nozzle modes
    //! \param params S0, S1, or S2 are used to indicate which dual-nozzle mode
    //!        to use
    virtual void M605Handler(QVector<QString> params);

    //! \brief Handler for all tool change commands ( T0, T1, ... T6)
    //! \param params Accepted by function for formatting check, but are not
    //!        used for the command
    virtual void THandler(QVector<QString> params);

  private:
    //! \brief Predefined string for home command
    QString m_home_string;

    //! \brief Predefined parameters referencing home command
    QVector<QString> m_home_parameters;
};
} // namespace ORNL
