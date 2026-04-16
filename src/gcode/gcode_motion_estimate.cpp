#include "gcode/gcode_motion_estimate.h"

#include <math.h>

#include <qcontainerfwd.h>
#include <qmath.h>
#include <qminmax.h>
#include <qnumeric.h>

#include "units/unit.h"

namespace ORNL {
void MotionEstimation::Init() {
    // values in Slicer-1 GCodes.cs are in mm/s2, convert to micron/s2
    m_v_acceleration = Acceleration((1 / 0.5) * 1000000 * 25.4 / 162560 * 1000);
    m_xy_acceleration = Acceleration((1 / 0.1) * 1000000 * 25.4 / 162560 * 1000);

    m_current_acceleration = m_xy_acceleration;

    m_previous_distance = Distance(0);
    m_previous_x = 0;
    m_previous_y = 0;
    m_previous_z = 0;
    m_previous_w = 0;
    m_previous_e = 0;
    m_previous_vertical = true; // initialize for the very first move

    m_incomingV = max_xy_speed;
}

Distance MotionEstimation::calculateTimeAndVolume(int layer, bool isFIncluded, bool isGOCommand,
                                                  QVector<bool> extrudersOn, Time& G1F_time, Time& layer_time,
                                                  Volume& layer_volume, bool use_b) {
    // minimum distance to be considered move for estimate calculation
    double m_min_threshold = 10;

    Distance dx, dy, dz, dw, de;
    dx = m_current_x - m_previous_x;
    dy = m_current_y - m_previous_y;
    dz = m_current_z - m_previous_z;
    dw = m_current_w - m_previous_w;
    de = m_current_e - m_previous_e;

    // count E move time only when E move is a standalone move
    if (qAbs(de) > m_min_threshold) {
        if (qAbs(dx) < m_min_threshold && qAbs(dy) < m_min_threshold) {
            if (m_current_speed > 0) {
                Time t = MotionEstimation::getTravelTime(de, m_current_speed, m_xy_acceleration);
                layer_time += t;
                MotionEstimation::setVVector(0, 0, w_table_speed());

                return 0;
            }
        }
        de = m_current_e;
    }

    // if using B and de > 0, calculate time based on de and then return
    // this is because time estimate is based on extrusion time not motion time
    if (qAbs(de) > m_min_threshold && use_b) {
        if (m_current_speed > 0) {
            Time t = MotionEstimation::getTravelTime(de, m_current_speed, m_xy_acceleration);
            layer_time += t;
            MotionEstimation::setVVector(0, 0, w_table_speed());
            return 0;
        }
        de = m_current_e;
    }

    // a Table move is always a standalone move
    if (qAbs(dw) > m_min_threshold && qAbs(dx) < m_min_threshold && qAbs(dy) < m_min_threshold) {
        Time t = MotionEstimation::getTravelTime(dw, w_table_speed, m_v_acceleration);
        layer_time += t;
        if (isFIncluded)
            G1F_time += t;
        MotionEstimation::setVVector(0, 0, w_table_speed());
        m_previous_w = m_current_w;

        MotionEstimation::addPreviousXYDecelerationTime(layer_time);

        m_previous_vertical = true;
        return 0;
    }

    // Z move: non-zero dz
    if (qAbs(dz) > m_min_threshold && qAbs(dx) < m_min_threshold && qAbs(dy) < m_min_threshold) {
        if (isFIncluded)
            z_speed = m_current_speed;
        Time t = MotionEstimation::getTravelTime(dz, z_speed, m_v_acceleration);
        layer_time += t;
        if (isFIncluded)
            G1F_time += t;
        MotionEstimation::setVVector(0, 0, z_speed());
        m_previous_z = m_current_z;

        MotionEstimation::addPreviousXYDecelerationTime(layer_time);

        m_previous_vertical = true;
        return 0;
    }

    // the rest is for a predominantly XY move
    Distance length = sqrt(dx * dx + dy * dy + dz * dz);

    if (length > m_min_threshold) {
        // ToDo, make sure that max_xy_speed is only for G0 XY moves!
        if (isGOCommand) {
            m_current_speed = max_xy_speed;
        }

        if (m_current_speed != 0) {
            if (m_previous_vertical) {
                layer_time += MotionEstimation::firstXYMove(length, dx, dy, dz, G1F_time, isFIncluded);
            }
            else {
                // theta: the angle the travel trajectory changes from the previous segment to the current one
                //   0 means reverse
                //   pi means no change
                double theta = MotionEstimation::getTheta(length, dx, dy, dz);
                if (theta <= 0) {
                    Time t = length / m_current_speed;
                    layer_time += t;
                    if (isFIncluded)
                        G1F_time += t;
                }
                else if (qAbs(theta - M_PI) < 0.1) {
                    // Reverse: This happens primarily during infill paths
                    // treat it as 1st move, and calculate slowdown time from the previous move
                    // ToDo: 0.1 is an arbitrary value. Need to learn how the machine decides
                    //   on slowdown/parabolic blend
                    layer_time += MotionEstimation::firstXYMove(length, dx, dy, dz, G1F_time, isFIncluded);
                    MotionEstimation::addPreviousXYDecelerationTime(layer_time);
                }
                else {
                    // relatively smoother turn: consider a parabolic blend
                    layer_time += MotionEstimation::continuousXYMove(theta, length, dx, dy, dz, G1F_time, isFIncluded);
                }
            }
        }

        // count the number of extruders on
        int extruders_on_count = 0;
        for (bool ext_on : extrudersOn) {
            if (ext_on) {
                extruders_on_count++;
            }
        }

        // if any extruders are on, calculate the extruded volume, accounting for multiple extruders on simultaneously
        if (extruders_on_count > 0) {
            Distance width, height;

            if (layer == 0) {
                height = initialLayerThickness;
                width = layer0extrusionWidth - initialLayerThickness;
            }
            else {
                height = layerThickness;
                width = extrusionWidth - layerThickness;
            }

            // Calculate cross-sectional area of the bead
            // (width * height) is the rectangular body of the bead
            // (M_PI * height * height / 4.0) is the circular ends
            Area bead_area = (width * height) + (M_PI * height * height / 4.0);

            layer_volume += bead_area * length * extruders_on_count;
        }

        m_previous_distance = length;
        m_previous_x = m_current_x;
        m_previous_y = m_current_y;
        m_previous_z = m_current_z;
        m_previous_w = m_current_w;
        m_previous_e = m_current_e;
        m_previous_vertical = false;

        return length;
    }

    return 0;
}

void MotionEstimation::setVVector(float vx, float vy, float vz) {
    m_previous_vv = m_current_vv;
    m_current_vv = QVector3D(vx, vy, vz);
}

// theta: the angle the travel trajectory changes from the previous segment to the current one
//   0 means reverse
//   pi means no change
double MotionEstimation::getTheta(Distance d, Distance dx, Distance dy, Distance dz) {
    double cosTheta =
        (((m_previous_vv.x() * dx + m_previous_vv.y() * dy + m_previous_vv.z() * dz) / (d * m_previous_vv.length()))());

    // Applying the bound to make sure that machine roundoff error does not cause a NaN
    cosTheta = qMin(1.0, cosTheta);
    cosTheta = qMax(-1.0, cosTheta);
    double theta = qAcos(cosTheta);
    return theta;
}

Time MotionEstimation::getTravelTime(Distance d, Velocity v, Acceleration a) {
    Time total;
    d = abs(d);
    Time accelTime = v / a;
    Distance accelDist = v * v / (2.0 * a);
    if (d > 2.0 * accelDist) {
        total = 2 * accelTime + (d - 2.0 * accelDist) / v;
    }
    else {
        total = sqrt(2.0 * d / a);
    }

    return total;
}

// this is called when parsing a vertical move
// If the previous move was an XY move, then it must have slowed down for a full stop
//  and deceleration costs extra time
void MotionEstimation::addPreviousXYDecelerationTime(Time& layer_time) {
    if (!m_previous_vertical) {
        if (m_previous_acceleration == 0) {
            m_previous_acceleration = m_current_acceleration;
        }
        if (m_previous_acceleration != 0) {
            layer_time += m_incomingV / m_previous_acceleration;
        }
    }
}

// consider only the acceleration from 0-speed
Time MotionEstimation::firstXYMove(Distance d, Distance dx, Distance dy, Distance dz, Time& G1F_time,
                                   bool isFIncluded) {
    Time time = 0;
    Time accelTime = m_current_speed / m_current_acceleration;
    Distance accelDist = 0.5 * m_current_acceleration * accelTime * accelTime;
    if (d > accelDist) {
        time = accelTime + (d - accelDist) / m_current_speed;
        m_incomingV = m_current_speed;
    }
    else {
        time = sqrt(2.0 * d / m_current_acceleration);
        m_incomingV = time * m_current_acceleration;
    }
    m_previous_vv.setX((m_incomingV * dx / d)());
    m_previous_vv.setY((m_incomingV * dy / d)());
    m_previous_vv.setZ((m_incomingV * dz / d)());

    // max_xy_speed is used in G0 XY travel
    if (isFIncluded && m_current_speed != max_xy_speed)
        G1F_time += d / m_current_speed;

    return time;
}

// consider the following
//  1. parabolic blend
//  2. time to decelerate from the previous move
//  3. time to accelerate for the current move
// Incoming velocity: Velocity m_incomingV and Vector3D m_previous_v, not m_previous_speed which is reserved for the
// previous Fxx parameter Current move: m_current_speed is still assumed as the speed
Time MotionEstimation::continuousXYMove(double theta, Distance d, Distance dx, Distance dy, Distance dz, Time& G1F_time,
                                        bool isFIncluded) {
    Time time = d / m_current_speed;
    if (isFIncluded)
        G1F_time += time;

    Time addTime = 0;
    // Consider two minor changes
    //   1. The arc trajectory saving over the two straight lines -- this shortens time
    //   2. If necessary, deceleration (previous segment) and acceleration (current segment) -- both increase time

    // set an arbitrary rule: the bypass length must be less than 1/3 of either path segment
    // if not, reduce the blend speed
    // In theory, if two speeds are not the same, the parabolic blend will not be an arc. For now just make it simple
    // To make it more accurate, it requires more machine specific and thus more precise parabolic blend curve

    // get the maximum arc radius based on the speed and the acceleration: a = V^2/R
    // the magnitude of the blendSpeed remains (the smaller of two speeds), while the acceleration changes the speed
    // vector direction
    Velocity blendSpeed = qMin(m_current_speed, m_incomingV);
    Distance blendRadius = blendSpeed * blendSpeed / m_current_acceleration;
    Distance byPassLength = blendRadius * qTan(0.5 * theta);

    // If the segment length is too small compared to the speed, the arc will cover beyond the segment
    // we assume that the bypass length is at most 1/3 of the shorter of the two segments
    // therefore a slow down might be necessary
    float bypassMaxRatio = 1.0 / 3.0; // arbitrary for now, should be printer specific
    Distance minDistance = qMin(d(), m_previous_distance());
    bool slowDown = byPassLength > minDistance * bypassMaxRatio;
    if (slowDown) {
        // if slow down is needed, calculate the new radius, blend speed and the bypass length
        blendRadius = bypassMaxRatio * minDistance / qTan(0.5 * theta);
        blendSpeed = sqrt(blendRadius * m_current_acceleration);
        byPassLength = blendRadius * qTan(0.5 * theta);
    }
    // parabolic blend saving: the added value is negative, representing its saving over the two straight line segments
    addTime += blendRadius * theta / blendSpeed - byPassLength / m_incomingV - byPassLength / m_current_speed;

    // There is also additional time to decelerate to blendSpeed in the incoming segment and
    //       acceleration from blendSpeed to outgoingV in the current segment

    // previous segment difference: add time if slow down is needed in the previous segment
    if (m_incomingV > blendSpeed) {
        // velocity must reduce from m_previous_f to blendSpeed, using deceleration = acceleration
        //   m_previous_f - a*t=blendSpeed:
        //       t=(m_previous_f-blendSpeed)/a
        //       distance_deceleration=(m_previous_f^2 - blendSpeed^2)/(2*accel)
        Time time_deceleration = (m_incomingV - blendSpeed) / m_current_acceleration;
        Distance distance_deceleration =
            (m_incomingV * m_incomingV - blendSpeed * blendSpeed) / (2.0 * m_current_acceleration);
        Time time_deceleration_orig = distance_deceleration / m_incomingV;
        addTime += time_deceleration - time_deceleration_orig;
    }

    // current segment difference: add time if speed up is necessary
    m_incomingV = m_current_speed;
    if (m_current_speed > blendSpeed) {
        // velocity must increase from blendSpeed to outgoingV, using accelRate
        //   blendSpeed + a*t = outgoingV  --> t=(outgoingV-blendSpeed)/a
        //   distance_acceleration=(outgoingV^2 - blendSpeed^2)/(2*accel)
        Time time_acceleration = (m_current_speed - blendSpeed) / m_current_acceleration;
        Distance distance_acceleration =
            (m_current_speed * m_current_speed - blendSpeed * blendSpeed) / (2.0 * m_current_acceleration);
        Time time_acceleration_orig = distance_acceleration / m_current_speed;

        // But the current segment may be too short fot it to gain to the full speed
        Distance remainingDistance = d - byPassLength;
        if (remainingDistance < distance_acceleration) {
            time_acceleration =
                (sqrt(blendSpeed * blendSpeed + 2 * m_current_acceleration * remainingDistance) - blendSpeed) /
                m_current_acceleration;
            time_acceleration_orig = remainingDistance / m_current_speed;

            // if so, outgoingVelocity is modified: incomingVelocity for the next move
            m_incomingV = blendSpeed + m_current_acceleration * time_acceleration;
        }
        addTime += time_acceleration - time_acceleration_orig;
    }
    m_previous_vv.setX((m_incomingV * dx / d)());
    m_previous_vv.setY((m_incomingV * dy / d)());
    m_previous_vv.setZ((m_incomingV * dz / d)());

    time = time + addTime;

    return time;
}

Acceleration MotionEstimation::m_v_acceleration;
Acceleration MotionEstimation::m_xy_acceleration;

Acceleration MotionEstimation::m_previous_acceleration;
Acceleration MotionEstimation::m_current_acceleration;

Distance MotionEstimation::m_previous_x;
Distance MotionEstimation::m_previous_y;
Distance MotionEstimation::m_previous_z;
Distance MotionEstimation::m_previous_w;
Distance MotionEstimation::m_previous_e;

Distance MotionEstimation::m_current_x;
Distance MotionEstimation::m_current_y;
Distance MotionEstimation::m_current_z;
Distance MotionEstimation::m_current_w;
Distance MotionEstimation::m_current_e;

Distance MotionEstimation::layerThickness;
Distance MotionEstimation::extrusionWidth;

Distance MotionEstimation::m_previous_distance;
Distance MotionEstimation::m_total_distance;
Distance MotionEstimation::m_printing_distance;
Distance MotionEstimation::m_travel_distance;

Distance MotionEstimation::initialLayerThickness;
Distance MotionEstimation::layer0extrusionWidth;

Velocity MotionEstimation::max_xy_speed;
Velocity MotionEstimation::w_table_speed;
Velocity MotionEstimation::z_speed;

Velocity MotionEstimation::m_current_speed;
Velocity MotionEstimation::m_incomingV;

// Actual velocty vectors considering acceleration and parabolic blend
QVector3D MotionEstimation::m_previous_vv;
QVector3D MotionEstimation::m_current_vv;

bool MotionEstimation::m_previous_vertical; // Z or W move
} // namespace ORNL
