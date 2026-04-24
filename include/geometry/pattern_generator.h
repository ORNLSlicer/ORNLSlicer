#pragma once

#include <qcontainerfwd.h>

#include "geometry/polygon_list.h"
#include "geometry/polyline.h"
#include "units/unit.h"

namespace ORNL {

/*!
 * \brief Static class that provides pattern generation capabilities.
 * Current patterns include:
 * Lines
 * Grid
 * Concentric
 * Triangles
 * Hexagons and Trianlges
 * HoneyComb
 * Radial Hatch
 */
class PatternGenerator {
  public:
    /*!
     * \brief Creates parallel lines as paths.
     * \param geometry: The bounds of the geometry under consideration
     * \param lineSpacing: is the distance between lines
     * \param rotation: Rotation of the pattern
     * \param globalBounds: bool to indicate whether to override min/max with supplied values.
     * Used to provide patterns depenedent on build volume instead of object.
     * \param min: Min value of global space to generate pattern from.  Used in conjuction with globalBounds.
     * \param max: Max value of global space to generate pattern from.  Used in conjuction with globalBounds.
     */
    static QVector<Polyline> GenerateLines(PolygonList geometry, Distance lineSpacing, Angle rotation,
                                           bool globalBounds = false, Point min = Point(), Point max = Point());
    /*!
     * \brief Creates grid lines as paths.
     * \param geometry: The bounds of the geometry under consideration
     * \param lineSpacing: is the distance between lines
     * \param rotation: Rotation of the pattern
     * \param globalBounds: bool to indicate whether to override min/max with supplied values.
     * Used to provide patterns depenedent on build volume instead of object.
     * \param min: Min value of global space to generate pattern from.  Used in conjuction with globalBounds.
     * \param max: Max value of global space to generate pattern from.  Used in conjuction with globalBounds.
     */
    static QVector<Polyline> GenerateGrid(PolygonList geometry, Distance lineSpacing, Angle rotation,
                                          bool globalBounds = false, Point min = Point(), Point max = Point());

    /*!
     * \brief Creates concentric lines as paths.
     * Version 1 is a convenience function that calls version 2 with lineSpacing = beadWidth
     * Most regions set lineSpacing = beadWidth.  Currently, only infill allows lineSpace != beadWidth.
     * \param geometry: The bounds of the geometry under consideration
     * \param beadWidth: Beadwidth of lines
     * \param lineSpacing: is the distance between lines
     */
    static QVector<Polyline> GenerateConcentric(PolygonList& geometry, Distance beadWidth);
    static QVector<Polyline> GenerateConcentric(PolygonList& geometry, Distance beadWidth, Distance lineSpacing);

    /*!
     * \brief Creates triangle lines as paths.
     * \param geometry: The bounds of the geometry under consideration
     * \param lineSpacing: is the distance between lines
     * \param rotation: Rotation of the pattern
     * \param globalBounds: bool to indicate whether to override min/max with supplied values.
     * Used to provide patterns depenedent on build volume instead of object.
     * \param min: Min value of global space to generate pattern from.  Used in conjuction with globalBounds.
     * \param max: Max value of global space to generate pattern from.  Used in conjuction with globalBounds.
     */
    static QVector<Polyline> GenerateTriangles(PolygonList geometry, Distance lineSpacing, Angle rotation,
                                               bool globalBounds = false, Point min = Point(), Point max = Point());

    /*!
     * \brief Creates hexagon and triangle lines as paths.
     * \param geometry: The bounds of the geometry under consideration
     * \param lineSpacing: is the distance between lines
     * \param rotation: Rotation of the pattern
     * \param globalBounds: bool to indicate whether to override min/max with supplied values.
     * Used to provide patterns depenedent on build volume instead of object.
     * \param min: Min value of global space to generate pattern from.  Used in conjuction with globalBounds.
     * \param max: Max value of global space to generate pattern from.  Used in conjuction with globalBounds.
     */
    static QVector<Polyline> GenerateHexagonsAndTriangles(PolygonList geometry, Distance lineSpacing, Angle rotation,
                                                          bool globalBounds = false, Point min = Point(),
                                                          Point max = Point());

    /*!
     * \brief Creates honeycomb lines as paths.
     * \param geometry: The bounds of the geometry under consideration
     * \param lineSpacing: is the distance between lines
     * \param rotation: Rotation of the pattern
     * \param globalBounds: bool to indicate whether to override min/max with supplied values.
     * Used to provide patterns depenedent on build volume instead of object.
     * \param min: Min value of global space to generate pattern from.  Used in conjuction with globalBounds.
     * \param max: Max value of global space to generate pattern from.  Used in conjuction with globalBounds.
     */
    static QVector<Polyline> GenerateHoneyComb(PolygonList geometry, Distance beadWidth, Distance lineSpacing,
                                               Angle rotation, bool globalBounds = false, Point min = Point(),
                                               Point max = Point());

    /*!
     * \brief Creates radial hatch lines as paths.
     * \param geometry: The bounds of the geometry under consideration
     * \param center: center of rotation
     * \param lineSpacing: the distance between lines
     * \param sector_rotation: rotation necessary to align geometry with y-axis (sector 1)
     * \param infill_rotation: infill rotation in addition to necessary sector rotation
     */
    static QVector<Polyline> GenerateRadialHatch(PolygonList geometry, Point center, Distance lineSpacing,
                                                 Angle sector_rotation, Angle infill_rotation);
};
} // namespace ORNL
