#include "threading/slicers/rpbf_slicer.h"

#include <math.h>

#include <nlohmann/json_fwd.hpp>
#include <qcontainerfwd.h>
#include <qdebug.h>
#include <qhashfunctions.h>
#include <qmath.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>
#include <qvectornd.h>

#include "gcode/writers/writer_base.h"
#include "geometry/mesh/mesh_base.h"
#include "geometry/plane.h"
#include "geometry/polygon.h"
#include "geometry/polygon_list.h"
#include "geometry/polyline.h"
#include "geometry/settings_polygon.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "part/part.h"
#include "slicing/buffered_slicer.h"
#include "slicing/preprocessor.h"
#include "slicing/slicing_utilities.h"
#include "step/layer/island/powder_sector_island.h"
#include "step/layer/layer.h"
#include "step/layer/powder_layer.h"
#include "threading/traditional_ast.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {

RPBFSlicer::RPBFSlicer(QString gcodeLocation) : TraditionalAST(gcodeLocation) {}

void RPBFSlicer::preProcess(nlohmann::json opt_data) {
    Preprocessor pp;

    pp.addInitialProcessing(
        [this](const Preprocessor::Parts& parts, const QSharedPointer<SettingsBase>& global_settings) {
            // Alter settings
            global_settings->makeGlobalAdjustments();

            // Check for overlaps of settings parts and prevent them
            if (SlicingUtilities::doPartsOverlap(parts.settings_parts, Plane(Point(1, 1, 1), QVector3D(0, 0, 1))))
                return true; // Cancel Slicing

            return false; // No error, so continune slicing
        });

    pp.addMeshProcessing([this](QSharedPointer<MeshBase> mesh, QSharedPointer<SettingsBase> part_sb) {
        // Clip meshes
        auto clipping_meshes = SlicingUtilities::GetMeshesByType(CSM->parts(), MeshType::kClipping);
        SlicingUtilities::ClipMesh(mesh, clipping_meshes);

        return false; // No error, so continune slicing
    });

    pp.addStepBuilder([this](QSharedPointer<BufferedSlicer::SliceMeta> next_layer_meta,
                             Preprocessor::ActivePartMeta& meta) {
        int sector_count = next_layer_meta->settings->setting<int>(PS::Infill::kSectorCount);
        Distance bead_width = next_layer_meta->settings->setting<Distance>(PS::Perimeter::kBeadWidth);
        int rings = next_layer_meta->settings->setting<int>(PS::Perimeter::kCount);
        QVector<QVector<SectorInformation>> sector_info(sector_count);

        QVector<PolygonList> split_geometry = next_layer_meta->geometry.splitIntoParts();

        // if(next_layer_meta->settings->setting<bool>(Constants))
        //            Polygon radial_mask;
        //            if(true)
        //                radial_mask = GeometryDecomposition::RadialSplit(next_layer_meta->geometry).first();

        for (PolygonList polygon_list : split_geometry) {
            // QVector<PolygonList> offsets;
            QVector<Polyline> offsets;
            PolygonList path_line = polygon_list.offset(-bead_width / 2);

            int ring_nr = 0;
            while (!path_line.isEmpty() && ring_nr < rings) {
                for (Polygon poly : path_line)
                    offsets.push_back(poly.toPolyline());

                path_line = path_line.offset(-bead_width);
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

        if (next_layer_meta->number >= meta.steps_processed) // This is a new layer
        {
            QSharedPointer<PowderLayer> layer =
                QSharedPointer<PowderLayer>::create(next_layer_meta->number + 1, next_layer_meta->settings);
            layer->setSettingsPolygons(next_layer_meta->settings_polygons);

            // Add data from cross-sectioning to a layer
            layer->setGeometry(next_layer_meta->geometry, next_layer_meta->average_normal);
            layer->setOrientation(next_layer_meta->plane,
                                  next_layer_meta->shift_amount + next_layer_meta->additional_shift);

            QVector<QVector<QSharedPointer<IslandBase>>> island_order(sector_count);
            for (int i = 0; i < sector_count; ++i) {
                for (SectorInformation si : sector_info[i]) {
                    QSharedPointer<PowderSectorIsland> isl = QSharedPointer<PowderSectorIsland>::create(
                        si, next_layer_meta->settings, QVector<SettingsPolygon>());
                    island_order[i].push_back(isl);
                    layer->addIsland(IslandType::kPowderSector, isl);
                }
            }
            layer->setIslandOrder(island_order);

            // Add new step to part
            meta.part->appendStep(layer);
        }
        else // This layer is being updated on a re-slice
        {
            QSharedPointer<PowderLayer> layer =
                meta.part->step(next_layer_meta->number, StepType::kLayer).dynamicCast<PowderLayer>();
            layer->flagIfDirtySettings(next_layer_meta->settings);

            if (layer->isDirty()) {
                QSharedPointer<PowderLayer> new_layer =
                    QSharedPointer<PowderLayer>::create(next_layer_meta->number + 1, next_layer_meta->settings);
                // add data from cross-sectioning to a layer
                new_layer->setGeometry(next_layer_meta->geometry, QVector3D());
                new_layer->setOrientation(next_layer_meta->plane,
                                          next_layer_meta->shift_amount + next_layer_meta->additional_shift);

                QVector<QSharedPointer<IslandBase>> islands;
                QVector<QVector<QSharedPointer<IslandBase>>> island_order(sector_count);
                for (int i = 0; i < sector_count; ++i) {
                    for (SectorInformation si : sector_info[i]) {
                        QSharedPointer<PowderSectorIsland> isl = QSharedPointer<PowderSectorIsland>::create(
                            si, next_layer_meta->settings, QVector<SettingsPolygon>());
                        island_order[i].push_back(isl);
                        islands.push_back(isl);
                    }
                }
                new_layer->setIslandOrder(island_order);
                new_layer->updateIslands(IslandType::kPowderSector, islands);

                // Update step on part
                meta.part->replaceStep(next_layer_meta->number, new_layer);
            }
        }

        return false;
    });

    pp.addStatusUpdate([this](double percentage) { emit statusUpdate(StatusUpdateStepType::kPreProcess, percentage); });

    pp.processAll();
}

void RPBFSlicer::splitIntoSectors(QVector<Polyline> perimeters, QSharedPointer<SettingsBase> layer_specific_settings,
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

    //        Polygon outer_contour = infill_geometry[0];
    //        Point mass_center = outer_contour.centerOfMass();
    //        Distance longest_radius;
    //        for(Point pt : outer_contour)
    //        {
    //            Distance radius = pt.distance(mass_center);
    //            if(radius > longest_radius)
    //                longest_radius = radius;
    //        }

    //        Distance shortest_radius;
    //        Polygon inner_contour;
    //        for(int i = 1, end = infill_geometry.size(); i < end; ++i)
    //        {
    //            Polygon current_poly = infill_geometry[i];
    //            if(current_poly.inside(mass_center))
    //            {
    //                for(Point pt : current_poly)
    //                {
    //                    Distance radius = pt.distance(mass_center);
    //                    if(radius < shortest_radius)
    //                    {
    //                        shortest_radius = radius;
    //                        inner_contour = current_poly;
    //                    }
    //                }
    //            }
    //        }

    //        Polygon outer_circle;
    //        outer_circle.reserve(outer_contour.size());
    //        float step = limit / outer_contour.size();
    //        float angle = 0.0f;

    //        for (int i = 0; i < segments; ++i, angle += step)
    //        {
    //            float vertX = mCenterX + qCos(angle) * mRadius;
    //            float vertY = mCenterY + qSin(angle) * mRadius;
    //            outer_circle.push_back(Point(vertX, vertY));
    //        }

    // Distance mid_radius = longest_radius - shortest_radius;
    // shortest_radius.x(shortest_radius.x() + mid_radius());// + mid_radius();
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

            Polygon poly;
        }
        sector_info[i].push_back(si);
        polyClip = polyClip.rotateAround(center, -step);
    }
}

void RPBFSlicer::postProcess(nlohmann::json opt_data) {
    // NOP - no travels need added, other post-processing instead
    for (auto part : CSM->parts()) {
        for (QSharedPointer<Step> step : part->steps(StepType::kLayer)) {
            // Remove shift & rotation compensation for clean objects
            step.dynamicCast<Layer>()->unorient();

            // compensate for the shift & rotation at the end of cross sectioning for dirty
            step.dynamicCast<Layer>()->reorient();
        }
    }
    emit statusUpdate(StatusUpdateStepType::kPostProcess, 100);
}

void RPBFSlicer::writeGCode() {
    QTextStream stream(&m_temp_gcode_output_file);
    QSharedPointer<WriterBase> base = m_base;

    if (this->shouldCancel())
        return;

    // Count number of steps for status bar
    int total_steps = 0;
    for (auto part : CSM->parts())
        total_steps += part->countStepPairs();

    int layer_num = 0;
    for (auto part : CSM->parts()) {
        stream << base->writeBeforePart();
        for (QSharedPointer<Step> step : part->steps(StepType::kAll)) {
            step->setDirtyBit(false);
            QSharedPointer<PowderLayer> layer = step.dynamicCast<PowderLayer>();
            stream << base->writeLayerChange(layer_num);
            stream << base->writeBeforeLayer(layer_num, layer->getSb());
            stream << layer->writeGCode(base);
            stream << base->writeAfterLayer();

            ++layer_num;
            emit statusUpdate(StatusUpdateStepType::kGcodeGeneraton, (double)(layer_num) / (double)total_steps * 100);
        }
        stream << base->writeAfterPart();
    }
    stream << base->writeShutdown();
}
} // namespace ORNL
