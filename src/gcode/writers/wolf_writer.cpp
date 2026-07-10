#include "gcode/writers/wolf_writer.h"

namespace ORNL {
WolfWriter::WolfWriter(GcodeMeta meta, const QSharedPointer<SettingsBase>& sb) : ORNLWriter(meta, sb) {}
} // namespace ORNL