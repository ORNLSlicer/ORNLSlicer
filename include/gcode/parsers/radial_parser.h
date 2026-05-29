#pragma once

#include <qcontainerfwd.h>

#include "gcode/parsers/common_parser.h"

namespace ORNL {
/*!
 * @class RadialParser
 * @brief Parser for Radial3Plus2 gcode with X/Y/Z/A/C motion.
 *
 * The existing visualization path does not model rotary axes directly.  This
 * parser validates A and C as numeric rotary parameters, rejects duplicate
 * rotary parameters on a move, then removes them before delegating the
 * visualization-relevant X/Y/Z/F handling to the shared linear parser logic.
 */
class RadialParser : public CommonParser {
  public:
    /*!
     * @brief Constructs a radial gcode parser.
     * @param meta Gcode syntax metadata for units and comment delimiters.
     * @param allowLayerAlter Whether parser-driven layer-time modification is allowed.
     * @param lines Original gcode lines.
     * @param upperLines Uppercase gcode lines used for parsing.
     */
    RadialParser(GcodeMeta meta, bool allowLayerAlter, QStringList& lines, QStringList& upperLines);

  protected:
    /*!
     * @brief Handles radial travel moves after validating and stripping A/C rotary axes.
     * @param params Raw G0 parameters.
     */
    void G0Handler(QVector<QString> params) override;

    /*!
     * @brief Handles radial print moves after validating and stripping A/C rotary axes.
     * @param params Raw G1 parameters.
     */
    void G1Handler(QVector<QString> params) override;

  private:
    /*!
     * @brief Validates A/C rotary parameters and returns remaining linear parameters.
     * @param params Raw gcode parameters for a motion command.
     * @return Parameters to pass to the common linear parser.
     */
    QVector<QString> stripRotaryAxes(QVector<QString> params);

    /*!
     * @brief Checks whether a parameter character is a supported radial rotary axis.
     * @param axis Parameter character.
     * @return True for A/a/C/c.
     */
    bool isRotaryAxis(QChar axis) const;

    /*!
     * @brief Checks whether the current line comment identifies a radial print move.
     * @return True for radial print comments.
     */
    bool isCommentedPrintMove() const;

    /*!
     * @brief Sets the extruder state used by CommonParser motion estimation.
     * @param on True to mark the extruder as printing.
     */
    void setExtruderActive(bool on);
};
} // namespace ORNL
