#pragma once

#include <QImage>
#include <QSharedPointer>
#include <QString>
#include <QTemporaryDir>
#include <QVector>

#include "geometry/polygon_list.h"

namespace ORNL {
enum class DlpOutputMode { kStatic = 0, kScrolling = 1 };

//! Physical dimensions use the same internal distance units as mesh coordinates.
struct DlpSettings {
    double x_min = 0, x_max = 0, y_min = 0, y_max = 0, z_min = 0, z_max = 0;
    double layer_height       = 0;
    double projection_width   = 0;
    int lane_pixel_width      = 0;
    int bit_depth             = 1;
    DlpOutputMode output_mode = DlpOutputMode::kStatic;
};

struct DlpLane {
    int number = 0;
    QString image_path;
    bool empty = true;
};

struct DlpLayer {
    int number = 0;
    double z   = 0;
    QString image_path;
    bool empty = true;
    QVector<DlpLane> lanes;
};

//! A completed build owns its temporary images independently of current settings.
struct DlpBuild {
    DlpSettings settings;
    int image_width       = 0;
    int image_height      = 0;
    int lane_count        = 0;
    int lane_image_height = 0;
    double pixel_pitch    = 0;
    //! X origins in full-layer pixel coordinates; the final footprint can overlap its predecessor.
    QVector<int> lane_starts;
    QVector<DlpLayer> layers;
    QSharedPointer<QTemporaryDir> storage;
};

class DlpRasterizer {
   public:
    //! Validates physical dimensions and initializes an empty build's raster layout.
    static bool initialize(DlpBuild& build, const DlpSettings& settings, QString* error = nullptr);

    //! Fills each mesh using even-odd contours, then unions the mesh masks.
    //! Image axes are +X right and +Y up; pixels are sampled at their centers.
    static QImage rasterize(const QVector<PolygonList>& sections, const DlpBuild& build);

    //! Stores full and fixed-width lane BMPs, including empty images and final-lane padding.
    static bool writeLayer(DlpBuild& build, int number, double z, const QImage& image, QString* error = nullptr);
};

class DlpExporter {
   public:
    static bool exportBuild(const DlpBuild& build, const QString& destination, QString* error = nullptr);
};
}  // namespace ORNL
