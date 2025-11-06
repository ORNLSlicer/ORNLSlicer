#pragma once

#include "geometry/path.h"
#include "geometry/segments/travel.h"

namespace ORNL {
/**
 * @brief The PathModifierGenerator class provides functions for modifying paths.
 */
class PathModifierGenerator {
  public:
    static void GenerateRotationAndTilt(Path& path, Point origin, bool rotate, bool& next_ccw, bool tilt);
    static void GenerateTravel(Path& path, Point current_location, Velocity velocity);

    /**
     * @brief GenerateOpenLoopPreStart generates a pre-start path for open loop paths.
     * @param path: The path to modify.
     * @param prestartDistance: The pre-start distance.
     * @param prestartSpeed: The pre-start speed.
     * @param prestartExtruderSpeed: The pre-start extruder speed.
     * @param enableWidthHeight: Whether to enable width and height mode.
     * @param areaMultiplier: The area multiplier.
     */
    static void GenerateOpenLoopPreStart(Path& path, Distance prestartDistance, Velocity prestartSpeed,
                                 AngularVelocity prestartExtruderSpeed, bool enableWidthHeight, double areaMultiplier);

    /**
     * @brief GenerateFlyingStart generates a flying start path which begins motion before enabling extrusion.
     * @param path: The path to modify.
     * @param flyingStartDistance: The length of the flying start motion.
     * @param flyingStartSpeed: The speed for the flying start motion speed.
     */
    static void GenerateFlyingStart(Path& path, Distance flyingStartDistance, Velocity flyingStartSpeed);

    /**
     * @brief GenerateInitialStartup generates an initial startup path.
     * @param path: The path to modify.
     * @param startDistance: The start distance.
     * @param startSpeed: The start speed.
     * @param extruderSpeed: The extruder speed.
     * @param enableWidthHeight: Whether to enable width and height mode.
     * @param areaMultiplier: The area multiplier.
     */
    static void GenerateInitialStartup(Path& path, Distance startDistance, Velocity startSpeed,
                                       AngularVelocity extruderSpeed, bool enableWidthHeight, double areaMultiplier);

    /**
     * @brief GenerateInitialStartupWithRampUp generates an initial startup path with ramp up.
     * @param path: The path to modify.
     * @param startDistance: The start distance.
     * @param startSpeed: The start speed.
     * @param endSpeed: The end speed.
     * @param startExtruderSpeed: The start extruder speed.
     * @param endExtruderSpeed: The end extruder speed.
     * @param steps: The number of steps in the ramp.
     * @param enableWidthHeight: Whether to enable width and height mode.
     * @param areaMultiplier: The area multiplier.
     */
    static void GenerateInitialStartupWithRampUp(Path& path, Distance startDistance, Velocity startSpeed,
                                                 Velocity endSpeed, AngularVelocity startExtruderSpeed,
                                                 AngularVelocity endExtruderSpeed, int steps, bool enableWidthHeight,
                                                 double areaMultiplier);

    /**
     * @brief GenerateSlowdown generates a slowdown path.
     * @param path: The path to modify.
     * @param slowDownDistance: The slowdown distance.
     * @param slowDownLiftDistance: The slowdown lift distance.
     * @param slowDownCutoffDistance: The slowdown cutoff distance.
     * @param slowDownSpeed: The slowdown speed.
     * @param extruderSpeed: The extruder speed.
     * @param enableWidthHeight: Whether to enable width and height.
     * @param areaMultiplier: The area multiplier.
     */
    static void GenerateSlowdown(Path& path, Distance slowDownDistance, Distance slowDownLiftDistance,
                                 Distance slowDownCutoffDistance, Velocity slowDownSpeed, AngularVelocity extruderSpeed,
                                 bool enableWidthHeight, double areaMultiplier);

    /**
     * @brief GenerateLayerLeadIn generates a layer lead-in path.
     * @param path: The path to modify.
     * @param lead_in_start: The lead-in point.
     * @param sb: The settings base.
     */
    static void GenerateLayerLeadIn(Path& path, const Point& lead_in_start, QSharedPointer<SettingsBase> sb);

    /**
     * @brief GenerateTrajectorySlowdown generates a trajectory slowdown path.
     * @param path: The path to modify.
     * @param sb: The settings base.
     */
    static void GenerateTrajectorySlowdown(Path& path, QSharedPointer<SettingsBase> sb);

    /**
     * @brief GenerateTipWipe generates a tip wipe path for closed contours.
     * @param path: The path to modify.
     * @param modifiers: The path modifiers.
     * @param wipeDistance: The wipe distance.
     * @param wipeSpeed: The wipe speed.
     * @param wipeAngle: The wipe angle.
     * @param extruderSpeed: The extruder speed.
     * @param tipWipeLiftDistance: The tip wipe lift distance.
     * @param tipWipeCutoffDistance: The tip wipe cutoff distance.
     */
    static void GenerateTipWipe(Path& path, PathModifiers modifiers, Distance wipeDistance, Velocity wipeSpeed,
                                Angle wipeAngle, AngularVelocity extruderSpeed, Distance tipWipeLiftDistance,
                                Distance tipWipeCutoffDistance);

    /**
     * @brief GenerateTipWipe generates a tip wipe path for skin/infill patterns.
     * @param path: The path to modify.
     * @param modifiers: The path modifiers.
     * @param wipeDistance: The wipe distance.
     * @param wipeSpeed: The wipe speed.
     * @param outerPath: The outer path to connect to
     * @param wipeAngle: The wipe angle.
     * @param extruderSpeed: The extruder speed.
     * @param tipWipeLiftDistance: The tip wipe lift distance.
     * @param tipWipeCutoffDistance: The tip wipe cutoff distance.
     */
    static void GenerateTipWipe(Path& path, PathModifiers modifiers, Distance wipeDistance, Velocity wipeSpeed,
                                QVector<Path>& outerPath, Angle wipeAngle, AngularVelocity extruderSpeed,
                                Distance tipWipeLiftDistance, Distance tipWipeCutoffDistance);

    /**
     * @brief GenerateForwardTipWipeOpenLoop generates a forward tip wipe path for open loop paths.
     * @param path: The path to modify.
     * @param modifiers: The path modifiers.
     * @param wipeDistance: The wipe distance.
     * @param wipeSpeed: The wipe speed.
     * @param extruderSpeed: The extruder speed.
     * @param tipWipeLiftDistance: The tip wipe lift distance.
     * @param tipWipeCutoffDistance: The tip wipe cutoff distance.
     */
    static void GenerateForwardTipWipeOpenLoop(Path& path, PathModifiers modifiers, Distance wipeDistance,
                                               Velocity wipeSpeed, AngularVelocity extruderSpeed,
                                               Distance tipWipeLiftDistance, Distance tipWipeCutoffDistance,
                                               bool clearTipWipDistanceCovered = true);

    /**
     * @brief GenerateSpiralLift generates a spiral lift path.
     * @param path: The path to modify.
     * @param spiralWidth: The spiral width.
     * @param spiralHeight: The spiral height.
     * @param spiralPoints: The number of spiral points.
     * @param spiralLiftVelocity: The spiral lift velocity.
     * @param supportsG3: Whether G3 is supported.
     */
    static void GenerateSpiralLift(Path& path, Distance spiralWidth, Distance spiralHeight, int spiralPoints,
                                   Velocity spiralLiftVelocity, bool supportsG3);

  private:
    /**
     * @brief writeSegment writes a segment to the path.
     * @param path: The path to modify.
     * @param start: The start point.
     * @param end: The end point.
     * @param width: The width.
     * @param height: The height.
     * @param speed: The speed.
     * @param acceleration: The acceleration.
     * @param extruder_speed: The extruder speed.
     * @param regionType: The region type.
     * @param pathModifiers: The path modifiers.
     * @param materialNumber: The material number.
     * @param extruders: The extruders.
     */
    static void writeSegment(Path& path, Point start, Point end, Distance width, Distance height, Velocity speed,
                             Acceleration acceleration, AngularVelocity extruder_speed, RegionType regionType,
                             PathModifiers pathModifiers, int materialNumber, QVector<int> extruders);

    /**
     * @brief writeArcSegment writes an arc segment to the path.
     * @param path: The path to modify.
     * @param start: The start point.
     * @param end: The end point.
     * @param center: The center point.
     * @param angle: The angle.
     * @param ccw: Whether the arc is counterclockwise.
     * @param width: The width.
     * @param height: The height.
     * @param speed: The speed.
     * @param acceleration: The acceleration.
     * @param extruder_speed: The extruder speed.
     * @param regionType: The region type.
     * @param pathModifiers: The path modifiers.
     * @param materialNumber: The material number.
     * @param extruders: The extruders.
     */
    static void writeArcSegment(Path& path, Point start, Point end, Point center, Angle angle, bool ccw, Distance width,
                                Distance height, Velocity speed, Acceleration acceleration,
                                AngularVelocity extruder_speed, RegionType regionType, PathModifiers pathModifiers,
                                int materialNumber, QVector<int> extruders);

    /**
     * @brief GenerateRamp generates a ramp path.
     * @param path: The path to modify.
     * @param segmentSplitted: Whether the segment is split.
     * @param segmentIndex: The segment index.
     * @param pathModifiers: The path modifiers.
     * @param rampLength: The ramp length.
     * @param speed: The speed.
     * @param extruderSpeed: The extruder speed.
     */
    static void GenerateRamp(Path& path, bool& segmentSplitted, int segmentIndex, PathModifiers pathModifiers,
                             Distance rampLength, Velocity speed, AngularVelocity extruderSpeed);

    //! \brief write tip wipe segments
    static void generateTipWipeSegment(Path& path, Point start, Point end, Distance width, Distance height,
                                       Velocity speed, Acceleration acceleration, AngularVelocity extruder_speed,
                                       RegionType regionType, PathModifiers pathModifiers, int materialNumber,
                                       QVector<int> extruders, Distance tipWipeCutoffDistance);

    //! \brief track the distance already covered
    static Distance tipWipeDistanceCovered;
};
} // namespace ORNL
