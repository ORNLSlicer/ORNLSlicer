#pragma once

#include <QString>

#include "configs/settings_base.h"

namespace ORNL {
//! \brief Evaluates the setting editor's feature dependencies without creating widgets.
class SettingActivity {
   public:
    //! \brief Returns why a setting is inactive, or an empty string for active or unknown settings.
    static QString inactiveReason(const QString& key, const SettingsBase& effective, const fifojson& master,
                                  const fifojson& input = fifojson());
};
}  // namespace ORNL
