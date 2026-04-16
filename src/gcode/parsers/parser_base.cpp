#include "gcode/parsers/parser_base.h"

#include <algorithm>
#include <functional>
#include <string>

#include <QDebug>
#include <QRegularExpressionMatch>
#include <QString>
#include <QStringList>
#include <QTextStream>
#include <qcontainerfwd.h>
#include <qhash.h>
#include <qnamespace.h>

#include "exceptions/exceptions.h"
#include "gcode/gcode_command.h"

namespace ORNL {
ParserBase::ParserBase() {
    m_block_split_delimiter = QRegularExpression("[\\(*\\)*\\,*\\s]|/");
    m_leading_colon = QChar(':');
    m_beginning_layer_matcher.setPattern("BEGINNING LAYER");
    m_layer_pattern = QRegularExpression("W*(\\d+)W*");
}

GcodeCommand ParserBase::parseCommand(QString command_string, int line_number) {
    // Clears the current command
    m_current_command_string.clear();
    m_current_gcode_command.clearParameters();
    m_current_gcode_command.clearComment();
    m_current_gcode_command.clearCommand();
    m_current_gcode_command.clearCommandID();
    m_current_gcode_command.setEndOfLayer(false);
    m_current_gcode_command.setMotionCommand(false);
    m_current_gcode_command.setLineNumber(line_number);

    // remove comments and return QString
    // need actual QString in order to split with regex
    extractComments(command_string);

    // trimming is handled as part of regex
    QVector<QString> command_string_split = command_string.split(m_block_split_delimiter, Qt::SkipEmptyParts);

    // just comments so return
    if (command_string_split.isEmpty()) {
        if (m_beginning_layer_matcher.indexIn(m_current_gcode_command.getComment()) != -1) {
            auto match = m_layer_pattern.match(m_current_gcode_command.getComment());
            if (match.hasMatch() && match.captured(0).toInt() > 0) {
                m_current_gcode_command.setEndOfLayer(true);
            }
        }

        return m_current_gcode_command;
    }

    // get command as std::string to easily collapse extra 0's, then convert to QString
    // for later matching.  Must have QString to match in hash so no need to keep ref.
    // However, all other parameters can remain as ref.
    std::string stdCommand = command_string_split[0].toStdString();
    int firstNonZero = std::min(stdCommand.find_first_not_of('0', 1), stdCommand.size() - 1);
    if (firstNonZero != 1)
        stdCommand.erase(1, firstNonZero);
    QString command = QString::fromStdString(stdCommand);

    QHash<QString, std::function<void(QVector<QString>)>>::iterator temp_iter;
    // First element should always be the command
    if ((temp_iter = m_command_mapping.find(command)) != m_command_mapping.end()) {
        // If the command is found set it as the new currrent command.
        setCurrentCommand(command);
        command_string_split.removeFirst();

        // If command is found send the rest of the split string to its
        // function handler
        temp_iter.value()(command_string_split);
    }
    return m_current_gcode_command;
}

void ParserBase::resetInternalState() {
    m_command_mapping.clear();
    m_line_comment.clear();
    m_current_command_string.clear();
    m_current_gcode_command.clearCommand();
    m_current_gcode_command.clearCommandID();
    m_current_gcode_command.clearComment();
    m_current_gcode_command.clearParameters();
}

void ParserBase::addCommandMapping(QString command_string, std::function<void(QVector<QString>)> function_handle) {
    if (m_command_mapping.contains(command_string))
        m_command_mapping.remove(command_string);

    m_command_mapping.insert(command_string, function_handle);
}

void ParserBase::setBlockCommentDelimiters(const QString beginning_delimiter, const QString ending_delimiter) {
    m_block_comment_starting_delimiter = beginning_delimiter;
    m_block_comment_ending_delimiter = ending_delimiter;
    m_block_comment_starting_delimiter_matcher.setPattern(beginning_delimiter);
    m_block_comment_ending_delimiter_matcher.setPattern(ending_delimiter);
}

bool ParserBase::startsWithDelimiter(QString& str) {
    if (m_block_comment_starting_delimiter_matcher.indexIn(str) == 0)
        return true;

    return false;
}

void ParserBase::setLineCommentString(const QString line_comment) { m_line_comment = line_comment; }

void ParserBase::extractComments(QString& command) {
    int start_index = m_block_comment_starting_delimiter_matcher.indexIn(command);
    if (start_index != -1) {
        int end_index = -1;
        if (m_block_comment_ending_delimiter != QString()) {
            end_index = m_block_comment_ending_delimiter_matcher.indexIn(
                command, start_index + m_block_comment_starting_delimiter.size());
            // delimiter defined but not found so bad format
            if (end_index == -1) {
                QString exceptionString;
                QTextStream(&exceptionString) << "Comment not closed within GCode file, on line "
                                              << m_current_gcode_command.getLineNumber() << "." << Qt::endl
                                              << "With GCode command string: " << getCurrentCommandString();
                throw IllegalParameterException(exceptionString);
            }
            else {
                end_index = end_index - start_index - m_block_comment_ending_delimiter.size();
            }
        }

        QString comment(command.mid(start_index + m_block_comment_starting_delimiter.size(), end_index));
        m_current_gcode_command.setComment(comment.trimmed());

        if (start_index > 0)
            command = command.left(start_index - 1);
        else
            command = QString();
    }
}

QString ParserBase::parseComment(QString& line) {
    int start_index =
        m_block_comment_starting_delimiter_matcher.indexIn(line) + m_block_comment_starting_delimiter.size();
    int end_index = -1;
    if (!m_block_comment_ending_delimiter.isEmpty()) {
        end_index = m_block_comment_ending_delimiter_matcher.indexIn(line, m_block_comment_starting_delimiter.size());
        // delimiter defined but not found so bad format
        if (end_index == -1) {
            QString exceptionString;
            QTextStream(&exceptionString) << "Comment not closed within GCode file, on line "
                                          << m_current_gcode_command.getLineNumber() << "." << Qt::endl
                                          << "With GCode command string: " << getCurrentCommandString();
            throw IllegalParameterException(exceptionString);
        }
        else {
            end_index = end_index - start_index;
        }
    }
    else
        end_index = line.length() - 1;

    return line.mid(start_index, end_index);
}

void ParserBase::setCurrentCommand(QString command) {
    bool no_error;
    m_current_command_string = command;
    m_current_gcode_command.setCommand(command.at(0).toLatin1());
    m_current_gcode_command.setCommandID(command.right(command.size() - 1).toInt(&no_error));
    if (!no_error) {
        QString exceptionString;
        QTextStream(&exceptionString) << "Error with numerical conversion for GCode command on GCode "
                                         "line "
                                      << m_current_gcode_command.getLineNumber() << "." << Qt::endl
                                      << "With GCode command string: " << getCurrentCommandString();
        //            throw IllegalArgumentException(exceptionString);
    }
}

const QString& ParserBase::getCurrentCommandString() const { return m_current_command_string; }

void ParserBase::setLineNumber(int linenumber) { m_current_gcode_command.setLineNumber(linenumber); }

QString ParserBase::getCommentStartDelimiter() { return m_block_comment_starting_delimiter; }

QString ParserBase::getCommentEndDelimiter() { return m_block_comment_ending_delimiter; }
} // namespace ORNL
