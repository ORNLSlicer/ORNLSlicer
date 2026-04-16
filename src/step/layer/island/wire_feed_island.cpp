#include "step/layer/island/wire_feed_island.h"

#include <qcontainerfwd.h>
#include <qsharedpointer.h>

#include "geometry/path.h"
#include "geometry/path_modifier.h"
#include "geometry/point.h"
#include "geometry/polygon_list.h"
#include "geometry/polyline.h"
#include "geometry/segment_base.h"
#include "geometry/segments/travel.h"
#include "geometry/settings_polygon.h"
#include "managers/settings/settings_manager.h"
#include "step/layer/island/island_base.h"
#include "step/layer/regions/region_base.h"
#include "step/layer/regions/skeleton.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
WireFeedIsland::WireFeedIsland(const PolygonList& geometry, const QSharedPointer<SettingsBase>& sb,
                               const QVector<SettingsPolygon>& settings_polygons)
    : IslandBase(geometry, sb, settings_polygons) {
    this->addRegion(QSharedPointer<Skeleton>::create(sb, 0, settings_polygons, true));
    m_island_type = IslandType::kWireFeed;
}

void WireFeedIsland::optimize(int layerNumber, Point& currentLocation,
                              QVector<QSharedPointer<RegionBase>>& previousRegions) {
    previousRegions.push_back(getRegion(RegionType::kSkeleton));
    QSharedPointer<Skeleton> wire_feed_region = getRegion(RegionType::kSkeleton).dynamicCast<Skeleton>();
    bool empty;
    QVector<Path> empty_path;

    wire_feed_region->optimize(layerNumber, currentLocation, empty_path, empty_path, empty);
    QVector<Path>& paths = wire_feed_region->getPaths();

    for (Path& path : paths) {
        PathModifierGenerator::GenerateInitialStartup(
            path, m_sb->setting<Distance>(ES::WireFeed::kWirePrestartDistance),
            m_sb->setting<Velocity>(PS::Skeleton::kSpeed), AngularVelocity(), false, 0.0);

        PathModifierGenerator::GenerateSlowdown(
            path, Distance(), Distance(), m_sb->setting<Distance>(ES::WireFeed::kWireCutoffDistance),
            m_sb->setting<Velocity>(PS::Skeleton::kSpeed), m_sb->setting<AngularVelocity>(PS::Skeleton::kExtruderSpeed),
            false, 0.0);

        QSharedPointer<SegmentBase> nextToLast = path.getSegments()[path.getSegments().size() - 1];
        nextToLast->getSb()->setSetting(SS::kFinalWireCoast, true);

        for (int i = path.getSegments().size() - 1; i >= 0; --i) {
            QSharedPointer<SegmentBase> seg = path.getSegments()[i];
            //                qDebug() <<
            //                seg->getSb()->setting<PathModifiers>(SS::kPathModifiers);
            if (seg->getSb()->setting<PathModifiers>(SS::kPathModifiers) != PathModifiers::kCoasting) {
                seg->getSb()->setSetting(SS::kFinalWireFeed, true);
                break;
            }
        }

        for (int i = 0, end = path.getSegments().size(); i < end; ++i) {
            QSharedPointer<SegmentBase> seg = path.getSegments()[i];
            if (dynamic_cast<TravelSegment*>(seg.data())) {
                seg->getSb()->setSetting(SS::kWireFeed, m_sb->setting<Distance>(ES::WireFeed::kWireStickoutDistance));

                seg->getSb()->setSetting(SS::kSpeed, m_sb->setting<Velocity>(ES::WireFeed::kInitialTravelSpeed));
            }
            else
                seg->getSb()->setSetting(SS::kWireFeed, seg->start().distance(seg->end()));

            //                if(seg->getSb()->contains(SS::kPathModifiers))
            //                {
            //                    PathModifiers mods =
            //                    seg->getSb()->setting<PathModifiers>(SS::kPathModifiers);
            //                    if(seg->getSb()->setting<PathModifiers>(SS::kPathModifiers) ==
            //                    PathModifiers::kInitialStartup)
            //                    {
            //                        //seg->getSb()->remove(SS::kPathModifiers);
            //                        seg->getSb()->setSetting(SS::kWireFeed,
            //                        seg->start().distance(seg->end()));

            //                    }

            //                    if(seg->getSb()->setting<PathModifiers>(SS::kPathModifiers) ==
            //                    PathModifiers::kCoasting)
            //                    {
            //                        //seg->getSb()->remove(SS::kPathModifiers);
            //                        seg->getSb()->setSetting(SS::kWireFeed,
            //                        seg->start().distance(seg->end()));

            //                        //seg->getSb()->setSetting(SS::kFinalWireFeed, true);
            //                        //path.getSegments()[i-1]->getSb()->setSetting(SS::kFinalWireFeed,
            //                        true);
            //                    }
            //                }
        }
    }

    //        travel_segment->getSb()->setSetting(SS::kWireFeed, true);
    //      paths.last().back()->getSb()->setSetting(SS::kFinalWireFeed, true);
}

void WireFeedIsland::setAnchorWireFeed(QVector<Polyline> anchor_lines) {
    QSharedPointer<Skeleton> wire_feed_region = getRegion(RegionType::kSkeleton).dynamicCast<Skeleton>();
    wire_feed_region->setAnchorWireFeed(anchor_lines);
}

} // namespace ORNL
