#pragma once

#include <vector>

#include <QSharedPointer>
#include <QString>
#include <QVector3D>
#include <QVector>

#include "geometry/segment_base.h"
#include "units/unit.h"

namespace ORNL {
//! \brief Generates and saves an STL representation of deposited G-code beads.
class AsPrintedModelExporter final {
  public:
    enum class StlFormat { kBinary, kAscii };
    enum class OriginMode { kPreserve, kLocalOrigin };
    enum class GeometryMode { kTrueBeadWidths, kCenterlines };

    struct Options {
        Options(bool include_support = false, bool include_travel = false, bool blend_corners = true,
                StlFormat format = StlFormat::kBinary, Distance output_unit = mm,
                OriginMode origin_mode = OriginMode::kLocalOrigin,
                GeometryMode geometry_mode = GeometryMode::kTrueBeadWidths, Distance centerline_diameter = 0.1 * mm,
                bool external_only = false)
            : include_support(include_support), include_travel(include_travel), blend_corners(blend_corners),
              format(format), output_unit(output_unit), origin_mode(origin_mode), geometry_mode(geometry_mode),
              centerline_diameter(centerline_diameter), external_only(external_only) {}

        bool include_support;
        bool include_travel;
        bool blend_corners;
        StlFormat format;
        Distance output_unit;
        OriginMode origin_mode;
        GeometryMode geometry_mode;
        Distance centerline_diameter;
        bool external_only;
    };

    struct Triangle {
        QVector3D a;
        QVector3D b;
        QVector3D c;
    };

    AsPrintedModelExporter() = delete;

    //! \brief Expands parsed G-code segments into bead mesh triangles in the requested output unit.
    static std::vector<Triangle> generateTriangles(const QVector<QVector<QSharedPointer<SegmentBase>>>& gcode,
                                                   Options options = Options());

    //! \brief Writes parsed G-code bead geometry as an STL in the requested output unit.
    static bool writeStl(const QString& path, const QVector<QVector<QSharedPointer<SegmentBase>>>& gcode,
                         QString* error_message = nullptr, Options options = Options());

  private:
    static bool shouldExportSegment(const QSharedPointer<SegmentBase>& segment, const Options& options);
};
} // namespace ORNL
