#pragma once

#include "QThread"
#include "gcode/gcode_meta.h"

namespace ORNL {
/*!
 * \class GCodeAMCMSaver
 * \brief Threaded class that provides additional gcode processing.
 */
class GCodeAMCMSaver : public QThread {
    Q_OBJECT
  public:
    //! \brief Constructor
    //! \param tempLocation: location of gcode file
    //! \param path: path to output
    //! \param filename: filename to output
    //! \param text: current gcode
    //! \param meta: meta used to generate gcode
    GCodeAMCMSaver(QString tempLocation, QString path, QString filename, QString text, GcodeMeta meta);

    //! \brief Function that is run when start is called on this thread.
    void run() override;

  private:
    //! \brief Temporary file location, output path, output filename, and text to output
    QString m_temp_location, m_path, m_filename, m_text;

    //! \brief Meta info determined from file
    GcodeMeta m_selected_meta;

}; // class GCodeAMCMSaver
} // namespace ORNL
