#include <QApplication>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <array>
#include <cstdlib>
#include <functional>

#include "configs/settings_base.h"
#include "gcode/writers/arc_specialties_writer.h"
#include "geometry/mesh/mesh_base.h"
#include "geometry/path.h"
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
    settings->setSetting(ORNL::PS::ArcSpecialties::kGenerateWeldScheduleFile, false);
    settings->setSetting(ORNL::PRS::GCode::kArcSpecialtiesG80WeldScheduleFile, QString());
    settings->setSetting(ORNL::PRS::MachineSetup::kSupportG3, true);
    const std::array<ORNL::RegionType, 3> regions = {ORNL::RegionType::kPerimeter, ORNL::RegionType::kInset,
                                                     ORNL::RegionType::kInfill};
    const std::array<int, 3> schedules            = {1, 2, 0};
    ORNL::CylindricalSlicer slicer(temp.filePath("unused.nc"));
    int case_number = 0;
    for (bool imperial : {false, true}) {
        auto meta            = ORNL::GcodeMetaList::ArcSpecialtiesMeta;
        meta.m_velocity_unit = imperial ? ORNL::inch / ORNL::minute : ORNL::mm / ORNL::minute;
        for (int mode : {0, 1, 2, 3}) {
            settings->setSetting(ORNL::PS::Layer::kSpeed, (mode == 3 ? 0.0 : 44.0) * ORNL::mm / ORNL::s);
            settings->setSetting(ORNL::PS::Slicing::kSlicingMode, mode == 0 ? 0 : 1);
            settings->setSetting(ORNL::PS::Slicing::kCylindricalPathPattern, mode == 2 ? 1 : 0);
            const QString output = temp.filePath(QString("case%1.nc").arg(++case_number));
            ORNL::GCodeArcSpecialtiesSaver saver(temp.path() + QDir::separator(), temp.path(), output,
                                                 "#FILE NAME[ G80=\"old.nc\" ]\n", meta);
            saver.run();
            QFile schedule(temp.filePath(QString("case%1_sch.nc").arg(case_number)));
            expect(schedule.open(QIODevice::ReadOnly), "Generated schedule must exist.");
            const QString text = QString::fromUtf8(schedule.readAll());
            for (size_t i = 0; i < regions.size(); ++i) {
                const ORNL::Velocity expected = (mode == 1   ? 44.0
                                                 : mode == 3 ? 10.0
                                                             : 11.0 * (i + 1)) *
                                                ORNL::mm / ORNL::s;
                const QString label           = QString("mode %1 region %2 units %3").arg(mode).arg(i).arg(imperial);
                const int start               = text.indexOf(QString("(Schedule %1 ").arg(schedules[i]));
                const int end                 = text.indexOf(QString("(Schedule %1 End)").arg(schedules[i]), start);
                const auto speed = QRegularExpression("V\\.S\\.SPEED = ([0-9.]+)").match(text.mid(start, end - start));
                expect(start >= 0 && end > start && speed.hasMatch() &&
                           ORNL::Testing::near(speed.captured(1).toDouble(), expected.to(meta.m_velocity_unit), 0.0001),
                       label + ": schedule speed must match the active slicing mode.");
                if (mode == 0) continue;
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
