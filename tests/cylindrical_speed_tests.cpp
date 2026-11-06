#include <QApplication>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <array>
#include <cstdlib>
#include <functional>
#include <utility>

#include "configs/settings_base.h"
#include "gcode/writers/arc_specialties_writer.h"
#include "geometry/mesh/mesh_base.h"
#include "geometry/path.h"
#include "managers/preferences_manager.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "test_utils.h"
#include "threading/gcode_arc_specialties_saver.h"
#include "threading/traditional_ast.h"
#define private public
#include "threading/slicers/cylindrical_slicer.h"
#undef private

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    Q_INIT_RESOURCE(configs);
    bool passed       = true;
    const auto expect = [&](bool condition, const QString& message) {
        passed &= ORNL::Testing::expect(condition, message.toStdString());
    };
    QTemporaryDir temp;
    expect(temp.isValid(), "Could not create export fixture directory.");
    if (!temp.isValid()) return EXIT_FAILURE;
    auto settings = ORNL::GSM->getGlobal();
    settings->setSetting(ORNL::PS::Layer::kSpeed, 44.0 * ORNL::mm / ORNL::s);
    settings->setSetting(ORNL::PS::Perimeter::kSpeed, 11.0 * ORNL::mm / ORNL::s);
    settings->setSetting(ORNL::PS::Inset::kSpeed, 22.0 * ORNL::mm / ORNL::s);
    settings->setSetting(ORNL::PS::Infill::kSpeed, 33.0 * ORNL::mm / ORNL::s);
    settings->setSetting(ORNL::PS::Skeleton::kSpeed, 44.0 * ORNL::mm / ORNL::s);
    settings->setSetting(ORNL::PS::ArcSpecialties::kGenerateWeldScheduleFile, false);
    settings->setSetting(ORNL::PRS::GCode::kArcSpecialtiesG80WeldScheduleFile, QString());
    settings->setSetting(ORNL::PRS::MachineSetup::kSupportG3, true);
    settings->setSetting(ORNL::PS::Helical::kHelicalPerimeterRevolutions, 1);
    settings->setSetting(ORNL::PS::Helical::kHelicalInsetRevolutions, 1);
    struct ScheduleParameter {
        QString suffix;
        QString assignment;
        double value;
        QString unit;
    };
    const std::array<ScheduleParameter, 19> parameters {{
        {"preflow", "Preflow", 1.25, "sec"},
        {"travel_delay", "TravelDelay", 2.5, "sec"},
        {"postflow", "Postflow", 3.75, "sec"},
        {"postpurge", "PostPurge", 4.5, "sec"},
        {"start_program", "Start1.Program", 1, "unitless"},
        {"start_wire_feed_speed", "Start1.WFS", 10.25, "in/min"},
        {"start_voltage", "Start1.Volts", 18.5, "V"},
        {"start_control", "Start1.Control", 2, "unitless"},
        {"start_tspd", "Start1.TSpd", 20.5, "in/min"},
        {"start_time", "Start1.Time", 5.25, "sec"},
        {"weld_program", "Weld.Program", 3, "unitless"},
        {"weld_wire_feed_speed", "Weld.WFS", 30.75, "in/min"},
        {"weld_voltage", "Weld.Volts", 23.5, "V"},
        {"weld_control", "Weld.Control", 4, "unitless"},
        {"crater_program", "Crater.Program", 5, "unitless"},
        {"crater_wire_feed_speed", "Crater.WFS", 40.125, "in/min"},
        {"crater_voltage", "Crater.Volts", 16.5, "V"},
        {"crater_control", "Crater.Control", 6, "unitless"},
        {"crater_time", "Crater.Time", 6.5, "sec"},
    }};
    for (const QString& region : {"perimeter", "inset", "infill", "skeleton"}) {
        for (const auto& parameter : parameters) {
            const QString key = "arc_specialties_" + region + "_" + parameter.suffix;
            if (parameter.unit == "in/min")
                settings->setSetting(key, parameter.value * ORNL::inch / ORNL::minute);
            else if (parameter.unit == "sec")
                settings->setSetting(key, parameter.value * ORNL::s);
            else if (parameter.unit == "V")
                settings->setSetting(key, parameter.value * ORNL::V);
            else
                settings->setSetting(key, static_cast<int>(parameter.value));
        }
    }
    const std::array<ORNL::RegionType, 4> regions = {ORNL::RegionType::kPerimeter, ORNL::RegionType::kInset,
                                                     ORNL::RegionType::kInfill, ORNL::RegionType::kSkeleton};
    const std::array<int, 4> schedules            = {1, 2, 0, 3};
    ORNL::CylindricalSlicer slicer(temp.filePath("unused.nc"));
    int case_number        = 0;
    const auto preferences = ORNL::PreferencesManager::getInstance();
    for (int unit_case : {0, 1, 2}) {
        const ORNL::Velocity velocity_unit = unit_case == 0   ? ORNL::mm / ORNL::s
                                             : unit_case == 1 ? ORNL::inch / ORNL::minute
                                                              : ORNL::mm / ORNL::minute;
        const ORNL::Time time_unit         = unit_case == 0 ? ORNL::s : unit_case == 1 ? ORNL::ms : ORNL::minute;
        const ORNL::Voltage voltage_unit   = unit_case == 0 ? ORNL::V : unit_case == 1 ? ORNL::mV : ORNL::uV;
        preferences->setVelocityUnit(velocity_unit);
        preferences->setTimeUnit(time_unit);
        preferences->setVoltageUnit(voltage_unit);
        for (bool imperial : {false, true}) {
            auto meta            = ORNL::GcodeMetaList::ArcSpecialtiesMeta;
            meta.m_velocity_unit = imperial ? ORNL::inch / ORNL::minute : ORNL::mm / ORNL::minute;
            for (int mode : {0, 1, 2, 3}) {
                settings->setSetting(ORNL::PS::Layer::kSpeed, (mode == 3 ? 0.0 : 44.0) * ORNL::mm / ORNL::s);
                settings->setSetting(ORNL::PS::Slicing::kSlicingMode, mode == 0 ? 0 : 1);
                settings->setSetting(ORNL::PS::Slicing::kCylindricalPathPattern, mode == 2 ? 1 : 0);
                const QString output = temp.filePath(QString("case%1.nc").arg(++case_number));
                ORNL::GCodeArcSpecialtiesSaver saver(output, "#FILE NAME[ G80=\"old.nc\" ]\n");
                saver.run();
                QFile schedule(temp.filePath(QString("case%1_sch.nc").arg(case_number)));
                expect(schedule.open(QIODevice::ReadOnly), "Generated schedule must exist.");
                const QString text = QString::fromUtf8(schedule.readAll());
                for (const auto& parameter : parameters) {
                    double expected_value = parameter.value;
                    QString unit_text     = parameter.unit;
                    if (parameter.unit == "sec") {
                        expected_value = ORNL::Time(parameter.value * ORNL::s).to(time_unit);
                        unit_text      = preferences->getTimeUnitText();
                    }
                    else if (parameter.unit == "V") {
                        expected_value = ORNL::Voltage(parameter.value * ORNL::V).to(voltage_unit);
                        unit_text      = preferences->getVoltageUnitText();
                    }
                    auto matches =
                        QRegularExpression("^V\\.E\\.Sch\\." + QRegularExpression::escape(parameter.assignment) +
                                               " = ([0-9.]+) +;([^\\n]*)$",
                                           QRegularExpression::MultilineOption)
                            .globalMatch(text);
                    int count    = 0;
                    bool correct = true;
                    while (matches.hasNext()) {
                        const auto match = matches.next();
                        ++count;
                        correct &= ORNL::Testing::near(match.captured(1).toDouble(), expected_value, 0.0001) &&
                                   match.captured(2).endsWith("(" + unit_text + ")");
                    }
                    expect(count == (mode == 2 ? 3 : 4) && correct,
                           QString("mode %1 units %2: %3 must use %4 and label its comment.")
                               .arg(mode)
                               .arg(imperial)
                               .arg(parameter.assignment, unit_text));
                }
                for (size_t i = 0; i < regions.size(); ++i) {
                    if (mode == 2 && regions[i] == ORNL::RegionType::kSkeleton) continue;
                    const ORNL::Velocity expected = (mode == 1   ? 44.0
                                                     : mode == 3 ? 10.0
                                                                 : 11.0 * (i + 1)) *
                                                    ORNL::mm / ORNL::s;
                    const QString label = QString("mode %1 region %2 units %3").arg(mode).arg(i).arg(imperial);
                    const int start     = text.indexOf(QString("(Schedule %1 ").arg(schedules[i]));
                    const int end       = text.indexOf(QString("(Schedule %1 End)").arg(schedules[i]), start);
                    const auto speed =
                        QRegularExpression("V\\.S\\.SPEED = ([0-9.]+) +;([^\\n]*)").match(text.mid(start, end - start));
                    expect(start >= 0 && end > start && speed.hasMatch() &&
                               ORNL::Testing::near(speed.captured(1).toDouble(), expected.to(ORNL::inch / ORNL::minute),
                                                   0.0001) &&
                               speed.captured(2).endsWith("(in/min)"),
                           label + ": schedule speed and comment must always use in/min.");
                    if (mode == 0 || regions[i] == ORNL::RegionType::kSkeleton) continue;
                    auto params = slicer.createSegmentSettings(settings, ORNL::Point(), false, regions[i]);
                    expect(ORNL::Testing::near(params->setting<ORNL::Velocity>(ORNL::SS::kSpeed)(), expected()),
                           label + ": generated segment must carry its region speed.");
                    ORNL::ArcSpecialtiesWriter writer(meta, settings);
                    const ORNL::Point from(10.0 * ORNL::mm, 0, 0);
                    const ORNL::Point to(0, 10.0 * ORNL::mm, 1.0 * ORNL::mm);
                    const QString feedrate = "F" + QString::number(expected.to(meta.m_velocity_unit), 'f', 4);
                    expect(writer.writeLine(from, to, params).contains(feedrate), label + ": line feedrate mismatch.");
                    const QString arc = writer.writeArc(from, to, ORNL::Point(), 90.0 * ORNL::degree, true, params);
                    expect(arc.contains("G03") && arc.contains(feedrate), label + ": arc feedrate mismatch.");
                }
            }
        }
    }
    preferences->setVelocityUnit(ORNL::mm / ORNL::s);
    preferences->setTimeUnit(ORNL::s);
    preferences->setVoltageUnit(ORNL::V);
    settings->setSetting(ORNL::PS::Slicing::kSlicingMode, 0);
    ORNL::GCodeArcSpecialtiesSaver snapshot_saver(temp.filePath("snapshot.nc"), "#FILE NAME[ G80=\"old.nc\" ]\n");
    preferences->setVelocityUnit(ORNL::mm / ORNL::minute);
    preferences->setTimeUnit(ORNL::ms);
    preferences->setVoltageUnit(ORNL::mV);
    snapshot_saver.run();
    QFile snapshot_schedule(temp.filePath("snapshot_sch.nc"));
    expect(snapshot_schedule.open(QIODevice::ReadOnly), "Schedule with captured UI units must exist.");
    const QString snapshot = QString::fromUtf8(snapshot_schedule.readAll());
    expect(snapshot.contains("V.E.Sch.Preflow = 1.25      ;Preflow Time (sec)") &&
               snapshot.contains("V.E.Sch.Weld.WFS = 30.75      ;Wire Feed Speed in Main Weld (in/min)") &&
               snapshot.contains("V.E.Sch.Weld.Volts = 23.5      ;Trim or Volts in Main Weld (V)") &&
               snapshot.contains("V.S.SPEED = 25.9843            ;Set speed value if it exists (in/min)"),
           "Schedule velocities must use in/min while time and voltage retain their captured UI units.");
    settings->setSetting(ORNL::PS::Slicing::kSlicingMode, 1);
    settings->setSetting(ORNL::PS::Slicing::kCylindricalPathPattern, 1);
    for (bool planar_enabled : {false, true}) {
        for (const QString& key : {"perimeter", "inset", "infill", "skeleton"})
            settings->setSetting(key, planar_enabled);
        for (const auto& [perimeter, inset] : std::array<std::pair<int, int>, 4> {{{1, 0}, {0, 1}, {0, 0}, {2, 3}}}) {
            settings->setSetting(ORNL::PS::Helical::kHelicalPerimeterRevolutions, perimeter);
            settings->setSetting(ORNL::PS::Helical::kHelicalInsetRevolutions, inset);
            const QString basename = QString("enabled%1").arg(++case_number);
            ORNL::GCodeArcSpecialtiesSaver saver(temp.filePath(basename + ".nc"), "#FILE NAME[ G80=\"old.nc\" ]\n");
            saver.run();
            QFile schedule(temp.filePath(basename + "_sch.nc"));
            expect(schedule.open(QIODevice::ReadOnly), "Filtered Helical schedule must exist.");
            const QString text    = QString::fromUtf8(schedule.readAll());
            const QString context = QString("Helical perimeter=%1 inset=%2 Planar toggles=%3")
                                        .arg(perimeter)
                                        .arg(inset)
                                        .arg(planar_enabled);
            expect(!text.contains("skeleton", Qt::CaseInsensitive) && !text.contains("$CASE 3\n"),
                   context + ": Skeleton must never be exported.");
            expect(text.contains("(Schedule 1 Start, perimeter)") == (perimeter > 0) &&
                       text.contains("$CASE 1\n") == (perimeter > 0),
                   context + ": Perimeter must follow its revolution count and retain schedule 1.");
            expect(
                text.contains("(Schedule 2 Start, inset)") == (inset > 0) && text.contains("$CASE 2\n") == (inset > 0),
                context + ": Inset must follow its revolution count and retain schedule 2.");
            expect(text.contains("$DEFAULT\n") && text.contains("(Schedule 0 / Default Schedule Start, infill)"),
                   context + ": Infill must retain the startup default schedule.");
            expect(text.count("V.E.Sch.Weld.Program = ") == 1 + (perimeter > 0) + (inset > 0),
                   context + ": Only active regions may export weld parameters.");
        }
    }
    settings->setSetting(ORNL::PRS::MachineSetup::kSyntax, ORNL::GcodeSyntax::kArcSpecialties);
    settings->setSetting(ORNL::PS::Slicing::kSlicingMode, 1);
    settings->setSetting(ORNL::PS::Slicing::kCylindricalPathPattern, 1);
    settings->setSetting(ORNL::PS::Helical::kHelicalPerimeterRevolutions, 1);
    settings->setSetting(ORNL::PS::Helical::kHelicalInsetRevolutions, 1);
    QString message;
    QObject::connect(ORNL::CSM.get(), &ORNL::SessionManager::forwardStatusUpdate, &app,
                     [&](const QString& text) { message = text; });
    const std::array<QString, 3> keys = {ORNL::PS::Perimeter::kSpeed, ORNL::PS::Inset::kSpeed,
                                         ORNL::PS::Infill::kSpeed};
    for (const auto& key : keys) {
        const auto valid_speed = settings->setting<ORNL::Velocity>(key);
        for (double invalid : {0.0, -1.0}) {
            settings->setSetting(key, invalid);
            message.clear();
            expect(!ORNL::CSM->doSlice() && message.contains("greater than zero for Helical"),
                   key + ": invalid active-region speeds must reject slicing.");
        }
        settings->setSetting(key, valid_speed);
    }
    settings->setSetting(ORNL::PS::Helical::kHelicalPerimeterRevolutions, 0);
    settings->setSetting(ORNL::PS::Helical::kHelicalInsetRevolutions, 0);
    for (const auto& key : keys) settings->setSetting(key, 0.0);
    message.clear();
    expect(!ORNL::CSM->doSlice() && message.startsWith("Infill Speed"),
           "Disabled shell regions must not require positive speeds.");
    for (const auto& key : keys) settings->setSetting(key, 15.0 * ORNL::mm / ORNL::s);
    settings->setSetting(ORNL::PS::Helical::kHelicalInsetRevolutions, 1);
    auto part = QSharedPointer<ORNL::Part>::create();
    part->setName("speed-validation");
    part->getSb()->setSetting(ORNL::PS::Inset::kSpeed, 0.0);
    ORNL::CSM->addPart(part, false);
    message.clear();
    expect(!ORNL::CSM->doSlice() && message.startsWith("Inset Speed"),
           "Invalid part-level speed overrides must reject slicing.");
    ORNL::CSM->clearParts();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
