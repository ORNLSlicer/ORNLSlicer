// Header
#include <QFile>
#include <QTextStream>
#include <QStringBuilder>
#include <QRegularExpression>
#include <QDir>
#include <QStringList>

#include <geometry/point.h>

#include "managers/settings/settings_manager.h"
#include "threading/gcode_sandia_saver.h"

namespace ORNL
{
GCodeSandiaSaver::GCodeSandiaSaver(QString tempLocation, QString path, QString filename, QString text, GcodeMeta meta) :
    m_temp_location(tempLocation), m_path(path), m_filename(filename), m_text(text), m_selected_meta(meta)
{
    //NOP
}

void GCodeSandiaSaver::run()
{
    //First, get necessary parameters from settings to rotate all pathing
    QChar comma(','), newline('\n'), space(' '), x('X'), y('Y'), z('Z'), f('F'), s('S'), zero('0');
    qint16 layerNum = 0;
    QStringList lines = m_text.split(newline);
    QString G0("G0"), G1("G1"), M3("M3 "), M5("M5"), commaSpace(", ");
    QString xval, yval, zval, velocity, feedrate;
    QString maxVelocity = QString::number(GSM->getGlobal()->setting<Velocity>(Constants::PrinterSettings::MachineSpeed::kMaxXYSpeed).to(m_selected_meta.m_velocity_unit), 'f', 4);
    zval = QString::number(GSM->getGlobal()->setting<Distance>(Constants::PrinterSettings::Dimensions::kZMax).to(m_selected_meta.m_distance_unit), 'f', 4);
    feedrate = QString::number(0);

    QFile tempFile(m_temp_location % "temp");
    if (tempFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        QTextStream out(&tempFile);
        out << m_text;
        tempFile.close();

        QFile::rename(tempFile.fileName(), m_filename);
    }

    QFileInfo fi(m_filename);
    QString filePath = fi.absolutePath() + QDir::separator() + fi.baseName() + "_output.src";

    QFile file(filePath);
    file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
    QTextStream out(&file);

    out << "&ACCESS RVP" % newline;
    if (!(GSM->getGlobal()->setting<int>(Constants::ExperimentalSettings::FileOutput::kSandiaMetalFile)))
    {
        out << "dInit()" % newline;
        out << "BAS(#BASE, 1)" % newline;
        out << "BAS(#TOOL,2)" % newline;
    }


    QString line;

    for(int i=0; i<lines.size(); i++)//(QString line : lines)
    {
        line = lines[i];
        if(line.startsWith(G0))
        {
            out << "$VEL.CP" % newline;
            QString temp = line.mid(0, line.indexOf(m_selected_meta.m_comment_starting_delimiter));
            QVector<QString> params = temp.split(space);

            if(params[0] == G0)
            {
                for(int i = 1, end = params.size(); i < end; ++i)
                {
                    if(params[i].startsWith(x))
                        xval = params[i].mid(1);
                    else if(params[i].startsWith(y))
                        yval = params[i].mid(1);
                    else if(params[i].startsWith(z))
                        zval = params[i].mid(1);
                }
            }
            QString aval = QString::number(GSM->getGlobal()->setting<Angle>(Constants::PrinterSettings::MachineSetup::kAxisA)());
            QString bval = QString::number(GSM->getGlobal()->setting<Angle>(Constants::PrinterSettings::MachineSetup::kAxisB)());
            QString cval = QString::number(GSM->getGlobal()->setting<Angle>(Constants::PrinterSettings::MachineSetup::kAxisC)());
            if (GSM->getGlobal()->setting<int>(Constants::ExperimentalSettings::FileOutput::kSandiaCVEL))
            {
                out << "LIN {X " % xval % ", Y " % yval % ", Z " % zval % ", A " % aval % ", B " % bval % ", C " % cval %
                           ", E1 0.000, E2 0.000, E3 0.000, E4 0.000, E5 0.000, E6 0.000 } C_VEL" %newline;
            }
            else
            {
                out << "LIN {X " % xval % ", Y " % yval % ", Z " % zval % ", A " % aval % ", B " % bval % ", C " % cval %
                           ", E1 0.000, E2 0.000, E3 0.000, E4 0.000, E5 0.000, E6 0.000 } C_DIS" %newline;
            }
        }
        else if(line.startsWith(G1))
        {
            if (i+1<lines.size() && lines[i+1].startsWith(M5))
            {
                if (!(GSM->getGlobal()->setting<int>(Constants::ExperimentalSettings::FileOutput::kSandiaMetalFile)))
                {
                    out << "dEarlyStop()" % newline;
                }
                else
                {
                    out << "fEarlyOff()" % newline;
                }

            }
            QString temp = line.mid(0, line.indexOf(m_selected_meta.m_comment_starting_delimiter));
            QVector<QString> params = temp.split(space);

            if(params[0] == G1)
            {
                for(int i = 1, end = params.size(); i < end; ++i)
                {
                    if(params[i].startsWith(x))
                        xval = params[i].mid(1);
                    else if(params[i].startsWith(y))
                        yval = params[i].mid(1);
                    else if(params[i].startsWith(z))
                        zval = params[i].mid(1);
                    else if(params[i].startsWith(f))
                        velocity = params[i].mid(1);
                }
            }
            if (velocity != feedrate)
            {
                out << "$VEL.CP = " % velocity % newline;
                feedrate = velocity;
            }

            QString aval = QString::number(GSM->getGlobal()->setting<Angle>(Constants::PrinterSettings::MachineSetup::kAxisA)());
            QString bval = QString::number(GSM->getGlobal()->setting<Angle>(Constants::PrinterSettings::MachineSetup::kAxisB)());
            QString cval = QString::number(GSM->getGlobal()->setting<Angle>(Constants::PrinterSettings::MachineSetup::kAxisC)());
            if (GSM->getGlobal()->setting<int>(Constants::ExperimentalSettings::FileOutput::kSandiaCVEL))
            {
                out << "LIN {X " % xval % ", Y " % yval % ", Z " % zval % ", A " % aval % ", B " % bval % ", C " % cval %
                           ", E1 0.000, E2 0.000, E3 0.000, E4 0.000, E5 0.000, E6 0.000 } C_VEL" %newline;
            }
            else
            {
                out << "LIN {X " % xval % ", Y " % yval % ", Z " % zval % ", A " % aval % ", B " % bval % ", C " % cval %
                           ", E1 0.000, E2 0.000, E3 0.000, E4 0.000, E5 0.000, E6 0.000 } C_DIS" %newline;
            }
        }
        else if(line.startsWith(M3))
        {
            if (!(GSM->getGlobal()->setting<int>(Constants::ExperimentalSettings::FileOutput::kSandiaMetalFile)))
            {
                out << "dStartPrinting()" % newline;
            }
            else
            {
                out << "fDelayStart()" % newline;
            }
        }
        else if(line.startsWith(M5))
        {
            if (!(GSM->getGlobal()->setting<int>(Constants::ExperimentalSettings::FileOutput::kSandiaMetalFile)))
            {
                out << "dWaitForStop()" % newline;
            }
            else
            {
                out << "fWaitForOff()" % newline;
            }
        }
    }
    if (!(GSM->getGlobal()->setting<int>(Constants::ExperimentalSettings::FileOutput::kSandiaMetalFile)))
    {
        out << newline % "dShutdown()";
    }

    out << newline % "END";
    file.close();
}

}  // namespace ORNL
