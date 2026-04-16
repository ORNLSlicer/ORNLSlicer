#pragma once

#include <exception>
#include <string>

#include <QString>

#define NEW_EX(name)                                                                                                   \
    class name : public ORNL::ExceptionBase {                                                                          \
      public:                                                                                                          \
        name(QString message = "") : ExceptionBase(#name ": " + message) {}                                            \
    };

namespace ORNL {
class ExceptionBase : public std::exception {
  public:
    ExceptionBase(QString message) : m_message(message.toStdString()) {}

    const char* what() const throw() { return m_message.c_str(); }

  protected:
    std::string m_message;
};

NEW_EX(IllegalArgumentException)

NEW_EX(IllegalParameterException)

NEW_EX(IncorrectPathSegment)

// TODO are both of these needed still?
NEW_EX(InvalidParseException)
NEW_EX(InvalidParserException)

NEW_EX(IOException)

NEW_EX(SettingValueException)

NEW_EX(JsonLoadException)

NEW_EX(UnknownRegionTypeException)

NEW_EX(ParserNotSetException)

NEW_EX(ZeroLayerHeightException)

NEW_EX(IncorrectPathSegmentType)

NEW_EX(UnknownUnitException)

NEW_EX(BadLayerRange)
} // namespace ORNL
