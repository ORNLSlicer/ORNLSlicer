#include "threading/gcode_marlin_saver.h"

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
#include "utilities/constants.h"

namespace ORNL {
GCodeMarlinSaver::GCodeMarlinSaver(QString tempLocation, QString path, QString filename, QString text, GcodeMeta meta)
    : m_temp_location(tempLocation), m_path(path), m_filename(filename), m_text(text), m_selected_meta(meta) {
    // NOP
}

void GCodeMarlinSaver::run() {
    // First, get necessary parameters from settings to rotate all pathing
    QChar space(' '), newline('\n'), x('X'), y('Y'), z('Z'), f('F'), e('E');
    QStringList lines = m_text.split(newline);
    QString G1("G1");
    QString xval("0.0"), yval("0.0"), zval("0.0"), xFeedrate, yFeedrate, zFeedrate, feedrate("0.0"),
        extrusionAmount("0.0");
    QString previousX(""), previousY(""), previousZ(""), previousExtrusionAmount("");
    QString xOut, yOut, zOut, xFeedrateOut, yFeedrateOut, zFeedrateOut;
    double timeStep = 0.0;
    QString temperature("510.0");
    QString isTravel("0");
    // zval =
    // QString::number(GSM->getGlobal()->setting<Distance>(PRS::Dimensions::kZMax).to(m_selected_meta.m_distance_unit),
    // 'f', 4);
    feedrate = QString::number(0);

    QFile tempFile(m_temp_location % "temp");
    if (tempFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream out(&tempFile);
        out << m_text;
        tempFile.close();

        QFile::rename(tempFile.fileName(), m_filename);
    }

    QFileInfo fi(m_filename);
    QString filePath = fi.absolutePath() + QDir::separator() + fi.baseName() + "_command_data.txt";

    QFile file(filePath);
    file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
    QTextStream out(&file);

    for (QString line : lines) {
        isTravel = "0";
        if (line.startsWith(G1)) {
            QString temp = line.mid(0, line.indexOf(m_selected_meta.m_comment_starting_delimiter));
            QVector<QString> params = temp.split(space);

            if (line.contains("TRAVEL") || line.contains("TIP WIPE") || line.contains("COAST") ||
                line.contains("LIFT") || line.contains("FLYING START")) {
                isTravel = "1";
                extrusionAmount = "0.0";
            }

            if (params[0] == G1) {
                for (int i = 1, end = params.size(); i < end; ++i) {
                    if (params[i].startsWith(x))
                        xval = params[i].mid(1);
                    else if (params[i].startsWith(y))
                        yval = params[i].mid(1);
                    else if (params[i].startsWith(z))
                        zval = params[i].mid(1);
                    else if (params[i].startsWith(f))
                        feedrate = params[i].mid(1);
                    else if (params[i].startsWith(e)) // Note that extrusion values aren't currently being used
                        extrusionAmount = params[i].mid(1);
                }
            }

            // Check to see if values have changed
            if (previousX == xval)
                xFeedrate = "0.0";
            else
                xFeedrate = feedrate;
            if (previousY == yval)
                yFeedrate = "0.0";
            else
                yFeedrate = feedrate;
            if (previousZ == zval)
                zFeedrate = "0.0";
            else
                zFeedrate = feedrate;
            if (previousExtrusionAmount == extrusionAmount)
                extrusionAmount = "0.0";

            // Convert mm to m and mm/min to m/s
            double mm = xval.toDouble();
            mm = mm / 1000;
            xOut = QString::number(mm);
            mm = yval.toDouble();
            mm = mm / 1000;
            yOut = QString::number(mm);
            mm = zval.toDouble();
            mm = mm / 1000;
            zOut = QString::number(mm);
            mm = xFeedrate.toDouble();
            mm = mm / 60 / 1000;
            xFeedrateOut = QString::number(mm);
            mm = yFeedrate.toDouble();
            mm = mm / 60 / 1000;
            yFeedrateOut = QString::number(mm);
            mm = zFeedrate.toDouble();
            mm = mm / 60 / 1000;
            zFeedrateOut = QString::number(mm);

            // Write output
            if (GSM->getGlobal()->setting<int>(ES::FileOutput::kMarlinTravels)) {
                timeStep += 0.01;
                out << timeStep << space
                    << xOut % space % yOut % space % zOut % space % xFeedrateOut % space % yFeedrateOut % space %
                           zFeedrateOut % space % temperature % space % isTravel % newline;
            }
            else {
                timeStep += 0.01;
                out << timeStep << space
                    << xOut % space % yOut % space % zOut % space % xFeedrateOut % space % yFeedrateOut % space %
                           zFeedrateOut % space % temperature % newline;
            }

            // Store current value as previous value
            previousX = xval;
            previousY = yval;
            previousZ = zval;
            previousExtrusionAmount = extrusionAmount;
        }
    }
    file.close();
}

} // namespace ORNL
