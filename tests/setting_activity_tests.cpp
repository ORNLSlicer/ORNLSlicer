#include <QCoreApplication>
#include <QFile>
#include <cstdlib>

#include "configs/setting_activity.h"
#include "test_utils.h"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    Q_INIT_RESOURCE(configs);
    bool passed       = true;
    const auto expect = [&passed](bool condition, const char* message) {
        passed &= ORNL::Testing::expect(condition, message);
    };
    fifojson master = {
        {"infill", {{"display", "Enable Infill"}, {"type", "boolean"}, {"default", false}}},
        {"mode",
         {{"display", "Slicing Mode"},
          {"type", "enumeration"},
          {"options", "Planar, Cylindrical, Image"},
          {"default", 0}}},
        {"count", {{"display", "Revolutions"}, {"type", "non_negative_int"}, {"default", 0}}},
        {"speed",
         {{"display", "Infill Speed"}, {"type", "speed"}, {"default", 1000}, {"depends", {{"infill", true}}}}}};
    ORNL::SettingsBase global;
    global.setSetting("speed", 2000.0);
    const auto reason = [&](const ORNL::SettingsBase& effective, const fifojson& input = fifojson()) {
        return ORNL::SettingActivity::inactiveReason("speed", effective, master, input);
    };
    expect(reason(global).contains("Enable Infill = true") && reason(global).contains("Enable Infill = false"),
           "Inactive boolean dependencies must explain the required and current feature states.");
    auto part = QSharedPointer<ORNL::SettingsBase>::create(global);
    part->setSetting("infill", true);
    expect(reason(*part).isEmpty(), "A part can enable a feature disabled by the global default.");
    ORNL::SettingsBase layer;
    layer.populate(part);
    layer.setSetting("infill", false);
    expect(!reason(layer).isEmpty(), "A layer override must use its own effective feature state.");
    layer.setSetting("infill", true);
    expect(reason(layer).isEmpty(), "Re-enabling the layer feature must reactivate its stored settings.");
    expect(ORNL::SettingActivity::inactiveReason("unknown", layer, master).isEmpty(),
           "Unknown settings must not fail evaluation or invent an inactivity reason.");

    master["speed"]["depends"] = {{"AND", {{{"mode", 0}}, {{"infill", true}}}}};
    expect(reason(layer).isEmpty(),
           "AND must accept both satisfied conditions, using master defaults for missing keys.");
    layer.setSetting("mode", 1);
    expect(reason(layer).contains("Slicing Mode = Planar") && reason(layer).contains("Slicing Mode = Cylindrical"),
           "Enumeration requirements and effective values must use the editor's display labels.");
    master["speed"]["depends"] = {
        {"OR",
         {{{"AND", {{{"mode", 0}}, {{"infill", true}}}}}, {{"AND", {{{"mode", 1}}, {{"NOT", {{{"count", 0}}}}}}}}}}};
    expect(
        !reason(layer).isEmpty() && reason(layer).contains(" or ") && reason(layer).contains("not (Revolutions = 0)"),
        "An inactive OR must explain both alternatives and the negated condition accurately.");
    layer.setSetting("count", 2);
    expect(reason(layer).isEmpty(), "A satisfied alternate branch must activate the setting.");
    layer.setSetting("mode", 0);
    layer.setSetting("count", 0);
    expect(reason(layer).isEmpty(), "The first OR branch can activate a setting while the second branch fails.");
    master["speed"]["depends"] = {{"NOT", {{{"AND", {{{"mode", 0}}, {{"infill", true}}}}}}}};
    expect(!reason(layer).isEmpty() && reason(layer).contains("not ((Slicing Mode = Planar and Enable Infill = true))"),
           "NOT applied to a subtree must negate the entire expression in both evaluation and explanation.");
    layer.setSetting("infill", false);
    expect(reason(layer).isEmpty(), "NOT of a failed conjunction must pass.");
    master["speed"]["depends"] = {{"NOT", {{{"OR", {{{"mode", 0}}, {{"infill", true}}}}}}}};
    expect(!reason(layer).isEmpty(), "NOT must reject a satisfied disjunction.");
    layer.setSetting("mode", 2);
    expect(reason(layer).isEmpty(), "NOT must accept a disjunction when both children fail.");

    master["speed"]["depends"] = {{"mode", 1}};
    layer.setSetting("mode", 1.9);
    expect(reason(layer).isEmpty(),
           "Enumeration dependency comparisons must use the integer index loaded by the editor.");
    master["speed"]["depends"] = {{"mode", 1.5}};
    expect(!reason(layer).isEmpty(), "A nonintegral required enum value must not match an integer combo index.");
    master["speed"]["depends"] = {{"mode", -1}};
    layer.setSetting("mode", 99);
    expect(reason(layer).isEmpty(), "An out-of-range enumeration must match the editor's empty selection index.");
    master["speed"]["depends"] = {{"count", 3}};
    layer.setSetting("count", 3.9);
    expect(reason(layer).isEmpty(), "Integer dependencies must compare the integer loaded by QSpinBox.");
    master["speed"]["depends"] = {{"count", 0}};
    layer.setSetting("count", -2);
    expect(reason(layer).isEmpty(), "Nonnegative dependency values must match the editor's minimum clamp.");

    master["speed"]["depends"]    = {{"infill", true}};
    master["speed"]["visible_if"] = {{"mode", 0}};
    layer.setSetting("infill", true);
    layer.setSetting("mode", 1);
    expect(!reason(layer).isEmpty(),
           "A failed visibility condition must flag an otherwise enabled setting as inactive.");
    const fifojson composite = {{"depends", {{"mode", 1}}}, {"visible_if", {{"mode", 1}}}};
    layer.setSetting("infill", false);
    expect(reason(layer, composite).isEmpty(),
           "Composite input dependencies and visibility must override scalar metadata.");
    expect(!reason(layer, fifojson {{"depends", ""}}).isEmpty(),
           "A composite without visible_if must retain the scalar visibility condition.");
    layer.setSetting("mode", 0);
    expect(!reason(layer).isEmpty(), "A satisfied visibility condition must not bypass a failed feature dependency.");
    layer.setSetting("infill", true);
    layer.setSetting("speed", 0.0);
    expect(reason(layer).isEmpty(), "Warning-only numeric validations must not mark enabled features inactive.");
    master["infill"]["depends"] = {{"mode", 1}};
    expect(reason(layer).isEmpty(),
           "Dependency leaves must compare controller values without traversing their own dependencies.");
    for (const fifojson& empty : {fifojson(""), fifojson::object(), fifojson::array(), fifojson()}) {
        master["speed"]["depends"] = empty;
        expect(reason(layer).isEmpty(), "Every supported empty dependency representation must be unrestricted.");
    }
    const auto settings_before  = layer.json();
    const auto master_before    = master;
    const auto composite_before = composite;
    reason(layer, composite);
    expect(layer.json() == settings_before && master == master_before && composite == composite_before,
           "Evaluation must not alter effective settings or metadata.");

    QFile metadata_file(":/configs/master.conf");
    expect(metadata_file.open(QIODevice::ReadOnly), "Could not read the generated setting metadata.");
    if (!metadata_file.isOpen()) return EXIT_FAILURE;
    const auto actual_master = fifojson::parse(metadata_file.readAll().toStdString());
    ORNL::SettingsBase actual;
    actual.setSetting("slicing_mode", 0);
    actual.setSetting("infill", false);
    expect(!ORNL::SettingActivity::inactiveReason("infill_speed", actual, actual_master).isEmpty(),
           "Actual Planar infill speed must be inactive while infill is disabled.");
    actual.setSetting("slicing_mode", 1);
    actual.setSetting("cylindrical_path_pattern", 1);
    expect(ORNL::SettingActivity::inactiveReason("infill_speed", actual, actual_master).isEmpty(),
           "Actual Helical infill speed must remain active through its alternate dependency branch.");
    actual.setSetting("helical_perimeter_revolutions", 0);
    expect(!ORNL::SettingActivity::inactiveReason("perimeter_speed", actual, actual_master).isEmpty(),
           "Actual Helical perimeter speed must be inactive with zero perimeter revolutions.");
    actual.setSetting("helical_perimeter_revolutions", 1);
    expect(ORNL::SettingActivity::inactiveReason("perimeter_speed", actual, actual_master).isEmpty(),
           "Actual Helical perimeter speed must become active for positive perimeter revolutions.");
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
