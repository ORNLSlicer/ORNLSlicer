#pragma once

#include <QScopedPointer>
#include <QVector>
#include <qcontainerfwd.h>
#include <qhash.h>
#include <qhashfunctions.h>
#include <qlist.h>
#include <qregularexpression.h>
#include <qset.h>
#include <qtmetamacros.h>

#include "gcode/gcode_command.h"
#include "gcode/gcode_meta.h"
#include "geometry/point.h"
#include "parser_base.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {
/*!
 * \class CommonParser
 * \brief This class implements a common parsing configuration for 3D
 * printers. This configuration can be changed and overwritten by its
 * subclasses by overriding the mapping between. command string and
 * associated funciton handler. \note The current commands that this class
 * implements are:
 *          - GCommands:
 *              - G0, G1, G2, G3, G4
 *          - MCommands:
 *              - None
 *          - Line Comment Delimiter:
 *              - ;
 *          - Block Comment Delimiter:
 *              - None
 *
 */
class CommonParser : public ParserBase {
    Q_OBJECT
  public:
    //! \brief Constructor that allows for unit type selection
    //! \param meta Meta to use for units and command styles
    //! \param allowLayerAlter Whether or not the layers can be altered to adhere to the min layer time
    //! \param lines Original lines of the gcode file
    //! \param upperLines Uppercase lines of the original used for ease of parsing/comparison
    CommonParser(GcodeMeta meta, bool allowLayerAlter, QStringList& lines, QStringList& upperLines);

    //! \brief Parse header lines and identify Slicer 1 or ORNLSlicer.
    void parseHeader();

    //! \brief Parse footer lines extracting necessary settings for parsing time,
    //! volume, and visualization.
    //! \return A copy of the settings found in the footer (used for visualization construction)
    QHash<QString, double> parseFooter();

    //! \brief Parse all remaining lines outside of header and footer.
    //! \param layerSkip: Layer interval to skip
    //! \return Copy of parsed gcode commands (used for visualization construction)
    QList<QList<GcodeCommand>> parseLines(int layerSkip);

    // ---- Regular Commands ----

    //! \brief Base configuration that contains all common configuration
    //! commands
    //!        for each machine.
    //!        The current commands that this function implements are:
    //!             - GCommands:
    //!                     - G0, G1, G2, G3, G4
    //!             - MCommands:
    //!                     - None
    //! \note This command clears all previous commands that were
    //! configured. \note This function should be overridden and called from
    //! a subclass to allow
    //!       a single command to enable all common commands and the
    //!       specific implementations that each machine needs.
    virtual void config();

    //! \brief Returns the current X position of the extruder.
    //! \return The number type converted to the internal distance unit.
    NT getXPos() const;

    //! \brief Returns the current X position of the extruder.
    //! \param distance_unit Unit of distance to convert the dextruder X
    //! position to. \return The number type converted to the passed
    //! distance unit.
    NT getXPos(Distance distance_unit) const;

    //! \brief Returns the current Y position of the extruder.
    //! \return The number type converted to the internal distance unit.
    NT getYPos() const;

    //! \brief Returns the current Y position of the extruder.
    //! \param distance_unit Unit of distance to convert the extruder Y
    //! position to. \return The number type converted to the passed
    //! distance unit.
    NT getYPos(Distance distance_unit) const;

    //! \brief Returns the current Z position of the extruder.
    //! \return The number type converted to the internal distance unit.
    NT getZPos() const;

    //! \brief Returns the current Z position of the extruder.
    //! \param distance_unit Unit of distance to convert the extuder Z
    //! position to. \return The number type converted to the passed
    //! distance unit.
    NT getZPos(Distance distance_unit) const;

    //! \brief Returns the current W position of the table..
    //! \return The number type converted to the internal distance unit.
    NT getWPos() const;

    //! \brief Returns the current W position of the table.
    //! \param distance_unit Unit of distance to convert the table position
    //! to. \return The number type converted to the passed distance unit.
    NT getWPos(Distance distance_unit) const;

    //! \brief Returns the current X position for the center of the arc.
    //! \return The number type converted to the internal distance unit.
    NT getArcXPos() const;

    //! \brief Returns the current X position for the center of the arc.
    //! \param distance_unit Unit of distance to convert the X-axis arc
    //! center position to. \return The number type converted to the passed
    //! distance unit.
    NT getArcXPos(Distance distance_unit) const;

    //! \brief Returns the current y position for the center of the arc.
    //! \return The number type converted to the internal distance unit.
    NT getArcYPos() const;

    //! \brief Returns the current y position for the center of the arc.
    //! \param distance_unit Unit of distance to convert the Y-axis arc
    //! center position to. \return The number type converted to the passed
    //! distance unit.
    NT getArcYPos(Distance distance_unit) const;

    //! \brief Returns the current speed of the extruder.
    //! \return The number type converted to the internal distance/time
    //! unit.
    NT getSpeed() const;

    //! \brief Returns the current speed of the extruder.
    //! \param velocity_unit Unit of velocity to convert the extruder speed
    //! to. \return The number type converted to the passed velocity unit.
    NT getSpeed(Velocity velocity_unit) const;

    //! \brief Returns the current angular velocity of the spindle.
    //! \return The number type converted to rev/min.
    NT getSpindleSpeed() const;

    //! \brief Returns the current angular velocity of the spindle.
    //! \param angular_velocity_unit Unit of angular velocity to convert the
    //! spindle speed to. \return The number type converted to the passed
    //! angular velocity unit.
    NT getSpindleSpeed(AngularVelocity angular_velocity_unit) const;

    //! \brief Returns the current acceleration of the extruder.
    //! \return The number type converted to the internal
    //! distance_unit/time_unit/time unit.
    NT getAcceleration() const;

    //! \brief Returns the current acceleration of the extruder.
    //! \param acceleration_unit Unit of acceleration to convert the
    //! acceleration to. \return The number type converted to the passed
    //! acceleration unit.
    NT getAcceleration(Acceleration acceleration_unit) const;

    //! \brief Returns the current sleep time of the extruder.
    //! \return The number type converted to the internal time unit.
    NT getSleepTime() const;

    //! \brief Returns the current sleep time of the extruder.
    //! \param time_unit Unit of time to convert the sleep time to.
    //! \return The number type converted to the defined time unit.
    NT getSleepTime(Time time_unit) const;

    //! \brief Completely restes the internal state of this parser. This
    //! includes all command mappings,
    //!        and position values. This does NOT include resetting units.
    void reset();

    //! \brief Returns the currently calculated times for each layer
    //! \return The time in seconds
    QList<QList<Time>> getLayerTimes();

    //! \brief Returns the currently calculated feedrate modifier for each layer
    //! \return The time in seconds
    QList<double> getLayerFeedRateModifiers();

    //! \brief Returns the current sleep time of the extruder.
    //! \param time_unit Unit of time to convert the sleep time to.
    //! \return The number type converted to the defined time unit.
    QList<Volume> getLayerVolumes();

    //! \brief Returns the total distance for the gcode file
    //! \return total distance in gcode file
    Distance getTotalDistance();

    //! \brief Returns the total printing distance for the gcode file
    //! \return printing distance in gcode file
    Distance getPrintingDistance();

    //! \brief Returns the total travel distance for the gcode file
    //! \return travel distance in gcode file
    Distance getTravelDistance();

    //! \brief Returns whether or not any lines were altered to enforce minimum layer times or expand/contract
    bool getWasModified();

    //! \brief Set cancel flag to stop parsing
    void cancelSlice();

    //! \brief Get lines for the start of each layer (for visualization)
    //! \return List of start lines for each layer
    QList<int> getLayerStartLines();

    //! \brief Get lines to skip visualization for if setting was turned on
    //! \return Set of all lines for which coloring should not apply
    QSet<int> getLayerSkipLines();

  signals:

    //! \brief Send status updates to slice dialog of progress
    //! \param type Current step process type
    //! \param percentCompleted current percentage complete
    void statusUpdate(StatusUpdateStepType type, int percentCompleted);

    //! \brief signal to main window with info
    //! \param parsingInfo Formatted QString with various pieces of info to display in
    //!  main_window's status window.  Info includes: time, volume, weight, and material info.
    void forwardInfoToMainWindow(QString parsingInfo);

  protected:
    //! \brief Returns the current line to the child parser
    //! \return Current line index
    int getCurrentLine();

    //! \brief Updates current end line index.  Used by RPBF_parser child class for command expansion.
    //! \param count Number of lines to add
    void alterCurrentEndLine(int count);

    //! \brief Sets the flag indicating the text was modified.  Triggers gcode_loader to reload text once
    //! processing complete.  Used by RPBF_parser since it modifies text.
    void setModified();

    //! \brief Sets the X position of the extruder.
    //! \param value Value to set the X position to.
    //! \note This value will be set using the defined unit type.
    void setXPos(NT value);

    //! \brief Sets the Y position of the extruder.
    //! \param value Value to set the Y position to.
    //! \note This value will be set using the defined unit type.
    void setYPos(NT value);

    //! \brief Sets the Z position of the extruder.
    //! \param value Value to set the Z position to.
    //! \note This value will be set using the defined unit type.
    void setZPos(NT value);

    //! \brief Sets the W position of the extruder.
    //! \param value Value to set the W position to.
    //! \note This value will be set using the defined unit type.
    void setWPos(NT value);

    //! \brief Sets the center arc position on the X axis.
    //! \param value Value to set the X arc center position to.
    //! \note This value will be set using the defined unit type.
    void setArcXPos(NT value);

    //! \brief Sets the center arc position on the Y axis.
    //! \param value Value to set the Y arc center position to.
    //! \note This value will be set using the defined unit type.
    void setArcYPos(NT value);

    //! \brief Sets the center arc position on the Z axis.
    //! \param value Value to set the Z arc center position to.
    //! \note This value will be set using the defined unit type.
    void setArcZPos(NT value);

    //! \brief Sets the center arc position using radius.
    //! \param value Value to set the R arc center position to.
    //! \note This value will be set using the defined unit type.
    void setArcRPos(NT value);

    //! \brief Sets the speed of the extruder.
    //! \param value Value to set the speed to.
    //! \note This value will be set using the defined unit type.
    void setSpeed(NT value);

    //! \brief Sets the spindle speed of the extruder.
    //! \param value Value to set the spindle speed to.
    //! \note This value will be set using the defined unit type.
    void setSpindleSpeed(NT value);

    //! \brief Sets the acceleration of the extruder.
    //! \param value Value to set the accleration to.
    //! \note This value will be set using the defined unit types.
    void setAcceleration(NT value);

    //! \brief Sets the sleep time of the extruder.
    //! \param value Value to set the sleep time to.
    //! \note This value will be set using the defined unit type.
    void setSleepTime(NT value);

    //! \brief Function handler for the 'G0' Gcode command for rapid linear
    //! movement. This function handler
    //!        accepts the following parameters:
    //!             - X: Changes the X position of the extruder.
    //!             - Y: Changes the Y position of the extruder.
    //!             - Z: Changes the Z position of the extruder.
    //!        There must exist one of the X, Y, or Z command.
    //! \param params List of parameters that have been split up to be
    //! parsed. \throws IllegalParameterException This occurs during either
    //! a conversion error, a parameter missing a value,
    //!                                   a duplicate parameter, if a
    //!                                   required parameter is not passed,
    //!                                   or an illegal parameter passed.
    virtual void G0Handler(QVector<QString> params);

    //! \brief Function handler for the 'G1' Gcode command for linear
    //! movement. This function handler
    //!        accepts the following parameters:
    //!             - X: Changes the X position of the extruder.
    //!             - Y: Changes the Y position of the extruder.
    //!             - Z: Changes the Z position of the extruder.
    //!             - F: Changes the flow rate of the extruder.
    //!        There must exist one of the X, Y, or Z command. The F command
    //!        is optional.
    //! \param params List of parameters that have been split up to be
    //! parsed. \throws IllegalParameterException This occurs during either
    //! a conversion error, a parameter missing a value,
    //!                                   a duplicate parameter, if a
    //!                                   required parameter is not passed,
    //!                                   or an illegal parameter passed.
    virtual void G1Handler(QVector<QString> params);

    void G1HandlerHelper(QVector<QString> params, QVector<QString> optionalParams);

    //! \brief Function handler for the 'G2' Gcode command for clockwise arc
    //! movement. This function handler
    //!        accepts the following parameters:
    //!             - X: Changes the X position of the extruder.
    //!             - Y: Changes the Y position of the extruder.
    //!             - I: Changes the X arc center position based off the
    //!             previous X position.
    //!             - I: Changes the Y arc center position based off the
    //!             previous Y position.
    //!             - F: Changes the flow rate of the extruder.
    //!        All X, Y, I, and J commands must exist or an exception is
    //!        thrown. The F command is optional.
    //! \param params List of parameters that have been split up to be
    //! parsed. \throws IllegalParameterException This occurs during either
    //! a conversion error, a parameter missing a value,
    //!                                   a duplicate parameter, if a
    //!                                   required parameter is not passed,
    //!                                   or an illegal parameter passed.
    virtual void G2Handler(QVector<QString> params);

    //! \brief Function handler for the 'G3' Gcode command for counter
    //! clockwise arc movement. This function handler
    //!        accepts the following parameters:
    //!             - X: Changes the X position of the extruder.
    //!             - Y: Changes the Y position of the extruder.
    //!             - I: Changes the X arc center position based off the
    //!             previous X position.
    //!             - I: Changes the Y arc center position based off the
    //!             previous Y position.
    //!             - F: Changes the flow rate of the extruder.
    //!        All X, Y, I, and J commands must exist or an exception is
    //!        thrown. The F command is optional.
    //! \param params List of parameters that have been split up to be
    //! parsed. \throws IllegalParameterException This occurs during either
    //! a conversion error, a parameter missing a value,
    //!                                   a duplicate parameter, if a
    //!                                   required parameter is not passed,
    //!                                   or an illegal parameter passed.
    virtual void G3Handler(QVector<QString> params);

    //! \brief Function handler for the 'G4' command for setting the sleep
    //! time of the extruder. This function handler
    //!        accepts the following parameters:
    //!             - L: Changes the amount of time to sleep.
    //!        The L command must exist or an exception is thrown.
    //! \param params List of parameters that have been split up to be
    //! parsed. \throws IllegalParameterException This occurs during either
    //! a conversion error, a parameter missing a value,
    //!                                   a duplicate parameter, if a
    //!                                   required parameter is not passed,
    //!                                   or an illegal parameter passed.
    virtual void G4Handler(QVector<QString> params);

    //! \brief Function handler for the 'G5' Gcode command for b-splines
    //!        accepts the following parameters:
    //!             - X: Changes the X position of the extruder.
    //!             - Y: Changes the Y position of the extruder.
    //!             - I: Changes the X arc center position based off the previous X position.
    //!             - J: Changes the Y control point position based off the previous Y position.
    //!             - P: Changes the X control point position based off the end X position.
    //!             - Q: Changes the Y control point position based off the end Y position.
    //!             - F: Changes the flow rate of the extruder.
    //! \param params List of parameters that have been split up to be
    //! parsed. \throws IllegalParameterException This occurs during either
    //! a conversion error, a parameter missing a value,
    //!                                   a duplicate parameter, if a
    //!                                   required parameter is not passed,
    //!                                   or an illegal parameter passed.
    virtual void G5Handler(QVector<QString> params);

    //! \brief Function handler for the 'M3' command for setting the sleep
    //! time of the extruder. This function handler
    //!        accepts the following parameters:
    //!             - L: Changes the amount of time to sleep.
    //!        The L command must exist or an exception is thrown.
    //! \param params List of parameters that have been split up to be
    //! parsed. \throws IllegalParameterException This occurs during either
    //! a conversion error, a parameter missing a value,
    //!                                   a duplicate parameter, if a
    //!                                   required parameter is not passed,
    //!                                   or an illegal parameter passed.
    virtual void M3Handler(QVector<QString> params);

    //! \brief Function handler for the 'G4' command for setting the sleep
    //! time of the extruder. This function handler
    //!        accepts the following parameters:
    //!             - L: Changes the amount of time to sleep.
    //!        The L command must exist or an exception is thrown.
    //! \param params List of parameters that have been split up to be
    //! parsed. \throws IllegalParameterException This occurs during either
    //! a conversion error, a parameter missing a value,
    //!                                   a duplicate parameter, if a
    //!                                   required parameter is not passed,
    //!                                   or an illegal parameter passed.
    virtual void M5Handler(QVector<QString> params);

    //! \brief Modify layer time by inserting dwell at the end of layer
    //! \param dwellTime Amount of time to dwell
    virtual void AddDwell(double dwellTime);

    //! \brief Gets min and max modifiers for adjusting feedrate
    //! \param minModifier minimum feedrate modifying factor
    //! \param maxModifier maximum feedrate modifying factor
    virtual void getMinMaxModifier(double& minModifier, double& maxModifier);

    //! \brief Modify layer time by adjusting feedrate for motions for that layer
    //! \param modifier Amount to modify feedrate from 0 to 1
    virtual void AdjustFeedrate(double modifier);

    //! \brief Helper function to remove rotation components: RX, RY, RZ, PA, and PC
    //!         These rotation commands occur for the ORNL syntax and do not contribute to the visualiztion, they don't
    //!         need parsed. This function creates a new line without those rotations for the purpose of
    //!         parsing/visualizing.
    //! \param currentLine: the line of g-code to be modified
    QString removeRotations(QString currentLine);

    //! \brief Helper function that throws an IllegalParameterException when
    //! multiple of the same parameter are encountered. \param parameter
    //! Duplicate Parameter id. \throws IllegalParameterException
    void throwMultipleParameterException(char parameter);

    //! \brief Helper function that throws an IllegalParameterException when
    //! a float conversion occurs. \throws IllegalParameterException
    void throwFloatConversionErrorException();

    //! \brief Helper function that throws an IllegalParameterException when
    //! an integer conversion occurs. \throws IllegalParameterException
    void throwIntegerConversionErrorException();

    //! \brief number of extruders/nozzles; parsed from settings
    int m_num_extruders = 0;

    //! \brief maintains where each extruder is on or off
    QVector<bool> m_extruders_on;

    //! \brief maintains which extruders are active, ie tracks tool changes
    QVector<bool> m_extruders_active;

    //! \brief offsets corresponding to each extruder, parsed from
    //!        settings at end of gcode file; used to visualize segments in
    //!        correct locations
    QVector<Point> m_extruder_offsets;

    bool m_dynamic_spindle_control; // true if on, false if off.
    bool m_park;                    // true if parking, false if not.

    // Purging variables
    bool m_return_to_prev_location;
    Time m_purge_time, m_wait_to_wipe_time, m_wait_time_to_start_purge;

    //! \brief Track where e is in absolute or relative mode.
    bool m_e_absolute;

    //! \brief Number of lines of code inserted by AddDwell
    int m_insertions;

    //! \brief Constant used for time adjustment and child parsers
    QChar m_space;

  private:
    //! \brief calculates distance for the current motion segment
    Distance getCurrentGXDistance();

    //! \brief After parsing footer, check that all necessary parameters were found.
    //! If not found, set them appropriately and assign local variables.
    void checkAndSetNecessarySettings();

    //! \brief Preallocate memory for processed gcode commands.  A copy is later returned
    //! for use in visualization construction.
    //! \param layerSkip: Layer interval to skip
    void preallocateVisualCommands(int layerSkip);

    //! \brief sets m_extruders_on according to which extruders are active
    void turnOnActiveExtruders();

    //! \brief sets m_extruders_on according to which extruders are active
    void turnOffActiveExtruders();

    // STATE VARIABLES

    //! \brief Numerous state variables.  Used to track current/previous values of various
    //! pieces of state.  This is necessary to produce time and volume estimates.
    int m_current_nozzle;

    Distance m_current_arc_center_x;
    Distance m_current_arc_center_y;
    Distance m_current_arc_center_z;
    Distance m_current_arc_radius;

    //! \brief Control points for splines
    Distance m_current_spline_control_a_x;
    Distance m_current_spline_control_a_y;
    Distance m_current_spline_control_b_x;
    Distance m_current_spline_control_b_y;

    AngularVelocity m_current_spindle_speed;

    double m_current_extruders_speed;

    Time m_sleep_time;

    // List of all calculated times/volumes for layers as well as total distance
    QList<QList<Time>> m_layer_times; // 2D list: row=layer #, col=extruder#

    //! \brief layer "G1 F" lines feedrate modifier
    QList<double> m_layer_FR_modifiers;

    //! \brief layer time from "G1 F" lines
    QList<Time> m_layer_G1F_times;
    QList<Volume> m_layer_volumes;

    // key information from the footer
    PrintMaterial printing_material;
    Density other_density;

    //! \brief Currently loaded parameters.  These come from the
    //! meta struct provided to the constructor.
    Distance m_distance_unit;
    Time m_time_unit;
    Angle m_angle_unit;
    Mass m_mass_unit;
    Velocity m_velocity_unit;
    Acceleration m_acceleration_unit;
    AngularVelocity m_angular_velocity_unit;
    QString m_layer_delimiter;
    QString m_layer_count_delimiter;

    //! \brief bool whether or not to allow alter layer times
    bool m_allow_layer_alter;
    //! \brief copy of lines and upper case version of lines loaded from file
    QStringList &m_lines, &m_upper_lines;
    //! \brief current line to parse, iterates from front of list
    int m_current_line;
    //! \brief current last line to parse, iterates from back of list
    int m_current_end_line;
    //! \brief settings loaded from footer
    QHash<QString, double> m_file_settings;
    //! \brief gcode commands that result in motion (to be provided for visualization)
    QList<QList<GcodeCommand>> m_motion_commands;
    //! \brief current layer
    int m_current_layer;

    //! \brief needed variables copied from constants
    QHash<QString, QString> m_necessary_variables_copy;
    //! \brief check to see if minimum layer time needs enforced
    ForceMinimumLayerTime m_min_layer_time_choice;
    //! \brief minimal layer time to enforce if turned on
    Time m_min_layer_time_allowed;
    //! \brief maximal layer time to enforce if turned on
    Time m_max_layer_time_allowed;
    //! \brief whether or not any layers were adjusted for minimum layer time or expand/contract for CLI types
    bool m_was_modified;
    //! \brief line of start of last layer
    int m_last_layer_line_start;

    //! \brief predefined constants for constructing new lines when adjusting for minimal layer time
    QString m_g4_prefix;
    QString m_g4_comment;
    QRegularExpression m_f_param_and_value;
    QRegularExpression m_q_param_and_value;
    QRegularExpression m_s_param_and_value;
    QChar m_f_parameter;
    QChar m_q_parameter;
    QChar m_s_parameter;

    //! \brief Flag to indicate cancelling parsing
    bool m_should_cancel;

    //! \brief Flag to indicate G1 line contains F command
    bool m_with_F_value;

    //! \brief Flag to indicate Z value should be multiplied by negative one, used for MVP syntax
    bool m_negate_z_value;

    //! \brief Line at which each layer start (used for visualization)
    QList<int> m_layer_start_lines;

    //! \brief Lines in a layer that should be skipped for visualization if setting is enabled
    QSet<int> m_layer_skip_lines;

}; // class CommonParser

} // namespace ORNL
