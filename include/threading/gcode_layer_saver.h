#pragma once

#include <QString>
#include <QThread>

#include <qtmetamacros.h>

#include "gcode/gcode_layer_export.h"

namespace ORNL {
class GCodeLayerSaver : public QThread {
    Q_OBJECT
   public:
    GCodeLayerSaver(QString path, QString part_name, QVector<GcodeLayer> layers, QString suffix);

    void run() override;

   signals:
    void completed(QString error);

   private:
    QString m_path;
    QString m_part_name;
    QVector<GcodeLayer> m_layers;
    QString m_suffix;
};
}  // namespace ORNL
