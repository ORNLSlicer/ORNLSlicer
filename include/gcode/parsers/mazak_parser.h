#pragma once

#include <qcontainerfwd.h>
#include <qhashfunctions.h>

#include "common_parser.h"
#include "gcode/gcode_meta.h"

namespace ORNL {
/*!
 * \class MazakParser
 * \brief This class implements the GCode parsing configuration for the
 * Mazak 3D printer(s). \note The current commands that this class
 * implements are:
 *          - G Commands:
 *              - G28, G92
 *          - Other Commands:
 *              - Feedrate
 */
class MazakParser : public CommonParser {
  public:
    //! \brief Standard constructor that specifies meta type and whether or not
    //! to alter layers for minimal layer time
    //! \param meta GcodeMeta struct that includes information about units and
    //! delimiters
    //! \param allowLayerAlter bool to determinw whether or not to alter layers to
    //! adhere to minimal layer times
    //! \param lines Original lines of the gcode file
    //! \param upperLines Uppercase lines of the original used for ease of parsing/comparison
    MazakParser(GcodeMeta meta, bool allowLayerAlter, QStringList& lines, QStringList& upperLines);

    //! \brief Function to initialize syntax specific handlers
    virtual void config() override;

  protected:
    //! \brief Handler for 'G1' command for motion
    //! G1 must be overridden due to unique format, formatting is adjusted and then
    //! referred to base G1 handler
    //! \param params Parameters of current interest include: X, Y, Z, speed
    void G1Handler(QVector<QString> params) override;

    //! \brief Handler for 'G441' command for turning on laser
    //! \param params Accepted by function for formatting check, but are not
    //! used for the command
    virtual void G441Handler(QVector<QString> params);

    //! \brief Handler for 'G442' command for turning off lazer
    //! \param params Accepted by function for formatting check, but are not
    //! used for the command
    virtual void G442Handler(QVector<QString> params);

    //! \brief Handler for '#981' command.  Used as variable replacement
    //! throughout gcode file
    //! \param params Should be single param containing speed
    virtual void FeedRateHandler(QVector<QString> params);

  private:
    //! \brief Feedrate replacement'#981' command.  Used as F variable.
    QString m_feedrate;

    //! \brief Predefined char for feedrate replacement.
    QChar m_f_parameter;

    //! \brief Predefined variable for '#981' command.  Used for lookup.
    QString m_feedrate_reference;
};
} // namespace ORNL
