#include "geometry/path.h"

#include <cassert>
#include <limits>

#include <qcontainerfwd.h>
#include <qlist.h>
#include <qquaternion.h>
#include <qsharedpointer.h>

#include "geometry/polyline.h"
#include "geometry/segment_base.h"
#include "geometry/segments/travel.h"
#include "managers/settings/settings_manager.h"
#include "units/unit.h"
#include "utilities/constants.h"

namespace ORNL {
void Path::add(const QSharedPointer<SegmentBase>& ps) { m_segments.append(ps); }

void Path::append(const QSharedPointer<SegmentBase>& ps) { m_segments.append(ps); }

void Path::append(Path path) {
    for (QSharedPointer<SegmentBase> seg : path.getSegments())
        m_segments.append(seg);
}

void Path::prepend(const QSharedPointer<SegmentBase>& ps) { m_segments.prepend(ps); }

void Path::insert(int index, const QSharedPointer<SegmentBase>& ps) { m_segments.insert(index, ps); }

void Path::remove(const QSharedPointer<SegmentBase>& ps) { m_segments.removeOne(ps); }

void Path::removeAt(int index) { m_segments.removeAt(index); }

void Path::reverseSegments() {
    QList<QSharedPointer<SegmentBase>> newSegments;
    newSegments.reserve(m_segments.size());

    for (int i = m_segments.size() - 1; i >= 0; --i) {
        m_segments[i]->reverse();
        newSegments.push_back(m_segments[i]);
    }
    m_segments = newSegments;
}

Path Path::operator+=(const QSharedPointer<SegmentBase>& ps) {
    m_segments.append(ps);
    return *this;
}

QList<QSharedPointer<SegmentBase>>::iterator Path::begin() { return m_segments.begin(); }

QList<QSharedPointer<SegmentBase>>::iterator Path::end() { return m_segments.end(); }

QList<QSharedPointer<SegmentBase>>::const_iterator Path::begin() const { return m_segments.constBegin(); }

QList<QSharedPointer<SegmentBase>>::const_iterator Path::end() const { return m_segments.constEnd(); }

QSharedPointer<SegmentBase> Path::operator[](const int index) const { return m_segments[index]; }

QSharedPointer<SegmentBase> Path::at(const int index) const { return m_segments[index]; }

QSharedPointer<SegmentBase> Path::front() const { return m_segments.front(); }

QSharedPointer<SegmentBase> Path::back() const { return m_segments.back(); }

int Path::size() const { return m_segments.size(); }

void Path::move(int from, int to) { m_segments.move(from, to); }

void Path::clear() { m_segments.clear(); }

QList<QSharedPointer<SegmentBase>>& Path::getSegments() { return m_segments; }

Distance Path::calculateLength() {
    Distance totalLength;
    for (QSharedPointer<SegmentBase> segment : m_segments)
        totalLength += segment->length();
    return totalLength;
}

Distance Path::calculateLengthNoTravel() {
    Distance totalLength;
    for (QSharedPointer<SegmentBase> segment : m_segments) {
        if (!segment->isPrintingSegment())
            continue;

        totalLength += segment->length();
    }
    return totalLength;
}

Distance Path::calculatePrintingLength() {
    Distance totalLength;
    for (QSharedPointer<SegmentBase> segment : m_segments) {
        if (TravelSegment* ts = dynamic_cast<TravelSegment*>(segment.data()))
            continue;
        totalLength += segment->length();
    }
    return totalLength;
}

void Path::transform(QQuaternion rotation, Point shift) {
    // rotate and then shift every segment in the path
    for (QSharedPointer<SegmentBase> path_segment : m_segments) {
        path_segment->rotate(rotation);
        path_segment->shift(shift);
    }
}

float Path::getMinZ() {
    // find the minimum of the segments in this path
    float path_min = std::numeric_limits<float>::max();
    for (QSharedPointer<SegmentBase> segment : m_segments) {
        float segment_min = segment->getMinZ();
        if (segment_min < path_min)
            path_min = segment_min;
    }
    return path_min;
}

void Path::removeTravels() {
    for (int i = m_segments.size() - 1; i >= 0; --i) {
        if (TravelSegment* ts = dynamic_cast<TravelSegment*>(m_segments[i].data()))
            m_segments.removeAt(i);
    }
}

bool Path::isClosed() { return m_segments.first()->start() == m_segments.last()->end(); }

void Path::setCCW(bool ccw) { m_ccw = ccw; }

bool Path::getCCW() { return m_ccw; }

void Path::addNozzle(int nozzle) {
    for (QSharedPointer<SegmentBase> segment : getSegments()) {
        segment->addNozzle(nozzle);
    }
}

void Path::adjustMultiNozzle() {
    if (GSM->getGlobal()->setting<bool>(ES::MultiNozzle::kEnableMultiNozzleMultiMaterial)) {
        // get the nozzle materials and offsets from settings
        int num_nozzles = GSM->getGlobal()->setting<int>(ES::MultiNozzle::kNozzleCount);
        QVector<Point> nozzle_offsets;
        nozzle_offsets.reserve(num_nozzles);

        QVector<int> nozzle_materials;
        nozzle_materials.reserve(num_nozzles);

        for (int nozzle = 0; nozzle < num_nozzles; ++nozzle) {
            Distance x = GSM->getGlobal()->setting<Distance>(ES::MultiNozzle::kNozzleOffsetX, nozzle);
            Distance y = GSM->getGlobal()->setting<Distance>(ES::MultiNozzle::kNozzleOffsetY, nozzle);
            Distance z = GSM->getGlobal()->setting<Distance>(ES::MultiNozzle::kNozzleOffsetZ, nozzle);
            int material = GSM->getGlobal()->setting<int>(ES::MultiNozzle::kNozzleMaterial, nozzle);

            nozzle_offsets.push_back(Point(x(), y(), z()));
            nozzle_materials.push_back(material);
        }

        // adjust segments to use nozzle according to materials
        for (auto& current_segment : getSegments()) {
            int segment_material = current_segment->getSb()->setting<int>(SS::kMaterialNumber);

            // find a nozzle with the current segment's material
            // selects first nozzle if more than one has correct material
            int current_segment_nozzle = -1;
            for (int i = 0; i < num_nozzles; ++i) {
                if (segment_material == nozzle_materials[i]) {
                    current_segment_nozzle = i;
                    break;
                }
            }
            assert(current_segment_nozzle >= 0); // assume there is at least one nozzle with the necessary material

            // shift segment according to nozzle offset
            Point nozzle_offset = nozzle_offsets[current_segment_nozzle];
            current_segment->shift(Point(0, 0, 0) - nozzle_offset);

            // set segment's nozzle
            QVector<int> nozzles;
            nozzles.append(current_segment_nozzle);
            current_segment->setNozzles(nozzles);
        }
    }
}

} // namespace ORNL
