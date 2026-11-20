#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <iostream>

#include "configs/settings_base.h"
#include "gcode/gcode_layer_export.h"
#include "gcode/writers/arc_specialties_writer.h"
#include "gcode/writers/cincinnati_writer.h"
#include "gcode/writers/haas_writer.h"
#include "gcode/writers/marlin_writer.h"
#include "gcode/writers/mvp_writer.h"
#include "gcode/writers/wolf_writer.h"
#include "geometry/segments/arc.h"
#include "geometry/segments/line.h"
#include "geometry/segments/travel.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "threading/abs_slicing_thread.h"
#include "threading/gcode_layer_saver.h"
#include "utilities/constants.h"
#include "windows/gcode_export.h"
#include "windows/main_window.h"

namespace {
QSharedPointer<ORNL::SettingsBase> writerSettings() {
    auto settings = QSharedPointer<ORNL::SettingsBase>::create();
    settings->setSetting(ORNL::PS::Travel::kFinalLiftDistance, 2.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Slicing::kSlicePlaneNormalZ, 1.0);
    settings->setSetting(ORNL::PS::Layer::kLayerHeight, 10.0 * ORNL::mm);
    settings->setSetting(ORNL::PRS::MachineSpeed::kZSpeed, 5.0 * ORNL::mm / ORNL::s);
    settings->setSetting(ORNL::PS::Travel::kSpeed, 10.0 * ORNL::mm / ORNL::s);
    settings->setSetting(ORNL::PRS::GCode::kEndCode, QString("M400"));
    settings->setSetting(ORNL::PRS::GCode::kEnableStartupCode, 1);
    settings->setSetting(ORNL::PRS::Dimensions::kWMax, 100.0 * ORNL::mm);
    settings->setSetting(ORNL::PRS::Dimensions::kInitialW, 100.0 * ORNL::mm);
    settings->setSetting(ORNL::PRS::Dimensions::kZOffset, -150.0 * ORNL::mm);
    settings->setSetting(ORNL::PRS::Dimensions::kLayerChangeAxis, ORNL::LayerChange::kBoth_Z_and_W);
    settings->setSetting(ORNL::SS::kSpeed, 5.0 * ORNL::mm / ORNL::s);
    settings->setSetting(ORNL::SS::kExtruderSpeed, 10);
    settings->setSetting(ORNL::SS::kRegionType, ORNL::RegionType::kPerimeter);
    settings->setSetting(ORNL::SS::kPathModifiers, ORNL::PathModifiers::kNone);
    settings->setSetting(ORNL::PRS::MachineSpeed::kGearRatio, 1.0);
    return settings;
}

QString paths(const QSharedPointer<ORNL::WriterBase>& writer, const QSharedPointer<ORNL::SettingsBase>& settings,
              int layer, bool travel, bool arc = false) {
    const ORNL::Point start(10.0 * ORNL::mm, 20.0 * ORNL::mm, (layer + 1) * 10.0 * ORNL::mm);
    const ORNL::Point end(30.0 * ORNL::mm, 40.0 * ORNL::mm, start.z());
    QString result = writer->writeBeforeRegion(ORNL::RegionType::kPerimeter);
    result += writer->writeBeforePath(ORNL::RegionType::kPerimeter);
    if (travel) {
        ORNL::TravelSegment approach(ORNL::Point(0, 0, start.z() - 10.0 * ORNL::mm()), start,
                                     ORNL::TravelLiftType::kLiftUpOnly);
        approach.setSb(settings);
        result += approach.writeGCode(writer);
    }
    if (arc) {
        ORNL::ArcSegment segment(start, end, ORNL::Point(start.x(), end.y(), start.z()), false);
        segment.setSb(settings);
        result += segment.writeGCode(writer);
    }
    else {
        ORNL::LineSegment segment(start, end);
        segment.setSb(settings);
        result += segment.writeGCode(writer);
    }
    result += writer->writeAfterPath(ORNL::RegionType::kPerimeter);
    result += writer->writeAfterRegion(ORNL::RegionType::kPerimeter);
    return result;
}

class SlicingHarness : public ORNL::AbstractSlicingThread {
   public:
    explicit SlicingHarness(const QString& path) : AbstractSlicingThread(path) {
        setMaxSteps(2);
    }
    void doSlice() override {}
    void preProcess(nlohmann::json) override {}
    void postProcess(nlohmann::json) override {}
    void cleanThread() override {}
    bool generate() {
        standalone_callbacks = 0;
        return writeGCodeFile();
    }
    bool cancel() {
        setCancel();
        return shouldCancel();
    }
    void writeGCode() override {
        auto settings = ORNL::GSM->getGlobal();
        if (flip_generation)
            settings->setSetting(ORNL::PS::ArcSpecialties::kGenerateWeldScheduleFile,
                                 !settings->setting<bool>(ORNL::PS::ArcSpecialties::kGenerateWeldScheduleFile));
        QTextStream stream(&m_temp_gcode_output_file);
        for (uint layer = 0; layer < 2; ++layer) {
            const float min_z = (layer + 1) * 10.0 * ORNL::mm();
            stream << m_base->writeLayerChange(layer) << m_base->writeBeforeLayer(min_z, settings);
            stream << paths(m_base, settings, layer, true) << m_base->writeAfterLayer();
            prepareLayerExport(layer, min_z, settings, [&](const QSharedPointer<ORNL::WriterBase>& writer) {
                ++standalone_callbacks;
                return paths(writer, settings, layer, true);
            });
        }
        stream << m_base->writeAfterPart();
    }
    int standalone_callbacks = 0;
    bool flip_generation     = false;
};

struct Program {
    QString source;
    QVector<ORNL::GcodeLayer> layers;
};

template <typename Writer>
Program program(const ORNL::GcodeMeta& meta, const QSharedPointer<ORNL::SettingsBase>& settings, bool travel = true,
                bool arc = false, bool prepare = true, int empty_layer = -1) {
    auto writer      = QSharedPointer<Writer>::create(meta, settings);
    const auto setup = [](const QSharedPointer<Writer>& target) { return target->writeInitialSetup(0, 0, 0, 0, 3); };
    const QString header = setup(writer);
    Program result {header, {}};
    for (int layer = 0; layer < 3; ++layer) {
        result.source += writer->writeLayerChange(layer);
        result.source += writer->writeBeforeLayer((layer + 1) * 10.0 * ORNL::mm(), settings);
        result.source += layer == empty_layer ? writer->writeEmptyStep() : paths(writer, settings, layer, travel, arc);
        result.source += writer->writeAfterLayer();
        if (prepare) {
            auto independent = QSharedPointer<Writer>::create(meta, settings);
            setup(independent);
            const QString standalone =
                independent->writeStandaloneLayer(layer, (layer + 1) * 10.0 * ORNL::mm(), settings, [&]() {
                    return layer == empty_layer ? independent->writeEmptyStep()
                                                : paths(independent, settings, layer, travel, arc);
                });
            result.layers.push_back({ORNL::WriterBase::layerNumber(settings, layer), header + standalone});
        }
    }
    result.source += writer->writeAfterPart();
    result.source += writer->writeShutdown();
    const QString footer =
        meta.m_comment_starting_delimiter + "settings footer" + meta.m_comment_ending_delimiter + '\n';
    result.source += footer;
    if (!result.layers.isEmpty()) { result.layers.last().text += footer; }
    return result;
}

QString readFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) { return {}; }
    return QString::fromUtf8(file.readAll());
}

bool exportProgram(ORNL::GcodeExport& dialog, const QString& path) {
    QEventLoop loop;
    QTimer driver, timeout;
    bool selected = false, completed = false, success = false;
    QObject::connect(&driver, &QTimer::timeout, [&] {
        for (QWidget* widget : QApplication::topLevelWidgets()) {
            if (!widget->isVisible()) continue;
            if (auto* chooser = qobject_cast<QFileDialog*>(widget); chooser && !selected) {
                chooser->selectFile(path);
                auto* buttons = chooser->findChild<QDialogButtonBox*>();
                auto* save    = buttons ? buttons->button(QDialogButtonBox::Save) : nullptr;
                if (save && save->isEnabled()) {
                    selected = true;
                    save->click();
                }
            }
            if (auto* message = qobject_cast<QMessageBox*>(widget)) {
                success   = message->icon() == QMessageBox::Information;
                completed = true;
                message->accept();
                loop.quit();
            }
        }
    });
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, [&] {
        for (QWidget* widget : QApplication::topLevelWidgets())
            if (auto* modal = qobject_cast<QDialog*>(widget); modal && modal->isVisible()) modal->reject();
        loop.quit();
    });
    driver.start(10);
    timeout.start(10000);
    dialog.show();
    QTimer::singleShot(0, &dialog, [&] { QMetaObject::invokeMethod(&dialog, "exportGcode", Qt::DirectConnection); });
    loop.exec();
    return selected && completed && success;
}
}  // namespace

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    Q_INIT_RESOURCE(configs);
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(styles);
    bool passed       = true;
    const auto expect = [&passed](bool condition, const char* message) {
        if (!condition) {
            std::cerr << message << '\n';
            passed = false;
        }
    };
    const auto settings      = writerSettings();
    const auto marlin_source = program<ORNL::MarlinWriter>(ORNL::GcodeMetaList::MarlinMeta, settings);
    auto layers              = marlin_source.layers;
    expect(layers.size() == 3, "Expected three independently generated layers.");
    if (layers.size() == 3) {
        const QString header = marlin_source.source.left(marlin_source.source.indexOf(";BEGINNING LAYER: 1"));
        for (int i = 0; i < layers.size(); ++i) {
            expect(layers[i].number == i + 1 && layers[i].text.startsWith(header),
                   "Expected each layer's number and complete original header.");
            for (const QString& command : {QString("M400"), QString("M104 S0"), QString("M140 S0"), QString("M84")}) {
                const int lift = layers[i].text.indexOf("TRAVEL FINAL LIFT");
                expect(lift >= 0 && layers[i].text.mid(lift).count(command) == 1,
                       "Expected a complete dialect shutdown exactly once after the final lift.");
            }
            expect(layers[i].text.contains("TRAVEL FINAL LIFT") && !layers[i].text.contains("LAYER EXPORT"),
                   "Expected a real final lift without export metadata.");
        }
        expect(layers.last().text.endsWith(";settings footer\n"), "Expected the final settings footer to survive.");
    }

    settings->setSetting(ORNL::PRS::GCode::kStartingLayerNumber, 120);
    const auto numbered_marlin = program<ORNL::MarlinWriter>(ORNL::GcodeMetaList::MarlinMeta, settings);
    const auto numbered_arc    = program<ORNL::ArcSpecialtiesWriter>(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);
    for (const auto& source : {numbered_marlin, numbered_arc}) {
        layers = source.layers;
        expect(layers.size() == 3, "Expected renumbered standalone layers to be prepared.");
        for (int i = 0; i < layers.size(); ++i) {
            expect(layers[i].number == 120 + i && layers[i].text.contains(QString("BEGINNING LAYER: %1").arg(120 + i)),
                   "Expected standalone programs and exported layer numbers to agree.");
            if (source.source == numbered_arc.source) {
                expect(layers[i].text.contains(QString("BEGINNING BEAD: %1.1").arg(120 + i)),
                       "Expected standalone Arc Specialties bead references to match their layer.");
            }
        }
    }
    QString normalized = numbered_marlin.source;
    for (int i = 0; i < 3; ++i) {
        normalized.replace(QString("BEGINNING LAYER: %1").arg(120 + i), QString("BEGINNING LAYER: %1").arg(1 + i));
    }
    expect(normalized == marlin_source.source, "Renumbering must preserve all other motion and setup commands.");
    QTemporaryDir numbered_directory;
    ORNL::GCodeLayerSaver numbered_saver(numbered_directory.path(), "part", numbered_marlin.layers, ".gcode");
    numbered_saver.run();
    expect(QFile::exists(numbered_directory.filePath("part_layer_120.gcode")) &&
               QFile::exists(numbered_directory.filePath("part_layer_122.gcode")),
           "Expected individual-layer filenames to use configured numbering.");
    settings->remove(ORNL::PRS::GCode::kStartingLayerNumber);

    const auto cincinnati_source = program<ORNL::CincinnatiWriter>(ORNL::GcodeMetaList::CincinnatiMeta, settings);
    expect(
        cincinnati_source.source ==
            program<ORNL::CincinnatiWriter>(ORNL::GcodeMetaList::CincinnatiMeta, settings, true, false, false).source,
        "Expected standalone generation to leave continuous Cincinnati commands unchanged.");
    layers = cincinnati_source.layers;
    expect(layers.size() == 3, "Expected three standalone Cincinnati layers.");
    if (layers.size() == 3) {
        const QString body = layers[1].text.mid(layers[1].text.indexOf("(BEGINNING LAYER: 2)"));
        // Fresh W=100, one 10 mm layer change => W=90. At part Z=20, Z = 20 - 150 + 90 = -40 mm.
        const QString expected_z = " Z" + QString::number((-40.0 * ORNL::mm).to(ORNL::in), 'f', 4);
        const QString expected_w = " W" + QString::number((90.0 * ORNL::mm).to(ORNL::in), 'f', 4);
        expect(body.contains(expected_z) && body.contains(expected_w) &&
                   body.indexOf(expected_z) < body.indexOf("TURN EXTRUDER ON"),
               "Expected the second standalone layer to establish its own Z/W coordinates despite zero travel lift.");
        expect(body.contains("M5") && body.contains("M30"), "Expected Cincinnati's own shutdown.");
    }

    const auto mvp_source = program<ORNL::MVPWriter>(ORNL::GcodeMetaList::MVPMeta, settings);
    expect(mvp_source.source ==
               program<ORNL::MVPWriter>(ORNL::GcodeMetaList::MVPMeta, settings, true, false, false).source,
           "Expected standalone generation to leave continuous MVP commands unchanged.");
    layers = mvp_source.layers;
    expect(layers.size() == 3, "Expected three MVP layers.");
    if (layers.size() == 3) {
        for (int i = 0; i < layers.size(); ++i) {
            const QString text   = layers[i].text;
            const int lift       = text.indexOf("TRAVEL FINAL LIFT Z");
            const int park       = text.indexOf("G0 X0 Y0", lift);
            const QString lift_z = " Z" + QString::number(-150.0 - (i + 1) * 10.0 - 2.0, 'f', 4);
            expect(lift >= 0 && park > lift && text.left(lift).endsWith(lift_z + " ;"),
                   "Expected each MVP layer's own endpoint plus clearance before its XY park.");
        }
    }

    settings->setSetting(ORNL::PS::Slicing::kSlicePlaneNormalX, 1.0);
    layers = program<ORNL::MVPWriter>(ORNL::GcodeMetaList::MVPMeta, settings).layers;
    const QRegularExpression tilted_lift("G0 X([0-9.]+) Y([0-9.]+) Z(-[0-9.]+) ;TRAVEL FINAL LIFT");
    const auto tilted_match = layers.size() == 3 ? tilted_lift.match(layers[0].text) : QRegularExpressionMatch();
    // Point coordinates use floats in internal units; compare within one micron, rather than formatting digits.
    expect(tilted_match.hasMatch() && qAbs(tilted_match.captured(1).toDouble() - 31.41421356) < 0.001 &&
               qAbs(tilted_match.captured(2).toDouble() - 40.0) < 0.001 &&
               qAbs(tilted_match.captured(3).toDouble() + 161.41421356) < 0.001,
           "Expected final clearance along the configured slicing normal from the layer endpoint.");
    settings->setSetting(ORNL::PS::Slicing::kSlicePlaneNormalX, 0.0);

    settings->setSetting(ORNL::PS::Travel::kFinalLiftDistance, 0.0 * ORNL::mm);
    layers = program<ORNL::MVPWriter>(ORNL::GcodeMetaList::MVPMeta, settings).layers;
    expect(layers.size() == 3 && !layers[0].text.contains("TRAVEL FINAL LIFT"),
           "Expected zero configured final lift to remain zero.");
    settings->setSetting(ORNL::PS::Travel::kFinalLiftDistance, 2.0 * ORNL::mm);

    // Spiral layers can begin with a printing segment, without a preceding TravelSegment.
    settings->setSetting(ORNL::PS::SpecialModes::kEnableSpiralize, true);
    layers = program<ORNL::MVPWriter>(ORNL::GcodeMetaList::MVPMeta, settings, false).layers;
    expect(layers.size() == 3 && layers[1].text.contains("TURN GUN ON") &&
               layers[1].text.contains("TURN EXTRUDER ON") &&
               layers[1].text.indexOf(";TRAVEL") < layers[1].text.indexOf("TURN GUN ON"),
           "Expected a fresh approach and pump startup for a standalone spiral layer.");
    layers = program<ORNL::CincinnatiWriter>(ORNL::GcodeMetaList::CincinnatiMeta, settings, false, true).layers;
    expect(layers.size() == 3 && layers[1].text.contains("TRAVEL") && layers[1].text.contains("G2"),
           "Expected an approach before an arc-only standalone layer.");
    settings->setSetting(ORNL::PS::SpecialModes::kEnableSpiralize, false);

    expect(program<ORNL::HaasWriter>(ORNL::GcodeMetaList::HaasMetricMeta, settings).layers.size() == 3,
           "Expected Haas standalone output.");
    expect(program<ORNL::WolfWriter>(ORNL::GcodeMetaList::WolfMeta, settings).layers.size() == 3,
           "Expected Wolf standalone output.");

    layers =
        program<ORNL::ArcSpecialtiesWriter>(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings, true, false, true, 0)
            .layers;
    expect(layers.size() == 3 && layers[0].text.contains("INTENTIONALLY BLANK") && layers[1].text.contains("#KIN ID"),
           "Expected deferred layer markers and an initial empty robot layer to retain independent startup state.");

    QTemporaryDir directory;
    expect(directory.isValid(), "Expected a temporary output directory.");
    if (directory.isValid()) {
        const QString stale_path = QDir(directory.path()).filePath("part_layer_99.gcode");
        QFile stale(stale_path);
        expect(stale.open(QIODevice::WriteOnly), "Expected a pre-existing layer file for the worker test.");
        stale.write("original\n");
        stale.close();
        ORNL::GCodeLayerSaver rejected(directory.path(), "part", {}, ".gcode");
        QString worker_error;
        QObject::connect(&rejected, &ORNL::GCodeLayerSaver::completed,
                         [&worker_error](const QString& message) { worker_error = message; });
        rejected.run();
        expect(!worker_error.isEmpty() && readFile(stale_path) == "original\n" &&
                   QDir(directory.path()).entryList(QDir::Files).size() == 1,
               "Expected missing standalone programs to preserve existing files and produce no outputs.");

        ORNL::GCodeLayerSaver saver(directory.path(), "part", mvp_source.layers, ".mpf");
        worker_error = "not completed";
        QObject::connect(&saver, &ORNL::GCodeLayerSaver::completed,
                         [&worker_error](const QString& message) { worker_error = message; });
        saver.run();
        const QString saved = readFile(QDir(directory.path()).filePath("part_layer_1.mpf"));
        expect(worker_error.isEmpty() && saved.contains("TRAVEL FINAL LIFT Z") && saved.contains("G0 X0 Y0"),
               "Expected the worker to persist each layer's clearance and parking commands.");
    }

    auto global              = ORNL::GSM->getGlobal();
    const QString export_key = ORNL::ES::FileOutput::kIndividualLayerOutput;
    expect(!global->setting<bool>(export_key), "Individual-layer preparation must be disabled by default.");
    global->populate(settings);
    global->setSetting(ORNL::PRS::MachineSetup::kSyntax, ORNL::GcodeSyntax::kMarlin);
    global->setSetting(ORNL::PRS::GCode::kEnableSettingsFooter, true);
    global->setSetting(ORNL::PRS::GCode::kLayerTimeComments, false);
    QTemporaryDir slice_directory;
    expect(slice_directory.isValid(), "Expected a temporary slicing output directory.");
    const QString slice_path = slice_directory.filePath("continuous.gcode");
    SlicingHarness slicer(slice_path);
    expect(slicer.generate(), "Expected default continuous generation to succeed.");
    expect(slicer.standalone_callbacks == 0 && slicer.layerPrograms().isEmpty(),
           "Disabled export must skip standalone generation and retain no programs.");
    const QString disabled_source = readFile(slice_path);
    global->setSetting(export_key, true);
    global->setSetting(ORNL::PRS::GCode::kStartingLayerNumber, 120);
    slicer.setGcodeOutput(slice_path);
    expect(slicer.generate(), "Expected opt-in standalone preparation to succeed.");
    const QString enabled_source = readFile(slice_path);
    const auto prepared          = slicer.layerPrograms();
    expect(slicer.standalone_callbacks == 2 && prepared.size() == 2,
           "Enabled export must prepare exactly one standalone program per layer.");
    for (const QString& source : {disabled_source, enabled_source}) {
        expect(!source.isEmpty() && !source.contains("LAYER EXPORT") && !source.contains("FINAL POSITION MOVES"),
               "Continuous G-code must contain no standalone export metadata, with export off or on.");
    }
    if (prepared.size() == 2) {
        const QString header = enabled_source.left(enabled_source.indexOf(";BEGINNING LAYER: 120"));
        const QString footer = enabled_source.mid(enabled_source.indexOf("\n;Settings Footer\n"));
        expect(!header.isEmpty() && footer.startsWith("\n;Settings Footer\n"),
               "Expected the continuous program to retain its normal header and settings footer.");
        for (int index = 0; index < prepared.size(); ++index) {
            const auto& layer = prepared[index];
            expect(layer.number == 120 + index && layer.text.startsWith(header) &&
                       layer.text.count("BEGINNING LAYER:") == 1 && layer.text.contains("TRAVEL FINAL LIFT") &&
                       layer.text.contains("M400") && layer.text.contains("M84") &&
                       !layer.text.contains("LAYER EXPORT"),
                   "Prepared layers must retain their own numbering, shared header, clearance, and shutdown.");
        }
        expect(prepared.last().text.endsWith(footer), "The last prepared layer must retain the settings footer.");
        ORNL::GCodeLayerSaver prepared_saver(slice_directory.path(), "standalone", prepared, ".gcode");
        prepared_saver.run();
        expect(readFile(slice_directory.filePath("standalone_layer_121.gcode")) == prepared.last().text,
               "The layer saver must persist prepared programs without requiring encoded metadata.");
    }
    global->setSetting(export_key, false);
    slicer.setGcodeOutput(slice_path);
    expect(slicer.layerPrograms().isEmpty(), "Starting another slice must clear previously prepared programs.");
    expect(slicer.generate() && slicer.standalone_callbacks == 0 && slicer.layerPrograms().isEmpty(),
           "Disabling export on a later slice must not reuse or regenerate stale standalone programs.");

    global->setSetting(export_key, true);
    global->setSetting(ORNL::PRS::MachineSetup::kSyntax, ORNL::GcodeSyntax::kHaasMetricNoComments);
    slicer.setGcodeOutput(slice_path);
    expect(slicer.generate() && slicer.standalone_callbacks == 2 && slicer.layerPrograms().size() == 2,
           "Haas Metric No Comments must prepare standalone programs without embedded layer markers.");
    expect(!readFile(slice_path).contains("LAYER EXPORT") && !readFile(slice_path).contains("FINAL POSITION MOVES"),
           "Haas Metric No Comments must not receive export metadata even with individual layers enabled.");

    ORNL::GcodeExport export_dialog(nullptr);
    export_dialog.updateOutputInformation(slice_path, ORNL::GcodeMetaList::MarlinMeta, prepared);
    export_dialog.show();
    app.processEvents();
    QLabel* layer_status = nullptr;
    for (QLabel* label : export_dialog.findChildren<QLabel*>()) {
        if (label->text().startsWith("Individual layers:")) { layer_status = label; }
    }
    expect(layer_status && layer_status->text().contains("will be saved"),
           "The export dialog must advertise separate files when the loaded slice prepared programs.");
    if (argc > 1) {
        expect(export_dialog.grab().save(QString(argv[1]) + "/layer-export-enabled.png"),
               "Could not capture the enabled layer export dialog.");
    }
    export_dialog.clearVisualizationInformation();
    expect(layer_status && layer_status->text().contains("before slicing"),
           "Beginning a new G-code load must clear the previous slice's layer export status.");
    export_dialog.updateOutputInformation(slice_path, ORNL::GcodeMetaList::MarlinMeta);
    app.processEvents();
    expect(layer_status && layer_status->text().contains("before slicing"),
           "Reopening a G-code file at the same path must not retain the previous slice's layer availability.");
    if (argc > 1) {
        expect(export_dialog.grab().save(QString(argv[1]) + "/layer-export-disabled.png"),
               "Could not capture the disabled layer export dialog.");
    }

    ORNL::CSM->setMostRecentSettingFolderLocation(slice_directory.path());
    ORNL::CSM->setMostRecentLayerBarSettingFolderLocation(slice_directory.path());
    auto* main_window              = ORNL::MainWindow::getInstance();
    ORNL::GcodeExport* main_export = nullptr;
    for (QWidget* widget : QApplication::topLevelWidgets()) {
        auto* candidate = qobject_cast<ORNL::GcodeExport*>(widget);
        if (candidate && candidate != &export_dialog) { main_export = candidate; }
    }
    expect(main_export != nullptr, "Expected the main window's export dialog.");
    if (main_export) {
        main_export->updateOutputInformation(slice_path, ORNL::GcodeMetaList::MarlinMeta, prepared);
        QLabel* main_layer_status = nullptr;
        for (QLabel* label : main_export->findChildren<QLabel*>()) {
            if (label->text().startsWith("Individual layers:")) { main_layer_status = label; }
        }
        expect(main_layer_status && main_layer_status->text().contains("will be saved"),
               "Expected the previous slice's prepared programs to be available before a new slice.");
        // No build parts: doSlice returns without importing another G-code file, like a cancelled or failed slice.
        expect(!ORNL::CSM->isBuildMode(), "Expected no build parts in the slice-start regression test.");
        expect(QMetaObject::invokeMethod(main_window, "doSlice", Qt::DirectConnection),
               "Could not invoke the main window's slice-start path.");
        expect(main_layer_status && main_layer_status->text().contains("before slicing"),
               "Starting a new slice must invalidate the export snapshot even without a subsequent G-code import.");
    }

    global->setSetting(ORNL::PRS::MachineSetup::kSyntax, ORNL::GcodeSyntax::kArcSpecialties);
    global->setSetting(ORNL::PS::Slicing::kSlicingMode, 0);
    global->setSetting(ORNL::PS::Perimeter::kSpeed, 5.0 * ORNL::mm / ORNL::s);
    global->setSetting(ORNL::PRS::GCode::kArcSpecialtiesG80WeldScheduleFile, QString());
    global->setSetting(ORNL::PRS::GCode::kStartingLayerNumber, 1);
    ORNL::GcodeExport arc_export(nullptr);
    for (bool individual : {false, true}) {
        for (bool generated : {false, true}) {
            global->setSetting(export_key, individual);
            global->setSetting(ORNL::PS::ArcSpecialties::kGenerateWeldScheduleFile, generated);
            const QString basename   = QString("arc_%1_%2").arg(individual).arg(generated);
            const QString input_path = slice_directory.filePath(basename + "_sliced.nc");
            SlicingHarness arc_slice(input_path);
            arc_slice.flip_generation = individual && generated;
            expect(arc_slice.generate(), "Expected a complete ARC slice.");
            expect(arc_slice.requiresWeldScheduleFile() == generated,
                   "The completed slice must retain the writer's generation decision.");
            expect(readFile(input_path).contains("FV.S.SPEED") == generated,
                   "The sliced motion must reflect its generation setting.");
            global->setSetting(ORNL::PS::ArcSpecialties::kGenerateWeldScheduleFile, !generated);
            arc_export.updateOutputInformation(input_path, ORNL::GcodeMetaList::ArcSpecialtiesMeta,
                                               arc_slice.layerPrograms(), arc_slice.requiresWeldScheduleFile());
            if (!individual) arc_export.clearVisualizationInformation();
            const QString output_path = slice_directory.filePath(basename + ".nc");
            expect(exportProgram(arc_export, output_path), "ARC GUI export must complete successfully.");
            expect(QFile::exists(slice_directory.filePath(basename + "_sch.nc")) == generated,
                   "Companion creation must follow the completed slice, not the current setting.");
            for (int layer : individual ? QList<int> {1, 2} : QList<int> {0}) {
                const QString path =
                    layer == 0 ? output_path : slice_directory.filePath(basename + QString("_layer_%1.nc").arg(layer));
                const QString saved = readFile(path);
                expect(!saved.isEmpty(), "Expected every exported ARC program.");
                if (generated)
                    expect(saved.contains("FV.S.SPEED") && saved.contains("G80=\"" + basename + "_sch.nc\""),
                           "Generated motion must retain its patched companion reference after disabling generation.");
                else
                    expect(!saved.contains("FV.S.SPEED") && saved.contains("F300.0000"),
                           "Numeric motion must remain numeric after enabling generation without slicing again.");
            }
            if (generated) {
                expect(arc_slice.cancel() && !arc_slice.requiresWeldScheduleFile(),
                       "Cancelling must clear unpublished slice export metadata.");
                arc_slice.setGcodeOutput(input_path);
                expect(!arc_slice.requiresWeldScheduleFile(), "A new output must not inherit a companion decision.");
                expect(arc_slice.generate() && !arc_slice.requiresWeldScheduleFile() &&
                           !readFile(input_path).contains("FV.S.SPEED"),
                       "A subsequent numeric slice must replace the previous generated-schedule decision.");
            }
        }
    }
    const QString manual_input = slice_directory.filePath("manual_input.nc");
    QFile manual_file(manual_input);
    expect(manual_file.open(QIODevice::WriteOnly), "Expected a reopened ARC program fixture.");
    manual_file.write("#FILE NAME[ G80=\"external_sch.nc\" ]\nG01 X=1 FV.S.SPEED\n");
    manual_file.close();
    global->setSetting(ORNL::PS::ArcSpecialties::kGenerateWeldScheduleFile, true);
    arc_export.updateOutputInformation(manual_input, ORNL::GcodeMetaList::ArcSpecialtiesMeta);
    const QString manual_output = slice_directory.filePath("manual_output.nc");
    expect(exportProgram(arc_export, manual_output) && readFile(manual_output).contains("G80=\"external_sch.nc\"") &&
               !QFile::exists(slice_directory.filePath("manual_output_sch.nc")),
           "Reopened/manual programs must not inherit a previous slice's companion requirement.");

    expect(ORNL::isGcodeLayerFileName("part_layer_12.gcode", "part", ".gcode"), "Expected a generated name.");
    expect(ORNL::isGcodeLayerFileName("part[1]_layer_12.gcode", "part[1]", ".gcode"), "Expected literal part names.");
    expect(!ORNL::isGcodeLayerFileName("part_layer_12.gcode.bak", "part", ".gcode"), "Expected an exact suffix.");
    expect(!ORNL::isGcodeLayerFileName("part_layer_notes.gcode", "part", ".gcode"), "Expected numeric layer names.");
    expect(!ORNL::isGcodeLayerFileName("part_layer_012.gcode", "part", ".gcode"), "Expected canonical layer numbers.");
    return passed ? 0 : 1;
}
