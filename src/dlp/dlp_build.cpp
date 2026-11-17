#include "dlp/dlp_build.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QSaveFile>
#include <QScopeGuard>
#include <QStorageInfo>
#include <QTextStream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include "dlp/dlp_storage.h"
#include "units/unit.h"

namespace ORNL {
namespace {
bool fail(QString* error, const QString& message) {
    if (error) *error = message;
    return false;
}

bool nearInteger(double value) {
    // Distance unit constants originate as floats, including GUI-entered printer dimensions.
    return std::abs(value - std::round(value)) <=
           std::numeric_limits<float>::epsilon() * std::max(1.0, std::abs(value));
}

double roundedCeil(double value) {
    if (std::round(value) >= 1.0 && nearInteger(value)) return std::round(value);
    return std::ceil(value);
}

bool imageEmpty(const QImage& image, const std::function<bool()>& canceled) {
    for (int y = 0; y < image.height(); ++y) {
        if (canceled && canceled()) return true;
        const uchar* row = image.constScanLine(y);
        for (int x = 0; x < image.width(); ++x) {
            if (row[x] != 0) return false;
        }
    }
    return true;
}

bool saveBmp(const QImage& image, const QString& path, int bit_depth, QString* error,
             const std::function<bool()>& canceled) {
    if (canceled && canceled()) return fail(error, "DLP slicing canceled.");
    QImage output;
    if (bit_depth == 1) {
        output = QImage(image.size(), QImage::Format_Mono);
        if (output.isNull()) return fail(error, "Could not allocate DLP bitmap: " + path);
        output.setColorTable({qRgb(0, 0, 0), qRgb(255, 255, 255)});
        output.fill(0);
        for (int y = 0; y < image.height(); ++y) {
            if (canceled && canceled()) return fail(error, "DLP slicing canceled.");
            const uchar* source = image.constScanLine(y);
            uchar* target       = output.scanLine(y);
            for (int x = 0; x < image.width(); ++x) {
                if (source[x] >= 128) target[x / 8] |= uchar(0x80 >> (x % 8));
            }
        }
    }
    else { output = image.convertToFormat(QImage::Format_Indexed8); }
    if (canceled && canceled()) return fail(error, "DLP slicing canceled.");
    if (output.isNull() || !output.save(path, "BMP")) return fail(error, "Could not write DLP image: " + path);
    if (canceled && canceled()) return fail(error, "DLP slicing canceled.");
    return true;
}
}  // namespace

bool DlpRasterizer::layout(DlpBuild& build, const DlpSettings& settings, QString* error) {
    const double values[] {settings.x_min, settings.x_max, settings.y_min,        settings.y_max,
                           settings.z_min, settings.z_max, settings.layer_height, settings.projection_width};
    for (double value : values) {
        if (!std::isfinite(value)) return fail(error, "DLP dimensions must be finite.");
    }
    if (settings.x_max <= settings.x_min || settings.y_max <= settings.y_min || settings.z_max <= settings.z_min)
        return fail(error, "DLP slicing requires positive rectangular printer dimensions.");
    if (settings.layer_height <= 0 || settings.projection_width <= 0 || settings.lane_pixel_width <= 0)
        return fail(error, "DLP layer height, projection width, and image pixel width must be greater than zero.");
    if (settings.lane_pixel_width != 4096)
        return fail(error, "DLP static and scrolling output require Projection Width in Pixels to be 4096.");
    if (settings.bit_depth != 1 && settings.bit_depth != 8) return fail(error, "DLP bitmap depth must be 1 or 8 bits.");
    if (settings.output_mode != DlpOutputMode::kStatic && settings.output_mode != DlpOutputMode::kScrolling)
        return fail(error, "DLP output mode must be Static or Scrolling.");

    const double pitch  = settings.projection_width / settings.lane_pixel_width;
    const double width  = roundedCeil((settings.x_max - settings.x_min) / pitch);
    const double height = roundedCeil((settings.y_max - settings.y_min) / pitch);
    double lanes        = roundedCeil((settings.x_max - settings.x_min) / settings.projection_width);
    const double lane_height =
        settings.output_mode == DlpOutputMode::kStatic ? 2160 : roundedCeil(height / 2176) * 2176;
    if (!std::isfinite(pitch) || pitch <= 0 || width < 1 || height < 1 || lanes < 1 ||
        width > std::numeric_limits<int>::max() || height > std::numeric_limits<int>::max() ||
        lanes > std::numeric_limits<int>::max() || lane_height > std::numeric_limits<int>::max())
        return fail(error, "DLP image dimensions exceed the supported range.");

    int overlap_pixels = 0;
    if (settings.interlaced_stitching) {
        if (!std::isfinite(settings.stitch_overlap) || settings.stitch_overlap <= 0)
            return fail(error, "DLP stitching overlap must be finite and greater than zero.");
        const double overlap = roundedCeil(settings.stitch_overlap / pitch);
        if (!std::isfinite(overlap) || overlap < 1 || overlap >= settings.lane_pixel_width)
            return fail(
                error,
                "DLP stitching overlap must be less than one complete projector width after rounding to pixels.");
        overlap_pixels = static_cast<int>(overlap);
        if (width > settings.lane_pixel_width) {
            const qint64 remaining = static_cast<qint64>(width) - settings.lane_pixel_width;
            const qint64 advance   = settings.lane_pixel_width - overlap_pixels;
            lanes                  = 1 + ((remaining + advance - 1) / advance);
        }
        else
            lanes = 1;
    }

    if (settings.output_mode == DlpOutputMode::kStatic && height > lane_height)
        return fail(error, QString("DLP static output is restricted to one 4096 x 2160 frame (%1 x %2 mm). "
                                   "Reduce the printer Y span or select Scrolling output.")
                               .arg(Distance(settings.projection_width).to(mm), 0, 'g', 12)
                               .arg(Distance(2160 * pitch).to(mm), 0, 'g', 12));
    const double pixel_span = (settings.x_max - settings.x_min) / pitch;
    if (lanes > 1 && !nearInteger(pixel_span))
        return fail(error, QString("DLP shifted lanes require the printer X span to be a whole number of pixels "
                                   "(%1 mm per pixel). Adjust the printer X bounds or Projection Width to align "
                                   "the final footprint with maximum X without shifting exposure geometry.")
                               .arg(Distance(pitch).to(mm), 0, 'g', 12));

    // Full layers use grayscale working buffers and bounded previews. Lane limits retain the 32-bit budget.
    const int allocation_limit = QImageReader::allocationLimit();
    const double raster_bytes  = std::ceil(width / 4.0) * 4.0 * height;
    const double lane_bytes    = settings.lane_pixel_width * lane_height * 4.0;
    if (settings.interlaced_stitching && allocation_limit > 0 &&
        ((2.0 * lanes - 1) * sizeof(int)) > allocation_limit * 1024.0 * 1024.0)
        return fail(error, QString("DLP stitching lane layout exceeds the %1 MiB allocation limit. "
                                   "Reduce the stitching overlap or printer X span.")
                               .arg(allocation_limit));
    if (allocation_limit > 0 && std::max(raster_bytes, lane_bytes) > allocation_limit * 1024.0 * 1024.0)
        return fail(error, QString("DLP images exceed the %1 MiB image allocation limit. Full layer: %2 x %3 pixels; "
                                   "lane: %4 x %5 pixels. Full grayscale raster: %6 MiB; "
                                   "lane 32-bit budget: %7 MiB. Increase Projection Width "
                                   "or reduce the printer XY dimensions.")
                               .arg(allocation_limit)
                               .arg(width, 0, 'f', 0)
                               .arg(height, 0, 'f', 0)
                               .arg(settings.lane_pixel_width)
                               .arg(lane_height, 0, 'f', 0)
                               .arg(raster_bytes / (1024.0 * 1024.0), 0, 'f', 1)
                               .arg(lane_bytes / (1024.0 * 1024.0), 0, 'f', 1));

    build          = DlpBuild();
    build.settings = settings;
    if (settings.output_mode == DlpOutputMode::kScrolling) build.settings.bit_depth = 1;
    build.pixel_pitch           = pitch;
    build.image_width           = static_cast<int>(width);
    build.image_height          = static_cast<int>(height);
    build.lane_count            = static_cast<int>(lanes);
    build.lane_image_height     = static_cast<int>(lane_height);
    build.stitch_overlap_pixels = overlap_pixels;
    if (settings.interlaced_stitching) {
        build.lane_starts.reserve(build.lane_count);
        build.stitch_band_starts.reserve(std::max(0, build.lane_count - 1));
    }
    for (int i = 0; i < build.lane_count; ++i) {
        const int start = settings.interlaced_stitching
                              ? static_cast<int>(qint64(i) * (settings.lane_pixel_width - overlap_pixels))
                              : i * settings.lane_pixel_width;
        build.lane_starts.append(i > 0 && i == build.lane_count - 1 ? build.image_width - settings.lane_pixel_width
                                                                    : start);
    }
    if (settings.interlaced_stitching) {
        // Center the requested stitching band inside each pair's actual shared footprint.
        for (int i = 1; i < build.lane_count; ++i)
            build.stitch_band_starts.append(static_cast<int>(
                (qint64(build.lane_starts[i - 1]) + settings.lane_pixel_width + build.lane_starts[i] - overlap_pixels) /
                2));
    }
    return true;
}

bool DlpRasterizer::initialize(DlpBuild& build, const DlpSettings& settings, QString* error) {
    if (!layout(build, settings, error)) return false;
    QString temporary_root;
    if (!DlpStorage::temporaryRoot(temporary_root, error)) return false;
    QDir().mkpath(temporary_root);
    build.storage = QSharedPointer<QTemporaryDir>::create(temporary_root + "/ornlslicer-dlp-XXXXXX");
    if (!build.storage->isValid())
        return fail(error, QString("Could not create temporary DLP output directory in \"%1\": %2\n"
                                   "Check that this location is writable and has free space.")
                               .arg(QDir::toNativeSeparators(temporary_root), build.storage->errorString()));
    return true;
}

QImage DlpRasterizer::rasterize(const QVector<PolygonList>& sections, const DlpBuild& build,
                                const std::function<bool()>& canceled) {
    if (canceled && canceled()) return {};
    QImage image(build.image_width, build.image_height, QImage::Format_Grayscale8);
    if (image.isNull()) return image;
    image.fill(0);
    const auto& settings = build.settings;
    QVector<double> intersections;
    for (int row = 0; row < image.height(); ++row) {
        if (canceled && canceled()) return {};
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

bool DlpRasterizer::writeLayer(DlpBuild& build, int number, double z, const QImage& image, QString* error,
                               const std::function<bool()>& canceled) {
    if (canceled && canceled()) return fail(error, "DLP slicing canceled.");
    if (!build.storage || !build.storage->isValid() || image.isNull() || image.width() != build.image_width ||
        image.height() != build.image_height)
        return fail(error, "DLP layer image does not match the build layout.");
    if (build.settings.interlaced_stitching &&
        (number < 1 || build.stitch_overlap_pixels < 1 ||
         build.stitch_overlap_pixels >= build.settings.lane_pixel_width ||
         build.lane_starts.size() != build.lane_count || build.stitch_band_starts.size() != build.lane_count - 1))
        return fail(error, "DLP stitching layer does not match the build layout.");
    const QImage mask = image.convertToFormat(QImage::Format_Grayscale8);
    QDir directory(build.storage->path());
    const QString folder = QString("Layer %1").arg(number);
    if (!directory.mkpath(folder)) return fail(error, "Could not create DLP layer directory.");
    directory.cd(folder);

    DlpLayer layer;
    layer.number     = number;
    layer.z          = z;
    layer.image_path = directory.absoluteFilePath(QString("Layer_%1_Full.bmp").arg(number));
    layer.empty      = imageEmpty(mask, canceled);
    if (!saveBmp(mask, layer.image_path, build.settings.bit_depth, error, canceled)) return false;
    for (int i = 0; i < build.lane_count; ++i) {
        if (canceled && canceled()) return fail(error, "DLP slicing canceled.");
        QImage lane_image(build.settings.lane_pixel_width, build.lane_image_height, QImage::Format_Grayscale8);
        if (lane_image.isNull()) return fail(error, "Could not allocate DLP lane image.");
        lane_image.fill(0);
        if (build.settings.interlaced_stitching) {
            // A common phase keeps ownership intervals ordered even when three footprints overlap.
            for (int y = 0; y < mask.height(); ++y) {
                if (canceled && canceled()) return fail(error, "DLP slicing canceled.");
                const int shift =
                    ((y / build.stitch_overlap_pixels + (number - 1) % 2) % 2) * build.stitch_overlap_pixels;
                const int first = i == 0 ? 0 : build.stitch_band_starts[i - 1] + shift;
                const int end   = i == build.lane_count - 1 ? mask.width() : build.stitch_band_starts[i] + shift;
                std::memcpy(lane_image.scanLine(y) + (first - build.lane_starts[i]), mask.constScanLine(y) + first,
                            end - first);
            }
        }
        else {
            const qint64 start       = qint64(i) * build.settings.lane_pixel_width;
            const int masked_columns = static_cast<int>(start - build.lane_starts[i]);
            const int count = static_cast<int>(std::min<qint64>(build.settings.lane_pixel_width, mask.width() - start));
            if (count > 0) {
                for (int y = 0; y < mask.height(); ++y) {
                    if (canceled && canceled()) return fail(error, "DLP slicing canceled.");
                    std::memcpy(lane_image.scanLine(y) + masked_columns, mask.constScanLine(y) + start, count);
                }
            }
        }
        DlpLane lane;
        lane.number     = i + 1;
        lane.image_path = directory.absoluteFilePath(QString("Layer%1_%2.bmp").arg(number).arg(lane.number));
        lane.empty      = imageEmpty(lane_image, canceled);
        if (!saveBmp(lane_image, lane.image_path, build.settings.bit_depth, error, canceled)) return false;
        layer.lanes.append(lane);
    }
    build.layers.append(layer);
    return true;
}

bool DlpExporter::exportBuild(const DlpBuild& build, const QString& destination, QString* error,
                              const std::function<bool(qint64, qint64)>& progress, bool* canceled) {
    if (canceled) *canceled = false;
    if (error) error->clear();
    if (!build.storage || !build.storage->isValid() || build.layers.isEmpty())
        return fail(error, "No completed DLP build is available to export.");
    const QString output = QDir(destination).absolutePath();
    const QFileInfo output_info(output);
    if (output_info.isDir()) {
        const QStorageInfo volume(output);
        if (QDir(output).isRoot() ||
            (volume.isValid() && output_info.canonicalFilePath() == QFileInfo(volume.rootPath()).canonicalFilePath()))
            return fail(error, "A DLP export cannot replace a filesystem root. Create a build subfolder in: " + output);
    }
    const auto available = [&]() {
        return !QFileInfo::exists(output) ||
               (!QFileInfo(output).isSymLink() && QFileInfo(output).isDir() &&
                QDir(output)
                    .entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System)
                    .isEmpty());
    };
    if (!available()) return fail(error, "Select a new or empty directory for the DLP export.");
    qint64 total = 0, completed = 0, image_count = 0;
    for (const auto& layer : build.layers) {
        total += QFileInfo(layer.image_path).size();
        ++image_count;
        for (const auto& lane : layer.lanes) {
            total += QFileInfo(lane.image_path).size();
            ++image_count;
        }
    }
    // Cached files already occupy space; reserve only the additional copy and estimated CSV rows.
    const QString parent = output_info.absolutePath();
    if (!DlpStorage::checkSpace(parent, double(total) + 256.0 * image_count + 128, error)) return false;
    if (!QDir().mkpath(parent)) return fail(error, "Could not create DLP export parent directory: " + parent);
    QTemporaryDir staging(parent + "/.ornlslicer-dlp-export-XXXXXX");
    if (!staging.isValid())
        return fail(error, "Could not create temporary DLP export directory: " + staging.errorString());
    // The private staging container protects partial files; the published child inherits normal mkdir defaults.
    const QString staged_output = staging.filePath("build");
    if (!QDir().mkdir(staged_output)) return fail(error, "Could not create staged DLP export directory.");
    QDir directory(staged_output);
    const auto cleanup_permissions = qScopeGuard([&] {
        if (QFileInfo::exists(staged_output))
            QFile::setPermissions(staged_output, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    });
    const auto keep_going          = [&]() {
        if (!progress || progress(completed, total)) return true;
        if (canceled) *canceled = true;
        return fail(error, "DLP export canceled.");
    };
    if (!keep_going()) return false;
    QSaveFile csv(directory.absoluteFilePath("build.csv"));
    if (!csv.open(QIODevice::WriteOnly | QIODevice::Text)) return fail(error, "Could not write DLP build.csv.");
    QTextStream stream(&csv);
    stream << "layer,z_mm,lane,file,empty,x_min_mm,y_max_mm,pixel_pitch_mm,width_px,height_px\n";
    QByteArray buffer(1024 * 1024, Qt::Uninitialized);
    for (const auto& layer : build.layers) {
        const QString folder = QString("Layer %1").arg(layer.number);
        if (!directory.mkpath(folder)) return fail(error, "Could not create DLP export layer directory.");
        auto copyImage = [&](int lane, const QString& source, bool empty) {
            const QString relative = folder + "/" + QFileInfo(source).fileName();
            const QString target   = directory.absoluteFilePath(relative);
            QFile input(source), output_file(target);
            if (!input.open(QIODevice::ReadOnly))
                return fail(error, "Could not read cached DLP image: " + source + "\n" + input.errorString());
            if (!output_file.open(QIODevice::WriteOnly))
                return fail(error, "Could not export DLP image: " + QDir(output).filePath(relative) + "\n" +
                                       output_file.errorString());
            while (!input.atEnd()) {
                if (!keep_going()) return false;
                const qint64 count = input.read(buffer.data(), buffer.size());
                if (count < 0)
                    return fail(error, "Could not read cached DLP image: " + source + "\n" + input.errorString());
                if (output_file.write(buffer.constData(), count) != count)
                    return fail(error, "Could not export DLP image: " + QDir(output).filePath(relative) + "\n" +
                                           output_file.errorString());
                completed += count;
                if (!keep_going()) return false;
            }
            if (!output_file.flush())
                return fail(error, "Could not finish DLP image: " + QDir(output).filePath(relative) + "\n" +
                                       output_file.errorString());
            stream << layer.number << ',' << QString::number(Distance(layer.z).to(mm), 'g', 15) << ',' << lane << ','
                   << relative << ',' << (empty ? 1 : 0) << ','
                   << QString::number(Distance(build.settings.x_min +
                                               (lane == 0 ? 0 : build.lane_starts[lane - 1] * build.pixel_pitch))
                                          .to(mm),
                                      'g', 15)
                   << ',' << QString::number(Distance(build.settings.y_max).to(mm), 'g', 15) << ','
                   << QString::number(Distance(build.pixel_pitch).to(mm), 'g', 15) << ','
                   << (lane == 0 ? build.image_width : build.settings.lane_pixel_width) << ','
                   << (lane == 0 ? build.image_height : build.lane_image_height) << '\n';
            return true;
        };
        if (!copyImage(0, layer.image_path, layer.empty)) return false;
        for (const auto& lane : layer.lanes) {
            if (!copyImage(lane.number, lane.image_path, lane.empty)) return false;
        }
    }
    stream.flush();
    if (stream.status() != QTextStream::Ok || !csv.commit()) return fail(error, "Could not finish DLP build.csv.");
    if (!keep_going()) return false;
    if (!available()) return fail(error, "The DLP export destination is no longer an empty directory: " + output);
    const QFileInfo current_output(output);
    const bool existing    = current_output.exists();
    const auto permissions = current_output.permissions();
    if (existing && !QFile::setPermissions(staged_output, permissions))
        return fail(error, "Could not preserve DLP export folder permissions: " + output);
    if (existing && !QDir().rmdir(output))
        return fail(error, "Could not prepare the empty DLP export directory: " + output);
    if (!QDir().rename(staged_output, output)) {
        if (existing && QDir().mkdir(output)) QFile::setPermissions(output, permissions);
        return fail(error, "Could not publish the completed DLP export directory: " + output);
    }
    return true;
}
}  // namespace ORNL
