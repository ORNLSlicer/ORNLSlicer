#pragma once

#include <QThread>
#include <qobject.h>
#include <qtmetamacros.h>

#include "gcode/gcode_meta.h"
#include "units/unit.h"

namespace ORNL {
/*!
 * \class GCodeSimulationOutput
 * \brief Threaded class that provides additional gcode processing for user with ABAQUS.
 */
class GCodeSimulationOutput : public QThread {
    Q_OBJECT
  public:
    //! \brief Constructor
    //! \param temp_location location of gcode file
    //! \param path path to output
    //! \param filename filename to output
    //! \param text current gcode
    //! \param meta meta used to generate gcode
    GCodeSimulationOutput(const QString& temp_location, const QString& path, const QString& filename,
                          const QString& text, const GcodeMeta& meta);

    //! \brief Function that is run when start is called on this thread.
    void run() override;

    //! \brief Calculate the time needed for the current move
    //! \param x the new X position
    //! \param y the new Y position
    //! \param z the new Z position
    //! \param w the new W position
    //! \param f the new velocity command
    void calculateTime(const QString& x, const QString& y, const QString& z, const QString& w, const QString& f);

  private:
    //! \brief Temporary file location, output path, output filename, and text to output
    QString m_temp_location, m_path, m_filename, m_text;

    //! \brief Current x, y, z, and w locations
    Distance m_current_x, m_current_y, m_current_z, m_current_w;

    //! \brief Defines whether the syntax uses metric units
    bool m_use_metric;

    //! \brief Defines the move as G0 and not requiring an F
    bool m_is_g0;

    //! \brief Current elapsed time
    Time m_current_time;

    //! \brief Meta info determined from file
    GcodeMeta m_selected_meta;

}; // class GCodeSimulationOutput
} // namespace ORNL
