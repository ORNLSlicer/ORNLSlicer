#pragma once

#include <QMap>
#include <QObject>
#include <QString>
#include <qcontainerfwd.h>
#include <qtmetamacros.h>

#include "geometry/point.h"

namespace ORNL {
//! \brief A plain old data structure class for a GCode command.
class GcodeCommand : public QObject {
    Q_OBJECT
  public:
    GcodeCommand();

    GcodeCommand(const GcodeCommand& other);

    //! \brief Gets the line number from the Gcode command.
    //! \return The Linenumber of the Gcode command.
    const int& getLineNumber() const;

    //! \brief Gets the command character from the Gcode command.
    //! \return The command character of the Gcode command.
    const char& getCommand() const;

    //! \brief Gets the command ID number from the Gcode command.
    //! \return The command ID number of the Gcode command.
    const int& getCommandID() const;

    //! \brief Gets bool for whether or not command is a motion command.
    //! \return The command motion boolean of the Gcode command.
    const bool& getCommandIsMotion() const;

    //! \brief Gets bool for whether or not command is the end of a layer.
    //! \return The end of layer boolean of the Gcode command.
    const bool& getCommandIsEndOfLayer() const;

    void clearCommand();
    void clearCommandID();

    //! \brief Gets the parameters mapping associated with that Gcode
    //! command. \return The parameters mapping of the Gcode command.
    const QMap<char, double>& getParameters() const;

    const QMap<char, double>& getOptionalParameters() const;

    //! \brief Gets the comment associated with that Gcode command.
    //! \return The Gcode command's comment
    const QString& getComment() const;

    // const QString &getCommandLine() const;
    // void setCommandLine(QString line);

    // Setters
    //! \brief Sets the line number.
    //! \param line_number Line number of the command.
    void setLineNumber(const int line_number);

    //! \brief Sets the command character.
    //! \param command Command character of the Gcode command.
    void setCommand(const char command);

    //! \brief Sets the command ID number of the Gcode command.
    //! \param commandID ID number of the GCode command.
    void setCommandID(const int commandID);

    //! \brief Sets the comment of the Gcode command.
    //! \param comment Comment string assoicated with the Gcode Command.
    void setComment(const QString& comment);

    //! \brief Sets the boolean motion command of the Gcode command.
    //! \param comment Boolean value whether or not command has motion.
    void setMotionCommand(bool isMotionCommand);

    //! \brief Sets the boolean end of layer of the Gcode command.
    //! \param comment Boolean value whether or not command start a new layer.
    void setEndOfLayer(bool isEndOfLayer);

    //! \brief Adds a parameter to the parameter mapping.
    //! \param param_key Character of the Gcode command parameter.
    //! \param parma_value Float value that conincides with the Gcode
    //! command key.
    void addParameter(const char param_key, const double param_value);

    //! \brief Adds a parameter to the optional parameter mapping.  Currently meant to hold
    //! the starting point for certain syntaxes that require both a start/end point to write
    //! commands rather than simply the next end point.
    //! \param param_key Character of the Gcode command parameter.
    //! \param parma_value Float value that conincides with the Gcode
    //! command key.
    void addOptionalParameter(const char param_key, const double param_value);

    //! \brief Removes a parameter to the parameter mapping.
    //! \param param_key Character of the Gcode command parameter.
    //! \return true if the key was sucessfully removed. false if the key
    //! was not in the mapping.
    bool removeParameter(const char param_key);

    //! \brief Clears all parameters from the mapping.
    void clearParameters();

    //! \brief Clears the comment string.
    void clearComment();

    //! \brief sets what extruders are on for this command
    //! \param extruders_on a list of the status of the extruders
    void setExtrudersOn(QVector<bool>& extruders_on);

    //! \brief gets a list of the extruder status for this command
    //! \return a list of booleans for the status
    const QVector<bool>& getExtrudersOn() const;

    //! \brief sets the extruder offsets for each nozzle
    //! \param extruder_offsets the distance from the first extruder
    void setExtruderOffsets(QVector<Point>& extruder_offsets);

    //! \brief gets the distances from the first extruder to the others
    //! \return a list of offsets
    const QVector<Point>& getExtruderOffsets() const;

    //! \brief sets extruder speed for this command
    //! \param extruders_speed
    void setExtrudersSpeed(double extruders_speed);

    //! \brief gets extruder speed for this command
    //! \return double value
    const double& getExtrudersSpeed() const;

    //! \brief Equality operator to check if two gcode commands are equal.
    //! \note Equality is determined for the mapping of both commands if the
    //! commands have the
    //!       same mappings, not the same order.
    bool operator==(const GcodeCommand& r);

    //! \brief Inequality operator to check for inequality.
    bool operator!=(const GcodeCommand& r);

    GcodeCommand& operator=(const GcodeCommand& other);

  private:
    int m_line_number;
    char m_command;
    int m_command_id;
    QMap<char, double> m_parameters;
    QMap<char, double> m_optional_parameters;
    QVector<bool> m_extruders_on;
    QVector<Point> m_extruder_offsets;
    double m_extruders_speed;
    QString m_comment;
    QString m_commandLine;
    bool m_is_motion_command;
    bool m_is_end_of_layer;
};
} // namespace ORNL
