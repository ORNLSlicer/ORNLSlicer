#pragma once

#include <array>

#include <qcontainerfwd.h>
#include <qset.h>

#include "gcode/gcode_command.h"
#include "gcode/parsers/common_parser.h"

namespace ORNL {
/*!
 * @class ArcSpecialtiesParser
 * @brief Parser for Arc Specialties gcode with X/Y/Z/XR/YR/ZR/AP/CP motion fields.
 *
 * This parser accepts Arc Specialties `KEY=value` motion fields, retains the modal XR/YR/ZR tool frame and
 * AP/CP positioner frame while work-object kinematics are enabled,
 * normalizes X/Y/Z/I/J/K/R/F into the shared parser's single-letter parameter
 * format, accepts known schedule-speed feedrate macros, and delegates motion estimation and visualization command
 * creation to CommonParser.
 */
class ArcSpecialtiesParser : public CommonParser {
   public:
    /*!
     * @brief Constructs an Arc Specialties gcode parser.
     * @param meta Gcode syntax metadata for units and comments.
     * @param allowLayerAlter Whether parser-driven layer-time modification is allowed.
     * @param lines Original gcode lines.
     * @param upperLines Uppercase gcode lines used for parsing.
     */
    ArcSpecialtiesParser(GcodeMeta meta, bool allowLayerAlter, QStringList& lines, QStringList& upperLines);

    /*!
     * @brief Normalizes G00/G01/G02/G03 commands before delegating to the shared parser.
     * @param command_string Raw command line.
     * @param line_number Original line number.
     * @return Parsed command.
     */
    GcodeCommand parseCommand(QString command_string, int line_number) override;

    //! @brief Registers Arc Specialties command handlers and modal arc-center commands.
    void config() override;

    //! @brief Clears shared parser state, modal tool/positioner frames, and the TRAFO mode.
    void reset() override;

   protected:
    /*!
     * @brief Handles Arc Specialties travel moves after validating and stripping orientation axes.
     * @param params Raw G0/G00 parameters.
     */
    void G0Handler(QVector<QString> params) override;

    /*!
     * @brief Handles Arc Specialties print moves after validating and stripping orientation axes.
     * @param params Raw G1/G01 parameters.
     */
    void G1Handler(QVector<QString> params) override;

    /*!
     * @brief Handles Arc Specialties clockwise arc moves after validating and stripping orientation axes.
     * @param params Raw G2/G02 parameters.
     */
    void G2Handler(QVector<QString> params) override;

    /*!
     * @brief Handles Arc Specialties counter-clockwise arc moves after validating and stripping orientation axes.
     * @param params Raw G3/G03 parameters.
     */
    void G3Handler(QVector<QString> params) override;

    //! @brief Enables absolute I/J arc-center parsing.
    void G161Handler(QVector<QString> params);

    //! @brief Enables relative I/J arc-center parsing.
    void G162Handler(QVector<QString> params);

    //! @brief Disables absolute I/J arc-center parsing.
    void G164Handler(QVector<QString> params);

    //! @brief Marks Arc Specialties welder output as active deposition.
    void G82Handler(QVector<QString> params);

    //! @brief Marks Arc Specialties welder output as inactive deposition.
    void G83Handler(QVector<QString> params);

   private:
    /*!
     * @brief Validates Arc Specialties parameters and returns common-parser-compatible linear parameters.
     * @param params Raw gcode parameters for a motion command.
     * @param ignore_inline_arc_optional_stop Whether to drop inline G81 tokens on G2/G3 moves.
     * @return Parameters to pass to the common parser.
     */
    QVector<QString> normalizeAndStripOrientationAxes(QVector<QString> params,
                                                      bool ignore_inline_arc_optional_stop = false);

    /*!
     * @brief Parses a raw Arc Specialties parameter into a key/value pair.
     * @param param Raw parameter token.
     * @param key Output parameter key.
     * @param value Output numeric string.
     */
    void splitParameter(const QString& param, QString& key, QString& value) const;

    /*!
     * @brief Checks whether a parameter key is a preview-relevant linear or feedrate field.
     * @param key Parameter key.
     * @return True for X, Y, Z, I, J, K, R, or F.
     */
    bool isCommonMotionKey(const QString& key) const;

    /*!
     * @brief Checks whether a parameter key is an Arc Specialties orientation field.
     * @param key Parameter key.
     * @return True for XR, YR, ZR, AP, or CP.
     */
    bool isOrientationKey(const QString& key) const;

    /*!
     * @brief Validates that a key has not already appeared on the current command.
     * @param key Parameter key.
     * @param used_keys Keys already seen.
     */
    void validateUniqueKey(const QString& key, QSet<QString>& used_keys);

    /*!
     * @brief Validates that a value is numeric.
     * @param value Raw value string.
     */
    void validateNumericValue(const QString& value);

    /*!
     * @brief Throws an illegal-parameter exception for unsupported Arc Specialties fields.
     * @param param Raw unsupported parameter.
     */
    void throwIllegalArcSpecialtiesParameter(const QString& param);

    /*!
     * @brief Checks whether the current line comment identifies a radial or helical print move.
     * @return True for cylindrical print comments.
     */
    bool isCommentedPrintMove() const;

    /*!
     * @brief Converts absolute I/J arc-center parameters to CommonParser relative offsets when G161 is active.
     * @param params Common-parser-compatible Arc Specialties arc parameters.
     * @return Relative-center parameters to delegate to CommonParser.
     */
    QVector<QString> convertAbsoluteArcCenterParams(const QVector<QString>& params);

    /*!
     * @brief Sets the deposition state used by CommonParser motion estimation.
     * @param on True to mark the parser as depositing material.
     */
    void setDepositionActive(bool on);

    /*!
     * @brief Runs a CommonParser arc handler with optional cylindrical print classification.
     * @param params Raw Arc Specialties arc parameters.
     * @param ccw True for G3/G03, false for G2/G02.
     */
    void handleArcFeedMove(QVector<QString> params, bool ccw);

    //! @brief Tracks whether G161 absolute-center mode is active while parsing Arc Specialties G-code.
    bool m_use_absolute_arc_centers = false;

    //! @brief Modal XR/YR/ZR angles in degrees and the axes explicitly supplied by the program.
    std::array<double, 3> m_tool_angles {};
    unsigned int m_known_tool_angles = 0;

    //! @brief Modal AP/CP positioner angles in degrees; snippets default to work-object motion.
    std::array<double, 2> m_positioner_angles {};
    unsigned int m_known_positioner_angles = 0;
    bool m_work_object_motion              = true;
};
}  // namespace ORNL
