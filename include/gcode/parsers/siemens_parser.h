#pragma once

#include <qcontainerfwd.h>
#include <qhashfunctions.h>

#include "gcode/gcode_meta.h"
#include "gcode/parsers/common_parser.h"

namespace ORNL {
/*!
 * \class SiemensParser
 * \brief This class implements the GCode parsing configuration for the
 * Siemens 3D printer(s). \note The current commands that this class
 * implements are:
 *          - Other Commands:
 *              - Bead Area, Extruder Off
 */
class SiemensParser : public CommonParser {
  public:
    //! \brief Standard constructor that specifies meta type and whether or not
    //! to alter layers for minimal layer time
    //! \param meta GcodeMeta struct that includes information about units and
    //! delimiters
    //! \param allowLayerAlter bool to determinw whether or not to alter layers to
    //! adhere to minimal layer times
    //! \param lines Original lines of the gcode file
    //! \param upperLines Uppercase lines of the original used for ease of parsing/comparison
    SiemensParser(GcodeMeta meta, bool allowLayerAlter, QStringList& lines, QStringList& upperLines);

    //! \brief Function to initialize syntax specific handlers
    virtual void config();

  protected:
    //! \brief Handler for 'BEAD_AREA' command for turning on extruder
    //! \param params Accepted by function for formatting check, but are not
    //! used for the command
    virtual void BeadAreaHandler(QVector<QString> params);

    //! \brief Handler for 'WHEN TRUE DO EXTR_END=2.0' command for turning off extruder
    //! \param params Accepted by function for formatting check, but are not
    //! used for the command
    virtual void ExtruderOffHandler(QVector<QString> params);
};
} // namespace ORNL
