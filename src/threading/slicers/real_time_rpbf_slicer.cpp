#include "threading/slicers/real_time_rpbf_slicer.h"

#include <math.h>

#include <QSharedPointer>
#include <nlohmann/json_fwd.hpp>
#include <qcontainerfwd.h>
#include <qhashfunctions.h>
#include <qmath.h>
#include <qvectornd.h>

#include "geometry/mesh/mesh_base.h"
#include "geometry/plane.h"
#include "geometry/polygon.h"
#include "geometry/polygon_list.h"
#include "geometry/polyline.h"
#include "geometry/settings_polygon.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "optimizers/layer_order_optimizer.h"
#include "slicing/buffered_slicer.h"
#include "slicing/preprocessor.h"
#include "slicing/slicing_utilities.h"
#include "step/layer/island/powder_sector_island.h"
#include "step/layer/layer.h"
#include "step/layer/powder_layer.h"
#include "step/step.h"
#include "threading/real_time_ast.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {

RealTimeRPBFSlicer::RealTimeRPBFSlicer(QString gcodeLocation) : RealTimeAST(gcodeLocation) {}

void RealTimeRPBFSlicer::initialSetup() {
    // Build a preprocessor
    m_preprocessor = QSharedPointer<Preprocessor>::create();

    // Add alter global settings, check for part overlap, segment root
    m_preprocessor->addInitialProcessing(
        [this](const Preprocessor::Parts& parts, const QSharedPointer<SettingsBase>& global_settings) {
            // Alter settings
            global_settings->makeGlobalAdjustments();

            // Check for overlaps of settings parts and prevent them
            if (SlicingUtilities::doPartsOverlap(parts.settings_parts, Plane(Point(1, 1, 1), QVector3D(0, 0, 1))))
                return true; // Cancel Slicing

            return false; // No error, so continune slicing
        });

    // Add mesh clipping
    m_preprocessor->addMeshProcessing([this](QSharedPointer<MeshBase> mesh, QSharedPointer<SettingsBase> part_sb) {
        // Clip meshes
        auto clipping_meshes = SlicingUtilities::GetMeshesByType(CSM->parts(), MeshType::kClipping);
        SlicingUtilities::ClipMesh(mesh, clipping_meshes);

        return false; // No error, so continune slicing
    });

    // Setup how to build steps
    m_preprocessor->addStepBuilder([this](QSharedPointer<BufferedSlicer::SliceMeta> next_layer_meta,
                                          Preprocessor::ActivePartMeta& meta) {
        meta.part->clearSteps(); // Remove and old steps

        int sectorCount = next_layer_meta->settings->setting<int>(PS::Infill::kSectorCount);
        Distance beadWidth = next_layer_meta->settings->setting<Distance>(PS::Perimeter::kBeadWidth);
        int rings = next_layer_meta->settings->setting<int>(PS::Perimeter::kCount);
        QVector<QVector<SectorInformation>> sector_info(sectorCount);

        QVector<PolygonList> split_geometry = next_layer_meta->geometry.splitIntoParts();
        for (PolygonList polyList : split_geometry) {
            // QVector<PolygonList> offsets;
            QVector<Polyline> offsets;
            PolygonList path_line = polyList.offset(-beadWidth / 2);

            int ring_nr = 0;
            while (!path_line.isEmpty() && ring_nr < rings) {
                for (Polygon poly : path_line)
                    offsets.push_back(poly.toPolyline());

                path_line = path_line.offset(-beadWidth);
                ring_nr++;
            }

            PolygonList infill_geometry;
            if (next_layer_meta->settings->setting<bool>(PS::Infill::kEnable)) {
                if (!offsets.isEmpty()) {
                    infill_geometry = path_line;
                    Distance default_overlap = next_layer_meta->settings->setting<Distance>(PS::Infill::kOverlap);
                    infill_geometry = infill_geometry.offset(default_overlap);
                }
                else
                    infill_geometry = next_layer_meta->geometry;
            }

            splitIntoSectors(offsets, next_layer_meta->settings, infill_geometry, sector_info, next_layer_meta->number);
        }

        QSharedPointer<PowderLayer> layer =
            QSharedPointer<PowderLayer>::create(next_layer_meta->number + 1, next_layer_meta->settings);
        layer->setSettingsPolygons(next_layer_meta->settings_polygons);

        // Add data from cross-sectioning to a layer
        layer->setGeometry(next_layer_meta->geometry, next_layer_meta->average_normal);
        layer->setOrientation(
            next_layer_meta->plane,
            next_layer_meta->shift_amount +
                next_layer_meta->additional_shift); // Offset this layer based on how many raftswe have added, rafts are
                                                    // always perpendicular to the build plate

        QVector<QVector<QSharedPointer<IslandBase>>> island_order(sectorCount);
        for (int i = 0; i < sectorCount; ++i) {
            for (SectorInformation si : sector_info[i]) {
                QSharedPointer<PowderSectorIsland> isl = QSharedPointer<PowderSectorIsland>::create(
                    si, next_layer_meta->settings, QVector<SettingsPolygon>());
                island_order[i].push_back(isl);
                layer->addIsland(IslandType::kPowderSector, isl);
            }
        }
        layer->setIslandOrder(island_order);
        meta.part->appendStep(layer);

        return false; // No need to stop slicing
    });

    m_preprocessor->addCrossSectionProcessing([this](Preprocessor::ActivePartMeta& meta) { return false; });

    m_preprocessor->addFinalProcessing(
        [this](const Preprocessor::Parts& parts, const QSharedPointer<SettingsBase>& global_settings) {
            m_layer_optimizer = QSharedPointer<LayerOrderOptimizer>::create();

            m_layer_optimizer->populateStep(parts.build_parts);

            return false; // No need to hault slicing
        });

    // Run inital processing
    m_preprocessor->processInital();
}

void RealTimeRPBFSlicer::preProcess(nlohmann::json opt_data) {
    m_cross_section_generated = m_preprocessor->processNext();
}

void RealTimeRPBFSlicer::postProcess(nlohmann::json opt_data) {
    for (auto part : CSM->parts()) {
        for (QSharedPointer<Step> step : part->steps(StepType::kLayer)) {
            // Remove shift & rotation compensation for clean objects
            step.dynamicCast<Layer>()->unorient();

            // compensate for the shift & rotation at the end of cross sectioning for dirty
            step.dynamicCast<Layer>()->reorient();
        }
    }
}

void RealTimeRPBFSlicer::writeGCode() {
    int layer_count = 0;
    for (auto part : CSM->parts()) {
        for (QSharedPointer<Step> step : part->steps(StepType::kLayer)) {
            if (this->shouldCancel())
                return;

            m_gcode_output += m_base->writeLayerChange(layer_count);

            m_gcode_output += m_base->writeBeforeLayer(step->getMinZ(), step->getSb());
            m_gcode_output += step->writeGCode(m_base);
            step->setDirtyBit(false);

            m_gcode_output += m_base->writeAfterLayer();

            ++layer_count;
        }
    }
}

void RealTimeRPBFSlicer::skip(int num_layers) {
    for (int i = 0, end = num_layers - 1; i < end; ++i)
        preProcess();
}

void RealTimeRPBFSlicer::splitIntoSectors(QVector<Polyline> perimeters,
                                          QSharedPointer<SettingsBase> layer_specific_settings,
                                          PolygonList infill_geometry, QVector<QVector<SectorInformation>>& sector_info,
                                          int layer_count) {
    //! Create a non-path border for the infill pattern
    //! Typical use for RPBF, 0 = y axis so must start at 90 degrees
    int sectors = layer_specific_settings->setting<int>(PS::Infill::kSectorCount);
    double step = (layer_specific_settings->setting<bool>(ES::RPBFSlicing::kSectorOffsettingEnable))
                      ? layer_specific_settings->setting<Angle>(ES::RPBFSlicing::kSectorSize)()
                      : (2.0 * M_PI) / sectors;
    double sectorOverlap = layer_specific_settings->setting<double>(ES::RPBFSlicing::kSectorOverlap);
    Point center = Point(layer_specific_settings->setting<double>(PRS::Dimensions::kXOffset),
                         layer_specific_settings->setting<double>(PRS::Dimensions::kYOffset));
    Distance radius = layer_specific_settings->setting<Distance>(PRS::Dimensions::kOuterRadius) + 10;
    double angleAdjust = 0;
    if (layer_specific_settings->setting<bool>(ES::RPBFSlicing::kSectorStaggerEnable))
        angleAdjust = layer_specific_settings->setting<double>(ES::RPBFSlicing::kSectorStaggerAngle);

    Polygon polyClip;
    polyClip.append(center);
    polyClip.append(Point(center.x() + radius * qSin(sectorOverlap), center.y() + radius * qCos(sectorOverlap)));
    polyClip.append(
        Point(center.x() + radius * qSin(-step - sectorOverlap), center.y() + radius * qCos(-step - sectorOverlap)));

    if (layer_specific_settings->setting<bool>(ES::RPBFSlicing::kSectorStaggerEnable))
        polyClip = polyClip.rotateAround(center,
                                         layer_specific_settings->setting<Angle>(ES::RPBFSlicing::kSectorStaggerAngle));

    double largeAngle = (step * sectors) * layer_count;
    double limit = 2.0 * M_PI;
    while (largeAngle > limit)
        largeAngle -= limit;

    if (layer_specific_settings->setting<bool>(ES::RPBFSlicing::kSectorOffsettingEnable))
        polyClip = polyClip.rotateAround(center, (2.0 * M_PI) - largeAngle);

    for (int i = 0; i < sectors; ++i) {
        // Calculate perimeters
        QVector<Polyline> allLines;
        for (Polyline line : perimeters) {
            allLines += (polyClip & line);
        }
        SectorInformation si;
        si.perimeters = allLines;
        si.start_vector = polyClip[1];

        if (layer_specific_settings->setting<bool>(PS::Infill::kEnable)) {
            si.start_angle = step * i;
            si.infill = polyClip & infill_geometry;
        }
        sector_info[i].push_back(si);
        polyClip = polyClip.rotateAround(center, -step);
    }
}
} // namespace ORNL
