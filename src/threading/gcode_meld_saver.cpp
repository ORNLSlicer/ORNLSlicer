#include "threading/gcode_meld_saver.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStringBuilder>
#include <QStringList>
#include <QTextStream>
#include <qcontainerfwd.h>
#include <qfileinfo.h>
#include <qobject.h>

#include "gcode/gcode_meta.h"
#include "managers/settings/settings_manager.h"
#include "units/unit.h"
#include "utilities/constants.h"

namespace ORNL {
GCodeMeldSaver::GCodeMeldSaver(QString tempLocation, QString path, QString filename, QString text, GcodeMeta meta)
    : m_temp_location(tempLocation), m_path(path), m_filename(filename), m_text(text), m_selected_meta(meta) {
    // NOP
}

void GCodeMeldSaver::run() {
    // First, get necessary parameters from settings to rotate all pathing
    QChar comma(','), newline('\n'), space(' '), x('X'), y('Y'), z('Z'), f('F'), s('S'), zero('0');
    QStringList lines = m_text.split(newline);
    QString G0("G0"), G1("G1"), M24("M24"), M25("M25");
    QString xval, yval, zval, velocity, feedrate;
    QString maxVelocity = QString::number(
        GSM->getGlobal()->setting<Velocity>(PRS::MachineSpeed::kMaxXYSpeed).to(m_selected_meta.m_velocity_unit), 'f',
        4);
    zval = QString::number(
        GSM->getGlobal()->setting<Distance>(PRS::Dimensions::kZMax).to(m_selected_meta.m_distance_unit), 'f', 4);
    feedrate = QString::number(0);

    QFile tempFile(m_temp_location % "temp");
    if (tempFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream out(&tempFile);
        out << m_text;
        tempFile.close();

        QFile::rename(tempFile.fileName(), m_filename);
    }

    QFileInfo fi(m_filename);
    QString filePath = fi.absolutePath() + QDir::separator() + fi.baseName() + "_commands.csv";

    QFile file(filePath);
    file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
    QTextStream out(&file);
    out << x % comma % y % comma % z % comma % "TFR" % comma % "AFR" % newline;

    for (QString line : lines) {
        if (line.startsWith(G0) || line.startsWith(G1) || line.startsWith(M24) || line.startsWith(M25)) {
            QString temp = line.mid(0, line.indexOf(m_selected_meta.m_comment_starting_delimiter));
            QVector<QString> params = temp.split(space);

            if (params[0] == G0) {
                velocity = maxVelocity;
                feedrate = zero;
            }
            if (params[0] == G0 || params[0] == G1) {
                for (int i = 1, end = params.size(); i < end; ++i) {
                    if (params[i].startsWith(x))
                        xval = params[i].mid(1);
                    else if (params[i].startsWith(y))
                        yval = params[i].mid(1);
                    else if (params[i].startsWith(z))
                        zval = params[i].mid(1);
                    else if (params[i].startsWith(f))
                        velocity = params[i].mid(1);
                }
            }
            if (params[0] == M24) {
                for (int i = 1, end = params.size(); i < end; ++i) {
                    if (params[i].startsWith(s)) {
                        feedrate = params[i].mid(1);
                        break;
                    }
                }
            }
            if (params[0] == M25) {
                feedrate = QString::number(0);
            }
            out << xval % comma % yval % comma % zval % comma % velocity % comma % feedrate % newline;
        }
    }
    file.close();
}

} // namespace ORNL
