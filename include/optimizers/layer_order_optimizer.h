#pragma once

#include <qcontainerfwd.h>
#include <qlist.h>
#include <qsharedpointer.h>

#include "configs/settings_base.h"
#include "part/part.h"
#include "step/global_layer.h"

namespace ORNL {
/*!
 * \class LayerOrderOptimizer
 * \brief Creates and orders global layers according to the user-selected schema
 */
class LayerOrderOptimizer {
  public:
    //! \brief Creates and orders global layers
    //! \param global_sb: global settings base
    //! \param build_parts: list of build parts to access steps
    static QList<QSharedPointer<GlobalLayer>> populateSteps(QSharedPointer<SettingsBase> global_sb,
                                                            QVector<QSharedPointer<Part>> build_parts);
};
} // namespace ORNL
