#include "external_files/exporters/rpbf_exporter.h"

#include <math.h>

#include <QStringBuilder>
#include <QStringList>
#include <qcontainerfwd.h>
#include <qdebug.h>
#include <qdir.h>
#include <qfiledevice.h>
#include <qhashfunctions.h>
#include <qstringliteral.h>

#include "gcode/gcode_meta.h"
#include "geometry/point.h"
#include "managers/settings/settings_manager.h"
#include "units/unit.h"
#include "utilities/constants.h"

namespace ORNL {
RPBFExporter::RPBFExporter(QString path, int layer_restart, int global_sector_restart, int scan_head_restart)
    : m_path(path) {
    m_layer_count = layer_restart;
    m_global_sector_count = global_sector_restart;
    m_scan_head_count = scan_head_restart;

    QString first_head = "/Head1/";
    QDir firstDir(m_path % first_head);
    if (!firstDir.exists())
        firstDir.mkdir(m_path % first_head);

    QString second_head = "/Head2/";
    QDir secondDir(m_path % second_head);
    if (!secondDir.exists())
        secondDir.mkdir(m_path % second_head);

    m_clocking_angle = GSM->getGlobal()->setting<Angle>(ES::RPBFSlicing::kClockingAngle);
}

void RPBFExporter::saveLayer(QString text) {
    QStringList sectors = condenseLayer(text);

    if (!sectors.isEmpty() && sectors.first() == " ") // If the first sector is empty remove it
        sectors.removeFirst();

    int layer_sector_count = 1;

    for (QString& sector : sectors) {
        QString finalFileName = QStringLiteral("%1").arg(m_global_sector_count, 7, 10, m_zero) % m_layer %
                                QStringLiteral("%1").arg(m_layer_count, 5, 10, m_zero) % m_sec %
                                QStringLiteral("%1").arg(layer_sector_count, 2, 10, m_zero) %
                                GcodeMetaList::RPBFMeta.m_file_suffix;

        QString finalPath;
        if (m_scan_head_count % 2 == 0)
            finalPath = m_path % m_first_head % finalFileName;
        else
            finalPath = m_path % m_second_head % finalFileName;

        QFile file(finalPath);

        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            QTextStream out(&file);
            out << sector;
        }
        file.close();

        ++m_global_sector_count;
        ++layer_sector_count;
        ++m_scan_head_count;
    }
}

QStringList RPBFExporter::condenseLayer(QString text) {
    QStringList lines = text.split(m_new_line);
    QStringList lines_copy = lines;

    m_sector_count = GSM->getGlobal()->setting<int>(PS::Infill::kSectorCount);
    Distance x_origin = GSM->getGlobal()->setting<Distance>(PRS::Dimensions::kXOffset);
    Distance y_origin = GSM->getGlobal()->setting<Distance>(PRS::Dimensions::kYOffset);
    m_origin = Point(x_origin.to(GcodeMetaList::RPBFMeta.m_distance_unit),
                     y_origin.to(GcodeMetaList::RPBFMeta.m_distance_unit));

    bool use_sector_offsetting = GSM->getGlobal()->setting<bool>(ES::RPBFSlicing::kSectorOffsettingEnable);
    int total_sectors = GSM->getGlobal()->setting<int>(PS::Infill::kSectorCount);

    if (use_sector_offsetting)
        m_sector_angle = GSM->getGlobal()->setting<Angle>(ES::RPBFSlicing::kSectorSize);
    else
        m_sector_angle = Angle(2 * M_PI / m_sector_count);

    QString comment;

    bool is_first_sector = true;
    bool first_in_block = true;
    int block_start_index = 0;

    Angle current_angle = m_clocking_angle;

    for (int line_index = 0, end = lines.size(); line_index < end; ++line_index) {
        auto line = lines[line_index];

        if (m_sector_header_matcher.indexIn(line) != -1) {
            if (is_first_sector)
                is_first_sector = false;
            else
                current_angle += m_sector_angle;
        }
        else if (m_layer_header_matcher.indexIn(line) != -1) // This should only be found once per call
        {
            is_first_sector = true;
            if (use_sector_offsetting)
                current_angle = m_clocking_angle + (m_sector_angle * total_sectors) * m_layer_count;
            else
                current_angle = m_clocking_angle;
        }
        else if (line.startsWith(m_poly_prefix) || line.startsWith(m_hatch_prefix)) {
            QStringList params = line.split(m_comma);
            // If more than a single command per line, visualization was skipped and no collapsing of commands
            // needs to happen.  All commands are contained on a single line, they simply need rotated.
            if (params.size() > 9) {
                for (int j = 2, end = params.size() - 1; j < end; j += 7) {
                    Point startPt(params[j].toFloat(), params[j + 1].toFloat());
                    startPt = startPt.rotateAround(m_origin, current_angle);
                    params[j] = QString::number(startPt.x(), 'f', 0);
                    params[j + 1] = QString::number(startPt.y(), 'f', 0);
                    Point endPt(params[j + 2].toFloat(), params[j + 3].toFloat());
                    endPt = endPt.rotateAround(m_origin, current_angle);
                    params[j + 2] = QString::number(endPt.x(), 'f', 0);
                    params[j + 3] = QString::number(endPt.y(), 'f', 0);
                    lines[line_index] = lines_copy[line_index] = params.join(m_comma);
                }
            }
            else {
                // There is a single command and subsequent commands must be collapsed onto a single line
                if (first_in_block) {
                    block_start_index = line_index;
                    first_in_block = false;

                    int commentIndex = params.last().indexOf(GcodeMetaList::RPBFMeta.m_comment_starting_delimiter);
                    comment = params.last().mid(commentIndex - 1);
                    params.last() = params.last().left(commentIndex - 1);
                    // QString line = lines[i].left(commentIndex - 1);

                    lines[line_index] = params.join(m_comma);
                    // QStringList params = line.split(comma);
                    params.replaceInStrings(m_spaces, QString());

                    Point startPt(params[2].toFloat(), params[3].toFloat());
                    startPt = startPt.rotateAround(m_origin, current_angle);
                    params[2] = QString::number(startPt.x(), 'f', 0);
                    params[3] = QString::number(startPt.y(), 'f', 0);
                    Point endPt(params[4].toFloat(), params[5].toFloat());
                    endPt = endPt.rotateAround(m_origin, current_angle);
                    params[4] = QString::number(endPt.x(), 'f', 0);
                    params[5] = QString::number(endPt.y(), 'f', 0);
                    lines_copy[line_index] = params.join(m_comma);
                }
                else {
                    // QStringList params = lines[i].split(comma);
                    params.removeFirst();
                    params.removeFirst();
                    params.replaceInStrings(m_spaces, QString());

                    int commentIndex = params.last().indexOf(GcodeMetaList::RPBFMeta.m_comment_starting_delimiter);
                    params.last() = params.last().left(commentIndex - 1);

                    lines[block_start_index].append(m_comma % params.join(m_comma));

                    Point startPt(params[0].toFloat(), params[1].toFloat());
                    startPt = startPt.rotateAround(m_origin, current_angle);
                    params[0] = QString::number(startPt.x(), 'f', 0);
                    params[1] = QString::number(startPt.y(), 'f', 0);
                    Point endPt(params[2].toFloat(), params[3].toFloat());
                    endPt = endPt.rotateAround(m_origin, current_angle);
                    params[2] = QString::number(endPt.x(), 'f', 0);
                    params[3] = QString::number(endPt.y(), 'f', 0);
                    lines_copy[block_start_index].append(m_comma % params.join(m_comma));

                    lines.removeAt(line_index);
                    lines_copy.removeAt(line_index);
                    --end;
                    --line_index;
                }
            }
        }
        else if (!first_in_block) {
            first_in_block = true;
            lines[block_start_index].append(comment);
        }
    }

    QString text_copy = lines_copy.join(m_new_line);
    QStringList sectors = text_copy.split(m_sector_header);

    if (sectors.first().startsWith(m_extra_header))
        sectors.pop_front();

    if (sectors.last() == m_new_line)
        sectors.pop_back();

    for (QString& sector : sectors) {
        int layer_header_index = m_layer_header_matcher.indexIn(sector);
        if (layer_header_index != -1)
            sector.removeAt(layer_header_index);

        sector.remove(m_comments);
        sector = sector.trimmed();

        if (sector.isEmpty())
            sector = " ";
    }

    ++m_layer_count;

    return sectors;
}

int RPBFExporter::getGlobalSectorCount() { return m_global_sector_count; }

int RPBFExporter::getScanHeadCount() { return m_scan_head_count; }
} // namespace ORNL
