#pragma once

#include <qcontainerfwd.h>

#include "units/unit.h"

namespace ORNL {
class MotionEstimation {
  public:
    //! \brief Initilizes the neccesary parameters and sets to default values
    static void Init();

    //! \brief Calculate time and volume contribution from motion
    //! \param layer, current layer number
    //! \param isFIncluded, if the current command statement include velocity / speed
    //! \param isGOCommand, if the current command statement is the fast non extruding move (G0)
    //! \param extrudersOn, list of extruders and there state indicating on or off currently
    //! \param G1F_time, G1 commands execution time estimates
    //! \param layer_time, accumulated time estimate for the entire layer
    //! \param layer_volume, accumulated volume estimate for the entire layer
    //! \param use_b, if using B filament axis, time calculation is based on extrusion not X/Y/Z distance
    static Distance calculateTimeAndVolume(int layer, bool isFIncluded, bool isGOCommand, QVector<bool> extrudersOn,
                                           Time& G1F_time, Time& layer_time, Volume& layer_volume, bool use_b);

    static Acceleration m_v_acceleration;
    static Acceleration m_xy_acceleration;

    static Acceleration m_previous_acceleration;
    static Acceleration m_current_acceleration;

    static Distance m_previous_x;
    static Distance m_previous_y;
    static Distance m_previous_z;
    static Distance m_previous_w;
    static Distance m_previous_e;

    static Distance m_current_x;
    static Distance m_current_y;
    static Distance m_current_z;
    static Distance m_current_w;
    static Distance m_current_e;

    static Distance layerThickness;
    static Distance extrusionWidth;

    static Distance m_previous_distance;
    static Distance m_total_distance;
    static Distance m_printing_distance;
    static Distance m_travel_distance;

    static Distance initialLayerThickness;
    static Distance layer0extrusionWidth;

    static Velocity max_xy_speed;
    static Velocity w_table_speed;
    static Velocity z_speed;

    static Velocity m_current_speed;
    static Velocity m_incomingV;

  private:
    //! \brief Set direction vector from last motion
    //! \param vx Direction vector x component
    //! \param vy Direction vector y component
    //! \param vz Direction vector z component
    static void setVVector(float vx, float vy, float vz);

    //! \brief Calculate angle change from previous to current motion
    //! \param d Distance moved
    //! \param dx Change in distance x component
    //! \param dy Change in distance y component
    //! \param dz Change in distance z component
    static double getTheta(Distance d, Distance dx, Distance dy, Distance dz);

    //! \brief Calculate travel time
    //! \param d Distance
    //! \param v Velocity
    //! \param a Acceleration
    static Time getTravelTime(Distance d, Velocity v, Acceleration a);

    //! \brief Add deceleration time from previous horizontal movement
    //! \param layer_volume accumulated volume estimate for the entire layer
    static void addPreviousXYDecelerationTime(Time& layer_time);

    //! \brief Calculate time addition from first horizontal move
    //! \param d Distance moved
    //! \param dx Change in distance x component
    //! \param dy Change in distance y component
    //! \param dz Change in distance z component
    //! \param G1F_time G1 commands execution accumulated time estimates
    //! \param isFIncluded, if the current command statement include velocity / speed
    static Time firstXYMove(Distance d, Distance dx, Distance dy, Distance dz, Time& G1F_time, bool isFIncluded);

    //! \brief Calculate time addition from continuous horizontal move
    //! \param theta angle between the two motion segments
    //! \param d Distance moved
    //! \param dx Change in distance x component
    //! \param dy Change in distance y component
    //! \param dz Change in distance z component
    //! \param G1F_time G1 commands execution accumulated time estimates
    //! \param isFIncluded, if the current command statement include velocity / speed
    static Time continuousXYMove(double theta, Distance d, Distance dx, Distance dy, Distance dz, Time& G1F_time,
                                 bool isFIncluded);

    static bool m_previous_vertical; // Z or W move

    // Actual velocty vectors considering acceleration and parabolic blend
    static QVector3D m_previous_vv;
    static QVector3D m_current_vv;
};
} // namespace ORNL
