#include "gcode/gcode_layer_export.h"

#include <QRegularExpression>
#include <QStringList>

namespace ORNL {
QVector<GcodeLayer> splitGcodeIntoLayers(const QString& text, const QString& comment_starting_delimiter) {
    if (comment_starting_delimiter.isEmpty()) { return {}; }

    const QRegularExpression layer_marker(
        "^\\s*" + QRegularExpression::escape(comment_starting_delimiter) + "\\s*BEGINNING\\s+LAYER\\s*:\\s*(\\d+)\\b",
        QRegularExpression::CaseInsensitiveOption);
    const QStringList lines = text.split('\n');

    QVector<int> layer_starts;
    QVector<int> layer_numbers;
    for (int line_index = 0; line_index < lines.size(); ++line_index) {
        const QRegularExpressionMatch match = layer_marker.match(lines[line_index]);
        if (!match.hasMatch()) { continue; }

        bool converted         = false;
        const int layer_number = match.captured(1).toInt(&converted);
        if (!converted) { continue; }

        layer_starts.push_back(line_index);
        layer_numbers.push_back(layer_number);
    }

    if (layer_starts.isEmpty()) { return {}; }

    const bool has_header = layer_starts.first() > 0;
    const QString header  = lines.mid(0, layer_starts.first()).join('\n');
    QVector<GcodeLayer> layers;
    layers.reserve(layer_starts.size());

    for (int layer_index = 0; layer_index < layer_starts.size(); ++layer_index) {
        const int start    = layer_starts[layer_index];
        const int end      = (layer_index + 1 < layer_starts.size()) ? layer_starts[layer_index + 1] : lines.size();
        QString layer_text = lines.mid(start, end - start).join('\n');

        if (end < lines.size()) { layer_text += '\n'; }
        if (has_header) { layer_text.prepend(header + '\n'); }

        layers.push_back({layer_numbers[layer_index], layer_text});
    }

    return layers;
}

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
