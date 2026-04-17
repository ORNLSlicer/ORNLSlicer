#pragma once

#include <qhashfunctions.h>
#include <qsharedpointer.h>

#include "configs/settings_base.h"

namespace ORNL {
/*!
 *  \class SettingsRange
 *  \brief A layer range of settings
 */
class SettingsRange {
  public:
    //! \brief creates a new settings range
    //! \param low the lower layer number to start at
    //! \param high the upper layer number to end at
    //! \param group_name the name of the group this belongs to
    //! \param sb the settings
    SettingsRange(int low, int high, QString group_name, QSharedPointer<SettingsBase> sb);

    //! \brief creates a new settings range
    //! \param low the lower layer number to start at
    //! \param high the upper layer number to end at
    //! \param group_name the name of the group this belongs to
    SettingsRange(int low, int high, QString group_name);

    //! \brief the lower layer number to start at
    //! \return a layer index
    int low();

    //! \brief the upper layer number to end at
    //! \return a layer index
    int high();

    //! \brief the name of the group this belongs to
    //! \return a group name
    QString groupName();

    //! \brief if the layer index is in this range
    //! \param index the layer index
    //! \return if it is in the range
    bool includesIndex(int index);

    //! \brief checks if this layer is a single dot
    //! \return if this is a single layer range
    bool isSingle();

    //! \brief gets the settings
    //! \return the settings base for this range
    QSharedPointer<SettingsBase> getSb();

    //! \brief sets the settings
    //! \param sb the settings base for this range
    void setSb(QSharedPointer<SettingsBase> sb);

    //! \brief sets this range's group
    //! \param group the name of the group
    void setGroup(QString group);

  private:
    // Layer number locations
    int m_low;
    int m_high;

    // Group
    QString m_group_name = "";

    // Settings
    QSharedPointer<SettingsBase> m_sb;
};
} // namespace ORNL
