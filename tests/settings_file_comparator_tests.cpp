#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <cstdlib>

#include "configs/settings_file_comparator.h"
#include "test_utils.h"

namespace {
using ORNL::SettingsFileComparator;

bool writeFile(const QString& path, const QString& contents) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return false;
    return file.write(contents.toUtf8()) >= 0;
}

bool comparesValuesAndMissingSettings() {
    const fifojson first = {
        {"header", {{"created_on", "ignored"}}},
        {"settings",
         fifojson::array({{{"same", 1}, {"changed", 2}, {"first_only", true}, {"ordered", {{"a", 1}, {"b", 2}}}},
                          {{"second_set", "first"}}})}};
    const fifojson second = {
        {"header", {{"created_on", "different but ignored"}}},
        {"settings",
         fifojson::array({{{"same", 1.0}, {"changed", 3}, {"second_only", false}, {"ordered", {{"b", 2}, {"a", 1}}}},
                          {{"second_set", "second"}, {"second_set_only", 4}}})}};

    const SettingsFileComparator::Result result = SettingsFileComparator::compare(first, second);
    bool passed                                 = true;
    passed &= ORNL::Testing::expect(result.errors.isEmpty(), "A valid comparison should not report errors.");
    passed &= ORNL::Testing::expect(result.changed_settings.size() == 2,
                                    "Expected changed settings from both settings sets.");
    passed &= ORNL::Testing::expect(result.changed_settings.at(0).key == "changed",
                                    "Changed settings should be ordered by settings set and key.");
    passed &= ORNL::Testing::expect(
        result.changed_settings.at(1).key == "second_set" && result.changed_settings.at(1).settings_index == 1,
        "A changed setting should retain its settings-set index.");
    passed &=
        ORNL::Testing::expect(result.only_in_first.size() == 1 && result.only_in_first.first().key == "first_only",
                              "Expected the setting found only in the first file.");
    passed &= ORNL::Testing::expect(result.only_in_second.size() == 2,
                                    "Expected settings found only in the second file and its second set.");
    return passed;
}

bool toleratesInsignificantFloatingPointDifferences() {
    const fifojson first = {
        {"settings", fifojson::array({{{"large_round_trip", 279399.98955875647},
                                       {"near_zero", 0.0},
                                       {"nested_round_trip", {{"maximum_xy_speed", 1142999.957285822}}},
                                       {"beyond_tolerance", 1.0},
                                       {"integer_difference", 1},
                                       {"meaningful_float_difference", 54.0}}})}};
    const fifojson second = {
        {"settings", fifojson::array({{{"large_round_trip", 279399.9895587564},
                                       {"near_zero", 5.0e-10},
                                       {"nested_round_trip", {{"maximum_xy_speed", 1142999.9572858217}}},
                                       {"beyond_tolerance", 1.000000002},
                                       {"integer_difference", 2},
                                       {"meaningful_float_difference", 50.0}}})}};

    const SettingsFileComparator::Result result = SettingsFileComparator::compare(first, second);
    bool passed                                 = true;
    passed &= ORNL::Testing::expect(result.errors.isEmpty(), "A valid numeric comparison should not report errors.");
    passed &= ORNL::Testing::expect(result.changed_settings.size() == 3,
                                    "Only numeric differences outside the tolerance should be reported.");
    passed &= ORNL::Testing::expect(result.changed_settings.at(0).key == "beyond_tolerance",
                                    "A floating-point difference beyond the absolute tolerance should be reported.");
    passed &= ORNL::Testing::expect(result.changed_settings.at(1).key == "integer_difference",
                                    "Integer settings should still use exact comparison.");
    passed &= ORNL::Testing::expect(result.changed_settings.at(2).key == "meaningful_float_difference",
                                    "Meaningful floating-point differences should still be reported.");
    return passed;
}

bool validatesSettingsShape() {
    const SettingsFileComparator::Result missing =
        SettingsFileComparator::compare(fifojson::object(), {{"settings", fifojson::array()}});
    const SettingsFileComparator::Result invalid_entry = SettingsFileComparator::compare(
        {{"settings", fifojson::array({1})}}, {{"settings", fifojson::array({fifojson::object()})}});

    bool passed = true;
    passed &= ORNL::Testing::expect(missing.errors.size() == 1, "A missing settings array should be rejected.");
    passed &=
        ORNL::Testing::expect(invalid_entry.errors.size() == 1, "A non-object settings entry should be rejected.");
    return passed;
}

bool reportsFileErrors() {
    QTemporaryDir directory;
    if (!ORNL::Testing::expect(directory.isValid(), "Could not create a temporary directory.")) return false;

    const QString valid_path   = directory.filePath("valid.s2c");
    const QString invalid_path = directory.filePath("invalid.s2c");
    if (!ORNL::Testing::expect(writeFile(valid_path, R"({"settings":[{"value":1}]})"),
                               "Could not write the valid settings fixture."))
        return false;
    if (!ORNL::Testing::expect(writeFile(invalid_path, "{not json"), "Could not write the invalid settings fixture."))
        return false;

    bool passed = true;
    passed &= ORNL::Testing::expect(SettingsFileComparator::compareFiles(valid_path, valid_path).errors.isEmpty(),
                                    "Two valid files should compare successfully.");
    passed &= ORNL::Testing::expect(!SettingsFileComparator::compareFiles(valid_path, invalid_path).errors.isEmpty(),
                                    "Invalid JSON should be reported.");
    passed &= ORNL::Testing::expect(
        !SettingsFileComparator::compareFiles(directory.filePath("missing.s2c"), valid_path).errors.isEmpty(),
        "An unreadable file should be reported.");
    return passed;
}
}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    bool passed = true;
    passed &= comparesValuesAndMissingSettings();
    passed &= toleratesInsignificantFloatingPointDifferences();
    passed &= validatesSettingsShape();
    passed &= reportsFileErrors();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
