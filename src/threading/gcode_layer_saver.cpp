#include "threading/gcode_layer_saver.h"

#include <QDir>
#include <QFile>
#include <QSet>
#include <QTextStream>

#include "gcode/gcode_layer_export.h"

namespace ORNL {
GCodeLayerSaver::GCodeLayerSaver(QString path, QString part_name, QVector<GcodeLayer> layers, QString suffix)
    : m_path(path), m_part_name(part_name), m_layers(std::move(layers)), m_suffix(suffix) {}

void GCodeLayerSaver::run() {
    if (m_layers.isEmpty()) {
        emit completed("No standalone layer programs available. Enable Individual Layer Export and re-slice.");
        return;
    }

    QSet<QString> exported_layer_file_names;
    for (const GcodeLayer& layer : m_layers) {
        const QString layer_base_name = m_part_name + "_layer_" + QString::number(layer.number) + m_suffix;
        const QString layer_file_name = QDir(m_path).filePath(layer_base_name);
        if (QFile::exists(layer_file_name)) { QFile::remove(layer_file_name); }

        QFile output_file(layer_file_name);
        if (!output_file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            emit completed("Could not write layer file: " + layer_file_name);
            return;
        }

        QTextStream out(&output_file);
        out << layer.text;
        out.flush();
        if (out.status() != QTextStream::Ok || !output_file.flush()) {
            emit completed("Could not write layer file: " + layer_file_name);
            return;
        }
        output_file.close();
        exported_layer_file_names.insert(layer_base_name);
    }

    QDir output_directory(m_path);
    const QStringList existing_file_names = output_directory.entryList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QString& existing_file_name : existing_file_names) {
        if (exported_layer_file_names.contains(existing_file_name) ||
            !isGcodeLayerFileName(existing_file_name, m_part_name, m_suffix)) {
            continue;
        }

        if (!output_directory.remove(existing_file_name)) {
            emit completed("Could not remove obsolete layer file: " + output_directory.filePath(existing_file_name));
            return;
        }
    }

    emit completed(QString());
}
}  // namespace ORNL
