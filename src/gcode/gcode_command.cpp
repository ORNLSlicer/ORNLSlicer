#include "gcode/gcode_command.h"

#include <QMap>
#include <QString>
#include <qcontainerfwd.h>

#include "geometry/point.h"

namespace ORNL {
GcodeCommand::GcodeCommand()
    : m_line_number(-1), m_command(0), m_command_id(-1), m_is_motion_command(false), m_is_end_of_layer(false),
      m_deposition_active(false), m_extruder_speed(0) {}

GcodeCommand::GcodeCommand(const GcodeCommand& other)
    : m_line_number(other.m_line_number), m_command(other.m_command), m_command_id(other.m_command_id),
      m_parameters(other.m_parameters), m_optional_parameters(other.m_optional_parameters), m_comment(other.m_comment),
      m_is_motion_command(other.m_is_motion_command), m_is_end_of_layer(other.m_is_end_of_layer),
      m_deposition_active(other.m_deposition_active), m_extruder_speed(other.m_extruder_speed) {}

//! \brief Retrieves the line number of the command
const int& GcodeCommand::getLineNumber() const { return m_line_number; }

//! \brief Retrieves the Command
const char& GcodeCommand::getCommand() const { return m_command; }

//! \brief Retrieves the Command ID
const int& GcodeCommand::getCommandID() const { return m_command_id; }

//! \brief Retrieves the Command motion boolean
const bool& GcodeCommand::getCommandIsMotion() const { return m_is_motion_command; }

//! \brief Retrieves the Command motion boolean
const bool& GcodeCommand::getCommandIsEndOfLayer() const { return m_is_end_of_layer; }

//! \brief Retrieves the Command
void GcodeCommand::clearCommand() { m_command = 0; }

//! \brief Retrieves the Command ID
void GcodeCommand::clearCommandID() { m_command_id = -1; }

//! \brief Retrieves all parameters
const QMap<char, double>& GcodeCommand::getParameters() const { return m_parameters; }

//! \brief Retrieves optional parameters
const QMap<char, double>& GcodeCommand::getOptionalParameters() const { return m_optional_parameters; }

//! \brief Retrieves the Comment
const QString& GcodeCommand::getComment() const { return m_comment; }

void GcodeCommand::setLineNumber(const int line_number) { m_line_number = line_number; }

void GcodeCommand::setCommand(const char command) { m_command = command; }

void GcodeCommand::setCommandID(const int commandID) { m_command_id = commandID; }

void GcodeCommand::setComment(const QString& comment) { m_comment = comment; }

void GcodeCommand::setMotionCommand(bool isMotionCommand) { m_is_motion_command = isMotionCommand; }

void GcodeCommand::setEndOfLayer(bool isEndOfLayer) { m_is_end_of_layer = isEndOfLayer; }

void GcodeCommand::addParameter(const char param_key, const double param_value) {
    m_parameters.insert(param_key, param_value);
}

void GcodeCommand::addOptionalParameter(const char param_key, const double param_value) {
    m_optional_parameters.insert(param_key, param_value);
}

bool GcodeCommand::removeParameter(const char param_key) { return m_parameters.remove(param_key) > 0; }

void GcodeCommand::clearParameters() {
    m_parameters.clear();
    m_optional_parameters.clear();
}

void GcodeCommand::clearComment() { m_comment.clear(); }

bool GcodeCommand::operator==(const GcodeCommand& r) {
    bool ret = getLineNumber() == r.getLineNumber() && getCommand() == r.getCommand() &&
               getCommandID() == r.getCommandID() && getComment() == r.getComment();

    QMap<char, double>::const_iterator temp;
    for (QMap<char, double>::const_iterator i = m_parameters.cbegin(); i != m_parameters.cend(); ++i) {
        ret &= (temp = r.m_parameters.find(i.key())) != r.m_parameters.end();
        ret &= temp.value() == i.value();
    }

    return ret;
}

bool GcodeCommand::operator!=(const GcodeCommand& r) { return !(*this == r); }

GcodeCommand& GcodeCommand::operator=(const GcodeCommand& other) {
    m_line_number = other.m_line_number;
    m_command = other.m_command;
    m_command_id = other.m_command_id;
    m_parameters = other.m_parameters;
    m_optional_parameters = other.m_optional_parameters;
    m_comment = other.m_comment;
    m_is_motion_command = other.m_is_motion_command;
    m_is_end_of_layer = other.m_is_end_of_layer;
    m_deposition_active = other.m_deposition_active;
    m_extruder_speed = other.m_extruder_speed;
    return *this;
}

void GcodeCommand::setDepositionActive(bool deposition_active) { m_deposition_active = deposition_active; }

bool GcodeCommand::getDepositionActive() const { return m_deposition_active; }

void GcodeCommand::setExtruderSpeed(double extruder_speed) { m_extruder_speed = extruder_speed; }

const double& GcodeCommand::getExtruderSpeed() const { return m_extruder_speed; }

} // namespace ORNL
