#pragma once

#include <QString>
#include <QVector>

namespace ORNL {
//! \brief A standalone layer program generated during slicing.
struct GcodeLayer {
    int number;
    QString text;
};

//! \brief Returns whether a file name matches the name generated for an individually exported layer.
//! \param file_name File name without its directory.
//! \param part_name Base name selected for the export.
//! \param suffix G-code file suffix, including its leading period.
bool isGcodeLayerFileName(const QString& file_name, const QString& part_name, const QString& suffix);
}  // namespace ORNL
