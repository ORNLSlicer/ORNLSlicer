#include <QCoreApplication>
#include <QTemporaryDir>
#include <cmath>
#include <cstdlib>
#include <iostream>

#include "managers/preferences_manager.h"
#include "units/unit.h"

namespace {
bool near(double lhs, double rhs) {
    return std::abs(lhs - rhs) < 1.0e-9;
}

bool expect(bool condition, const char* message) {
    if (!condition) std::cerr << message << '\n';
    return condition;
}
}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    QTemporaryDir temp_dir;
    if (!expect(temp_dir.isValid(), "Could not create temporary directory.")) return EXIT_FAILURE;

    QSharedPointer<ORNL::PreferencesManager> preferences = ORNL::PreferencesManager::getInstance();

    preferences->setStepStlLinearDeflection(0.025);
    if (!expect(near(preferences->getStepStlLinearDeflection().to(ORNL::mm), 0.025),
                "STEP STL linear deflection should be set from millimeters."))
        return EXIT_FAILURE;

    const fifojson preferences_json = preferences->json();
    if (!expect(preferences_json.contains("step_stl_linear_deflection"),
                "STEP STL linear deflection should be exported with preferences JSON."))
        return EXIT_FAILURE;
    if (!expect(near(preferences_json["step_stl_linear_deflection"].get<double>(), (0.025 * ORNL::mm)()),
                "STEP STL linear deflection should be stored as a Distance value."))
        return EXIT_FAILURE;

    const QString preferences_path = temp_dir.path() + "/custom.preferences";
    preferences->exportPreferences(preferences_path);
    preferences->setStepStlLinearDeflection(1.5);
    preferences->importPreferences(preferences_path);
    if (!expect(near(preferences->getStepStlLinearDeflection().to(ORNL::mm), 0.025),
                "STEP STL linear deflection should round-trip through preferences files."))
        return EXIT_FAILURE;

    preferences->setStepStlLinearDeflection(0.0);
    if (!expect(near(preferences->getStepStlLinearDeflection().to(ORNL::mm), 0.001),
                "STEP STL linear deflection should be clamped to a positive minimum."))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
