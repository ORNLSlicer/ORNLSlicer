#pragma once

#include <qcontainerfwd.h>
#include <qhash.h>
#include <qhashfunctions.h>
#include <qlatin1stringview.h>

#include "common_parser.h"
#include "gcode/gcode_meta.h"

namespace ORNL {
/*!
 * \class GKNParser
 * \brief This class implements the GCode parsing configuration for the
 * GKN 3D printer(s). \note The current commands that this class
 * implements are:
 *          - G Commands:
 *              - G0, G1, G4
 *          - M Commands:
 *              - M3, M5, M6, M7
 *          - Other Commands:
 *              - Print Speed, Rapid Speed, Scan Speed
 */
class GKNParser : public CommonParser {
  public:
    //! \brief Standard constructor that specifies meta type and whether or not
    //! to alter layers for minimal layer time
    //! \param meta GcodeMeta struct that includes information about units and
    //! delimiters
    //! \param allowLayerAlter bool to determinw whether or not to alter layers to
    //! adhere to minimal layer times
    //! \param lines Original lines of the gcode file
    //! \param upperLines Uppercase lines of the original used for ease of parsing/comparison
    GKNParser(GcodeMeta meta, bool allowLayerAlter, QStringList& lines, QStringList& upperLines);

    //! \brief Function to initialize syntax specific handlers
    virtual void config() override;

  protected:
    //! \brief Handler for 'G0' command for motion
    //! G0 must be overridden due to unique format, formatting is adjusted and then
    //! referred to base G0 handler
    //! \param params Parameters of current interest include: X, Y, Z
    void G0Handler(QVector<QString> params) override;

    //! \brief Handler for 'G1' command for motion
    //! G1 must be overridden due to unique format, formatting is adjusted and then
    //! referred to base G1 handler
    //! \param params Parameters of current interest include: X, Y, Z, speed
    void G1Handler(QVector<QString> params) override;

    //! \brief Handler for 'G4' command for motion
    //! G4 must be overridden due to unique format, formatting is adjusted and then
    //! referred to base G0 handler
    //! \param params Parameters of current interest include: wait time
    void G4Handler(QVector<QString> params) override;

    //! \brief Handler for 'M3' command
    //! M3 must be overriden as all other formats use it for extruder on
    //! \param params Accepted by function for formatting check, but are not
    //! used for the command
    void M3Handler(QVector<QString> params) override;

    //! \brief Handler for 'M5' command
    //! M5 must be overriden as all other formats use it for extruder off
    //! \param params Accepted by function for formatting check, but are not
    //! used for the command
    void M5Handler(QVector<QString> params) override;

    //! \brief Handler for 'M6' command for turning on extruder
    //! \param params Accepted by function for formatting check, but are not
    //! used for the command
    virtual void M6Handler(QVector<QString> params);

    //! \brief Handler for 'M7' command for turning on extruder
    //! \param params Accepted by function for formatting check, but are not
    //! used for the command
    virtual void M7Handler(QVector<QString> params);

    //! \brief Handler for 'PRINT_SPEED' command.  Used as variable replacement
    //! throughout gcode file
    //! \param params Should be single param containing speed
    virtual void PrintSpeedHandler(QVector<QString> params);

    //! \brief Handler for 'RAPID_SPEED' command.  Used as variable replacement
    //! throughout gcode file
    //! \param params Should be single param containing speed
    virtual void RapidSpeedHandler(QVector<QString> params);

    //! \brief Handler for 'SCAN_SPEED' command.  Used as variable replacement
    //! throughout gcode file
    //! \param params Should be single param containing speed
    virtual void ScanSpeedHandler(QVector<QString> params);

  private:
    //! \brief Predefined strings used to adjust formatting for G commands
    QLatin1String m_x_param, m_y_param, m_z_param, m_f_param, m_p_param;

    //! \brief Lookup for value replacement in G commands from previously identified
    //! variables: print, rapid, and scan speed
    QHash<QString, QString> m_default_values;
};
} // namespace ORNL
