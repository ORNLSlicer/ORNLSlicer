#include <QCoreApplication>
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>

#include "geometry/segments/arc.h"
#include "geometry/segments/line.h"
#include "graphics/support/tool_orientation_preview.h"
#include "test_utils.h"
#include "utilities/constants.h"

namespace {
using GCode = QVector<QVector<QSharedPointer<ORNL::SegmentBase>>>;
using ORNL::SegmentDisplayType;
using ORNL::Testing::expect;
using ORNL::Testing::near;

float millimeterInView() {
    return (1.0 * ORNL::mm)() * ORNL::Constants::OpenGL::kObjectToView;
}

QSharedPointer<ORNL::SegmentBase> line(uint number, float start_mm, float end_mm,
                                       SegmentDisplayType type = SegmentDisplayType::kLine, bool oriented = true,
                                       uint layer = 0) {
    const float scale = millimeterInView();
    auto segment      = QSharedPointer<ORNL::LineSegment>::create(ORNL::Point(start_mm * scale, 0, 0),
                                                                  ORNL::Point(end_mm * scale, 0, 0));
    segment->setDisplayInfo(scale, std::abs(end_mm - start_mm) * scale, scale, type, Qt::white, number, layer);
    if (oriented) { segment->setToolOrientation(QQuaternion()); }
    return segment;
}

void refreshAll(ORNL::ToolOrientationPreview& preview, const GCode& gcode,
                SegmentDisplayType hidden = SegmentDisplayType::kNone) {
    preview.refresh(gcode, 0, std::numeric_limits<uint>::max(), 0, std::numeric_limits<uint>::max(), hidden);
}

QVector<uint> sampledLines(const ORNL::ToolOrientationPreview& preview) {
    QVector<uint> result;
    for (const auto& pose : preview.sampledPoses()) { result.append(pose.line); }
    return result;
}

bool boundsSamplingAndKeepsExactEndpoints() {
    GCode gcode(1);
    for (uint i = 0; i < 1001; ++i) { gcode[0].append(line(i + 1, i, i + 1)); }
    ORNL::ToolOrientationPreview preview;
    preview.reset(gcode);
    refreshAll(preview, gcode);

    const auto& samples = preview.sampledPoses();
    bool passed         = expect(samples.size() == 200, "A dense path should be limited to 200 ordinary pose samples.");
    if (samples.size() != 200) { return false; }
    passed &= expect(samples.first().line == 1 && samples.last().line == 1001,
                     "Sampling must retain the first and last visible command endpoints.");
    for (qsizetype i = 0; i < samples.size(); ++i) {
        passed &= expect(near(samples[i].position.x(), samples[i].line * millimeterInView(), 1.0e-3) &&
                             samples[i].position.y() == 0 && samples[i].position.z() == 0,
                         "Pose samples must use exact segment endpoints rather than interpolated positions.");
        if (i > 0) {
            const uint gap = samples[i].line - samples[i - 1].line;
            passed &= expect(gap >= 5 && gap <= 6, "Dense samples must cover the path evenly in G-code order.");
        }
    }

    // The active pose may be an endpoint omitted by the bounded ordinary sample set.
    preview.updateSelection({1}, {});
    passed &= expect(preview.activePose() && preview.activePose()->line == 2,
                     "Selection must retain an active pose even when ordinary sampling omitted its endpoint.");
    return passed;
}

bool selectionFollowsRecencyAndVisiblePlayback() {
    const GCode gcode {{line(10, 0, 1), line(20, 1, 2), line(30, 2, 3)}};
    ORNL::ToolOrientationPreview preview;
    preview.reset(gcode);
    refreshAll(preview, gcode);
    bool passed = expect(preview.activePose() && preview.activePose()->line == 30,
                         "Without a selection, the last visible oriented endpoint should be active.");
    preview.updateSelection({9, 19}, {});
    passed &= expect(preview.activePose() && preview.activePose()->line == 20,
                     "The most recently selected command should own the active pose.");
    preview.updateSelection({9}, {});
    passed &= expect(preview.activePose() && preview.activePose()->line == 10,
                     "Reselecting an endpoint should move it to the end of selection recency.");
    preview.updateSelection({}, {9});
    passed &= expect(preview.activePose() && preview.activePose()->line == 20,
                     "Deselecting the active endpoint should restore the previous visible selection.");
    preview.refresh(gcode, 0, 0, 0, 0, SegmentDisplayType::kNone);
    passed &= expect(preview.activePose() && preview.activePose()->line == 10,
                     "A selected endpoint outside the visible range must not override the playback pose.");
    preview.clearSelection();
    preview.refresh(gcode, 0, 0, 0, 1, SegmentDisplayType::kNone);
    passed &= expect(preview.activePose() && preview.activePose()->line == 20,
                     "Advancing the segment range must advance the fallback pose after clearing selection.");
    return passed;
}

bool filteringUsesRawIndicesWithinTheLayerRange() {
    const GCode gcode {
        {line(10, 0, 1, SegmentDisplayType::kTravel), line(20, 1, 2), line(30, 2, 3, SegmentDisplayType::kLine, false)},
        {line(40, 3, 4, SegmentDisplayType::kLine, true, 1), line(50, 4, 5, SegmentDisplayType::kSupport, true, 1),
         line(60, 5, 6, SegmentDisplayType::kTravel, true, 1), line(70, 6, 7, SegmentDisplayType::kLine, true, 1)}};
    ORNL::ToolOrientationPreview preview;
    preview.reset(gcode);
    preview.refresh(gcode, 1, 1, 1, 3, SegmentDisplayType::kSupport);
    bool passed = expect(sampledLines(preview) == QVector<uint>({60, 70}),
                         "Segment indices must restart at the first visible layer and count hidden segments.");
    preview.refresh(gcode, 0, 1, 3, 5, SegmentDisplayType::kSupport | SegmentDisplayType::kTravel);
    passed &= expect(sampledLines(preview) == QVector<uint>({40}),
                     "Unoriented segments must still consume raw indices, and both hidden masks must apply.");
    preview.updateSelection({49}, {});
    passed &= expect(preview.activePose() && preview.activePose()->line == 40,
                     "A hidden selected support endpoint must not produce an active triad.");
    preview.refresh(gcode, 1, 1, 1, 2, SegmentDisplayType::kSupport | SegmentDisplayType::kTravel);
    passed &= expect(preview.sampledPoses().isEmpty() && !preview.activePose(),
                     "An entirely hidden visible range must produce no ordinary or active pose.");
    preview.refresh(gcode, 2, 3, 0, 10, SegmentDisplayType::kNone);
    passed &= expect(preview.sampledPoses().isEmpty() && !preview.activePose(),
                     "An out-of-range layer selection must produce no poses.");
    return passed;
}

bool distinguishesMissingFramesAndClearsFileState() {
    const GCode gcode {{line(1, 0, 1, SegmentDisplayType::kLine, false), line(2, 1, 2)}};
    ORNL::ToolOrientationPreview preview;
    preview.reset(gcode);
    refreshAll(preview, gcode);
    bool passed =
        expect(sampledLines(preview) == QVector<uint>({2}) && preview.sampledPoses().first().orientation.isIdentity(),
               "An explicitly supplied identity frame is supported; missing orientation is not.");
    preview.updateSelection({1}, {});
    const GCode replacement {{line(2, 0, 1), line(3, 1, 2)}};
    preview.reset(replacement);
    passed &= expect(preview.sampledPoses().isEmpty() && !preview.activePose(),
                     "Loading a replacement path must clear cached samples before refreshing.");
    refreshAll(preview, replacement);
    passed &= expect(preview.activePose() && preview.activePose()->line == 3,
                     "Loading another file must discard selection even if its line numbers overlap.");
    preview.reset({});
    refreshAll(preview, {});
    passed &= expect(preview.sampledPoses().isEmpty() && !preview.activePose(),
                     "Clearing a file must remove ordinary and active pose state.");
    const GCode unsupported {{line(1, 0, 100, SegmentDisplayType::kLine, false)}};
    preview.reset(unsupported);
    refreshAll(preview, unsupported);
    passed &= expect(preview.sampledPoses().isEmpty() && !preview.activePose(),
                     "A path without supported orientation must not acquire assumed axes.");
    return passed;
}

bool sizesAxesFromTheCompleteOrientedPath() {
    ORNL::ToolOrientationPreview preview;
    bool passed = true;
    for (const auto [length_mm, expected_mm] :
         std::array<std::pair<float, float>, 3> {{{0.1f, 1}, {100, 2}, {2000, 10}}}) {
        const GCode gcode {{line(1, 0, length_mm)}};
        preview.reset(gcode);
        passed &= expect(near(preview.axisLength(), expected_mm * millimeterInView(), 1.0e-5),
                         "Axis length must use preview units and clamp to the physical 1–10 mm range.");
    }
    const GCode gcode {{line(1, 0, 100), line(2, 100, 1000000, SegmentDisplayType::kLine, false)}};
    preview.reset(gcode);
    preview.refresh(gcode, 0, 0, 0, 0, SegmentDisplayType::kNone);
    const float full_path_size = preview.axisLength();
    preview.refresh(gcode, 0, 0, 1, 1, SegmentDisplayType::kNone);
    passed &= expect(near(full_path_size, 2.0f * millimeterInView(), 1.0e-5) && preview.axisLength() == full_path_size,
                     "Unoriented geometry and changes to visible ranges must not resize the tool axes.");

    const float radius = 100.0f * millimeterInView();
    auto circle        = QSharedPointer<ORNL::ArcSegment>::create(ORNL::Point(radius, 0, 0), ORNL::Point(radius, 0, 0),
                                                                  ORNL::Point(), 2.0 * M_PI * ORNL::rad, true);
    circle->setToolOrientation(QQuaternion());
    preview.reset({{circle}});
    passed &= expect(near(preview.axisLength(), 0.02f * 2.0f * radius * std::sqrt(2.0f), 1.0e-4),
                     "A full-circle path must contribute its diameter to sizing despite identical endpoints.");
    return passed;
}
}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    bool passed = boundsSamplingAndKeepsExactEndpoints();
    passed &= selectionFollowsRecencyAndVisiblePlayback();
    passed &= filteringUsesRawIndicesWithinTheLayerRange();
    passed &= distinguishesMissingFramesAndClearsFileState();
    passed &= sizesAxesFromTheCompleteOrientedPath();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
