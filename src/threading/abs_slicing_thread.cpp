#include "threading/abs_slicing_thread.h"

#include <QApplication>
#include <QStringBuilder>
#include <QTextStream>
#include <algorithm>
#include <climits>
#include <exception>
#include <limits>
#include <memory>
#include <stdexcept>

#include <qdebug.h>
#include <qfiledevice.h>
#include <qfileinfo.h>
#include <qhashfunctions.h>
#include <qobject.h>
#include <qregularexpression.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>
#include <qtypes.h>

#include "gcode/gcode_layer_export.h"
#include "gcode/gcode_meta.h"
#include "gcode/parsers/adamantine_parser.h"
#include "gcode/parsers/aerobasic_parser.h"
#include "gcode/parsers/arc_specialties_parser.h"
#include "gcode/parsers/beam_parser.h"
#include "gcode/parsers/cincinnati_parser.h"
#include "gcode/parsers/common_parser.h"
#include "gcode/parsers/marlin_parser.h"
#include "gcode/parsers/mazak_parser.h"
#include "gcode/parsers/mvp_parser.h"
#include "gcode/parsers/siemens_parser.h"
#include "gcode/parsers/tormach_parser.h"
#include "gcode/writers/adamantine_writer.h"
#include "gcode/writers/aerobasic_writer.h"
#include "gcode/writers/aml3d_writer.h"
#include "gcode/writers/arc_specialties_writer.h"
#include "gcode/writers/cincinnati_writer.h"
#include "gcode/writers/dmg_dmu_writer.h"
#include "gcode/writers/gudel_writer.h"
#include "gcode/writers/haas_metric_no_comments_writer.h"
#include "gcode/writers/haas_writer.h"
#include "gcode/writers/hurco_writer.h"
#include "gcode/writers/ingersoll_writer.h"
#include "gcode/writers/juggerbot_writer.h"
#include "gcode/writers/kraussmaffei_writer.h"
#include "gcode/writers/mach4_writer.h"
#include "gcode/writers/marlin_writer.h"
#include "gcode/writers/mazak_writer.h"
#include "gcode/writers/meld_writer.h"
#include "gcode/writers/meltio_writer.h"
#include "gcode/writers/mvp_writer.h"
#include "gcode/writers/okuma_writer.h"
#include "gcode/writers/ornl_writer.h"
#include "gcode/writers/reprap_writer.h"
#include "gcode/writers/romi_fanuc_writer.h"
#include "gcode/writers/sandia_writer.h"
#include "gcode/writers/siemens_writer.h"
#include "gcode/writers/thermwood_writer.h"
#include "gcode/writers/tormach_writer.h"
#include "gcode/writers/wolf_writer.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
namespace {
GcodeMeta metaForSyntax(GcodeSyntax syntax) {
    switch (syntax) {
        case GcodeSyntax::kAML3D:
            return GcodeMetaList::AML3DMeta;
        case GcodeSyntax::kBeam:
            return GcodeMetaList::BeamMeta;
        case GcodeSyntax::kCincinnati:
        case GcodeSyntax::kThermwood:
            return GcodeMetaList::CincinnatiMeta;
        case GcodeSyntax::kDmgDmu:
            return GcodeMetaList::DmgDmuAndBeamMeta;
        case GcodeSyntax::kGudel:
            return GcodeMetaList::GudelMeta;
        case GcodeSyntax::kHaasInch:
            return GcodeMetaList::HaasInchMeta;
        case GcodeSyntax::kHaasMetric:
        case GcodeSyntax::kHaasMetricNoComments:
        case GcodeSyntax::kOkuma:
            return GcodeMetaList::HaasMetricMeta;
        case GcodeSyntax::kHurco:
            return GcodeMetaList::HurcoMeta;
        case GcodeSyntax::kIngersoll:
            return GcodeMetaList::IngersollMeta;
        case GcodeSyntax::kKraussMaffei:
            return GcodeMetaList::KraussMaffeiMeta;
        case GcodeSyntax::kJuggerBot:
        case GcodeSyntax::kMach4:
        case GcodeSyntax::kMarlin:
            return GcodeMetaList::MarlinMeta;
        case GcodeSyntax::kRepRap:
            return GcodeMetaList::RepRapMeta;
        case GcodeSyntax::kMazak:
            return GcodeMetaList::MazakMeta;
        case GcodeSyntax::kMeld:
            return GcodeMetaList::MeldMeta;
        case GcodeSyntax::kMeltio:
            return GcodeMetaList::MeltioMeta;
        case GcodeSyntax::kMVP:
            return GcodeMetaList::MVPMeta;
        case GcodeSyntax::kORNLMetric:
            return GcodeMetaList::ORNLMetricMeta;
        case GcodeSyntax::kORNL:
            return GcodeMetaList::ORNLMeta;
        case GcodeSyntax::kArcSpecialties:
            return GcodeMetaList::ArcSpecialtiesMeta;
        case GcodeSyntax::kRomiFanuc:
            return GcodeMetaList::RomiFanucMeta;
        case GcodeSyntax::kSandia:
            return GcodeMetaList::SandiaMeta;
        case GcodeSyntax::kSiemens:
            return GcodeMetaList::SiemensMeta;
        case GcodeSyntax::kTormach:
            return GcodeMetaList::TormachMeta;
        case GcodeSyntax::kWolf:
            return GcodeMetaList::WolfMeta;
        case GcodeSyntax::kAeroBasic:
            return GcodeMetaList::AeroBasicMeta;
        case GcodeSyntax::kAdamantine:
            return GcodeMetaList::AdamantineMeta;
        default:
            return GcodeMetaList::MarlinMeta;
    }
}

std::unique_ptr<CommonParser> makeLayerTimeParser(GcodeSyntax syntax, QStringList& original_lines,
                                                  QStringList& upper_lines) {
    constexpr bool kAllowLayerAlter = true;
    const GcodeMeta meta            = metaForSyntax(syntax);

    switch (syntax) {
        case GcodeSyntax::kAML3D:
            return std::make_unique<CincinnatiParser>(meta, kAllowLayerAlter, original_lines, upper_lines);
        case GcodeSyntax::kBeam:
            return std::make_unique<BeamParser>(meta, kAllowLayerAlter, original_lines, upper_lines);
        case GcodeSyntax::kCincinnati:
        case GcodeSyntax::kThermwood:
            return std::make_unique<CincinnatiParser>(meta, kAllowLayerAlter, original_lines, upper_lines);
        case GcodeSyntax::kKraussMaffei:
        case GcodeSyntax::kJuggerBot:
        case GcodeSyntax::kMach4:
        case GcodeSyntax::kMarlin:
        case GcodeSyntax::kRepRap:
            return std::make_unique<MarlinParser>(meta, kAllowLayerAlter, original_lines, upper_lines);
        case GcodeSyntax::kMazak:
            return std::make_unique<MazakParser>(meta, kAllowLayerAlter, original_lines, upper_lines);
        case GcodeSyntax::kMVP:
            return std::make_unique<MVPParser>(meta, kAllowLayerAlter, original_lines, upper_lines);
        case GcodeSyntax::kArcSpecialties:
            return std::make_unique<ArcSpecialtiesParser>(meta, kAllowLayerAlter, original_lines, upper_lines);
        case GcodeSyntax::kSiemens:
            return std::make_unique<SiemensParser>(meta, kAllowLayerAlter, original_lines, upper_lines);
        case GcodeSyntax::kTormach:
            return std::make_unique<TormachParser>(meta, kAllowLayerAlter, original_lines, upper_lines);
        case GcodeSyntax::kAeroBasic:
            return std::make_unique<AeroBasicParser>(meta, kAllowLayerAlter, original_lines, upper_lines);
        case GcodeSyntax::kAdamantine:
            return std::make_unique<AdamantineParser>(meta, kAllowLayerAlter, original_lines, upper_lines);
        default:
            return std::make_unique<CommonParser>(meta, kAllowLayerAlter, original_lines, upper_lines);
    }
}

QString layerTimeComment(const GcodeMeta& meta, Time layer_time) {
    return meta.m_comment_starting_delimiter %
           QString("ORNL_SLICER_LAYER_TIME_ESTIMATE=%1").arg(layer_time.to(s), 0, 'f', 3) %
           meta.m_comment_ending_delimiter;
}
}  // namespace

AbstractSlicingThread::AbstractSlicingThread(QString outputLocation, bool skipGcode)
    : QObject(), m_min(0), m_max(INT_MAX), m_should_cancel(false) {
    m_skip_gcode = skipGcode;
    setGcodeOutput(outputLocation);

    this->moveToThread(&m_internal_thread);
    m_internal_thread.start();
}

AbstractSlicingThread::~AbstractSlicingThread() {
    m_internal_thread.quit();
    m_internal_thread.wait();
}

void AbstractSlicingThread::setBounds(int min, int max) {
    m_min = min;
    m_max = max;
}

int AbstractSlicingThread::getMinBound() {
    return m_min;
}

int AbstractSlicingThread::getMaxBound() {
    return m_max;
}

qint64 AbstractSlicingThread::getTimeElapsed() {
    return m_elapsed_time;
}

namespace {
QSharedPointer<WriterBase> makeWriter(GcodeSyntax syntax, const QSharedPointer<SettingsBase>& settings) {
    switch (syntax) {
        case GcodeSyntax::kAML3D:
            return QSharedPointer<AML3DWriter>(new AML3DWriter(GcodeMetaList::AML3DMeta, settings));
        case GcodeSyntax::kCincinnati:
            return QSharedPointer<CincinnatiWriter>(new CincinnatiWriter(GcodeMetaList::CincinnatiMeta, settings));
        case GcodeSyntax::kDmgDmu:
            return QSharedPointer<DMGDMUWriter>(new DMGDMUWriter(GcodeMetaList::DmgDmuAndBeamMeta, settings));
        case GcodeSyntax::kGudel:
            return QSharedPointer<GudelWriter>(new GudelWriter(GcodeMetaList::GudelMeta, settings));
        case GcodeSyntax::kHaasInch:
            return QSharedPointer<HaasWriter>(new HaasWriter(GcodeMetaList::HaasInchMeta, settings));
        case GcodeSyntax::kHaasMetric:
            return QSharedPointer<HaasWriter>(new HaasWriter(GcodeMetaList::HaasMetricMeta, settings));
        case GcodeSyntax::kHaasMetricNoComments:
            return QSharedPointer<HaasMetricNoCommentsWriter>(
                new HaasMetricNoCommentsWriter(GcodeMetaList::HaasMetricMeta, settings));
        case GcodeSyntax::kHurco:
            return QSharedPointer<HurcoWriter>(new HurcoWriter(GcodeMetaList::HurcoMeta, settings));
        case GcodeSyntax::kIngersoll:
            return QSharedPointer<IngersollWriter>(new IngersollWriter(GcodeMetaList::IngersollMeta, settings));
        case GcodeSyntax::kKraussMaffei:
            return QSharedPointer<KraussMaffeiWriter>(
                new KraussMaffeiWriter(GcodeMetaList::KraussMaffeiMeta, settings));
        case GcodeSyntax::kMarlin:
            return QSharedPointer<MarlinWriter>(new MarlinWriter(GcodeMetaList::MarlinMeta, settings));
        case GcodeSyntax::kJuggerBot:
            return QSharedPointer<JuggerBotWriter>(new JuggerBotWriter(GcodeMetaList::MarlinMeta, settings));
        case GcodeSyntax::kMazak:
            return QSharedPointer<MazakWriter>(new MazakWriter(GcodeMetaList::MazakMeta, settings));
        case GcodeSyntax::kMeld:
            return QSharedPointer<MeldWriter>(new MeldWriter(GcodeMetaList::MeldMeta, settings));
        case GcodeSyntax::kMeltio:
            return QSharedPointer<MeltioWriter>(new MeltioWriter(GcodeMetaList::MeltioMeta, settings));
        case GcodeSyntax::kMVP:
            return QSharedPointer<MVPWriter>(new MVPWriter(GcodeMetaList::MVPMeta, settings));
        case GcodeSyntax::kOkuma:
            return QSharedPointer<OkumaWriter>(new OkumaWriter(GcodeMetaList::HaasMetricMeta, settings));
        case GcodeSyntax::kORNL:
            return QSharedPointer<ORNLWriter>(new ORNLWriter(GcodeMetaList::ORNLMeta, settings));
        case GcodeSyntax::kRomiFanuc:
            return QSharedPointer<RomiFanucWriter>(new RomiFanucWriter(GcodeMetaList::RomiFanucMeta, settings));
        case GcodeSyntax::kSandia:
            return QSharedPointer<SandiaWriter>(new SandiaWriter(GcodeMetaList::SandiaMeta, settings));
        case GcodeSyntax::kSiemens:
            return QSharedPointer<SiemensWriter>(new SiemensWriter(GcodeMetaList::SiemensMeta, settings));
        case GcodeSyntax::kThermwood:
            return QSharedPointer<ThermwoodWriter>(new ThermwoodWriter(GcodeMetaList::CincinnatiMeta, settings));
        case GcodeSyntax::kTormach:
            return QSharedPointer<TormachWriter>(new TormachWriter(GcodeMetaList::TormachMeta, settings));
        case GcodeSyntax::kWolf:
            return QSharedPointer<WolfWriter>(new WolfWriter(GcodeMetaList::WolfMeta, settings));
        case GcodeSyntax::kRepRap:
            return QSharedPointer<RepRapWriter>(new RepRapWriter(GcodeMetaList::RepRapMeta, settings));
        case GcodeSyntax::kMach4:
            return QSharedPointer<Mach4Writer>(new Mach4Writer(GcodeMetaList::MarlinMeta, settings));
        case GcodeSyntax::kAeroBasic:
            return QSharedPointer<AeroBasicWriter>(new AeroBasicWriter(GcodeMetaList::AeroBasicMeta, settings));
        case GcodeSyntax::kAdamantine:
            return QSharedPointer<AdamantineWriter>(new AdamantineWriter(GcodeMetaList::AdamantineMeta, settings));
        case GcodeSyntax::kORNLMetric:
            return QSharedPointer<ORNLWriter>(new ORNLWriter(GcodeMetaList::ORNLMetricMeta, settings));
        case GcodeSyntax::kArcSpecialties:
            return QSharedPointer<ArcSpecialtiesWriter>(
                new ArcSpecialtiesWriter(GcodeMetaList::ArcSpecialtiesMeta, settings));
        default:
            return QSharedPointer<CincinnatiWriter>(new CincinnatiWriter(GcodeMetaList::CincinnatiMeta, settings));
    }
}
}  // namespace

void AbstractSlicingThread::setGcodeOutput(QString output) {
    m_layer_programs.clear();
    m_requires_weld_schedule_file = false;
    m_syntax                      = GSM->getGlobal()->setting<GcodeSyntax>(PRS::MachineSetup::kSyntax);
    m_base                        = makeWriter(m_syntax, GSM->getGlobal());

    if (!m_skip_gcode) {
        m_temp_gcode_output_file.setFileName(output);
        m_temp_gcode_output_file.open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text);
    }

    QFileInfo fi(output);
    m_temp_gcode_dir = fi.absoluteDir();
}

void AbstractSlicingThread::setCancel() {
    m_should_cancel = true;
}

bool AbstractSlicingThread::shouldCancel() {
    if (m_should_cancel) {
        m_should_cancel = false;
        m_temp_gcode_output_file.close();
        m_layer_programs.clear();
        m_requires_weld_schedule_file = false;
        return true;
    }

    return false;
}

void AbstractSlicingThread::setMaxSteps(int steps) {
    m_max_steps = steps;
}

int AbstractSlicingThread::getMaxSteps() {
    return m_max_steps;
}

void AbstractSlicingThread::forwardStatus(StatusUpdateStepType type, int completedPercentage) {
    emit statusUpdate(type, completedPercentage);
}

bool AbstractSlicingThread::writeGCodeFile() {
    m_layer_programs.clear();
    m_requires_weld_schedule_file = false;
    try {
        writeGCodeSetup();
        writeGCode();
        writeGCodeShutdown();
        m_requires_weld_schedule_file = m_syntax == GcodeSyntax::kArcSpecialties &&
                                        m_base.staticCast<ArcSpecialtiesWriter>()->shouldGenerateWeldScheduleFile();
        return true;
    } catch (const std::invalid_argument& error) {
        m_temp_gcode_output_file.resize(0);
        m_temp_gcode_output_file.close();
        m_layer_programs.clear();
        emit sliceFailed(QString::fromUtf8(error.what()));
        return false;
    }
}

void AbstractSlicingThread::writeGCodeSetup() {
    m_generate_individual_layers =
        m_syntax != GcodeSyntax::kAdamantine && GSM->getGlobal()->setting<bool>(ES::FileOutput::kIndividualLayerOutput);
    m_layer_export_header.clear();
    QTextStream stream(&m_layer_export_header);

    float minimum_x(std::numeric_limits<float>::max()), minimum_y(std::numeric_limits<float>::max()),
        maximum_x(std::numeric_limits<float>::min()), maximum_y(std::numeric_limits<float>::min()),
        maximum_z(std::numeric_limits<float>::lowest());
    bool has_part_bounds = false;

    for (QSharedPointer<Part> curr_part : CSM->parts()) {
        if (curr_part->rootMesh()->type() == MeshType::kClipping)  // Skip parts that were used for clipping
            continue;

        minimum_x       = std::min(minimum_x, curr_part->rootMesh()->min().x());
        minimum_y       = std::min(minimum_y, curr_part->rootMesh()->min().y());
        maximum_x       = std::max(maximum_x, curr_part->rootMesh()->max().x());
        maximum_y       = std::max(maximum_y, curr_part->rootMesh()->max().y());
        maximum_z       = std::max(maximum_z, curr_part->rootMesh()->max().z());
        has_part_bounds = true;
    }

    if (has_part_bounds) { m_base->setBuildMaximumZ(Distance(maximum_z)); }

    stream << m_base->writeSlicerHeader(toString(m_syntax));
    stream << m_base->writeSettingsHeader(m_syntax);
    stream << m_base->writeInitialSetup(Distance(minimum_x), Distance(minimum_y), Distance(maximum_x),
                                        Distance(maximum_y), m_max_steps);

    stream.flush();
    QTextStream output(&m_temp_gcode_output_file);
    output << m_layer_export_header;

    // Reproduce the state established by the shared header, without inheriting any earlier layer.
    m_layer_export_setup = [=, this]() {
        auto settings = GSM->getGlobal();
        if (m_syntax == GcodeSyntax::kArcSpecialties) {
            settings = QSharedPointer<SettingsBase>::create(*settings);
            settings->setSetting(PS::ArcSpecialties::kGenerateWeldScheduleFile,
                                 m_base.staticCast<ArcSpecialtiesWriter>()->shouldGenerateWeldScheduleFile());
        }
        auto writer = makeWriter(m_syntax, settings);
        if (has_part_bounds) { writer->setBuildMaximumZ(Distance(maximum_z)); }
        writer->writeInitialSetup(Distance(minimum_x), Distance(minimum_y), Distance(maximum_x), Distance(maximum_y),
                                  m_max_steps);
        return writer;
    };
}

void AbstractSlicingThread::prepareLayerExport(
    uint layer_number, float min_z, const QSharedPointer<SettingsBase>& settings,
    const std::function<QString(const QSharedPointer<WriterBase>&)>& emit_layer) {
    if (!m_generate_individual_layers) { return; }
    auto writer = m_layer_export_setup();
    const QString standalone =
        writer->writeStandaloneLayer(layer_number, min_z, settings, [&]() { return emit_layer(writer); });
    m_layer_programs.push_back({WriterBase::layerNumber(settings, layer_number), m_layer_export_header + standalone});
}

void AbstractSlicingThread::writeGCodeShutdown() {
    {
        QTextStream stream(&m_temp_gcode_output_file);
        stream << m_base->writeShutdown();
        if (m_syntax != GcodeSyntax::kMVP) {
            const QString footer = m_base->writeSettingsFooter();
            stream << footer;
            if (!m_layer_programs.isEmpty()) { m_layer_programs.last().text += footer; }
        }
        stream.flush();
    }
    writeLayerTimeComments();
    m_temp_gcode_output_file.close();
}

void AbstractSlicingThread::writeLayerTimeComments() {
    if (!GSM->getGlobal()->setting<bool>(PRS::GCode::kLayerTimeComments)) { return; }

    const QString file_name = m_temp_gcode_output_file.fileName();
    if (!m_temp_gcode_output_file.isOpen() || !m_temp_gcode_output_file.flush() || !m_temp_gcode_output_file.seek(0)) {
        qWarning() << "Unable to read generated G-Code for layer time comments:" << file_name;
        return;
    }

    QString text;
    {
        QTextStream in(&m_temp_gcode_output_file);
        text = in.readAll();
    }

    try {
        QStringList original_lines = text.split('\n');
        QStringList upper_lines    = text.toUpper().split('\n');

        std::unique_ptr<CommonParser> parser = makeLayerTimeParser(m_syntax, original_lines, upper_lines);
        parser->parseHeader();
        parser->parseFooter();
        parser->parseLines();

        const QList<Time> adjusted_layer_times = parser->getAdjustedLayerTimes();
        if (adjusted_layer_times.isEmpty()) { return; }

        const GcodeMeta meta = metaForSyntax(m_syntax);
        const QRegularExpression layer_marker("^\\s*" + QRegularExpression::escape(meta.m_comment_starting_delimiter) +
                                                  "\\s*BEGINNING\\s+LAYER\\s*:\\s*(\\d+)\\b",
                                              QRegularExpression::CaseInsensitiveOption);

        QStringList annotated_lines = text.split('\n');
        int layer_marker_count      = 0;
        for (const QString& line : annotated_lines) {
            if (layer_marker.match(line).hasMatch()) { ++layer_marker_count; }
        }

        const int layer_time_offset = adjusted_layer_times.size() > layer_marker_count ? 1 : 0;
        int generated_layer_index   = 0;
        for (int line_index = 0; line_index < annotated_lines.size(); ++line_index) {
            const QRegularExpressionMatch match = layer_marker.match(annotated_lines[line_index]);
            if (!match.hasMatch()) { continue; }

            const int layer_time_index = generated_layer_index++ + layer_time_offset;
            if (layer_time_index >= adjusted_layer_times.size()) { continue; }

            annotated_lines.insert(line_index + 1, layerTimeComment(meta, adjusted_layer_times[layer_time_index]));
            ++line_index;
        }

        if (!m_temp_gcode_output_file.resize(0) || !m_temp_gcode_output_file.seek(0)) {
            qWarning() << "Unable to write generated G-Code layer time comments:" << file_name;
            return;
        }

        QTextStream out(&m_temp_gcode_output_file);
        out << annotated_lines.join('\n');
        out.flush();
    } catch (const std::exception& exception) {
        qWarning() << "Unable to add generated G-Code layer time comments:" << exception.what();
        return;
    }
}
}  // namespace ORNL
