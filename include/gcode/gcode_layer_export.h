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
}  // namespace ORNL
