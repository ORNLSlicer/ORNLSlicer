#pragma once

#include <QThread>

#include <qobject.h>
#include <qtmetamacros.h>

#include "units/unit.h"

namespace ORNL {
/*!
 * \class GCodeArcSpecialtiesSaver
 * \brief Threaded class that writes Arc Specialties G-Code plus the generated weld schedule companion file.
 */
class GCodeArcSpecialtiesSaver : public QThread {
    Q_OBJECT
   public:
    //! \brief Constructor
    //! \param filename: filename to output
    //! \param text: current gcode
    GCodeArcSpecialtiesSaver(QString filename, QString text);

    //! \brief Saves both files synchronously, returning an error or an empty string on success.
    QString save();

    //! \brief Function that is run when start is called on this thread.
    void run() override;

   signals:
    void completed(QString error);

   private:
    //! \brief Writes the generated Arc Specialties weld schedule companion file.
    QString writeWeldScheduleFile(const QString& file_path);

    //! \brief Replaces the G80 schedule file reference in the main G-Code text.
    void updateScheduleReference(const QString& schedule_file_name);

    //! \brief Output filename and text to output
    QString m_filename, m_text;

    //! \brief Time and voltage UI units captured before the export thread starts.
    Time m_time_unit;
    Voltage m_voltage_unit;
};  // class GCodeArcSpecialtiesSaver
}  // namespace ORNL
