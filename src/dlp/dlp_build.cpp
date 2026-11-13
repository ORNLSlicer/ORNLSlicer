#include "dlp/dlp_build.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QSaveFile>
#include <QTextStream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include "units/unit.h"

namespace ORNL {
namespace {
bool fail(QString* error, const QString& message) {
    if (error) *error = message;
    return false;
}

double roundedCeil(double value) {
    const double nearest = std::round(value);
    if (nearest >= 1.0 &&
        std::abs(value - nearest) <= 16.0 * std::numeric_limits<double>::epsilon() * std::max(1.0, std::abs(value)))
        return nearest;
    return std::ceil(value);
}

bool imageEmpty(const QImage& image) {
    for (int y = 0; y < image.height(); ++y) {
        const uchar* row = image.constScanLine(y);
        for (int x = 0; x < image.width(); ++x) {
            if (row[x] != 0) return false;
        }
    }
    return true;
}

bool saveBmp(const QImage& image, const QString& path, int bit_depth, QString* error) {
    QImage output;
    if (bit_depth == 1) {
        output = image.convertToFormat(QImage::Format_Mono, {qRgb(0, 0, 0), qRgb(255, 255, 255)}, Qt::ThresholdDither);
    }
    else {
        QVector<QRgb> colors;
        colors.reserve(256);
        for (int i = 0; i < 256; ++i) colors.append(qRgb(i, i, i));
        output = image.convertToFormat(QImage::Format_Indexed8, colors, Qt::ThresholdDither);
    }
    if (output.isNull() || !output.save(path, "BMP")) return fail(error, "Could not write DLP image: " + path);
    return true;
}
}  // namespace

bool DlpRasterizer::initialize(DlpBuild& build, const DlpSettings& settings, QString* error) {
    const double values[] {settings.x_min, settings.x_max, settings.y_min,        settings.y_max,
                           settings.z_min, settings.z_max, settings.layer_height, settings.projection_width};
    for (double value : values) {
        if (!std::isfinite(value)) return fail(error, "DLP dimensions must be finite.");
    }
    if (settings.x_max <= settings.x_min || settings.y_max <= settings.y_min || settings.z_max <= settings.z_min)
        return fail(error, "DLP slicing requires positive rectangular printer dimensions.");
    if (settings.layer_height <= 0 || settings.projection_width <= 0 || settings.lane_pixel_width <= 0)
        return fail(error, "DLP layer height, projection width, and image pixel width must be greater than zero.");
    if (settings.bit_depth != 1 && settings.bit_depth != 8) return fail(error, "DLP bitmap depth must be 1 or 8 bits.");

    const double pitch  = settings.projection_width / settings.lane_pixel_width;
    const double width  = roundedCeil((settings.x_max - settings.x_min) / pitch);
    const double height = roundedCeil((settings.y_max - settings.y_min) / pitch);
    const double lanes  = roundedCeil((settings.x_max - settings.x_min) / settings.projection_width);
    if (!std::isfinite(pitch) || pitch <= 0 || width < 1 || height < 1 || lanes < 1 ||
        width > std::numeric_limits<int>::max() || height > std::numeric_limits<int>::max() ||
        lanes > std::numeric_limits<int>::max())
        return fail(error, "DLP image dimensions exceed the supported range.");

    // The preview can expand BMP pixels to 32 bits; honor its image reader's allocation limit before slicing.
    const int allocation_limit = QImageReader::allocationLimit();
    const double preview_bytes = std::max(width, double(settings.lane_pixel_width)) * height * 4.0;
    if (allocation_limit > 0 && preview_bytes > allocation_limit * 1024.0 * 1024.0)
        return fail(error,
                    QString("DLP images exceed the %1 MiB image allocation limit. Full layer: %2 x %3 pixels; "
                            "lane: %4 x %3 pixels. Reduce Projection Width in Pixels, increase Projection Width, "
                            "or reduce the printer XY dimensions.")
                        .arg(allocation_limit)
                        .arg(width, 0, 'f', 0)
                        .arg(height, 0, 'f', 0)
                        .arg(settings.lane_pixel_width));

    build              = DlpBuild();
    build.settings     = settings;
    build.pixel_pitch  = pitch;
    build.image_width  = static_cast<int>(width);
    build.image_height = static_cast<int>(height);
    build.lane_count   = static_cast<int>(lanes);
    build.storage      = QSharedPointer<QTemporaryDir>::create(QDir::tempPath() + "/ornlslicer-dlp-XXXXXX");
    if (!build.storage->isValid()) return fail(error, "Could not create temporary DLP output directory.");
    return true;
}

QImage DlpRasterizer::rasterize(const QVector<PolygonList>& sections, const DlpBuild& build) {
    QImage image(build.image_width, build.image_height, QImage::Format_Grayscale8);
    if (image.isNull()) return image;
    image.fill(0);
    const auto& settings = build.settings;
    QVector<double> intersections;
    for (int row = 0; row < image.height(); ++row) {
        const double y = settings.y_max - ((row + 0.5) * build.pixel_pitch);
        if (y < settings.y_min) continue;
        uchar* pixels = image.scanLine(row);
        for (const auto& section : sections) {
            intersections.clear();
            for (const auto& polygon : section) {
                if (polygon.size() < 3) continue;
                for (int i = 0; i < polygon.size(); ++i) {
                    const Point& a = polygon[i];
                    const Point& b = polygon[(i + 1) % polygon.size()];
                    if ((a.y() <= y && b.y() > y) || (b.y() <= y && a.y() > y))
                        intersections.append(a.x() + ((y - a.y()) * (b.x() - a.x()) / (b.y() - a.y())));
                }
            }
            std::sort(intersections.begin(), intersections.end());
            for (int i = 0; i + 1 < intersections.size(); i += 2) {
                const double left  = std::max(intersections[i], settings.x_min);
                const double right = std::min(intersections[i + 1], settings.x_max);
                if (left >= right) continue;
                const int first = static_cast<int>(std::clamp(
                    std::ceil((left - settings.x_min) / build.pixel_pitch - 0.5), 0.0, double(image.width())));
                const int end   = static_cast<int>(std::clamp(
                    std::ceil((right - settings.x_min) / build.pixel_pitch - 0.5), 0.0, double(image.width())));
                if (end > first) std::memset(pixels + first, 255, end - first);
            }
        }
    }
    return image;
}

bool DlpRasterizer::writeLayer(DlpBuild& build, int number, double z, const QImage& image, QString* error) {
    if (!build.storage || !build.storage->isValid() || image.isNull() || image.width() != build.image_width ||
        image.height() != build.image_height)
        return fail(error, "DLP layer image does not match the build layout.");
    const QImage mask = image.convertToFormat(QImage::Format_Grayscale8);
    QDir directory(build.storage->path());
    const QString folder = QString("Layer %1").arg(number);
    if (!directory.mkpath(folder)) return fail(error, "Could not create DLP layer directory.");
    directory.cd(folder);

    DlpLayer layer;
    layer.number     = number;
    layer.z          = z;
    layer.image_path = directory.absoluteFilePath(QString("Layer_%1_Full.bmp").arg(number));
    layer.empty      = imageEmpty(mask);
    if (!saveBmp(mask, layer.image_path, build.settings.bit_depth, error)) return false;
    for (int i = 0; i < build.lane_count; ++i) {
        QImage lane_image(build.settings.lane_pixel_width, build.image_height, QImage::Format_Grayscale8);
        if (lane_image.isNull()) return fail(error, "Could not allocate DLP lane image.");
        lane_image.fill(0);
        const qint64 start = qint64(i) * build.settings.lane_pixel_width;
        const int count    = static_cast<int>(std::min<qint64>(build.settings.lane_pixel_width, mask.width() - start));
        if (count > 0) {
            for (int y = 0; y < mask.height(); ++y)
                std::memcpy(lane_image.scanLine(y), mask.constScanLine(y) + start, count);
        }
        DlpLane lane;
        lane.number     = i + 1;
        lane.image_path = directory.absoluteFilePath(QString("Layer%1_%2.bmp").arg(number).arg(lane.number));
        lane.empty      = imageEmpty(lane_image);
        if (!saveBmp(lane_image, lane.image_path, build.settings.bit_depth, error)) return false;
        layer.lanes.append(lane);
    }
    build.layers.append(layer);
    return true;
}

bool DlpExporter::exportBuild(const DlpBuild& build, const QString& destination, QString* error) {
    if (!build.storage || !build.storage->isValid() || build.layers.isEmpty())
        return fail(error, "No completed DLP build is available to export.");
    QDir directory(destination);
    if (QFileInfo::exists(destination) &&
        (!QFileInfo(destination).isDir() ||
         !directory.entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System).isEmpty()))
        return fail(error, "Select a new or empty directory for the DLP export.");
    if (!directory.mkpath(".")) return fail(error, "Could not create DLP export directory: " + destination);
    QSaveFile csv(directory.absoluteFilePath("build.csv"));
    if (!csv.open(QIODevice::WriteOnly | QIODevice::Text)) return fail(error, "Could not write DLP build.csv.");
    QTextStream stream(&csv);
    stream << "layer,z_mm,lane,file,empty\n";
    for (const auto& layer : build.layers) {
        const QString folder = QString("Layer %1").arg(layer.number);
        if (!directory.mkpath(folder)) return fail(error, "Could not create DLP export layer directory.");
        auto copyImage = [&](int lane, const QString& source, bool empty) {
            const QString relative = folder + "/" + QFileInfo(source).fileName();
            const QString target   = directory.absoluteFilePath(relative);
            if (!QFile::copy(source, target)) return fail(error, "Could not export DLP image: " + target);
            stream << layer.number << ',' << QString::number(Distance(layer.z).to(mm), 'g', 15) << ',' << lane << ','
                   << relative << ',' << (empty ? 1 : 0) << '\n';
            return true;
        };
        if (!copyImage(0, layer.image_path, layer.empty)) return false;
        for (const auto& lane : layer.lanes) {
            if (!copyImage(lane.number, lane.image_path, lane.empty)) return false;
        }
    }
    stream.flush();
    if (stream.status() != QTextStream::Ok || !csv.commit()) return fail(error, "Could not finish DLP build.csv.");
    return true;
}
}  // namespace ORNL
