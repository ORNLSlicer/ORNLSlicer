#pragma once

#include "gcode/writers/ornl_writer.h"

namespace ORNL {
/*!
 * \class WolfWriter
 * \brief The gcode writer for the Wolf syntax
 */
class WolfWriter : public ORNLWriter {
  public:
    //! \brief Constructor
    WolfWriter(GcodeMeta meta, const QSharedPointer<SettingsBase>& sb);
}; // class WolfWriter
} // namespace ORNL
