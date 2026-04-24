#pragma once

#include <QSharedPointer>
#include <QString>
#include <nlohmann/json.hpp>

#include "utilities/qt_json_conversion.h"

namespace ORNL {
/*!
 * \class SettingsBase
 * \brief Base class for all settings containers
 */

class SettingsBase {
  public:
    //! \brief Default Constructor
    SettingsBase();

    /*!
     * \brief update the value of a setting
     *
     * \note This function is templated and needs to stay in the header (best option)
     */
    template <typename T> void setSetting(QString key, T value, int extruder_index = 0) {
        m_json[extruder_index][key.toStdString()] = value;
    }

    /*!
     * \brief Returns value of setting
     *
     * \note This function is templated and needs to stay in the header (best option)
     */
    template <typename T> T setting(QString key_root, int extruder_index = 0) const {
        // If this sb contains the key, return it.
        T return_value;
        // No longer use suffixing so full key is same as root
        QString key_full = key_root;
        if (this->contains(key_root, extruder_index)) {
            // prefer suffixed setting
            return_value = m_json[extruder_index][key_full.toStdString()].get<T>();
        }
        else {
            return_value = T(); // Found nothing, return default value.
        }
        return return_value;
    }

    // Update this settings base with the settings from another base. The result is a union of the two bases.
    void populate(const QSharedPointer<SettingsBase> other);
    // void populate(const nlohmann::json& j);
    void populate(const fifojson& j);

    // Remove settings from current base
    void splice(const fifojson& j);

    /*! \brief Returns whether a settings is contained in the settingsbase.
     *
     * \note This function is templated and needs to stay in the header (best option)
     */
    bool contains(QString key, int extruder_index = 0) const;

    //! \brief Return empty status.
    bool empty() const;

    //! \brief Remove setting.
    void remove(QString key, int extruder_index = 0);

    // Clear the associated json.
    void reset();

    //! \brief Returns json from the settings
    fifojson& json();

    //! \brief Sets the internal json to the passed object.
    void json(const fifojson& j);

    //! \brief adjusts this settings base according to programmatic conflicts of settings
    void makeGlobalAdjustments();

    //! \brief adjusts this settings base according to programmatic conflicts of settings and layer number
    //! \param the layer number to adjust for
    void makeLocalAdjustments(int layer_number = 0);

  protected:
    // Json array.
    fifojson m_json = fifojson::array({});
};
} // namespace ORNL
