#pragma once

#include <QString>
#include <QVector>

namespace ORNL {
//! \brief A single layer extracted from a generated G-code file.
struct GcodeLayer {
    int number;
    QString text;
};

//! \brief Splits generated G-code into layer files while preserving the original header.
//! \param text Complete G-code text, including its header.
//! \param comment_starting_delimiter G-code comment delimiter used before layer markers.
//! \return One entry for each BEGINNING LAYER marker, with the header prepended.
QVector<GcodeLayer> splitGcodeIntoLayers(const QString& text, const QString& comment_starting_delimiter);

//! \brief Returns whether a file name matches the name generated for an individually exported layer.
//! \param file_name File name without its directory.
//! \param part_name Base name selected for the export.
//! \param suffix G-code file suffix, including its leading period.
bool isGcodeLayerFileName(const QString& file_name, const QString& part_name, const QString& suffix);
}  // namespace ORNL
