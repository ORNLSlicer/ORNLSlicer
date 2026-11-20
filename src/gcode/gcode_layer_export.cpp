#include "gcode/gcode_layer_export.h"

namespace ORNL {
bool isGcodeLayerFileName(const QString& file_name, const QString& part_name, const QString& suffix) {
    const QString prefix = part_name + "_layer_";
    if (!file_name.startsWith(prefix) || !file_name.endsWith(suffix)) { return false; }

    const qsizetype number_length = file_name.size() - prefix.size() - suffix.size();
    if (number_length <= 0) { return false; }

    const QString number_text = file_name.mid(prefix.size(), number_length);
    bool converted            = false;
    const int layer_number    = number_text.toInt(&converted);
    return converted && layer_number >= 0 && QString::number(layer_number) == number_text;
}
}  // namespace ORNL
