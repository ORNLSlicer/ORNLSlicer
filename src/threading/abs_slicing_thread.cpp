#include "threading/abs_slicing_thread.h"

#include <algorithm>
#include <climits>
#include <exception>
#include <limits>
#include <memory>

#include <QApplication>
#include <QStringBuilder>
#include <QTextStream>
#include <qdebug.h>
#include <qfiledevice.h>
#include <qfileinfo.h>
#include <qhashfunctions.h>
#include <qobject.h>
#include <qregularexpression.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>
#include <qtypes.h>

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
    const GcodeMeta meta = metaForSyntax(syntax);

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
} // namespace

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

int AbstractSlicingThread::getMinBound() { return m_min; }

int AbstractSlicingThread::getMaxBound() { return m_max; }

qint64 AbstractSlicingThread::getTimeElapsed() { return m_elapsed_time; }

void AbstractSlicingThread::setGcodeOutput(QString output) {
    m_syntax = GSM->getGlobal()->setting<GcodeSyntax>(PRS::MachineSetup::kSyntax);
    switch (m_syntax) {
        case GcodeSyntax::kAML3D:
            m_base = QSharedPointer<AML3DWriter>(new AML3DWriter(GcodeMetaList::AML3DMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kCincinnati:
            m_base =
                QSharedPointer<CincinnatiWriter>(new CincinnatiWriter(GcodeMetaList::CincinnatiMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kDmgDmu:
            m_base = QSharedPointer<DMGDMUWriter>(new DMGDMUWriter(GcodeMetaList::DmgDmuAndBeamMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kGudel:
            m_base = QSharedPointer<GudelWriter>(new GudelWriter(GcodeMetaList::GudelMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kHaasInch:
            m_base = QSharedPointer<HaasWriter>(new HaasWriter(GcodeMetaList::HaasInchMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kHaasMetric:
            m_base = QSharedPointer<HaasWriter>(new HaasWriter(GcodeMetaList::HaasMetricMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kHaasMetricNoComments:
            m_base = QSharedPointer<HaasMetricNoCommentsWriter>(
                new HaasMetricNoCommentsWriter(GcodeMetaList::HaasMetricMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kHurco:
            m_base = QSharedPointer<HurcoWriter>(new HurcoWriter(GcodeMetaList::HurcoMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kIngersoll:
            m_base =
                QSharedPointer<IngersollWriter>(new IngersollWriter(GcodeMetaList::IngersollMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kKraussMaffei:
            m_base = QSharedPointer<KraussMaffeiWriter>(
                new KraussMaffeiWriter(GcodeMetaList::KraussMaffeiMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kMarlin:
            m_base = QSharedPointer<MarlinWriter>(new MarlinWriter(GcodeMetaList::MarlinMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kJuggerBot:
            m_base = QSharedPointer<JuggerBotWriter>(new JuggerBotWriter(GcodeMetaList::MarlinMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kMazak:
            m_base = QSharedPointer<MazakWriter>(new MazakWriter(GcodeMetaList::MazakMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kMeld:
            m_base = QSharedPointer<MeldWriter>(new MeldWriter(GcodeMetaList::MeldMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kMeltio:
            m_base = QSharedPointer<MeltioWriter>(new MeltioWriter(GcodeMetaList::MeltioMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kMVP:
            m_base = QSharedPointer<MVPWriter>(new MVPWriter(GcodeMetaList::MVPMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kOkuma:
            m_base = QSharedPointer<OkumaWriter>(new OkumaWriter(GcodeMetaList::HaasMetricMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kORNL:
            m_base = QSharedPointer<ORNLWriter>(new ORNLWriter(GcodeMetaList::ORNLMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kRomiFanuc:
            m_base =
                QSharedPointer<RomiFanucWriter>(new RomiFanucWriter(GcodeMetaList::RomiFanucMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kSandia:
            m_base = QSharedPointer<SandiaWriter>(new SandiaWriter(GcodeMetaList::SandiaMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kSiemens:
            m_base = QSharedPointer<SiemensWriter>(new SiemensWriter(GcodeMetaList::SiemensMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kThermwood:
            m_base =
                QSharedPointer<ThermwoodWriter>(new ThermwoodWriter(GcodeMetaList::CincinnatiMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kTormach:
            m_base = QSharedPointer<TormachWriter>(new TormachWriter(GcodeMetaList::TormachMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kWolf:
            m_base = QSharedPointer<WolfWriter>(new WolfWriter(GcodeMetaList::WolfMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kRepRap:
            m_base = QSharedPointer<RepRapWriter>(new RepRapWriter(GcodeMetaList::RepRapMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kMach4:
            m_base = QSharedPointer<Mach4Writer>(new Mach4Writer(GcodeMetaList::MarlinMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kAeroBasic:
            m_base =
                QSharedPointer<AeroBasicWriter>(new AeroBasicWriter(GcodeMetaList::AeroBasicMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kAdamantine:
            m_base =
                QSharedPointer<AdamantineWriter>(new AdamantineWriter(GcodeMetaList::AdamantineMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kORNLMetric:
            m_base = QSharedPointer<ORNLWriter>(new ORNLWriter(GcodeMetaList::ORNLMetricMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kArcSpecialties:
            m_base = QSharedPointer<ArcSpecialtiesWriter>(
                new ArcSpecialtiesWriter(GcodeMetaList::ArcSpecialtiesMeta, GSM->getGlobal()));
            break;
        default:
            m_base =
                QSharedPointer<CincinnatiWriter>(new CincinnatiWriter(GcodeMetaList::CincinnatiMeta, GSM->getGlobal()));
    }

    if (!m_skip_gcode) {
        m_temp_gcode_output_file.setFileName(output);
        m_temp_gcode_output_file.open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text);
    }

    QFileInfo fi(output);
    m_temp_gcode_dir = fi.absoluteDir();
}

void AbstractSlicingThread::setCancel() { m_should_cancel = true; }

bool AbstractSlicingThread::shouldCancel() {
    if (m_should_cancel) {
        m_should_cancel = false;
        m_temp_gcode_output_file.close();
        return true;
    }

    return false;
}

void AbstractSlicingThread::setMaxSteps(int steps) { m_max_steps = steps; }

int AbstractSlicingThread::getMaxSteps() { return m_max_steps; }

void AbstractSlicingThread::forwardStatus(StatusUpdateStepType type, int completedPercentage) {
    emit statusUpdate(type, completedPercentage);
}

void AbstractSlicingThread::writeGCodeSetup() {
    QTextStream stream(&m_temp_gcode_output_file);

    float minimum_x(std::numeric_limits<float>::max()), minimum_y(std::numeric_limits<float>::max()),
        maximum_x(std::numeric_limits<float>::min()), maximum_y(std::numeric_limits<float>::min());

    for (QSharedPointer<Part> curr_part : CSM->parts()) {
        if (curr_part->rootMesh()->type() == MeshType::kClipping) // Skip parts that were used for clipping
            continue;

        minimum_x = std::min(minimum_x, curr_part->rootMesh()->min().x());
        minimum_y = std::min(minimum_y, curr_part->rootMesh()->min().y());
        maximum_x = std::max(maximum_x, curr_part->rootMesh()->max().x());
        maximum_y = std::max(maximum_y, curr_part->rootMesh()->max().y());
    }

    stream << m_base->writeSlicerHeader(toString(m_syntax));
    stream << m_base->writeSettingsHeader(m_syntax);
    stream << m_base->writeInitialSetup(Distance(minimum_x), Distance(minimum_y), Distance(maximum_x),
                                        Distance(maximum_y), m_max_steps);
}

void AbstractSlicingThread::writeGCodeShutdown() {
    {
        QTextStream stream(&m_temp_gcode_output_file);
        stream << m_base->writeShutdown();
        if (m_syntax != GcodeSyntax::kMVP)
            stream << m_base->writeSettingsFooter();
        stream.flush();
    }
    writeLayerTimeComments();
    m_temp_gcode_output_file.close();
}

void AbstractSlicingThread::writeLayerTimeComments() {
    if (!GSM->getGlobal()->setting<bool>(PRS::GCode::kLayerTimeComments)) {
        return;
    }

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
        QStringList upper_lines = text.toUpper().split('\n');

        std::unique_ptr<CommonParser> parser = makeLayerTimeParser(m_syntax, original_lines, upper_lines);
        parser->parseHeader();
        parser->parseFooter();
        parser->parseLines();

        const QList<Time> adjusted_layer_times = parser->getAdjustedLayerTimes();
        if (adjusted_layer_times.isEmpty()) {
            return;
        }

        const GcodeMeta meta = metaForSyntax(m_syntax);
        const QRegularExpression layer_marker("^\\s*" + QRegularExpression::escape(meta.m_comment_starting_delimiter) +
                                                  "\\s*BEGINNING\\s+LAYER\\s*:\\s*(\\d+)\\b",
                                              QRegularExpression::CaseInsensitiveOption);

        int layer_time_index = 1;
        QStringList annotated_lines = text.split('\n');
        for (int line_index = 0; line_index < annotated_lines.size() && layer_time_index < adjusted_layer_times.size();
             ++line_index) {
            const QRegularExpressionMatch match = layer_marker.match(annotated_lines[line_index]);
            if (!match.hasMatch()) {
                continue;
            }

            annotated_lines.insert(line_index + 1, layerTimeComment(meta, adjusted_layer_times[layer_time_index]));
            ++line_index;
            ++layer_time_index;
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
} // namespace ORNL
