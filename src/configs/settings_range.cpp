#include "configs/settings_range.h"

#include <qhashfunctions.h>
#include <qsharedpointer.h>

#include "configs/settings_base.h"

namespace ORNL {
SettingsRange::SettingsRange(int low, int high, QString group_name, QSharedPointer<SettingsBase> sb) {
    m_low = low;
    m_high = high;
    m_group_name = group_name;
    m_sb = sb;
}

SettingsRange::SettingsRange(int low, int high, QString group_name) {
    m_low = low;
    m_high = high;
    m_group_name = group_name;
    m_sb = QSharedPointer<SettingsBase>::create();
}

int SettingsRange::low() { return m_low; }

int SettingsRange::high() { return m_high; }

QString SettingsRange::groupName() { return m_group_name; }

QSharedPointer<SettingsBase> SettingsRange::getSb() { return m_sb; }

void SettingsRange::setSb(QSharedPointer<SettingsBase> sb) { m_sb = sb; }

bool SettingsRange::includesIndex(int index) { return index >= m_low && index <= m_high; }

bool SettingsRange::isSingle() { return (m_low == m_high); }

void SettingsRange::setGroup(QString group) { m_group_name = group; }
} // namespace ORNL
