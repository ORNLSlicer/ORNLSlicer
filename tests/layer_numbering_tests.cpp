#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QLineEdit>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <cstdlib>
#include <limits>
#include <stdexcept>

#include "gcode/writers/aerobasic_writer.h"
#include "gcode/writers/arc_specialties_writer.h"
#include "gcode/writers/marlin_writer.h"
#include "gcode/writers/reprap_writer.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "test_utils.h"
#include "threading/abs_slicing_thread.h"
#include "threading/gcode_aml3d_saver.h"
#include "threading/gcode_tormach_saver.h"
#include "utilities/constants.h"
#include "widgets/settings/setting_tab.h"

namespace {
class SlicingHarness : public ORNL::AbstractSlicingThread {
   public:
    explicit SlicingHarness(const QString& path) : AbstractSlicingThread(path) {}
    void doSlice() override {}
    void preProcess(nlohmann::json) override {}
    void postProcess(nlohmann::json) override {}
    void cleanThread() override {}
    void writeGCode() override {
        // Exercise the same sequence-overflow guard used by the real slicers.
        m_base->layerNumber(ORNL::GSM->getGlobal(), 1);
    }
    bool generate() {
        return writeGCodeFile();
    }
    QString annotate(const QString& text) {
        m_temp_gcode_output_file.write(text.toUtf8());
        writeLayerTimeComments();
        m_temp_gcode_output_file.seek(0);
        return QString::fromUtf8(m_temp_gcode_output_file.readAll());
    }
};
}  // namespace

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    Q_INIT_RESOURCE(configs);
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(styles);

    bool passed       = true;
    const auto expect = [&passed](bool condition, const char* message) {
        passed &= ORNL::Testing::expect(condition, message);
    };
    auto settings     = QSharedPointer<ORNL::SettingsBase>::create();
    const QString key = ORNL::PRS::GCode::kStartingLayerNumber;
    ORNL::MarlinWriter marlin(ORNL::GcodeMetaList::MarlinMeta, settings);
    const QString legacy = marlin.writeLayerChange(0) + marlin.writeLayerChange(1);
    expect(legacy == ";BEGINNING LAYER: 1\n;BEGINNING LAYER: 2\n", "Missing setting must preserve legacy numbering.");
    settings->setSetting(key, 1);
    expect(marlin.writeLayerChange(0) + marlin.writeLayerChange(1) == legacy, "Default numbering must be unchanged.");
    settings->setSetting(key, 120);
    ORNL::AeroBasicWriter aero(ORNL::GcodeMetaList::AeroBasicMeta, settings);
    ORNL::RepRapWriter reprap(ORNL::GcodeMetaList::RepRapMeta, settings);
    settings->setSetting(ORNL::MS::Cooling::kEnable, true);
    settings->setSetting(ORNL::MS::Cooling::kMaxSpeed, 100);
    for (ORNL::WriterBase* writer : QList<ORNL::WriterBase*> {&marlin, &aero, &reprap}) {
        expect(writer->writeLayerChange(0).contains("BEGINNING LAYER: 120"), "First layer must use configured number.");
        expect(writer->writeLayerChange(1).contains("BEGINNING LAYER: 121"), "Later layers must be sequential.");
    }
    expect(!reprap.writeLayerChange(0).contains("M106") && !reprap.writeLayerChange(1).contains("M106") &&
               reprap.writeLayerChange(2).contains("M106"),
           "Renumbering must preserve first-layer cooling behavior.");

    ORNL::ArcSpecialtiesWriter arc(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);
    settings->setSetting(ORNL::PRS::GCode::kArcSpecialtiesEmitBlockNumbers, true);
    arc.writeInitialSetup(0, 0, 0, 0, 3);
    for (uint index = 0; index < 3; ++index) {
        arc.writeLayerChange(index);
        arc.writeBeforeLayer(0, settings);
        expect(
            arc.writeBeforeRegion(ORNL::RegionType::kPerimeter, 0).contains(QString("N%1 ").arg((120 + index) * 10000)),
            "Arc Specialties block numbers must use configured layer numbering.");
        arc.writeBeforePath(ORNL::RegionType::kPerimeter);
        const QString block =
            arc.writeTravel(ORNL::Point(0, 0, 0), ORNL::Point(1000, 0, 0), ORNL::TravelLiftType::kNoLift, settings);
        expect(block.contains(QString("BEGINNING BEAD: %1.1").arg(120 + index)),
               "Bead labels must match layer labels.");
    }
    settings->setSetting(key, std::numeric_limits<int>::max());
    expect(ORNL::WriterBase::layerNumber(settings, 0) == std::numeric_limits<int>::max(),
           "Largest valid layer must fit.");
    arc.writeLayerChange(0);
    arc.writeBeforeLayer(0, settings);
    expect(arc.writeBeforeRegion(ORNL::RegionType::kPerimeter, 0).contains("N21474836470000 "),
           "Block arithmetic must not overflow.");
    try {
        ORNL::WriterBase::layerNumber(settings, 1);
        expect(false, "An overflowing layer sequence must be rejected.");
    } catch (const std::invalid_argument&) {}
    for (const fifojson& invalid : {fifojson(0), fifojson(-1), fifojson(1.5), fifojson("120"), fifojson(true),
                                    fifojson(nullptr), fifojson(2147483648ULL), fifojson(1e100)}) {
        settings->setSetting(key, invalid);
        try {
            marlin.writeLayerChange(0);
            expect(false, "Invalid layer numbers must be rejected by writers.");
        } catch (const std::invalid_argument& error) {
            expect(QString(error.what()).contains("Starting Layer Number"),
                   "Invalid settings need an actionable error.");
        }
    }
    settings->setSetting(key, 120.0);
    expect(ORNL::WriterBase::layerNumber(settings, 0) == 120,
           "Integral numeric settings from existing spin boxes must work.");

    auto global = ORNL::GSM->getGlobal();
    QString message;
    QObject::connect(ORNL::CSM.get(), &ORNL::SessionManager::forwardStatusUpdate, &app,
                     [&message](const QString& status) { message = status; });
    global->setSetting(key, -1);
    expect(!ORNL::CSM->doSlice() && message.contains("Starting Layer Number"),
           "Invalid imported settings must reject slicing cleanly.");
    global->setSetting(key, std::numeric_limits<int>::max());
    global->setSetting(ORNL::PRS::MachineSetup::kSyntax, ORNL::GcodeSyntax::kMarlin);
    QTemporaryDir temp;
    expect(temp.isValid(), "Could not create test output directory.");
    const QString failed_path = temp.filePath("overflow.gcode");
    {
        SlicingHarness slicer(failed_path);
        bool failed = false;
        QObject::connect(
            &slicer, &ORNL::AbstractSlicingThread::sliceFailed, &slicer, [&failed](const QString&) { failed = true; },
            Qt::DirectConnection);
        expect(!slicer.generate() && failed,
               "Sequence overflow must signal failure without escaping the slicing thread.");
    }
    expect(QFileInfo(failed_path).size() == 0, "Failed generation must clear partial G-code.");

    global->setSetting(key, 120);
    global->setSetting(ORNL::PRS::GCode::kLayerTimeComments, true);
    global->setSetting(ORNL::MS::Cooling::kForceMinLayerTime, false);
    global->setSetting(ORNL::PRS::MachineSpeed::kMaxXYSpeed, 1000.0 * ORNL::mm / ORNL::s);
    global->setSetting(ORNL::PS::Layer::kLayerHeight, 0.2 * ORNL::mm);
    const auto annotations = [&](int start) {
        SlicingHarness slicer(temp.filePath(QString("times-%1.gcode").arg(start)));
        const QString source =
            QString("G21\n;BEGINNING LAYER: %1\nG1 F60 X1 ;TRAVEL\n;BEGINNING LAYER: %2\nG1 F60 X3 ;TRAVEL\n")
                .arg(start)
                .arg(start + 1);
        const QString result = slicer.annotate(source);
        QStringList times;
        for (const QString& line : result.split('\n')) {
            if (line.contains("ORNL_SLICER_LAYER_TIME_ESTIMATE=")) { times.append(line); }
        }
        return times;
    };
    const QStringList default_times = annotations(1);
    expect(default_times.size() == 2 && annotations(120) == default_times,
           "Layer time comments must use generated order, independent of labels.");

    const auto read_file = [](const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) { return QString(); }
        return QString::fromUtf8(file.readAll());
    };
    // Exporters must use the file's labels even when current settings differ.
    for (int start : {1, 120}) {
        const QString filename = temp.filePath(QString("part-%1.gcode").arg(start));
        const QString source   = QString(
                                     "(BEGINNING LAYER: %1)\nG1 X1 Y2 Z3 F60 (PERIMETER)\n(BEGINNING LAYER: %2)\nG1 X4 "
                                     "Y5 Z6 F60 (PERIMETER)\nM30\n")
                                     .arg(start)
                                     .arg(start + 1);
        ORNL::GCodeAML3DSaver aml(temp.path() + "/", temp.path(), filename, source, ORNL::GcodeMetaList::AML3DMeta);
        aml.run();
        const QString prefix = temp.filePath(QString("part-%1_1_1_").arg(start));
        expect(read_file(prefix + QString::number(start) + "_deposit.csv").contains("0,1,1,2,3,"),
               "AML3D first CSV must keep its label and contents.");
        expect(read_file(prefix + QString::number(start + 1) + "_deposit.csv").contains("0,2,4,5,6,"),
               "AML3D subsequent CSV must keep its label and contents.");
        ORNL::GCodeTormachSaver tormach(temp.path() + "/", temp.path(), filename, source,
                                        ORNL::GcodeMetaList::TormachMeta);
        tormach.run();
        const QString apt = read_file(temp.filePath(QString("part-%1_output.apt").arg(start)));
        expect(apt.contains(QString("$$ Layer: %1\n").arg(start)) &&
                   apt.contains(QString("$$ Layer: %1\n").arg(start + 1)),
               "Tormach APT references must follow source layer labels.");
    }

    ORNL::SettingTab tab(nullptr, "G-Code", QIcon(), 0, false, global);
    const auto& metadata = ORNL::GSM->getMaster()->json();
    tab.addRow(ORNL::PRS::GCode::kLayerTimeComments, metadata.at(ORNL::PRS::GCode::kLayerTimeComments.toStdString()));
    tab.expandTab();
    tab.resize(650, 150);
    tab.show();
    app.processEvents();
    if (argc > 1) { expect(tab.grab().save(QString(argv[1]) + "/before.png"), "Could not save before screenshot."); }
    global->remove(key);
    tab.addRow(key, metadata.at(key.toStdString()));
    app.processEvents();
    auto* spin = tab.findChild<QSpinBox*>();
    expect(spin != nullptr && spin->minimum() == 1 && spin->value() == 1,
           "UI must default to 1 and require positive integers.");
    if (spin) {
        spin->setValue(std::numeric_limits<int>::max());
        expect(global->setting<int>(key) == std::numeric_limits<int>::max(),
               "UI must preserve large layer numbers exactly.");
        tab.reload();
        expect(spin->value() == std::numeric_limits<int>::max(),
               "Reloading must preserve the largest valid layer number.");
        ORNL::SettingTab reopened(nullptr, "G-Code", QIcon(), 0, false, global);
        reopened.addRow(key, metadata.at(key.toStdString()));
        expect(reopened.findChild<QSpinBox*>()->value() == std::numeric_limits<int>::max(),
               "Reopening settings must preserve large layer numbers exactly.");
        const auto* editor    = spin->findChild<QLineEdit*>();
        const auto* validator = editor ? editor->validator() : nullptr;
        expect(validator != nullptr, "Integer input must have a validator.");
        if (validator) {
            QString invalid = "0";
            int position    = 1;
            expect(validator->validate(invalid, position) != QValidator::Acceptable, "UI must reject zero.");
            invalid = "1.5";
            expect(validator->validate(invalid, position) == QValidator::Invalid, "UI must reject fractional numbers.");
        }
        spin->setValue(120);
        expect(global->setting<int>(key) == 120, "UI edits must update the global setting.");
    }
    if (argc > 1) { expect(tab.grab().save(QString(argv[1]) + "/after.png"), "Could not save after screenshot."); }
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
