#include "geometry/pattern_generator.h"

#include <cmath>

#include <qcontainerfwd.h>

#include "geometry/point.h"
#include "geometry/polygon.h"
#include "geometry/polygon_list.h"
#include "geometry/polyline.h"
#include "units/unit.h"

namespace ORNL {

QVector<Polyline> PatternGenerator::GenerateLines(PolygonList geometry, Distance lineSpacing, Angle rotation,
                                                  bool globalBounds, Point min, Point max) {
    geometry = geometry.rotate(rotation);

    if (!globalBounds) {
        min = geometry.min();
        max = geometry.max();
    }
    else {
        min = min.rotate(rotation);
        max = max.rotate(rotation);
    }

    //! The result we get after intersecting the polygons with the grid lines
    QVector<Polyline> cutlines;

    //! The space left over after all the max number of cutlines are generated
    Distance freeSpace = (max.toDistance3D().x - min.toDistance3D().x) % lineSpacing;

    //! start at the bounding box's minimum x value and go all the way to the bounding box's maximum x value.
    //! As we go along, every "line_spacing" distance we intersect the polygons with the grid lines
    for (Distance x = min.toDistance3D().x + (freeSpace / 2); x < max.toDistance3D().x; x += lineSpacing) {
        //! Create the grid lines
        Polyline cutline;
        cutline << Point(x(), min.y());
        cutline << Point(x(), max.y());

        //! Intersect the polygons and the gridlines and store them
        //! \note This calls ClipperLib
        cutlines += geometry & cutline;
    }

    //! Unrotate polygons
    for (int i = 0; i < cutlines.size(); i++) {
        cutlines[i] = cutlines[i].rotate(-rotation);
        if (i % 2 == 0)
            cutlines[i] = cutlines[i].reverse();
    }

    return cutlines;
}

QVector<Polyline> PatternGenerator::GenerateGrid(PolygonList geometry, Distance lineSpacing, Angle rotation,
                                                 bool globalBounds, Point min, Point max) {
    QVector<Polyline> result;
    //! Call computeLine with our base rotation
    result.append(PatternGenerator::GenerateLines(geometry, lineSpacing, rotation, globalBounds, min, max));

    //! Call computeLine with our base rotation plus 90 deg
    result.append(PatternGenerator::GenerateLines(geometry, lineSpacing, rotation + 90 * deg, globalBounds, min, max));

    return result;
}

QVector<Polyline> PatternGenerator::GenerateConcentric(PolygonList& geometry, Distance beadWidth) {
    return GenerateConcentric(geometry, beadWidth, beadWidth);
}

QVector<Polyline> PatternGenerator::GenerateConcentric(PolygonList& geometry, Distance beadWidth,
                                                       Distance lineSpacing) {
    QVector<Polyline> cutlines;
    PolygonList path_line = geometry.offset(-beadWidth / 2);

    while (!path_line.isEmpty()) {
        for (Polygon polygon : path_line) {
            Polyline polyline;
            polyline.reserve(polygon.size());
            for (Point point : polygon) {
                polyline << point;
            }
            cutlines.append(polyline);
        }
        path_line = path_line.offset(-lineSpacing);
    }

    if (path_line.size() != 0)
        geometry = path_line.offset(-beadWidth / 2);

    return cutlines;
}

QVector<Polyline> PatternGenerator::GenerateTriangles(PolygonList geometry, Distance lineSpacing, Angle rotation,
                                                      bool globalBounds, Point min, Point max) {
    QVector<Polyline> result;
    //! \note line_spacing should be the distance from the base of a triangle to its peak in this function
    //! \note All the triangles are equilateral

    //! Computes each side of the triangles(3 in total), rotating 60 deg after each time.
    for (int i = 0; i < 3; ++i) {
        PolygonList rotated_geometry = geometry.rotate(rotation);

        if (!globalBounds) {
            min = rotated_geometry.min();
            max = rotated_geometry.max();
        }
        else {
            min = min.rotate(rotation);
            max = max.rotate(rotation);
        }

        QVector<Polyline> cutlines;

        //! The space left over after all the max number of cutlines are generated
        Distance freeSpace = (max.toDistance3D().x - min.toDistance3D().x) % lineSpacing;
        int cutCount = ceil(((max.toDistance3D().x - min.toDistance3D().x) / lineSpacing)());

        //! \note Always ensure that there is an odd number of lines so that there is always a line intersecting the
        //! center
        if (cutCount % 2 == 0) {
            --cutCount;
            freeSpace += lineSpacing;
        }
        if (cutCount < 0) {
            cutCount = 0;
        }

        //! start at the bounding box's minimum x value and go all the way to the bounding box's maximum x value.
        //! As we go along, every "line_spacing" distance we intersect the polygons with the grid lines
        //! offseting by half of freespace ensures that a line will always cut through the center
        for (Distance x = min.toDistance3D().x + (freeSpace / 2); x < max.toDistance3D().x; x += lineSpacing) {
            //! Create the grid lines
            Polyline cutline;
            cutline << Point(x(), min.y());
            cutline << Point(x(), max.y());

            //! Intersect the polygons and the gridlines and store them
            cutlines += rotated_geometry & cutline;
        }

        //! Unrotate polygons
        for (int i = 0; i < cutlines.size(); i++) {
            cutlines[i] = cutlines[i].rotate(-rotation);
            if (i % 2 == 0) {
                cutlines[i] = cutlines[i].reverse();
            }
        }

        result.append(cutlines);

        // 1.0472 rad = 60 degrees
        rotation += 1.0472f;
    }
    return result;
}

QVector<Polyline> PatternGenerator::GenerateHexagonsAndTriangles(PolygonList geometry, Distance lineSpacing,
                                                                 Angle rotation, bool globalBounds, Point min,
                                                                 Point max) {
    QVector<Polyline> result;

    for (int i = 0; i < 3; ++i) {
        PolygonList rotated_geometry = geometry.rotate(rotation);

        if (!globalBounds) {
            min = rotated_geometry.min();
            max = rotated_geometry.max();
        }
        else {
            min = min.rotate(rotation);
            max = max.rotate(rotation);
        }

        QVector<Polyline> cutlines;

        Distance freeSpace = (max.toDistance3D().x - min.toDistance3D().x) % lineSpacing;
        int cutCount = ceil(((max.toDistance3D().x - min.toDistance3D().x) / lineSpacing)());

        if (cutCount % 2 == 1) {
            --cutCount;
            freeSpace += lineSpacing;
        }
        if (cutCount < 0) {
            cutCount = 0;
        }

        for (Distance x = min.toDistance3D().x + (freeSpace / 2); x < max.toDistance3D().x; x += lineSpacing) {
            // create the grid lines
            Polyline cutline;
            cutline << Point(x(), min.y());
            cutline << Point(x(), max.y());

            cutlines += rotated_geometry & cutline;
        }

        for (int i = 0; i < cutlines.size(); i++) {
            cutlines[i] = cutlines[i].rotate(-rotation);
            if (i % 2 == 0) {
                cutlines[i] = cutlines[i].reverse();
            }
        }

        result.append(cutlines);

        // 1.0472 rad = 60 degrees;
        rotation += 1.0472f;
    }
    return result;
}

QVector<Polyline> PatternGenerator::GenerateHoneyComb(PolygonList geometry, Distance beadWidth, Distance lineSpacing,
                                                      Angle rotation, bool globalBounds, Point min, Point max) {
    geometry = geometry.rotate(rotation);

    if (!globalBounds) {
        min = geometry.min();
        max = geometry.max();
    }
    else {
        min = min.rotate(rotation);
        max = max.rotate(rotation);
    }

    //! \note r^2 = 3/4 * R^2 is used to find the radius of a circumscribed polygon from a supplied side length
    Distance verticalLineSpacing = sqrt(((lineSpacing * lineSpacing) * 0.75)) * 2;
    Distance horizontalLineSpacing = lineSpacing;

    QVector<Polyline> cutlines;

    //! Calculate how many cuts we need on each axis.
    //! \note A hexagon's width is two times its side length.
    //! \note A hexagon's height is 1/2 of its vertical spacing.
    int xCutCount = ceil((max.x() - min.x()) / (horizontalLineSpacing() * 2));
    int yCutCount = ceil((max.y() - min.y()) / (verticalLineSpacing() / 2 + beadWidth()));

    Point currentLocation = min;

    for (int yCount = 0; yCount < yCutCount; yCount++) {
        Polyline row;
        for (int xCount = 0; xCount < (xCutCount * 4); xCount++) {
            switch (xCount % 4) {
                case 0: {
                    if (xCount == 0)
                        row << currentLocation;
                    currentLocation = Point(currentLocation.x() + horizontalLineSpacing, currentLocation.y());
                    row << currentLocation;
                    break;
                }
                case 1: {
                    if (yCount % 2)
                        currentLocation = Point(currentLocation.x() + (horizontalLineSpacing / 2),
                                                currentLocation.y() - verticalLineSpacing / 2);
                    else
                        currentLocation = Point(currentLocation.x() + (horizontalLineSpacing / 2),
                                                currentLocation.y() + verticalLineSpacing / 2);
                    row << currentLocation;
                    break;
                }
                case 2: {
                    currentLocation = Point(currentLocation.x() + horizontalLineSpacing, currentLocation.y());
                    row << currentLocation;
                    break;
                }
                case 3: {
                    if (yCount % 2)
                        currentLocation = Point(currentLocation.x() + (horizontalLineSpacing / 2),
                                                currentLocation.y() + verticalLineSpacing / 2);
                    else
                        currentLocation = Point(currentLocation.x() + (horizontalLineSpacing / 2),
                                                currentLocation.y() - verticalLineSpacing / 2);
                    row << currentLocation;
                    break;
                }
            }
        }

        if (yCount % 2) {
            currentLocation = Point(min.x(), currentLocation.y() + beadWidth);
        }
        else {
            currentLocation = Point(min.x(), currentLocation.y() + verticalLineSpacing + beadWidth);
        }

        cutlines += geometry & row;
    }

    //! \note Rotate back our cutlines to match the rotation
    for (int i = 0; i < cutlines.size(); i++) {
        cutlines[i] = cutlines[i].rotate(-rotation);
        if (i % 2 == 0) {
            cutlines[i] = cutlines[i].reverse();
        }
    }

    return cutlines;
}

QVector<Polyline> PatternGenerator::GenerateRadialHatch(PolygonList geometry, Point center, Distance lineSpacing,
                                                        Angle sector_rotation, Angle infill_rotation) {
    PolygonList geometry_oriented = geometry.rotateAround(center, sector_rotation);
    geometry_oriented = geometry_oriented.rotate(infill_rotation);

    //! Get the bounding box for the polygons. outline_minimum is the minimum
    Point outline_minimum = geometry_oriented.min();
    Point outline_maximum = geometry_oriented.max();

    //! The result we get after intersecting the polygons with the grid lines
    QVector<Polyline> cutlines;

    //! The space left over after all the max number of cutlines are generated
    Distance freeSpace = (outline_maximum.toDistance3D().x - outline_minimum.toDistance3D().x) % lineSpacing;

    //! start at the bounding box's minimum x value and go all the way to the bounding box's maximum x value.
    //! As we go along, every "line_spacing" distance we intersect the polygons with the grid lines
    int alternate = 0;
    for (Distance x = outline_minimum.toDistance3D().x + (freeSpace / 2); x < outline_maximum.toDistance3D().x;
         x += lineSpacing) {
        //! Create the grid lines
        Polyline cutline;
        if (alternate % 2 == 0) {
            cutline << Point(x(), outline_minimum.y());
            cutline << Point(x(), outline_maximum.y());
        }
        else {
            cutline << Point(x(), outline_maximum.y());
            cutline << Point(x(), outline_minimum.y());
        }

        //! Intersect the polygons and the gridlines and store them
        //! \note This calls ClipperLib
        cutlines += geometry_oriented & cutline;

        ++alternate;
    }

    //! Unrotate polygons
    for (int j = 0; j < cutlines.size(); ++j) {
        cutlines[j] = cutlines[j].rotate(-(infill_rotation));
        cutlines[j] = cutlines[j].rotateAround(center, -sector_rotation);
    }
    return cutlines;
}
} // namespace ORNL
