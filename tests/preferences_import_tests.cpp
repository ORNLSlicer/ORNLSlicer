#include <QFile>
#include <QObject>
#include <QTemporaryDir>
#include <cstdlib>

#include "managers/preferences_manager.h"
#include "test_utils.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace {
bool writePreferences(const QString& path, const fifojson& preferences) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return false;

    return file.write(preferences.dump(4).c_str()) >= 0;
}
}  // namespace

int main() {
    QTemporaryDir temp_dir;
    if (!ORNL::Testing::expect(temp_dir.isValid(), "Could not create a temporary directory.")) return EXIT_FAILURE;

    const auto preferences_manager = ORNL::PreferencesManager::getInstance();
    fifojson imported_preferences  = preferences_manager->json();

    const ORNL::Distance imported_distance = preferences_manager->getDistanceUnit() == ORNL::mm ? ORNL::in : ORNL::mm;
    const bool imported_invert_camera      = !preferences_manager->invertCamera();
    const int imported_layer_lag           = preferences_manager->getLayerLag() == 321 ? 654 : 321;
    const int imported_segment_lag         = preferences_manager->getSegmentLag() == 32 ? 65 : 32;
    const QColor imported_travel_color(QStringLiteral("#123456"));

    imported_preferences["distance"]                       = imported_distance;
    imported_preferences["invert_camera"]                  = imported_invert_camera;
    imported_preferences["layer_lag"]                      = imported_layer_lag;
    imported_preferences["segment_lag"]                    = imported_segment_lag;
    imported_preferences["visualization_colors"]["Travel"] = imported_travel_color.name().toStdString();

    const QString preferences_path = temp_dir.path() + "/import.preferences";
    if (!ORNL::Testing::expect(writePreferences(preferences_path, imported_preferences),
                               "Could not write preference fixture."))
        return EXIT_FAILURE;

    int aggregate_unit_change_count  = 0;
    bool signal_observed_final_state = false;
    QObject::connect(preferences_manager.get(), &ORNL::PreferencesManager::anyUnitChanged, [&] {
        ++aggregate_unit_change_count;
        signal_observed_final_state =
            preferences_manager->getDistanceUnit() == imported_distance &&
            preferences_manager->invertCamera() == imported_invert_camera &&
            preferences_manager->getLayerLag() == imported_layer_lag &&
            preferences_manager->getSegmentLag() == imported_segment_lag &&
            preferences_manager->getVisualizationColor(ORNL::VisualizationColors::kTravel) == imported_travel_color;
    });

    preferences_manager->importPreferences(preferences_path);

    bool passed = true;
    passed &= ORNL::Testing::expect(aggregate_unit_change_count == 1,
                                    "A preference import should emit one aggregate unit-change notification.");
    passed &= ORNL::Testing::expect(signal_observed_final_state,
                                    "Import consumers should only observe the complete imported state.");
    passed &= ORNL::Testing::expect(preferences_manager->getDistanceUnit() == imported_distance,
                                    "The imported distance unit was not applied.");
    passed &= ORNL::Testing::expect(preferences_manager->invertCamera() == imported_invert_camera,
                                    "The imported camera preference was not applied.");
    passed &= ORNL::Testing::expect(preferences_manager->getLayerLag() == imported_layer_lag,
                                    "The imported layer lag was not applied.");
    passed &= ORNL::Testing::expect(preferences_manager->getSegmentLag() == imported_segment_lag,
                                    "The imported segment lag was not applied.");
    passed &= ORNL::Testing::expect(
        preferences_manager->getVisualizationColor(ORNL::VisualizationColors::kTravel) == imported_travel_color,
        "The imported visualization color was not applied.");

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
