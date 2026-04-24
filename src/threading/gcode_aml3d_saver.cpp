#include "threading/gcode_aml3d_saver.h"

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
GCodeAML3DSaver::GCodeAML3DSaver(QString tempLocation, QString path, QString filename, QString text, GcodeMeta meta)
    : m_temp_location(tempLocation), m_path(path), m_filename(filename), m_text(text), m_selected_meta(meta) {
    // NOP
}

void GCodeAML3DSaver::run() {
    // Setup necessary parameters and get needed settings
    QChar comma(','), newline('\n'), space(' '), x('X'), y('Y'), z('Z'), f('F'), s('S'), zero('0');
    int beadNum, pointNum, layerNum;
    QStringList lines = m_text.split(newline);
    QString G0("G0"), G1("G1"), zeroString("0"), one("1");
    QString xval, yval, zval, velocity, feedrate, beadOutput, pointOutput, weaveLength, weaveWidth;
    QString maxVelocity = QString::number(
        GSM->getGlobal()->setting<Velocity>(PRS::MachineSpeed::kMaxXYSpeed).to(m_selected_meta.m_velocity_unit), 'f',
        4);
    zval = QString::number(
        GSM->getGlobal()->setting<Distance>(PRS::Dimensions::kZMax).to(m_selected_meta.m_distance_unit), 'f', 4);
    feedrate = QString::number(0);
    layerNum = 0;
    pointNum = 0;
    weaveLength = QString::number(
        GSM->getGlobal()->setting<Distance>(ES::FileOutput::kAML3DWeaveLength).to(m_selected_meta.m_distance_unit), 'f',
        4);
    weaveWidth = QString::number(
        GSM->getGlobal()->setting<Distance>(ES::FileOutput::kAML3DWeaveWidth).to(m_selected_meta.m_distance_unit), 'f',
        4);

    int start = 0;
    bool endOfFile = false;
    // While loop should always evaluate true, but need a way to contain the file operations
    // so that multiple files can be opened, written to, and closed within the parsing of the g-code lines
    while (!(lines.isEmpty())) {
        layerNum++;
        beadNum = 0;

        QFile tempFile(m_temp_location % "temp");
        if (tempFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            QTextStream out(&tempFile);
            out << m_text;
            tempFile.close();

            QFile::rename(tempFile.fileName(), m_filename);
        }

        // AML3D file name structure uses three numbers: stage_part_layer
        // for now, we only increment the layer number
        QString fileSuffix = "_1_1_" % QString::number(layerNum);

        QFileInfo fi(m_filename);
        QString filePath = fi.absolutePath() + QDir::separator() + fi.baseName() + fileSuffix + "_deposit.csv";

        QFile file(filePath);

        file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
        QTextStream out(&file);
        // Set Row 1 titles for each column
        out << "bead_number" % comma % "point_number" % comma % x % comma % y % comma % z % comma % "rot_ax1" % comma %
                   "rot_ax2" % comma % "torch_x" % comma % "torch_y" % comma % "torch_z" % comma % "weave_on" % comma %
                   "weave_length" % comma % "weave_width" % comma % "travel_speed" % comma % "job" % newline;

        for (int i = start; i < lines.size(); i++) {
            start++;

            // The BEGINNING LAYER string denotes the start of a new layer which means we need a new file
            if (lines[i].contains("BEGINNING LAYER")) {
                // Ignore the start of the first layer
                if (lines[i].contains("(BEGINNING LAYER: 1)")) {
                    continue;
                }

                break;
            }
            // M30 signals the end of the G-Code file
            if (lines[i].contains("M30")) {
                endOfFile = true;
                break;
            }
            // Ignore all travel lines. AML3D controller calculates all travel moves.
            if (lines[i].contains("TRAVEL") || lines[i].contains("SET INITIAL FEEDRATE")) {
                // Use travel lower to signify the start of a new bead
                if (lines[i].contains("TRAVEL LOWER Z")) {
                    beadNum++;
                    pointNum = 0;
                }
                else
                    continue;
            }
            // Parse all G0 and G1 lines to create a new output in the csv
            if (lines[i].startsWith(G0) || lines[i].startsWith(G1)) {
                QString temp = lines[i].mid(0, lines[i].indexOf(m_selected_meta.m_comment_starting_delimiter));
                QVector<QString> params = temp.split(space);

                // There shouldn't be any G0 motions in the output, and velocity isn't even passed via csv, but I'm
                // leaving this line for reference
                if (params[0] == G0) {
                    velocity = maxVelocity;
                    feedrate = zero;
                }
                if (params[0] == G0 || params[0] == G1) {
                    for (int j = 1, end = params.size(); j < end; ++j) {
                        if (params[j].startsWith(x))
                            xval = params[j].mid(1);
                        else if (params[j].startsWith(y))
                            yval = params[j].mid(1);
                        else if (params[j].startsWith(z))
                            zval = params[j].mid(1);
                        else if (params[j].startsWith(f))
                            velocity = params[j].mid(1);
                    }
                }

                // Read the travel lower Z line just to extract the Z height and save it for future lines
                if (lines[i].contains("TRAVEL LOWER Z")) {
                    continue;
                }

                pointNum++;
                beadOutput = QString::number(beadNum);
                pointOutput = QString::number(pointNum);
                out << beadOutput % comma % pointOutput % comma % xval % comma % yval % comma % zval % comma %
                           zeroString % comma % zeroString % comma % zeroString % comma % zeroString % comma %
                           zeroString % comma % one % comma % weaveLength % comma % weaveWidth % comma % velocity %
                           comma % one % newline;
            }
        }

        file.close();

        if (endOfFile)
            break;
    }
}

} // namespace ORNL
