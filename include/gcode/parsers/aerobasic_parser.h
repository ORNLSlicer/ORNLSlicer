#pragma once

#include <qcontainerfwd.h>
#include <qhashfunctions.h>

#include "common_parser.h"
#include "gcode/gcode_meta.h"

namespace ORNL {
/*!
 * \class AeroBasicParser
 * \brief This class implements the GCode parsing configuration for the
 * AeroBasic 3D printer(s).
 */
class AeroBasicParser : public CommonParser {
  public:
    //! \brief Standard constructor that specifies meta type and whether or not
    //! to alter layers for minimal layer time
    //! \param meta GcodeMeta struct that includes information about units and
    //! delimiters
    //! \param allowLayerAlter bool to determinw whether or not to alter layers to
    //! adhere to minimal layer times
    //! \param lines Original lines of the gcode file
    //! \param upperLines Uppercase lines of the original used for ease of parsing/comparison
    AeroBasicParser(GcodeMeta meta, bool allowLayerAlter, QStringList& lines, QStringList& upperLines);

    //! \brief forwards aliases to common parser version
    //! \param params gcode-params
    void G0Handler(QVector<QString> params) override;

    //! \brief forwards aliases to common parser version
    //! \param params gcode-params
    void G1Handler(QVector<QString> params) override;

    //! \brief forwards aliases to common parser version
    //! \param params gcode-params
    void G2Handler(QVector<QString> params) override;

    //! \brief forwards aliases to common parser version
    //! \param params gcode-params
    void G3Handler(QVector<QString> params) override;

    //! \brief forwards aliases to common parser version
    //! \param params gcode-params
    void G4Handler(QVector<QString> params) override;

    //! \brief Function to initialize syntax specific handlers
    virtual void config() override;
};
} // namespace ORNL
