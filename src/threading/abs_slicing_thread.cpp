#include "threading/abs_slicing_thread.h"

#include "QApplication"
#include "gcode/gcode_meta.h"
#include "gcode/writers/adamantine_writer.h"
#include "gcode/writers/aerobasic_writer.h"
#include "gcode/writers/aml3d_writer.h"
#include "gcode/writers/cincinnati_writer.h"
#include "gcode/writers/dmg_dmu_writer.h"
#include "gcode/writers/five_axis_marlin_writer.h"
#include "gcode/writers/gkn_writer.h"
#include "gcode/writers/gudel_writer.h"
#include "gcode/writers/haas_metric_no_comments_writer.h"
#include "gcode/writers/haas_writer.h"
#include "gcode/writers/hurco_writer.h"
#include "gcode/writers/ingersoll_writer.h"
#include "gcode/writers/kraussmaffei_writer.h"
#include "gcode/writers/mach4_writer.h"
#include "gcode/writers/marlin_pellet_writer.h"
#include "gcode/writers/marlin_writer.h"
#include "gcode/writers/mazak_writer.h"
#include "gcode/writers/meld_writer.h"
#include "gcode/writers/meltio_writer.h"
#include "gcode/writers/mvp_writer.h"
#include "gcode/writers/okuma_writer.h"
#include "gcode/writers/ornl_writer.h"
#include "gcode/writers/reprap_writer.h"
#include "gcode/writers/romi_fanuc_writer.h"
#include "gcode/writers/rpbf_writer.h"
#include "gcode/writers/sandia_writer.h"
#include "gcode/writers/siemens_writer.h"
#include "gcode/writers/skybaam_writer.h"
#include "gcode/writers/thermwood_writer.h"
#include "gcode/writers/tormach_writer.h"
#include "managers/session_manager.h"

namespace ORNL {
AbstractSlicingThread::AbstractSlicingThread(QString outputLocation, bool skipGcode)
    : QObject(), m_min(0), m_max(INT_MAX), m_should_cancel(false), m_should_communicate(false) {
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
        case GcodeSyntax::k5AxisMarlin:
            m_base = QSharedPointer<FiveAxisMarlinWriter>(
                new FiveAxisMarlinWriter(GcodeMetaList::MarlinMeta, GSM->getGlobal()));
            break;
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
        case GcodeSyntax::kGKN:
            m_base = QSharedPointer<GKNWriter>(new GKNWriter(GcodeMetaList::GKNMeta, GSM->getGlobal()));
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
        case GcodeSyntax::kMarlinPellet:
            m_base =
                QSharedPointer<MarlinPelletWriter>(new MarlinPelletWriter(GcodeMetaList::MarlinMeta, GSM->getGlobal()));
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
        case GcodeSyntax::kRPBF:
            m_base = QSharedPointer<RPBFWriter>(new RPBFWriter(GcodeMetaList::RPBFMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kSandia:
            m_base = QSharedPointer<SandiaWriter>(new SandiaWriter(GcodeMetaList::SandiaMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kSiemens:
            m_base = QSharedPointer<SiemensWriter>(new SiemensWriter(GcodeMetaList::SiemensMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kSkyBaam:
            m_base = QSharedPointer<SkyBaamWriter>(new SkyBaamWriter(GcodeMetaList::SkyBaamMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kThermwood:
            m_base =
                QSharedPointer<ThermwoodWriter>(new ThermwoodWriter(GcodeMetaList::CincinnatiMeta, GSM->getGlobal()));
            break;
        case GcodeSyntax::kTormach:
            m_base = QSharedPointer<TormachWriter>(new TormachWriter(GcodeMetaList::TormachMeta, GSM->getGlobal()));
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
            m_base =
                QSharedPointer<ORNLWriter>(new ORNLWriter(GcodeMetaList::ORNLMetricMeta, GSM->getGlobal()));
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

void AbstractSlicingThread::setCommunicate(bool communicate) { m_should_communicate = communicate; }

bool AbstractSlicingThread::shouldCommunicate() { return m_should_communicate; }

void AbstractSlicingThread::setNetworkData(StatusUpdateStepType stage, QString data) {
    if (stage == StatusUpdateStepType::kGcodeGeneraton) {
        m_temp_gcode_output_file.open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text);
        QTextStream stream(&m_temp_gcode_output_file);
        stream << data;
        m_temp_gcode_output_file.close();
        emit sliceComplete();
    }
}

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
    QTextStream stream(&m_temp_gcode_output_file);
    stream << m_base->writeShutdown();
    if (m_syntax != GcodeSyntax::kMVP)
        stream << m_base->writeSettingsFooter();
    m_temp_gcode_output_file.close();
}
} // namespace ORNL
