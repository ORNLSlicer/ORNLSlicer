#pragma once

#include <qcontainerfwd.h>
#include <qhashfunctions.h>

#include "common_parser.h"
#include "gcode/gcode_meta.h"

namespace ORNL {
/*!
 * \class CincinnatiParser
 * \brief This class implements the GCode parsing configuration for the
 * Cincinnati 3D printer(s). \note The current commands that this class
 * implements are:
 *          - M Commands:
 *              - M0, M1, M3, M5, M10, M11, M12, M13, M14, M15, M16, M30,
 * M60, M61, M64, M65, M66, M68, M69
 *          - Tool Change Commands:
 *              - All T commands are checked for syntax errors but otherwise
 * do nothing.
 *          - Block Comment Delimiter:
 *              - ()
 *      The commands inherited from the CommonParser superclass are:
 *          - G Commands:
 *              - G0, G1, G2, G3, G4
 *          - Line Comment Delimiter:
 *              - ; \note The ';' comment delimiter is overriden and made
 * invalid in for the parser.
 *
 */
class CincinnatiParser : public CommonParser {
  public:
    //! \brief Standard constructor that specifies meta type and whether or not
    //! to alter layers for minimal layer time
    //! \param meta GcodeMeta struct that includes information about units and
    //! delimiters
    //! \param allowLayerAlter bool to determinw whether or not to alter layers to
    //! adhere to minimal layer times
    //! \param lines Original lines of the gcode file
    //! \param upperLines Uppercase lines of the original used for ease of parsing/comparison
    CincinnatiParser(GcodeMeta meta, bool allowLayerAlter, QStringList& lines, QStringList& upperLines);

    virtual void config();

  protected:
    //! \brief Function handler for the 'M0' command for waiting for user
    //! input to continue runnning.
    //!        This function handler accepts no parameters but takes them
    //!        for error checking purposes.
    //! \throws IllegalParameterException This occurs when another parameter
    //! is passed to this function.
    void M0Handler(QVector<QString> params);

    //! \brief Function handler for the 'M1' command for waiting for user
    //! input to stop the machine.
    //!        This function handler accepts no parameters but takes them
    //!        for error checking purposes.
    //! \throws IllegalParameterException This occurs when another parameter
    //! is passed to this function.
    virtual void M1Handler(QVector<QString> params);

    //! \brief Function handler for the 'M3' command for turning the extruder on
    //! and setting the spindle speed rate.
    //!        The function handler accepts the following parameters:
    //!             - S: Sets the spindle speed rate for the extruder.
    //!        The S command must exist or an exception is thrown.
    //! \param params List of parameters that have been split up to be
    //! parsed. \throws IllegalParameterException This occurs during either
    //! a conversion error, a parameter missing a value,
    //!                                   a duplicate parameter, if a
    //!                                   required parameter is not passed,
    //!                                   or an illegal parameter passed.
    // virtual void M3Handler(QVector<QString> params);

    //! \brief Function handler for the 'M5' command for turning off the
    //! extruder.
    //!        This function handler accepts no parameters but takes them
    //!        for error checking purposes.
    //! \throws IllegalParameterException This occurs when another parameter
    //! is passed to this function.
    // virtual void M5Handler(QVector<QString> params);

    //! \brief Function handler for the 'M10' command for turning on dynamic
    //! spindle/extruder servo control.
    //!        This function handler accepts no parameters but takes them
    //!        for error checking purposes.
    //! \throws IllegalParameterException This occurs when another parameter
    //! is passed to this function.
    virtual void M10Handler(QVector<QString> params);

    //! \brief Function handler for the 'M11' command for turning off
    //! dynamic spindle/extruder servo control.
    //!        This function handler accepts no parameters but takes them
    //!        for error checking purposes.
    //! \throws IllegalParameterException This occurs when another parameter
    //! is passed to this function.
    virtual void M11Handler(QVector<QString> params);

    //! \brief Function handler for the 'M12' command for adjusting the
    //! perminiter spindle.
    //!        This function handler accepts no parameters but takes them
    //!        for error checking purposes.
    //! \throws IllegalParameterException This occurs when another parameter
    //! is passed to this function.
    virtual void M12Handler(QVector<QString> params);

    //! \brief Function Handler for the 'M13' command for inset spindle
    //! adjustment.
    // TODO: Figure out what insert spindle override even means.
    // think this means there is a special fillset type for this.
    virtual void M13Handler(QVector<QString> params);

    //! \brief Function Handler that performs all modifcations to the
    //!        internal data structures if a M14 command is encountered
    // TODO: Figure out what infill override means.
    // Think this means there is a special infill setting that is most
    // likely hard coded in.
    virtual void M14Handler(QVector<QString> params);

    //! \brief Function Handler that performs all modifcations to the
    //!        internal data structures if a M15 command is encountered
    // TODO: Figure out what it means by skin spindle override.
    // Think this means that the machine is doing a skin fill type and does
    // a different path type etc.
    virtual void M15Handler(QVector<QString> params);

    //! \brief Function Handler for the 'M16' command that
    // TODO: Just resets everything?
    // Just resets all override numbers to 100%. Not sure how to interact
    // with this since there is no HMI buttons.
    //!        This function handler accepts no parameters but takes them
    //!        for error checking purposes.
    //! \throws IllegalParameterException This occurs when another parameter
    //! is passed to this function.
    virtual void M16Handler(QVector<QString> params);

    //! \brief Function handler for the 'M30' command for stating the end of
    //! Gcode commands within a Gcode file..
    //!        This function handler accepts no parameters but takes them
    //!        for error checking purposes.
    //! \throws IllegalParameterException This occurs when another parameter
    //! is passed to this function.
    virtual void M30Handler(QVector<QString> params);

    // TODO: this
    //! \brief Function handler for the 'M60' command for turning on the
    //! feed shaker.
    //!        The function handler accepts the following parameters:
    //!             - L: ???
    //!             - P: ???
    //!        The L, P command must exist or an exception is thrown.
    //! \param params List of parameters that have been split up to be
    //! parsed. \throws IllegalParameterException This occurs during either
    //! a conversion error, a parameter missing a value,
    //!                                   a duplicate parameter, if a
    //!                                   required parameter is not passed,
    //!                                   or an illegal parameter passed.
    virtual void M60Handler(QVector<QString> params);

    //! \brief Function Handler for the 'M61' command for turning the feed
    //! shaker off.
    //!        This function handler accepts no parameters but takes them
    //!        for error checking purposes.
    //! \throws IllegalParameterException This occurs when another parameter
    //! is passed to this function.
    virtual void M61Handler(QVector<QString> params);

    //! \brief Function Handler for the 'M64' command for turing on and
    //! setting the tamper control level.
    //!        The function handler accepts the following parameters:
    //!             - L: Voltage level between 0.0 - 1.0
    //!        The L command must exist or an exception is thrown.
    //! \param params List of parameters that have been split up to be
    //! parsed. \throws IllegalParameterException This occurs during either
    //! a conversion error, a parameter missing a value,
    //!                                   a duplicate parameter, if a
    //!                                   required parameter is not passed,
    //!                                   or an illegal parameter passed.
    virtual void M64Handler(QVector<QString> params);

    //! \brief Function Handler for the 'M65' command for turning off the
    //! tamper control level.
    //!        The function accepts no parameters but takes them for error
    //!        checking purposes.
    //! \throws IllegalParameterException This occurs when another parameter
    //! is passed to this function.
    virtual void M65Handler(QVector<QString> params);

    //! \brief Function Handler for the 'M66' command for setting the
    //! acceleration value.
    //!        The function handler accepts the following parameters:
    //!             - L: Acceleration value
    //!        The L command must exist or an exception is thrown.
    //! \param params List of parameters that have been split up to be
    //! parsed. \throws IllegalParameterException This occurs during either
    //! a conversion error, a parameter missing a value,
    //!                                   a duplicate parameter, if a
    //!                                   required parameter is not passed,
    //!                                   or an illegal parameter passed.
    virtual void M66Handler(QVector<QString> params);

    //! \brief Function Handler for the 'M68' command that sends the
    //! extruder to the park position.
    //!        The function accepts no parameters but takes them for error
    //!        checking purposes.
    //! \throws IllegalParameterException This occurs when another parameter
    //! is passed to this function.
    virtual void M68Handler(QVector<QString> params);

    //! \brief Function handler for the 'M69' Gcode command for purging the
    //! extruder. This fucntion handler
    //!        accepts the following parameters:
    //!             - F: Changes the spindle feedrate, in a range from 0 -
    //!             400, defaults to 250 if not provided.
    //!             - L: Tells the extruder to return to the previous
    //!             position. If not passed then it defualts to not
    //!             returning.
    //!                 - if 0, then it does not.
    //!                 - if 1, then the extruder returns to the previous
    //!                 location.
    //!             - P: Sets the amount of time to purge the extruder in
    //!             seconds. If not passed this defaults to 60 seconds.
    //!             - S: Sets the amount of time to wait before wiping the
    //!             extruder. If not passed this defaults to 0 seconds.
    //!             - T: Sets the time to wait before starting to purge in
    //!             seconds. If not passed this defaults to 0 seconds.
    //!        None of the parameters passed are required.
    //! \param params List of parameters that have been split up to be
    //! parsed. \throws IllegalParameterException This occurs during either
    //! a conversion error, a parameter missing a value,
    //!                                   a duplicate parameter, or if an
    //!                                   illegal parameter passed.
    virtual void M69Handler(QVector<QString> params);

    //! \brief Funciton handler for tool changes in GCode. For now its just
    //! a placeholder function.
    virtual void ToolChangeHandler(QVector<QString> params);

  private:
    bool m_voltage_control = true;
    double m_voltage_control_value;
};
} // namespace ORNL
