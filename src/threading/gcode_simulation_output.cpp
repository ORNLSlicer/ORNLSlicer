#include "threading/gcode_simulation_output.h"

#include <cstddef>

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStringBuilder>
#include <QStringList>
#include <QTextStream>
#include <qcontainerfwd.h>
#include <qfileinfo.h>
#include <qobject.h>
#include <qtypes.h>

#include "gcode/gcode_meta.h"
#include "managers/settings/settings_manager.h"
#include "units/unit.h"
#include "utilities/constants.h"

namespace ORNL {
GCodeSimulationOutput::GCodeSimulationOutput(const QString& temp_location, const QString& path, const QString& filename,
                                             const QString& text, const GcodeMeta& meta)
    : m_temp_location(temp_location), m_path(path), m_filename(filename), m_text(text), m_selected_meta(meta) {
    // NOP
}

void GCodeSimulationOutput::run() {
    constexpr QChar comma(','), new_line('\n'), space(' '), x('X'), y('Y'), z('Z'), w('W'), f('F'), s('S'), zero('0');
    constexpr qint16 layer_num = 0;
    QStringList lines = m_text.split(new_line);
    QString g0("G0"), g1("G1"), m3("M3"), m5("M5"), m64("M64"), comma_space(", ");
    QString xval("0"), yval("0"), zval, wval, sval("0"), extruding("0"), velocity, rapid_velocity;
    zval = QString::number(
        GSM->getGlobal()->setting<Distance>(PRS::Dimensions::kZMax).to(m_selected_meta.m_distance_unit), 'f', 4);
    wval = QString::number(
        GSM->getGlobal()->setting<Distance>(PRS::Dimensions::kWMax).to(m_selected_meta.m_distance_unit), 'f', 4);
    rapid_velocity = QString::number(
        GSM->getGlobal()->setting<Velocity>(PS::Travel::kSpeed).to(m_selected_meta.m_velocity_unit), 'f', 4);

    m_current_x = 0;
    m_current_y = 0;
    m_current_z = zval.toDouble();
    m_current_w = wval.toDouble();
    m_current_time = 0;
    m_use_metric = true;
    m_is_g0 = false;

    QFile temp_file(m_temp_location % "temp");
    if (temp_file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream out(&temp_file);
        out << m_text;
        temp_file.close();

        QFile::rename(temp_file.fileName(), m_filename);
    }

    QFileInfo file_info(m_filename);
    QString file_path = file_info.absolutePath() + QDir::separator() + file_info.baseName() + "_simulation_output.txt";

    QFile file(file_path);
    file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
    QTextStream out(&file);

    QString line;

    for (size_t i = 0; i < lines.size(); i++) {
        line = lines[i];
        sval = "1"; // reset sval

        // Cincinnati uses inches and inches/minute
        if (line.contains("Cincinnati")) {
            m_use_metric = false;
        }

        // Extrusion on/off needs to appear in the simulation txt file 1 line before it happens in the g-code\
        // Look at the next line to determine the proper value for the "extruding" flag
        if (i + 1 < lines.size() &&
            lines[i + 1].startsWith(g1)) // Check next motion to see if extrusion turns off with S0
        {
            QString temp = lines[i + 1].mid(0, line.indexOf(m_selected_meta.m_comment_starting_delimiter));
            QVector<QString> params = temp.split(space);

            if (params[0] == g1) {
                for (size_t i = 1, end = params.size(); i < end; i++) {
                    if (params[i].startsWith(s)) {
                        sval = params[i].mid(1);
                    }
                }
                m_is_g0 = false;
            }

            if (sval == "0" || sval == "0.0000") {
                extruding = "0";
            }
        }
        else if (i + 1 < lines.size() && (lines[i + 1].startsWith(m3) || lines[i + 1].startsWith(m64))) {
            extruding = "1";
        }
        else if (i + 1 < lines.size() && lines[i + 1].startsWith(m5)) {
            extruding = "0";
        }

        // Look at current line to create the output
        if (line.startsWith(g0)) {
            QString temp = line.mid(0, line.indexOf(m_selected_meta.m_comment_starting_delimiter));
            QVector<QString> params = temp.split(space);

            if (params[0] == g0) {
                for (size_t i = 1, end = params.size(); i < end; i++) {
                    if (params[i].startsWith(x)) {
                        xval = params[i].mid(1);
                    }
                    else if (params[i].startsWith(y)) {
                        yval = params[i].mid(1);
                    }
                    else if (params[i].startsWith(z)) {
                        zval = params[i].mid(1);
                    }
                    else if (params[i].startsWith(w)) {
                        wval = params[i].mid(1);
                    }
                }
                m_is_g0 = true;
            }

            calculateTime(xval, yval, zval, wval, rapid_velocity);

            out << m_current_time() << comma_space % xval % comma_space % yval % comma_space << m_current_z()
                << comma_space % extruding % new_line;
        }
        else if (line.startsWith(g1)) {
            QString temp = line.mid(0, line.indexOf(m_selected_meta.m_comment_starting_delimiter));
            QVector<QString> params = temp.split(space);

            if (params[0] == g1) {
                for (size_t i = 1, end = params.size(); i < end; i++) {
                    if (params[i].startsWith(x)) {
                        xval = params[i].mid(1);
                    }
                    else if (params[i].startsWith(y)) {
                        yval = params[i].mid(1);
                    }
                    else if (params[i].startsWith(z)) {
                        zval = params[i].mid(1);
                    }
                    else if (params[i].startsWith(w)) {
                        wval = params[i].mid(1);
                    }
                    else if (params[i].startsWith(f)) {
                        velocity = params[i].mid(1);
                    }
                    else if (params[i].startsWith(s)) {
                        sval = params[i].mid(1);
                    }
                }
                m_is_g0 = false;
            }

            if (sval == "0" || sval == "0.0000") {
                extruding = "0";
            }

            calculateTime(xval, yval, zval, wval, velocity);

            out << m_current_time() << comma_space % xval % comma_space % yval % comma_space << m_current_z()
                << comma_space % extruding % new_line;
        }
    }

    file.close();
}

void GCodeSimulationOutput::calculateTime(const QString& x, const QString& y, const QString& z, const QString& w,
                                          const QString& f) {
    Distance dist = 0;
    Distance temp_x = x.toDouble();
    Distance temp_y = y.toDouble();
    Distance temp_z = z.toDouble();
    Distance temp_w = w.toDouble();
    Velocity temp_f = f.toDouble();
    Velocity temp_f_seconds;
    Distance dx = temp_x - m_current_x;
    Distance dy = temp_y - m_current_y;
    Distance dz = temp_z - m_current_z;
    Distance dw = temp_w - m_current_w;
    Time temp_time = 0;

    // Set acceleration default value
    Acceleration current_accel;

    if (m_use_metric) {
        current_accel = 1500; // units mm/s^2 - this is ~0.153g for JuggerBot3D
    }
    else {
        current_accel = 23.165; // units in/s^2 - this is ~0.06g for CI
    }

    // Calculate distance to be used for time calculation
    // If only W moves, use W speed and acceleration. Typically happens with a G0
    if (dx == 0 && dy == 0 && dz == 0 && dw != 0) {
        dist = dw;
        if (m_is_g0) {
            temp_f = GSM->getGlobal()
                         ->setting<Velocity>(PRS::MachineSpeed::kWTableSpeed)
                         .to(m_selected_meta.m_velocity_unit);
        }
        // Update acceleration for W table moves which only exist on the CI syntax, so units are in/s^2
        current_accel = 7.87;
    }
    // If only Z moves, use Z speed and acceleration. Typically happens with a G0
    else if (dx == 0 && dy == 0 && dz != 0 && dw == 0) {
        dist = dz;
        if (m_is_g0) {
            temp_f =
                GSM->getGlobal()->setting<Velocity>(PRS::MachineSpeed::kZSpeed).to(m_selected_meta.m_velocity_unit);
        }
        // For Z only moves, set acceleration to 200mm/s^2 or 7.87in/s*2
        if (m_use_metric) {
            current_accel = 200; // units mm/s^2
        }
        else {
            current_accel = 7.87; // units in/s^2
        }
    }
    // Predominantly X and Y motion
    else {
        dist = sqrt(dx * dx + dy * dy + dz * dz + dw * dw);
    }

    dist = abs(dist);
    temp_f_seconds = temp_f / 60.0;

    Time accel_time = temp_f_seconds / current_accel;
    Distance accel_dist = (temp_f_seconds * temp_f_seconds) / (2.0 * current_accel);

    if (dist > 2.0 * accel_dist) {
        temp_time = 2 * accel_time + (dist - 2.0 * accel_dist) / temp_f_seconds;
    }
    else {
        temp_time = sqrt(2.0 * dist / current_accel);
    }

    // Add time to the total time
    m_current_time += temp_time;

    // Update Current Values
    m_current_x = temp_x;
    m_current_y = temp_y;
    m_current_z = temp_z + abs(temp_w);
    m_current_w = temp_w;
}
} // namespace ORNL
