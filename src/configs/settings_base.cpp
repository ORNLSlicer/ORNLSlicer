#include "configs/settings_base.h"

#include <qhashfunctions.h>
#include <qsharedpointer.h>

#include "geometry/point.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "utilities/qt_json_conversion.h"

namespace ORNL {
SettingsBase::SettingsBase() { //: m_json(nlohmann::json::object()){
    // NOP
}

void SettingsBase::populate(const QSharedPointer<SettingsBase> other) { this->populate(other->m_json); }

void SettingsBase::populate(const fifojson& j) {
    int index = 0;
    for (auto& array : j.items()) {
        for (auto it : array.value().items()) {
            m_json[index][it.key()] = it.value();
        }
        index++;
    }
}

void SettingsBase::splice(const fifojson& j) {
    for (auto& array : j.items()) {
        for (auto it : array.value().items()) {
            m_json.erase(it.key());
        }
    }
}

bool SettingsBase::contains(QString key, int extruder_index) const {
    if (m_json.size() == 0) {
        return false;
    }
    if (extruder_index == 0) {
        return m_json[extruder_index].contains(key.toStdString());
    }
    else
        return false;
}

bool SettingsBase::empty() const { return m_json.empty(); }

void SettingsBase::remove(QString key, int extruder_index) { m_json[extruder_index].erase((key).toStdString()); }

void SettingsBase::reset() { m_json.clear(); }

fifojson& SettingsBase::json() { return m_json; }

void SettingsBase::json(const fifojson& j) { m_json = j; }

void SettingsBase::makeGlobalAdjustments() {
    // Spiralize - should print one single perimeter bead on every layer without and modifiers
    if (setting<bool>(PS::SpecialModes::kEnableSpiralize)) {
        // Perimeter
        setSetting(PS::Perimeter::kCount, 1);
        // Inset
        setSetting(PS::Inset::kEnable, false);
        // Skeleton
        setSetting(PS::Skeleton::kEnable, false);
        // Skin
        setSetting(PS::Skin::kEnable, false);
        // Infill
        setSetting(PS::Infill::kEnable, false);
        // Support
        setSetting(PS::Support::kEnable, false);
        // Laser Scanner
        setSetting(PS::LaserScanner::kLaserScanner, false);
        // Thermal Scanner
        setSetting(PS::ThermalScanner::kThermalScanner, false);
        // Platform Adhesion
        setSetting(MS::PlatformAdhesion::kRaftEnable, false);
        setSetting(MS::PlatformAdhesion::kBrimEnable, false);
        setSetting(MS::PlatformAdhesion::kSkirtEnable, false);

        setSetting(MS::TipWipe::kPerimeterEnable, false);
    }
}

void SettingsBase::makeLocalAdjustments(int layer_number) {
    makeGlobalAdjustments();

    // perimeter adjustment - by default if on or off for all layers, settings will reflect that
    // if alternating, certain layers need adjusting
    PrintDirection perimeterDirection =
        static_cast<PrintDirection>(setting<int>(PS::Ordering::kPerimeterReverseDirection));
    if (perimeterDirection == PrintDirection::kReverse_Alternating_Layers) {
        if (layer_number % 2 == 0)
            setSetting(PS::Ordering::kPerimeterReverseDirection, (int)PrintDirection::kReverse_off);
        else
            setSetting(PS::Ordering::kPerimeterReverseDirection, (int)PrintDirection::kReverse_All_Layers);
    }

    // inset adjustment - by default if on or off for all layers, settings will reflect that
    // if alternating, certain layers need adjusting
    PrintDirection insetDirection = static_cast<PrintDirection>(setting<int>(PS::Ordering::kInsetReverseDirection));
    if (insetDirection == PrintDirection::kReverse_Alternating_Layers) {
        if (layer_number % 2 == 0)
            setSetting(PS::Ordering::kInsetReverseDirection, (int)PrintDirection::kReverse_off);
        else
            setSetting(PS::Ordering::kInsetReverseDirection, (int)PrintDirection::kReverse_All_Layers);
    }

    PointOrderOptimization point_order =
        static_cast<PointOrderOptimization>(setting<int>(PS::Optimizations::kPointOrder));

    // alternating seam adjustment
    if (point_order == PointOrderOptimization::kCustomPoint) {
        Point p1(setting<double>(PS::Optimizations::kCustomPointXLocation),
                 setting<double>(PS::Optimizations::kCustomPointYLocation));
        Point p2(setting<double>(PS::Optimizations::kCustomPointSecondXLocation),
                 setting<double>(PS::Optimizations::kCustomPointSecondYLocation));
        double dx = setting<double>(PS::Optimizations::kCustomPointXIncrement);
        double dy = setting<double>(PS::Optimizations::kCustomPointYIncrement);

        bool enable_second = setting<bool>(PS::Optimizations::kEnableSecondCustomLocation);
        bool enable_second_every_two = setting<bool>(PS::Optimizations::kEnableSecondCustomLocationEveryTwo);

        if (enable_second && enable_second_every_two) {
            if (layer_number % 3 == 0 || layer_number % 4 == 0) {
                setSetting(PS::Optimizations::kCustomPointXLocation, p2.x());
                setSetting(PS::Optimizations::kCustomPointYLocation, p2.y());
            }
        }
        else if (enable_second) {
            if (layer_number % 2 == 0) {
                setSetting(PS::Optimizations::kCustomPointXLocation, p2.x() + (dx * layer_number));
                setSetting(PS::Optimizations::kCustomPointYLocation, p2.y() + (dy * layer_number));
            }
            else {
                setSetting(PS::Optimizations::kCustomPointXLocation, p1.x() + (dx * layer_number));
                setSetting(PS::Optimizations::kCustomPointYLocation, p1.y() + (dy * layer_number));
            }
        }
        else {
            setSetting(PS::Optimizations::kCustomPointXLocation, p1.x() + (dx * layer_number));
            setSetting(PS::Optimizations::kCustomPointYLocation, p1.y() + (dy * layer_number));
        }
    }

    if (setting<bool>(PS::Infill::kEnable)) {
        // modify the infill_angle for each specific layer before the setting being passed to the island and region
        Angle infill_angle = setting<Angle>(PS::Infill::kAngle);
        Angle infill_angle_rotation = setting<Angle>(PS::Infill::kAngleRotation);

        // For issue #239: combine infill for every X layers
        int combineXLayers = setting<int>(PS::Infill::kCombineXLayers);
        int combineLayerShift = setting<int>(PS::Infill::kCombineLayerShift);
        if (combineXLayers > 1) {
            // layer_number is 0 based, while Layer::m_layer_nr is 1 based
            if ((combineLayerShift >= layer_number + 1) ||
                (((layer_number + 1 - combineLayerShift) > 0) &&
                 ((layer_number + 1 - combineLayerShift) % combineXLayers != 0))) {
                setSetting(PS::Infill::kEnable, false);
            }
            else {
                infill_angle = infill_angle + infill_angle_rotation * (layer_number / combineXLayers);
            }
        }
        else {
            infill_angle = infill_angle + infill_angle_rotation * layer_number;
        }
        setSetting(PS::Infill::kAngle, infill_angle);
    }

    if (setting<bool>(PS::Skin::kEnable)) {
        // modify the skin_angle for each specific layer before the setting being passed to the island and region
        Angle skin_angle = setting<Angle>(PS::Skin::kAngle);
        Angle skin_angle_rotation = setting<Angle>(PS::Skin::kAngleRotation);
        skin_angle = skin_angle + skin_angle_rotation * layer_number;
        setSetting(PS::Skin::kAngle, skin_angle);

        if (setting<bool>(PS::Skin::kInfillEnable)) {
            Angle skin_infill_angle = setting<Angle>(PS::Skin::kInfillAngle);
            Angle skin_infill_angle_rotation = setting<Angle>(PS::Skin::kInfillRotation);
            skin_infill_angle = skin_infill_angle + skin_infill_angle_rotation * layer_number;
            setSetting(PS::Skin::kInfillAngle, skin_infill_angle);
        }
    }

    if (setting<bool>(ES::RPBFSlicing::kSectorStaggerEnable)) {
        Angle staggerAngle = setting<Angle>(ES::RPBFSlicing::kSectorStaggerAngle);
        if (layer_number % 2 == 1)
            setSetting(ES::RPBFSlicing::kSectorStaggerAngle, staggerAngle * -1);
        else
            setSetting(ES::RPBFSlicing::kSectorStaggerAngle, 0);
    }
}
} // namespace ORNL
