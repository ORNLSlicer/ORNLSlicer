#include "threading/gcode_tormach_saver.h"

#include "QDir"
#include "QFile"
#include "QRegularExpression"
#include "QStringBuilder"
#include "QStringList"
#include "QTextStream"
#include "managers/settings/settings_manager.h"

namespace ORNL {
GCodeTormachSaver::GCodeTormachSaver(QString tempLocation, QString path, QString filename, QString text, GcodeMeta meta)
    : m_temp_location(tempLocation), m_path(path), m_filename(filename), m_text(text), m_selected_meta(meta) {
    // NOP
}

void GCodeTormachSaver::run() {
    // First, get necessary parameters from settings to rotate all pathing
    QChar comma(','), newline('\n'), space(' '), x('X'), y('Y'), z('Z'), f('F'), s('S'), zero('0');
    qint16 layerNum = 0;
    QStringList lines = m_text.split(newline);
    QString G0("G0"), G1("G1"), M3("M3"), M5("M5"), M64("M64"), M65("M65"), commaSpace(", ");
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
    QString filePath = fi.absolutePath() + QDir::separator() + fi.baseName() + "_output.apt";

    QFile file(filePath);
    file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
    QTextStream out(&file);

    out << "$$ ORNL SLICER-2" % newline;
    out << "$$*" % newline;
    out << "$$ -> MFGNO" % newline;
    out << "PARTNO / CF HY-80 3 x 1 1 1 CF HY-80 3 x 1" % newline;
    out << "MACHIN / MILL, 01" % newline;
    out << "$$ -> CUTCOM_GEOMETRY_TYPE /" % newline;
    out << "UNITS / MM" % newline;
    out << "CALSUB/START_PROG" % newline;
    out << "PARTNO/1 1 CF HY-80 3 x 1" % newline;
    out << "PPRINT/ --- TOOLLIST BEGIN ---" % newline;
    out << "PPRINT/ --- TOOLLIST END --- " % newline;
    out << "$$-> CSYS / 1.0000000000, 0.0000000000, 0.0000000000, 0.0000000000, 0.0000000000, 1.0000000000, "
           "0.0000000000, 0.0000000000, 0.0000000000, 0.0000000000, 1.0000000000, 0.0000000000" %
               newline;
    out << "MULTAX/ ON" % newline;
    out << "PPRINT/ --- files_x\\toolChange_comment.txt --- " % newline;
    out << "PPRINT/ - T1 R    2.25000 L   12.25000     0.00000 TORCH 1  *" % newline;
    out << "PPRINT/ OPERATION 2 " % newline;
    out << "PPRINT/" % newline;
    out << "PPRINT/ CF HY-80 3 x 1" % newline;
    out << "PPRINT/ T2 Additive Manufacturing" % newline;
    out << "PPRINT/ --- " % newline;
    out << "$$ ()" % newline;
    out << "$$ ( -------------------- additive_toolchange.txt --- )" % newline;
    out << "$$ LASER TOOL CHANGE" % newline;
    out << "$$ ( -------------------- )" % newline;
    out << "$$ ()" % newline;
    out << "LOADTL/2" % newline;

    if (GSM->getGlobal()->setting<int>(ES::FileOutput::kTormachMode) == static_cast<int>(TormachMode::kMode21)) {
        out << "wirefeed speed" % newline;
        out << "voltage" % newline;
    }
    else if (GSM->getGlobal()->setting<int>(ES::FileOutput::kTormachMode) == static_cast<int>(TormachMode::kMode40)) {
        out << "wirefeed speed" % newline;
        out << "power" % newline;
    }
    else if (GSM->getGlobal()->setting<int>(ES::FileOutput::kTormachMode) == static_cast<int>(TormachMode::kMode102)) {
        out << "wirefeed speed" % newline;
        out << "trim" % newline;
        out << "frequency" % newline;
    }
    else if (GSM->getGlobal()->setting<int>(ES::FileOutput::kTormachMode) == static_cast<int>(TormachMode::kMode274)) {
        out << "wirefeed speed" % newline;
        out << "trim" % newline;
        out << "ultimarc" % newline;
    }
    else if (GSM->getGlobal()->setting<int>(ES::FileOutput::kTormachMode) == static_cast<int>(TormachMode::kMode509)) {
        out << "wirefeed speed" % newline;
        out << "trim" % newline;
        out << "ultimarc" % newline;
    }

    out << "PPRINT/ --- files_x\\job_start.txt ---" % newline;
    out << "PPRINT/ OPERATION 2" % newline;
    out << "PPRINT/" % newline;
    out << "PPRINT/ CF HY-80 3 x 1" % newline;
    out << "PPRINT/ T2 Additive Manufacturing" % newline;
    out << "CALSUB/START_JOB" % newline;
    out << "SEQUENCE/ BEGIN,toolpath" % newline;
    out << "PPRINT/ ---" % newline;
    out << "FEDRAT/ MMPM, 300 "% newline;

    for (QString line : lines) {
        if (line.startsWith(G0)) {
            out << "RAPID" % newline;
            QString temp = line.mid(0, line.indexOf(m_selected_meta.m_comment_starting_delimiter));
            QVector<QString> params = temp.split(space);

            if (params[0] == G0) {
                for (int i = 1, end = params.size(); i < end; ++i) {
                    if (params[i].startsWith(x))
                        xval = params[i].mid(1);
                    else if (params[i].startsWith(y))
                        yval = params[i].mid(1);
                    else if (params[i].startsWith(z))
                        zval = params[i].mid(1);
                }
            }
            out << "GOTO / " % xval % commaSpace % yval % commaSpace % zval % commaSpace % "0.0000, 0.0000, 0.0000" %
                       newline;
        }
        else if (line.startsWith(G1)) {
            QString temp = line.mid(0, line.indexOf(m_selected_meta.m_comment_starting_delimiter));
            QVector<QString> params = temp.split(space);

            if (params[0] == G1) {
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
            out << "FEDRAT/ MMPM, " % velocity % newline;
            out << "GOTO / " % xval % commaSpace % yval % commaSpace % zval % commaSpace % "0.0000, 0.0000, 0.0000" %
                       newline;
        }
        else if (line.startsWith(M3) || line.startsWith(M64)) {
            out << "$$ ( -------------------- additiveDevice_on.txt --- )" % newline;
            out << "CALSUB/START_DEPO" % newline;
            out << "$$ ( -------------------- )" % newline;
        }
        else if (line.startsWith(M5) || line.startsWith(M65)) {
            out << "$$ ( -------------------- additiveDevice_off.txt --- )" % newline;
            out << "CALSUB/STOP_DEPO" % newline;
            out << "$$ ( -------------------- )" % newline;
        }
        else if (line.startsWith("(BEGINNING LAYER:") || line.startsWith(";BEGINNING LAYER:")) {
            layerNum++;
            out << "$$ Layer: " << layerNum << "\n";
        }
        else if (line.startsWith("(UPDATE VOLTAGE FOR TIP WIPE)")) {
            out << "$$ Set New Welder Voltage\n";
        }
    }
    file.close();
}

} // namespace ORNL
