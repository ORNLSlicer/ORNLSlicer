#include "threading/gcode_rpbf_saver.h"

#include <math.h>

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStringBuilder>
#include <QStringList>
#include <QTextStream>
#include <qcontainerfwd.h>
#include <qlatin1stringview.h>
#include <qobject.h>
#include <qstringliteral.h>
#include <qstringmatcher.h>

#include "gcode/gcode_meta.h"
#include "geometry/point.h"
#include "managers/settings/settings_manager.h"
#include "units/unit.h"
#include "utilities/constants.h"

namespace ORNL {
GCodeRPBFSaver::GCodeRPBFSaver(QString tempLocation, QString path, QString filename, QString text, GcodeMeta meta,
                               double clockAngle, bool offset_enabled, Angle sector_width)
    : m_temp_location(tempLocation), m_path(path), m_filename(filename), m_text(text), m_selected_meta(meta),
      m_clock_angle(clockAngle), m_is_offset_enabled(offset_enabled), m_sector_width(sector_width) {
    // NOP
}

void GCodeRPBFSaver::run() {
    // First, get necessary parameters from settings to rotate all pathing
    QChar comma(','), newline('\n');
    QStringList lines = m_text.split(newline);
    QStringList lines_copy = lines;
    QRegularExpression comments(R"(//.*)");

    QStringMatcher x_origin_matcher(PRS::Dimensions::kXOffset), y_origin_matcher(PRS::Dimensions::kXOffset),
        sector_count_matcher(PS::Infill::kSectorCount);

    Distance x_origin, y_origin;
    int sector_count;
    bool xFound = false, yFound = false, sectorFound = false;
    int lastLine = lines.size() - 1;
    while (!xFound || !yFound || !sectorFound) {
        if (x_origin_matcher.indexIn(lines[lastLine]) != -1) {
            QString valWithDelimiter = lines[lastLine].split(' ')[1];
            valWithDelimiter.chop(m_selected_meta.m_comment_ending_delimiter.size());
            x_origin = valWithDelimiter.toInt();
            xFound = true;
        }
        if (y_origin_matcher.indexIn(lines[lastLine]) != -1) {
            QString valWithDelimiter = lines[lastLine].split(' ')[1];
            valWithDelimiter.chop(m_selected_meta.m_comment_ending_delimiter.size());
            y_origin = valWithDelimiter.toInt();
            yFound = true;
        }
        if (sector_count_matcher.indexIn(lines[lastLine]) != -1) {
            QString valWithDelimiter = lines[lastLine].split(' ')[1];
            valWithDelimiter.chop(m_selected_meta.m_comment_ending_delimiter.size());
            sector_count = valWithDelimiter.toInt();
            sectorFound = true;
        }
        --lastLine;
    }

    // Then, collapse build commands and output CLI-compliant primary file
    // Rotate lines in copy to use for next section
    Point origin(x_origin.to(m_selected_meta.m_distance_unit), y_origin.to(m_selected_meta.m_distance_unit));
    Angle sectorAngle;

    if (m_is_offset_enabled)
        sectorAngle = m_sector_width;
    else
        sectorAngle = (2.0 * M_PI) / sector_count;

    Angle currentAngle;
    bool firstInBlock = true, firstSinceLayer = true;
    int blockStartIndex = 0;
    QString polyPrefix("$$POLYLINE/"), hatchPrefix("$$HATCHES/");
    QString comment;
    QString sectorHeader = "//SECTOR SPLIT//";
    QStringMatcher sectorHeaderMatcher(sectorHeader), layerHeaderMatcher(m_selected_meta.m_layer_delimiter);
    QRegularExpression spaces("^\\s+");
    int layer_count = 0;
    int total_sectors = GSM->getGlobal()->setting<int>(PS::Infill::kSectorCount);
    double limit = 2.0 * M_PI;

    for (int i = 0, end = lines.size(); i < end; ++i) {
        if (sectorHeaderMatcher.indexIn(lines[i]) != -1) {
            if (firstSinceLayer) {
                firstSinceLayer = false;
            }
            else
                currentAngle += sectorAngle;
        }
        else if (layerHeaderMatcher.indexIn(lines[i]) != -1) {
            firstSinceLayer = true;
            if (m_is_offset_enabled) {
                double largeAngle = (sectorAngle() * total_sectors) * layer_count;
                while (largeAngle > limit)
                    largeAngle -= limit;

                currentAngle = m_clock_angle + largeAngle;
            }
            else
                currentAngle = m_clock_angle;
            layer_count++;
        }
        else if (lines[i].startsWith(polyPrefix) || lines[i].startsWith(hatchPrefix)) {
            QStringList params = lines[i].split(comma);
            // If more than a single command per line, visualization was skipped and no collapsing of commands
            // needs to happen.  All commands are contained on a single line, they simply need rotated.
            if (params.size() > 9) {
                for (int j = 2, end = params.size() - 1; j < end; j += 7) {
                    Point startPt(params[j].toFloat(), params[j + 1].toFloat());
                    startPt = startPt.rotateAround(origin, currentAngle);
                    params[j] = QString::number(startPt.x(), 'f', 0);
                    params[j + 1] = QString::number(startPt.y(), 'f', 0);
                    Point endPt(params[j + 2].toFloat(), params[j + 3].toFloat());
                    endPt = endPt.rotateAround(origin, currentAngle);
                    params[j + 2] = QString::number(endPt.x(), 'f', 0);
                    params[j + 3] = QString::number(endPt.y(), 'f', 0);
                    lines[i] = lines_copy[i] = params.join(comma);
                }
            }
            else {
                // There is a single command and subsequent commands must be collapsed onto a single line
                if (firstInBlock) {
                    blockStartIndex = i;
                    firstInBlock = false;

                    int commentIndex = params.last().indexOf(m_selected_meta.m_comment_starting_delimiter);
                    comment = params.last().mid(commentIndex - 1);
                    params.last() = params.last().left(commentIndex - 1);
                    // QString line = lines[i].left(commentIndex - 1);

                    lines[i] = params.join(comma);
                    // QStringList params = line.split(comma);
                    params.replaceInStrings(spaces, QString());

                    Point startPt(params[2].toFloat(), params[3].toFloat());
                    startPt = startPt.rotateAround(origin, currentAngle);
                    params[2] = QString::number(startPt.x(), 'f', 0);
                    params[3] = QString::number(startPt.y(), 'f', 0);
                    Point endPt(params[4].toFloat(), params[5].toFloat());
                    endPt = endPt.rotateAround(origin, currentAngle);
                    params[4] = QString::number(endPt.x(), 'f', 0);
                    params[5] = QString::number(endPt.y(), 'f', 0);
                    lines_copy[i] = params.join(comma);
                }
                else {
                    // QStringList params = lines[i].split(comma);
                    params.removeFirst();
                    params.removeFirst();
                    params.replaceInStrings(spaces, QString());

                    int commentIndex = params.last().indexOf(m_selected_meta.m_comment_starting_delimiter);
                    params.last() = params.last().left(commentIndex - 1);

                    lines[blockStartIndex].append(comma % params.join(comma));

                    Point startPt(params[0].toFloat(), params[1].toFloat());
                    startPt = startPt.rotateAround(origin, currentAngle);
                    params[0] = QString::number(startPt.x(), 'f', 0);
                    params[1] = QString::number(startPt.y(), 'f', 0);
                    Point endPt(params[2].toFloat(), params[3].toFloat());
                    endPt = endPt.rotateAround(origin, currentAngle);
                    params[2] = QString::number(endPt.x(), 'f', 0);
                    params[3] = QString::number(endPt.y(), 'f', 0);
                    lines_copy[blockStartIndex].append(comma % params.join(comma));

                    lines.removeAt(i);
                    lines_copy.removeAt(i);
                    --end;
                    --i;
                }
            }
        }
        else if (!firstInBlock) {
            firstInBlock = true;
            lines[blockStartIndex].append(comment);
        }
    }
    m_text = lines.join(newline);

    QFile tempFile(m_temp_location % "temp");
    if (tempFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream out(&tempFile);
        out << m_text;
        tempFile.close();

        QFile::rename(tempFile.fileName(), m_filename);
    }

    // CLI-compliant primary file must now be customized for RPBF
    // remove all comments and split into sectors with unique numbering system

    QLatin1Char zero('0');
    QLatin1String Layer("_Layer"), Sec("_Sec");
    QStringMatcher newLayer(m_selected_meta.m_layer_delimiter);

    m_path += '/';

    int currentLayer = 1, globalSector = 1, layerSector = 1;
    QString m_text_copy = lines_copy.join(newline);
    QStringList sectors = m_text_copy.split(sectorHeader);
    sectors.pop_front();

    int scan_head = 0;
    QString first_head = "/Head1/";
    QDir firstDir(m_path % first_head);
    if (!firstDir.exists())
        firstDir.mkdir(m_path % first_head);

    QString second_head = "/Head2/";
    QDir secondDir(m_path % second_head);
    if (!secondDir.exists())
        secondDir.mkdir(m_path % second_head);

    bool nextLayer = false;
    for (QString sector : sectors) {
        if (newLayer.indexIn(sector) != -1)
            nextLayer = true;

        sector.remove(comments);
        sector = sector.trimmed();

        if (sector.isEmpty())
            sector = " ";

        QString finalFileName = QStringLiteral("%1").arg(globalSector, 7, 10, zero) % Layer %
                                QStringLiteral("%1").arg(currentLayer, 5, 10, zero) % Sec %
                                QStringLiteral("%1").arg(layerSector, 2, 10, zero) % m_selected_meta.m_file_suffix;

        QString finalPath;
        if (scan_head % 2 == 0)
            finalPath = m_path % first_head % finalFileName;
        else
            finalPath = m_path % second_head % finalFileName;

        QFile file(finalPath);

        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            QTextStream out(&file);
            out << sector;
        }
        file.close();

        ++globalSector;
        ++layerSector;
        ++scan_head;

        if (nextLayer) {
            ++currentLayer;
            layerSector = 1;
            nextLayer = false;
        }
    }
}

} // namespace ORNL
