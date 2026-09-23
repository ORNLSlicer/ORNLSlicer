#pragma once

#include <cmath>
#include <iostream>
#include <string>

#include "geometry/point.h"

namespace ORNL {
//! \brief Commonly used testing functions
namespace Testing {

/**
 * @brief Asserts that a condition is true, printing an error message to std::cerr on failure.
 * @param condition The boolean condition expected to be true.
 * @param message   The error message to print if the condition is false.
 * @return True if condition is true, false otherwise.
 */
inline bool expect(bool condition, const char* message) {
    if (!condition) std::cerr << message << '\n';
    return condition;
}

/**
 * @brief Asserts that a condition is true, printing an error message to std::cerr on failure.
 * @param condition The boolean condition expected to be true.
 * @param message   The error message to print if the condition is false.
 * @return True if condition is true, false otherwise.
 */
inline bool expect(bool condition, const std::string& message) {
    if (!condition) std::cerr << message << '\n';
    return condition;
}

/**
 * @brief Checks if two values are within a given tolerance.
 * @param actual    The value being tested.
 * @param expected  The expected target value.
 * @param tolerance The maximum acceptable difference (defaults to 1.0e-4).
 * @return True if |actual - expected| <= tolerance.
 */
inline bool near(double actual, double expected, double tolerance = 1.0e-4) {
    return std::abs(actual - expected) <= tolerance;
}

/**
 * @brief Checks if a 2D point's coordinates match expected values within tolerance.
 * @param point     The point to test.
 * @param x         The expected X coordinate.
 * @param y         The expected Y coordinate.
 * @param tolerance The maximum acceptable difference for each coordinate (defaults to 1.0e-4).
 * @return True if both coordinates match within tolerance, false otherwise.
 */
inline bool near2DPoint(const Point& point, double x, double y, double tolerance = 1.0e-4) {
    return near(point.x(), x, tolerance) && near(point.y(), y, tolerance);
}

/**
 * @brief Checks if a 3D point's coordinates match expected values within tolerance.
 * @param point     The point to test.
 * @param x         The expected X coordinate.
 * @param y         The expected Y coordinate.
 * @param z         The expected Z coordinate.
 * @param tolerance The maximum acceptable difference for each coordinate (defaults to 1.0e-4).
 * @return True if all three coordinates match within tolerance, false otherwise.
 */
inline bool near3DPoint(const Point& point, double x, double y, double z, double tolerance = 1.0e-4) {
    return near(point.x(), x, tolerance) && near(point.y(), y, tolerance) && near(point.z(), z, tolerance);
}

}  // namespace Testing
}  // namespace ORNL
