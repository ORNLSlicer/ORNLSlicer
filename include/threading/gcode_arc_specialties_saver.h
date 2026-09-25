#pragma once

#include <QThread>

#include <qobject.h>
#include <qtmetamacros.h>

#include "gcode/gcode_meta.h"

namespace ORNL {
/*!
 * \class GCodeArcSpecialtiesSaver
 * \brief Threaded class that writes Arc Specialties G-Code plus the generated weld schedule companion file.
 */
class GCodeArcSpecialtiesSaver : public QThread {
    Q_OBJECT
   public:
    //! \brief Constructor
    //! \param tempLocation: location of gcode file
    //! \param path: path to output
    //! \param filename: filename to output
    //! \param text: current gcode
    //! \param meta: meta used to generate gcode
    GCodeArcSpecialtiesSaver(QString tempLocation, QString path, QString filename, QString text, GcodeMeta meta);

    //! \brief Function that is run when start is called on this thread.
    void run() override;

   private:
    //! \brief Writes the generated Arc Specialties weld schedule companion file.
    void writeWeldScheduleFile(const QString& file_path);

    //! \brief Replaces the G80 schedule file reference in the main G-Code text.
    void updateScheduleReference(const QString& schedule_file_name);

    //! \brief Temporary file location, output path, output filename, and text to output
    QString m_temp_location, m_path, m_filename, m_text;

    //! \brief Meta info determined from file
    GcodeMeta m_selected_meta;
};  // class GCodeArcSpecialtiesSaver
}  // namespace ORNL
