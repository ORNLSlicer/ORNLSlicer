#include "configs/settings_base.h"

namespace ORNL
{
    SettingsBase::SettingsBase() {//: m_json(nlohmann::json::object()){
        // NOP
    }

    void SettingsBase::populate(const QSharedPointer<SettingsBase> other) {
        this->populate(other->m_json);
    }

    void SettingsBase::populate(const fifojson& j) {
        int index=0;
        for(auto& array : j.items()){
            for (auto it : array.value().items()) {
                m_json[index][it.key()] = it.value();
            }
            index++;
        }
    }

    void SettingsBase::splice(const fifojson& j) {
        for(auto& array : j.items()){
            for (auto it : array.value().items()) {
                m_json.erase(it.key());
            }
        }
    }

    bool SettingsBase::contains(QString key,  int extruder_index) const {
        if(m_json.size()==0){
            return false;
        }
        if(extruder_index == 0)
        {
            return m_json[extruder_index].contains(key.toStdString());
        }else
            return false;
    }

    bool SettingsBase::empty() const {
        return m_json.empty();
    }

    void SettingsBase::remove(QString key, int extruder_index) {
        m_json[extruder_index].erase((key).toStdString());
    }

    void SettingsBase::reset() {
         m_json.clear();
    }

    fifojson& SettingsBase::json() {
        return m_json;
    }

    void SettingsBase::json(const fifojson& j) {
        m_json = j;
    }

    void SettingsBase::makeGlobalAdjustments()
    {
        //Spiralize - should print one single perimeter bead on every layer without and modifiers
        if(setting<bool>(Constants::ProfileSettings::SpecialModes::kEnableSpiralize))
        {
            //Perimeter
            setSetting(Constants::ProfileSettings::Perimeter::kCount, 1);
            //Inset
            setSetting(Constants::ProfileSettings::Inset::kEnable, false);
            //Skeleton
            setSetting(Constants::ProfileSettings::Skeleton::kEnable, false);
            //Skin
            setSetting(Constants::ProfileSettings::Skin::kEnable, false);
            //Infill
            setSetting(Constants::ProfileSettings::Infill::kEnable, false);
            //Support
            setSetting(Constants::ProfileSettings::Support::kEnable, false);
            //Laser Scanner
            setSetting(Constants::ProfileSettings::LaserScanner::kLaserScanner, false);
            //Thermal Scanner
            setSetting(Constants::ProfileSettings::ThermalScanner::kThermalScanner, false);
            //Platform Adhesion
            setSetting(Constants::MaterialSettings::PlatformAdhesion::kRaftEnable, false);
            setSetting(Constants::MaterialSettings::PlatformAdhesion::kBrimEnable, false);
            setSetting(Constants::MaterialSettings::PlatformAdhesion::kSkirtEnable, false);

            setSetting(Constants::MaterialSettings::TipWipe::kPerimeterEnable, false);
        }
    }

    void SettingsBase::makeLocalAdjustments(int layer_number)
    {
        makeGlobalAdjustments();

        //perimeter adjustment - by default if on or off for all layers, settings will reflect that
        //if alternating, certain layers need adjusting
        PrintDirection perimeterDirection = static_cast<PrintDirection>(setting<int>(Constants::ProfileSettings::Ordering::kPerimeterReverseDirection));
        if(perimeterDirection == PrintDirection::kReverse_Alternating_Layers)
        {
            if(layer_number % 2 == 0)
                setSetting(Constants::ProfileSettings::Ordering::kPerimeterReverseDirection, (int)PrintDirection::kReverse_off);
            else
                setSetting(Constants::ProfileSettings::Ordering::kPerimeterReverseDirection, (int)PrintDirection::kReverse_All_Layers);
        }

        //inset adjustment - by default if on or off for all layers, settings will reflect that
        //if alternating, certain layers need adjusting
        PrintDirection insetDirection = static_cast<PrintDirection>(setting<int>(Constants::ProfileSettings::Ordering::kInsetReverseDirection));
        if(insetDirection == PrintDirection::kReverse_Alternating_Layers)
        {
            if(layer_number % 2 == 0)
                setSetting(Constants::ProfileSettings::Ordering::kInsetReverseDirection, (int)PrintDirection::kReverse_off);
            else
                setSetting(Constants::ProfileSettings::Ordering::kInsetReverseDirection, (int)PrintDirection::kReverse_All_Layers);
        }

        //alternating seam adjustment
        PointOrderOptimization pointOrder = static_cast<PointOrderOptimization>(setting<int>(Constants::ProfileSettings::Optimizations::kPointOrder));
        if(pointOrder == PointOrderOptimization::kCustomPoint && setting<bool>(Constants::ProfileSettings::Optimizations::kEnableSecondCustomLocation)
            && setting<bool>(Constants::ProfileSettings::Optimizations::kEnableSecondCustomLocationEveryTwo))
        {
            if(layer_number % 4 == 0)
            {
                setSetting(Constants::ProfileSettings::Optimizations::kCustomPointXLocation,
                               (double)setting<double>(Constants::ProfileSettings::Optimizations::kCustomPointSecondXLocation));
                setSetting(Constants::ProfileSettings::Optimizations::kCustomPointYLocation,
                               (double)setting<double>(Constants::ProfileSettings::Optimizations::kCustomPointSecondYLocation));
            }
            else if(layer_number % 3 == 0)
            {
                setSetting(Constants::ProfileSettings::Optimizations::kCustomPointXLocation,
                           (double)setting<double>(Constants::ProfileSettings::Optimizations::kCustomPointSecondXLocation));
                setSetting(Constants::ProfileSettings::Optimizations::kCustomPointYLocation,
                           (double)setting<double>(Constants::ProfileSettings::Optimizations::kCustomPointSecondYLocation));
            }
        }
        else if(pointOrder == PointOrderOptimization::kCustomPoint && setting<bool>(Constants::ProfileSettings::Optimizations::kEnableSecondCustomLocation))
        {
            if(layer_number % 2 == 0)
            {
                setSetting(Constants::ProfileSettings::Optimizations::kCustomPointXLocation,
                           (double)setting<double>(Constants::ProfileSettings::Optimizations::kCustomPointSecondXLocation));
                setSetting(Constants::ProfileSettings::Optimizations::kCustomPointYLocation,
                           (double)setting<double>(Constants::ProfileSettings::Optimizations::kCustomPointSecondYLocation));
            }
        }

        if(setting<bool>(Constants::ProfileSettings::Infill::kEnable))
        {
            //modify the infill_angle for each specific layer before the setting being passed to the island and region
            Angle infill_angle = setting<Angle>(Constants::ProfileSettings::Infill::kAngle);
            Angle infill_angle_rotation = setting<Angle>(Constants::ProfileSettings::Infill::kAngleRotation);

            //For issue #239: combine infill for every X layers
            int combineXLayers = setting<int>(Constants::ProfileSettings::Infill::kCombineXLayers);
            int combineLayerShift = setting<int>(Constants::ProfileSettings::Infill::kCombineLayerShift);
            if(combineXLayers > 1)
            {
                //layer_number is 0 based, while Layer::m_layer_nr is 1 based
                if((combineLayerShift >= layer_number + 1) ||
                    (((layer_number + 1 - combineLayerShift) > 0) &&
                     ((layer_number + 1 - combineLayerShift) % combineXLayers != 0)))
                    setSetting(Constants::ProfileSettings::Infill::kEnable, false);
                else
                    infill_angle = infill_angle + infill_angle_rotation * (layer_number / combineXLayers);
            }
            else
            {
                infill_angle = infill_angle + infill_angle_rotation * layer_number;
            }
            setSetting(Constants::ProfileSettings::Infill::kAngle, infill_angle);
        }

        if(setting<bool>(Constants::ProfileSettings::Skin::kEnable))
        {
            //modify the skin_angle for each specific layer before the setting being passed to the island and region
            Angle skin_angle = setting<Angle>(Constants::ProfileSettings::Skin::kAngle);
            Angle skin_angle_rotation = setting<Angle>(Constants::ProfileSettings::Skin::kAngleRotation);
            skin_angle = skin_angle + skin_angle_rotation * layer_number;
            setSetting(Constants::ProfileSettings::Skin::kAngle, skin_angle);

            if(setting<bool>(Constants::ProfileSettings::Skin::kInfillEnable))
            {
                Angle skin_infill_angle = setting<Angle>(Constants::ProfileSettings::Skin::kInfillAngle);
                Angle skin_infill_angle_rotation = setting<Angle>(Constants::ProfileSettings::Skin::kInfillRotation);
                skin_infill_angle = skin_infill_angle + skin_infill_angle_rotation * layer_number;
                setSetting(Constants::ProfileSettings::Skin::kInfillAngle, skin_infill_angle);
            }
        }

        if(setting<bool>(Constants::ExperimentalSettings::RPBFSlicing::kSectorStaggerEnable))
        {
            Angle staggerAngle = setting<Angle>(Constants::ExperimentalSettings::RPBFSlicing::kSectorStaggerAngle);
            if(layer_number % 2 == 1)
                setSetting(Constants::ExperimentalSettings::RPBFSlicing::kSectorStaggerAngle, staggerAngle * -1);
            else
                setSetting(Constants::ExperimentalSettings::RPBFSlicing::kSectorStaggerAngle, 0);
        }
    }
}  // namespace ORNL
