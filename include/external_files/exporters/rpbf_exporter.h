#pragma once

#include <utility>

#include <QRegularExpression>
#include <QString>
#include <qcontainerfwd.h>
#include <qlatin1stringview.h>
#include <qstringmatcher.h>

#include "gcode/gcode_meta.h"
#include "geometry/point.h"
#include "units/unit.h"

namespace ORNL {
//! \class RPBFExporter
//! \brief A class that can be used to condense and convert RPBF gcode into the proper format
class RPBFExporter {
  public:
    //! \brief Constructor
    //! \param root the root filepath to save to
    //! \param layer_restart the layer number to restart at
    //! \param global_sector_restart the global sector to restart at
    //! \param scan_head_restart the scan head to restart at
    RPBFExporter(QString root, int layer_restart = 0, int global_sector_restart = 1, int scan_head_restart = 0);

    //! \brief saves a layer to the proper file(s)
    //! \param text the raw gcode as text
    void saveLayer(QString text);

    //! \brief converts a raw gcode layer into the proper format for RPBF
    //! \note this condenses polyline/ hatch commands and removes comments
    //! \param text the raw gcode as text
    //! \return a list of sectors where each string in the list is proper RPBF gcode
    QStringList condenseLayer(QString text);

    //! \brief gets the total number of sectors processed
    //! \return return the number of sectors done
    int getGlobalSectorCount();

    //! \brief gets the scan head count
    //! \return return the scan head count
    int getScanHeadCount();

  private:
    //! \brief extracts a pair that contains the origin point and the sector count from header
    //! \param lines gcode
    //! \return a QPair where: first- origin point, second- sector count
    std::pair<Point, int> getMetaInfo(QStringList& lines);

    //! \brief Delimiters
    const QChar m_comma = ',', m_new_line = '\n';

    //! \brief Prefixes
    const QString m_poly_prefix = "$$POLYLINE/", m_hatch_prefix = "$$HATCHES/";

    //! \brief Headers
    const QString m_sector_header = "//SECTOR SPLIT//";
    const QString m_extra_header = "//G-Code generated";

    //! \brief Regular expressions
    const QRegularExpression m_spaces = QRegularExpression("^\\s+");
    const QRegularExpression m_comments = QRegularExpression(R"(//.*)");

    //! \brief String matchers
    const QStringMatcher m_sector_header_matcher = QStringMatcher(m_sector_header),
                         m_layer_header_matcher = QStringMatcher(GcodeMetaList::RPBFMeta.m_layer_delimiter);

    //! \brief File path additions
    const QString m_first_head = "/Head1/", m_second_head = "/Head2/";
    const QLatin1Char m_zero = QLatin1Char('0');
    const QLatin1String m_layer = QLatin1String("_Layer"), m_sec = QLatin1String("_Sec");

    Angle m_sector_angle;
    Angle m_clocking_angle;

    //! \brief these are pulled from settings
    int m_sector_count;
    Point m_origin;

    //! \brief running totals of layers, sectors, and heads
    int m_layer_count = 0;
    int m_global_sector_count = 1;
    int m_scan_head_count = 0;

    //! \brief where to save to
    QString m_path;
};
} // namespace ORNL
