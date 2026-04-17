#pragma once

#include <qcontainerfwd.h>
#include <qhashfunctions.h>
#include <qlatin1stringview.h>

#include "common_parser.h"
#include "gcode/gcode_meta.h"

namespace ORNL {
/*!
 * \class RPBFParser
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
class RPBFParser : public CommonParser {
  public:
    //! \brief Standard constructor that specifies meta type and whether or not
    //! to alter layers for minimal layer time
    //! \param meta GcodeMeta struct that includes information about units and
    //! delimiters
    //! \param allowLayerAlter bool to determinw whether or not to alter layers to
    //! adhere to minimal layer times
    //! \param lines Original lines of the gcode file
    //! \param upperLines Uppercase lines of the original used for ease of parsing/comparison
    RPBFParser(GcodeMeta meta, bool allowLayerAlter, QStringList& lines, QStringList& upperLines);

    //! \brief Function to initialize syntax specific handlers
    virtual void config();

  protected:
    //! \brief Handler for '$$Polyline' command.
    //! \param params Should include identifier, total, followed by total number of segments
    //! of the form x_start, y_start, x_end, y_end, 0, 0, 0
    virtual void PolylinesHandler(QVector<QString> params);

    //! \brief Handler for '$$Hatches' command.
    //! \param params Should include identifier, total, followed by total number of segments
    //! of the form x_start, y_start, x_end, y_end, 0, 0, 0
    virtual void HatchesHandler(QVector<QString> params);

  private:
    //! \brief Helper function to expand commands to work with UI
    //! \param command Command: Polyline or Hatch
    //! \param type Region type used for comment
    //! \param prefix Command prefix that is the same for each expanded line.  Contains placeholder index and vector
    //! total. \param params Individual parameters for each command: x_start, y_start, x_end, y_end, 0, 0, 0
    QString SegmentHelper(QString command, QString type, QVector<QString> prefix, QVector<QString> params);

    //! \brief Predefined strings used to adjust formatting for commands
    QLatin1String m_x_param, m_y_param, m_comma, m_space, m_polyline, m_hatches;

    //! \brief copies of the meta and actual lines since parser must expand lines for UI compliance
    GcodeMeta m_meta_copy;
    QStringList &m_lines_copy, &m_upper_lines_copy;
};
} // namespace ORNL
