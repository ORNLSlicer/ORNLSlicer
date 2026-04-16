#pragma once

#include <functional>

#include <qcontainerfwd.h>
#include <qhashfunctions.h>
#include <qobject.h>
#include <qregularexpression.h>
#include <qstringmatcher.h>
#include <qtmetamacros.h>

#include "gcode/gcode_command.h"

namespace ORNL {
/*!
 * \class ParserBase
 * \brief This class implements the core functionality for parsing
 * Gcodecommands that are character delimted with parameters , comments, and
 * comment commands.
 */
class ParserBase : public QObject {
    Q_OBJECT
  public:
    //! \brief Parses the command that is passed from the command string,
    //!        and sends the command to its corresponding handler.
    //! \param command_string Gcode command string
    //! \param line_number line number from file
    //! \return The command split into
    //! parameters, comments, command, and commandID.
    virtual GcodeCommand parseCommand(QString command_string, int line_number);

    //! \brief Pure virtual function that configures the parsers command
    //! strings, handlers pairs, for commands
    //!        , control commands, and comment delimiters.
    virtual void config() = 0;

    //! \brief Gets the current command of the parser.
    //! \return Last used command string that was parsed.
    const QString& getCurrentCommandString() const;

    //! \brief Determines if lines starts with delimiter character
    //! Used for parsing footer when full gcodecommand is not warranted
    //! \return bool true if starts with delimiter
    bool startsWithDelimiter(QString& str);

  protected:
    //! \brief Default Constructor
    ParserBase();

    //! \brief Adds a command to the command mapping hash with its
    //! correspoding handler. These commands
    //!        will be passed thier parameters in a QStringList separated by
    //!        the delimiter, by defualt the space character.
    //! \param command_string The Gcode command string.
    //! \param function_handle Function that accepts one argument and will
    //! handle manipulating the internal data
    //!                        structures when passed.
    void addCommandMapping(QString command_string, std::function<void(QVector<QString>)> function_handle);

    //! \brief Sets the comment delimters for use when extracting comments
    //! from the line of text.
    //!        If a pair of these delimiters is found within a command. Any
    //!        characters between them
    //!        will be ignored from the actual command. But can have special
    //!        comment parsing later.
    //! \param beginning_delimter String to indicate the begining of a block
    //! style comment.
    //! \param ending_delimter String to indicate the end of
    //! a block style comment.
    void setBlockCommentDelimiters(const QString beginning_delimiter, const QString ending_delimiter);

    //! \brief Sets the line comment string that will be used to signify
    //! that anything
    //!        after this string and before a newline, should be set as a
    //!        comment and not parsed. Special comment parsing can be
    //!        performed afterwards.
    //! \param line_comment String to indicate the begining of a line style
    //! comment.
    void setLineCommentString(const QString line_comment);

    //! \brief Parses and removes any comments from the command string.
    //! \param command A GCode command line.
    //! \note This function does modify the string passed.
    void extractComments(QString& command);

    //! \brief Returns the comment in a string rather than inserting it into a gcode
    //! command.  Useful for footer processing.
    //! \param comment A Comment within a Gcode command.
    QString parseComment(QString& line);

    //! \brief Sets the current command of the parser to the current string.
    //! \param command Sets the current command in use.
    void setCurrentCommand(QString command);

    //! \brief Sets the line number for the current Gcode command
    //! \param linenumber Linenumber associated with the GCode command.
    void setLineNumber(int linenumber);

    //! \brief Gets delimiter to begin comment
    QString getCommentStartDelimiter();

    //! \brief Gets delimiter to end comment
    QString getCommentEndDelimiter();

    void resetInternalState();

    // TODO: Need to figure out a way to make this private and use accessor
    // methods.
    GcodeCommand m_current_gcode_command;

    //! \brief A regular expression to find the layer number in a comment
    QRegularExpression m_layer_pattern;

    //! \brief Maps a GCode command to a function handlerm
    //! \brief This function throws a multiple parameter exception.
    QHash<QString, std::function<void(QVector<QString>)>>
        m_command_mapping; //!< Mappings of GCode command strings
                           //!< to function handlers which take parameters

  private:
    //! \brief Current line comment
    QString m_line_comment;
    //! \brief Current starting delimiter
    QString m_block_comment_starting_delimiter;
    //! \brief Current ending delimiter
    QString m_block_comment_ending_delimiter;
    //! \brief Current command string
    QString m_current_command_string;
    //! \brief Current starting delimiter matcher for quicker string comparison
    QStringMatcher m_block_comment_starting_delimiter_matcher;
    //! \brief Current ending delimiter matcher for quicker string comparison
    QStringMatcher m_block_comment_ending_delimiter_matcher;
    //! \brief Delimiter for splitting all commands from all syntaxes
    QRegularExpression m_block_split_delimiter;
    //! \brief Char for leading colon collapse (faster execution)
    QChar m_leading_colon;
    //! \brief Matcher for identifying new layers for quicker string comparison
    QStringMatcher m_beginning_layer_matcher;

}; // class ParserBase
} // namespace ORNL
