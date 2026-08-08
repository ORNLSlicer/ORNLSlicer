#include "gcode/as_printed_model_exporter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include <QByteArray>
#include <QDataStream>
#include <QSaveFile>
#include <QTextStream>
#include <QVector3D>

#include "managers/settings/settings_manager.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;
constexpr float kVectorEpsilon = 1.0e-6f;
constexpr float kVectorEpsilonSquared = kVectorEpsilon * kVectorEpsilon;
constexpr int kCornerBlendSegments = 24;

QString formatFloat(float value) { return QString::number(value, 'g', 9); }

QVector3D normalFor(const AsPrintedModelExporter::Triangle& triangle) {
    QVector3D normal = QVector3D::crossProduct(triangle.b - triangle.a, triangle.c - triangle.a);
    if (!qFuzzyIsNull(normal.lengthSquared())) {
        normal.normalize();
    }
    return normal;
}

void appendStlVertex(QTextStream& out, const QVector3D& vertex) {
    out << "      vertex " << formatFloat(vertex.x()) << ' ' << formatFloat(vertex.y()) << ' '
        << formatFloat(vertex.z()) << '\n';
}

void appendBinaryVector(QDataStream& out, const QVector3D& vector) {
    out << static_cast<float>(vector.x()) << static_cast<float>(vector.y()) << static_cast<float>(vector.z());
}

QVector3D slicePlaneNormal() {
    QVector3D normal = {GSM->getGlobal()->setting<float>(PS::Slicing::kSlicePlaneNormalX),
                        GSM->getGlobal()->setting<float>(PS::Slicing::kSlicePlaneNormalY),
                        GSM->getGlobal()->setting<float>(PS::Slicing::kSlicePlaneNormalZ)};
    if (normal.lengthSquared() <= kVectorEpsilonSquared) {
        normal = QVector3D(0.0f, 0.0f, 1.0f);
    }
    else {
        normal.normalize();
    }

    return normal;
}

std::array<QVector3D, 2> planeBasis(const QVector3D& normal) {
    const QVector3D reference =
        std::fabs(normal.x()) < 0.9f ? QVector3D(1.0f, 0.0f, 0.0f) : QVector3D(0.0f, 1.0f, 0.0f);
    QVector3D u = QVector3D::crossProduct(normal, reference);
    if (u.lengthSquared() <= kVectorEpsilonSquared) {
        u = QVector3D(0.0f, 1.0f, 0.0f);
    }
    else {
        u.normalize();
    }

    QVector3D v = QVector3D::crossProduct(normal, u);
    v.normalize();

    return {u, v};
}

bool pointsConnected(const Point& a, const Point& b) {
    return (a.toQVector3D() - b.toQVector3D()).lengthSquared() <= kVectorEpsilonSquared;
}

float viewToOutputScale(const Distance& output_unit) {
    return static_cast<float>(Constants::OpenGL::kViewToObject / output_unit());
}

QVector3D viewToOutput(const QVector3D& vertex, float output_scale) { return vertex * output_scale; }

bool isDegenerateSegment(const QSharedPointer<SegmentBase>& segment) {
    return (segment->end().toQVector3D() - segment->start().toQVector3D()).lengthSquared() <= kVectorEpsilonSquared;
}

bool isCorner(const QSharedPointer<SegmentBase>& previous, const QSharedPointer<SegmentBase>& current) {
    QVector3D incoming = previous->end().toQVector3D() - previous->start().toQVector3D();
    QVector3D outgoing = current->end().toQVector3D() - current->start().toQVector3D();

    if (incoming.lengthSquared() <= kVectorEpsilonSquared || outgoing.lengthSquared() <= kVectorEpsilonSquared) {
        return false;
    }

    incoming.normalize();
    outgoing.normalize();
    return QVector3D::dotProduct(incoming, outgoing) < 0.999f;
}

void appendCornerBlend(std::vector<AsPrintedModelExporter::Triangle>& triangles,
                       const QSharedPointer<SegmentBase>& previous,
                       const QSharedPointer<SegmentBase>& current, float output_scale) {
    if (!pointsConnected(previous->end(), current->start()) || !isCorner(previous, current)) {
        return;
    }

    const float radius = std::max(previous->displayWidth(), current->displayWidth()) / 2.0f;
    const float height = std::max(previous->displayHeight(), current->displayHeight());
    if (radius <= kVectorEpsilon || height <= kVectorEpsilon) {
        return;
    }

    const QVector3D center = previous->end().toQVector3D();
    const QVector3D normal = slicePlaneNormal();
    const auto [u, v] = planeBasis(normal);
    const QVector3D top_center = center + (normal * (height / 2.0f));
    const QVector3D bottom_center = center - (normal * (height / 2.0f));

    std::array<QVector3D, kCornerBlendSegments> top_vertices;
    std::array<QVector3D, kCornerBlendSegments> bottom_vertices;
    for (int i = 0; i < kCornerBlendSegments; ++i) {
        const float theta = (static_cast<float>(i) / static_cast<float>(kCornerBlendSegments)) * kTwoPi;
        const QVector3D offset = ((std::cos(theta) * u) + (std::sin(theta) * v)) * radius;
        top_vertices[i] = top_center + offset;
        bottom_vertices[i] = bottom_center + offset;
    }

    for (int i = 0; i < kCornerBlendSegments; ++i) {
        const int next = (i + 1) % kCornerBlendSegments;
        triangles.push_back({viewToOutput(top_center, output_scale), viewToOutput(top_vertices[i], output_scale),
                             viewToOutput(top_vertices[next], output_scale)});
        triangles.push_back({viewToOutput(bottom_center, output_scale),
                             viewToOutput(bottom_vertices[next], output_scale),
                             viewToOutput(bottom_vertices[i], output_scale)});
        triangles.push_back({viewToOutput(bottom_vertices[i], output_scale),
                             viewToOutput(bottom_vertices[next], output_scale),
                             viewToOutput(top_vertices[i], output_scale)});
        triangles.push_back({viewToOutput(top_vertices[i], output_scale),
                             viewToOutput(bottom_vertices[next], output_scale),
                             viewToOutput(top_vertices[next], output_scale)});
    }
}

void appendCornerBlends(std::vector<AsPrintedModelExporter::Triangle>& triangles,
                        const QVector<QSharedPointer<SegmentBase>>& connected_segments, float output_scale) {
    if (connected_segments.size() < 2) {
        return;
    }

    for (qsizetype i = 1; i < connected_segments.size(); ++i) {
        appendCornerBlend(triangles, connected_segments[i - 1], connected_segments[i], output_scale);
    }

    appendCornerBlend(triangles, connected_segments.back(), connected_segments.front(), output_scale);
}

void normalizeToLocalOrigin(std::vector<AsPrintedModelExporter::Triangle>& triangles) {
    if (triangles.empty()) {
        return;
    }

    QVector3D minimum(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                      std::numeric_limits<float>::max());

    for (const AsPrintedModelExporter::Triangle& triangle : triangles) {
        for (const QVector3D& vertex : {triangle.a, triangle.b, triangle.c}) {
            minimum.setX(std::min(minimum.x(), vertex.x()));
            minimum.setY(std::min(minimum.y(), vertex.y()));
            minimum.setZ(std::min(minimum.z(), vertex.z()));
        }
    }

    for (AsPrintedModelExporter::Triangle& triangle : triangles) {
        triangle.a -= minimum;
        triangle.b -= minimum;
        triangle.c -= minimum;
    }
}

bool writeAsciiStl(const QString& path, const std::vector<AsPrintedModelExporter::Triangle>& triangles,
                   QString* error_message) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (error_message != nullptr) {
            *error_message = file.errorString();
        }
        return false;
    }

    QTextStream out(&file);
    out << "solid ornlslicer_as_printed\n";

    for (const AsPrintedModelExporter::Triangle& triangle : triangles) {
        const QVector3D normal = normalFor(triangle);
        out << "  facet normal " << formatFloat(normal.x()) << ' ' << formatFloat(normal.y()) << ' '
            << formatFloat(normal.z()) << '\n';
        out << "    outer loop\n";
        appendStlVertex(out, triangle.a);
        appendStlVertex(out, triangle.b);
        appendStlVertex(out, triangle.c);
        out << "    endloop\n";
        out << "  endfacet\n";
    }

    out << "endsolid ornlslicer_as_printed\n";

    if (!file.commit()) {
        if (error_message != nullptr) {
            *error_message = file.errorString();
        }
        return false;
    }

    return true;
}

bool writeBinaryStl(const QString& path, const std::vector<AsPrintedModelExporter::Triangle>& triangles,
                    QString* error_message) {
    if (triangles.size() > std::numeric_limits<quint32>::max()) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("The as-printed model has too many facets for binary STL.");
        }
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error_message != nullptr) {
            *error_message = file.errorString();
        }
        return false;
    }

    QByteArray header(80, ' ');
    const QByteArray label = QByteArrayLiteral("ORNLSlicer as-printed binary STL");
    std::copy(label.begin(), label.end(), header.begin());
    if (file.write(header) != header.size()) {
        if (error_message != nullptr) {
            *error_message = file.errorString();
        }
        return false;
    }

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    out.setFloatingPointPrecision(QDataStream::SinglePrecision);
    out << static_cast<quint32>(triangles.size());

    for (const AsPrintedModelExporter::Triangle& triangle : triangles) {
        appendBinaryVector(out, normalFor(triangle));
        appendBinaryVector(out, triangle.a);
        appendBinaryVector(out, triangle.b);
        appendBinaryVector(out, triangle.c);
        out << static_cast<quint16>(0);
    }

    if (out.status() != QDataStream::Ok) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("Could not write binary STL data.");
        }
        return false;
    }

    if (!file.commit()) {
        if (error_message != nullptr) {
            *error_message = file.errorString();
        }
        return false;
    }

    return true;
}
} // namespace

std::vector<AsPrintedModelExporter::Triangle>
AsPrintedModelExporter::generateTriangles(const QVector<QVector<QSharedPointer<SegmentBase>>>& gcode,
                                          Options options) {
    std::vector<Triangle> triangles;
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<float> colors;
    const float output_scale = viewToOutputScale(options.output_unit);

    for (const QVector<QSharedPointer<SegmentBase>>& layer : gcode) {
        QVector<QSharedPointer<SegmentBase>> connected_segments;

        const auto flushConnectedSegments = [&triangles, &connected_segments, output_scale]() {
            appendCornerBlends(triangles, connected_segments, output_scale);
            connected_segments.clear();
        };

        for (const QSharedPointer<SegmentBase>& segment : layer) {
            if (!shouldExportSegment(segment, options)) {
                if (options.blend_corners) {
                    flushConnectedSegments();
                }
                continue;
            }

            if (options.blend_corners && !connected_segments.isEmpty() &&
                !pointsConnected(connected_segments.back()->end(), segment->start())) {
                flushConnectedSegments();
            }

            const std::size_t start = vertices.size();
            segment->createGraphic(vertices, normals, colors);
            const std::size_t end = vertices.size();

            for (std::size_t i = start; i + 8 < end; i += 9) {
                triangles.push_back({viewToOutput(QVector3D(vertices[i + 0], vertices[i + 1], vertices[i + 2]),
                                                  output_scale),
                                     viewToOutput(QVector3D(vertices[i + 3], vertices[i + 4], vertices[i + 5]),
                                                  output_scale),
                                     viewToOutput(QVector3D(vertices[i + 6], vertices[i + 7], vertices[i + 8]),
                                                  output_scale)});
            }

            if (options.blend_corners) {
                connected_segments.push_back(segment);
            }
        }

        if (options.blend_corners) {
            flushConnectedSegments();
        }
    }

    if (options.origin_mode == OriginMode::kLocalOrigin) {
        normalizeToLocalOrigin(triangles);
    }

    return triangles;
}

bool AsPrintedModelExporter::writeStl(const QString& path,
                                      const QVector<QVector<QSharedPointer<SegmentBase>>>& gcode,
                                      QString* error_message, Options options) {
    const std::vector<Triangle> triangles = generateTriangles(gcode, options);
    switch (options.format) {
        case StlFormat::kAscii:
            return writeAsciiStl(path, triangles, error_message);
        case StlFormat::kBinary:
            return writeBinaryStl(path, triangles, error_message);
    }

    return false;
}

bool AsPrintedModelExporter::shouldExportSegment(const QSharedPointer<SegmentBase>& segment,
                                                 const Options& options) {
    if (segment.isNull()) {
        return false;
    }
    if (isDegenerateSegment(segment)) {
        return false;
    }

    const SegmentDisplayType type = segment->displayType();
    if (!options.include_travel && static_cast<bool>(type & SegmentDisplayType::kTravel)) {
        return false;
    }
    if (!options.include_support && static_cast<bool>(type & SegmentDisplayType::kSupport)) {
        return false;
    }
    if (!segment->depositionActive() && !static_cast<bool>(type & SegmentDisplayType::kTravel)) {
        return false;
    }

    return true;
}
} // namespace ORNL
