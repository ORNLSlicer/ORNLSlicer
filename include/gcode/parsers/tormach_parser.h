#pragma once

#include <qcontainerfwd.h>
#include <qhashfunctions.h>

#include "common_parser.h"
#include "gcode/gcode_meta.h"

namespace ORNL {
/*!
 * \class TormachParser
 * \brief This class implements the GCode parsing configuration for the
 * Tormach 3D printer(s). \note The current commands that this class
 * implements are:
 *          - M Commands:
 *              - M64, M65
 */
class TormachParser : public CommonParser {
  public:
    //! \brief Standard constructor that specifies meta type and whether or not
    //! to alter layers for minimal layer time
    //! \param meta GcodeMeta struct that includes information about units and
    //! delimiters
    //! \param allowLayerAlter bool to determinw whether or not to alter layers to
    //! adhere to minimal layer times
    //! \param lines Original lines of the gcode file
    //! \param upperLines Uppercase lines of the original used for ease of parsing/comparison
    TormachParser(GcodeMeta meta, bool allowLayerAlter, QStringList& lines, QStringList& upperLines);

    //! \brief Function to initialize syntax specific handlers
    virtual void config();

  protected:
    //! \brief Handler for 'M64' command for turning on extruder
    //! \param params Accepted by function for formatting check, but are not
    //! used for the command
    virtual void M64Handler(QVector<QString> params);

    //! \brief Handler for 'M65' command for turning off extruder
    //! \param params Accepted by function for formatting check, but are not
    //! used for the command
    virtual void M65Handler(QVector<QString> params);
};
} // namespace ORNL
