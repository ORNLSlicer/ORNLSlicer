#pragma once

#include <QThread>
#include <qobject.h>
#include <qtmetamacros.h>

#include "gcode/gcode_meta.h"
#include "units/unit.h"

namespace ORNL {
/*!
 * \class GCodeAdamantineSaver
 * \brief Threaded class that provides additional gcode processing for Adamantine input SCAN file.
 */
class GCodeAdamantineSaver : public QThread {
    Q_OBJECT
  public:
    //! \brief Constructor
    //! \param tempLocation: location of gcode file
    //! \param path: path to output
    //! \param filename: filename to output
    //! \param text: current gcode
    //! \param meta: meta used to generate gcode
    //! \param clockAngle: additional offset angle for rotation
    GCodeAdamantineSaver(QString tempLocation, QString path, QString filename, QString text, GcodeMeta meta);

    //! \brief Function that is run when start is called on this thread.
    void run() override;

  private:
    //! \brief Temporary file location, output path, output filename, and text to output
    QString m_temp_location, m_path, m_filename, m_text;

    //! \brief Meta info determined from file
    GcodeMeta m_selected_meta;

    //! \brief Clock angle in deg
    double m_clock_angle;

    bool m_is_offset_enabled = false;

    Angle m_sector_width;

}; // class GCodeAdamantineSaver
} // namespace ORNL
