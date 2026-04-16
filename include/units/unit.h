#pragma once

#include <math.h>

#include <limits>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>

#include <QMetaType>
#include <QString>
#include <QtMath>

#include "utilities/qt_json_conversion.h"

using json = fifojson;

namespace ORNL {
using NT = double; //!< \typedef Number Type

/*!
 * \class Unit
 * \brief Unit aware variable
 */
template <int U1, int U2, int U3, int U4, int U5, int U6> class Unit {
  public:
    Unit(NT value_ = NT(0)) : m_value(value_) {}

    /*!
     * This turns the class into a function object that allows
     * the user to easily get at the value.
     */
    NT operator()() const {
#if UNITS_FUNC
    #warning("This function returns the internal value for the units class. Only use if this is exactly what you want.")
#endif
        return m_value;
    }
#define UNITS_FUNC true
    /*!
     * Helper function to get a text representation of the
     * object's dimensions.  It is static because the
     * representation is known at compile time.
     */
    static std::string dim(void) {
        std::stringstream s;
        s << "<" << U1 << "," << U2 << "," << U3 << "," << U4 << "," << U5 << "," << U6 << ">";
        return s.str();
    }

    //! Helper function for unit conversions.
    virtual NT to(const Unit& u) const {
        // round conversion factors to deal with numerical imprecision
        double retValue = m_value / u.m_value;
        retValue = QString::number(retValue, 'g', 6).toDouble();
        return retValue;
    }

    //! Turn a raw value into a unit from Unit 'u'.
    virtual NT from(const NT& value, const Unit& u) {
        m_value = value * u.m_value;
        return m_value;
    }

    Unit& operator=(const Unit& rhs) {
        m_value = rhs.m_value;
        return *this;
    }

    // Arithmetic operators
    Unit& operator+=(const Unit& rhs) {
        m_value += rhs.m_value;
        return *this;
    }

    Unit& operator-=(const Unit& rhs) {
        m_value -= rhs.m_value;
        return *this;
    }

    Unit& operator*=(const NT& rhs) {
        m_value *= rhs;
        return *this;
    }

    Unit& operator/=(const NT& rhs) {
        m_value /= rhs;
        return *this;
    }

  protected:
    NT m_value;
}; // class Unit

// Addition
template <int U1, int U2, int U3, int U4, int U5, int U6>
const Unit<U1, U2, U3, U4, U5, U6> operator+(const Unit<U1, U2, U3, U4, U5, U6>& lhs,
                                             const Unit<U1, U2, U3, U4, U5, U6>& rhs) {
    return Unit<U1, U2, U3, U4, U5, U6>(lhs() + rhs());
}

template <int U1, int U2, int U3, int U4, int U5, int U6>
const Unit<U1, U2, U3, U4, U5, U6> operator+(const Unit<U1, U2, U3, U4, U5, U6>& lhs, const NT& rhs) {
    return Unit<U1, U2, U3, U4, U5, U6>(lhs() + rhs);
}

template <int U1, int U2, int U3, int U4, int U5, int U6>
const Unit<U1, U2, U3, U4, U5, U6> operator+(const NT& lhs, const Unit<U1, U2, U3, U4, U5, U6>& rhs) {
    return Unit<U1, U2, U3, U4, U5, U6>(lhs + rhs());
}

// Subtraction
template <int U1, int U2, int U3, int U4, int U5, int U6>
const Unit<U1, U2, U3, U4, U5, U6> operator-(const Unit<U1, U2, U3, U4, U5, U6>& lhs,
                                             const Unit<U1, U2, U3, U4, U5, U6>& rhs) {
    return Unit<U1, U2, U3, U4, U5, U6>(lhs() - rhs());
}

template <int U1, int U2, int U3, int U4, int U5, int U6>
const Unit<U1, U2, U3, U4, U5, U6> operator-(const Unit<U1, U2, U3, U4, U5, U6>& lhs, const NT& rhs) {
    return Unit<U1, U2, U3, U4, U5, U6>(lhs() - rhs);
}

template <int U1, int U2, int U3, int U4, int U5, int U6>
const Unit<U1, U2, U3, U4, U5, U6> operator-(const NT& lhs, const Unit<U1, U2, U3, U4, U5, U6>& rhs) {
    return Unit<U1, U2, U3, U4, U5, U6>(lhs - rhs());
}

// Modulo
template <int U1, int U2, int U3, int U4, int U5, int U6>
const Unit<U1, U2, U3, U4, U5, U6> operator%(const Unit<U1, U2, U3, U4, U5, U6>& lhs,
                                             const Unit<U1, U2, U3, U4, U5, U6>& rhs) {
    return Unit<U1, U2, U3, U4, U5, U6>(fmod(lhs(), rhs()));
}

template <int U1, int U2, int U3, int U4, int U5, int U6>
const Unit<U1, U2, U3, U4, U5, U6> operator%(const Unit<U1, U2, U3, U4, U5, U6>& lhs, const NT& rhs) {
    return Unit<U1, U2, U3, U4, U5, U6>(fmod(lhs(), rhs));
}

template <int U1, int U2, int U3, int U4, int U5, int U6>
const Unit<U1, U2, U3, U4, U5, U6> operator%(const NT& lhs, const Unit<U1, U2, U3, U4, U5, U6>& rhs) {
    return Unit<U1, U2, U3, U4, U5, U6>(fmod(lhs, rhs()));
}

// Negation
template <int U1, int U2, int U3, int U4, int U5, int U6>
const Unit<U1, U2, U3, U4, U5, U6> operator-(const Unit<U1, U2, U3, U4, U5, U6>& rhs) {
    return Unit<U1, U2, U3, U4, U5, U6>(-rhs());
}

// Multiplication
template <int U1, int U2, int U3, int U4, int U5, int U6>
const Unit<U1, U2, U3, U4, U5, U6> operator*(const NT& lhs, const Unit<U1, U2, U3, U4, U5, U6>& rhs) {
    return Unit<U1, U2, U3, U4, U5, U6>(lhs * rhs());
}

template <int U1, int U2, int U3, int U4, int U5, int U6>
const Unit<U1, U2, U3, U4, U5, U6> operator*(const Unit<U1, U2, U3, U4, U5, U6>& lhs, const NT& rhs) {
    return rhs * lhs;
}

template <int U1a, int U2a, int U3a, int U4a, int U5a, int U6a, int U1b, int U2b, int U3b, int U4b, int U5b, int U6b>
const Unit<U1a + U1b, U2a + U2b, U3a + U3b, U4a + U4b, U5a + U5b, U6a + U6b>
operator*(const Unit<U1a, U2a, U3a, U4a, U5a, U6a>& lhs, const Unit<U1b, U2b, U3b, U4b, U5b, U6b>& rhs) {
    return Unit<U1a + U1b, U2a + U2b, U3a + U3b, U4a + U4b, U5a + U5b, U6a + U6b>(lhs() * rhs());
}

// Division
template <int U1, int U2, int U3, int U4, int U5, int U6>
const Unit<U1, U2, U3, U4, U5, U6> operator/(const Unit<U1, U2, U3, U4, U5, U6>& lhs, const NT& rhs) {
    return Unit<U1, U2, U3, U4, U5, U6>(lhs() / rhs);
}

template <int U1, int U2, int U3, int U4, int U5, int U6>
const Unit<-U1, -U2, -U3, -U4, -U5, -U6> operator/(const NT& lhs, const Unit<U1, U2, U3, U4, U5, U6>& rhs) {
    return Unit<-U1, -U2, -U3, -U4, -U5, -U6>(lhs / rhs());
}

template <int U1a, int U2a, int U3a, int U4a, int U5a, int U6a, int U1b, int U2b, int U3b, int U4b, int U5b, int U6b>
const Unit<U1a - U1b, U2a - U2b, U3a - U3b, U4a - U4b, U5a - U5b, U6a - U6b>
operator/(const Unit<U1a, U2a, U3a, U4a, U5a, U6a>& lhs, const Unit<U1b, U2b, U3b, U4b, U5b, U6b>& rhs) {
    return Unit<U1a - U1b, U2a - U2b, U3a - U3b, U4a - U4b, U5a - U5b, U6a - U6b>(lhs() / rhs());
}

// Comparisons
template <int U1, int U2, int U3, int U4, int U5, int U6>
bool operator==(const Unit<U1, U2, U3, U4, U5, U6>& lhs, const Unit<U1, U2, U3, U4, U5, U6>& rhs) {
    // return fabs(lhs() - rhs()) < std::numeric_limits< NT >::epsilon();
    // std::numeric_limits< Double >::epsilon() = 1e-16
    // and this stringent comparison results in a crash if app.references does not match exactly
    // Picked a threshold of 1e-7 so that is is less than the difference between degC and K
    //     const Temperature degC = 1 + 1e-6 K
    return fabs(lhs() - rhs()) / rhs() < 1.0e-7;
}

template <int U1, int U2, int U3, int U4, int U5, int U6>
bool operator==(const Unit<U1, U2, U3, U4, U5, U6>& lhs, const NT& rhs) {
    return fabs(lhs() - rhs) < std::numeric_limits<NT>::epsilon();
}

template <int U1, int U2, int U3, int U4, int U5, int U6>
bool operator==(const NT& lhs, const Unit<U1, U2, U3, U4, U5, U6>& rhs) {
    return fabs(lhs - rhs()) < std::numeric_limits<NT>::epsilon();
}

template <int U1, int U2, int U3, int U4, int U5, int U6>
bool operator!=(const Unit<U1, U2, U3, U4, U5, U6>& lhs, const Unit<U1, U2, U3, U4, U5, U6>& rhs) {
    return !(lhs() == rhs());
}

template <int U1, int U2, int U3, int U4, int U5, int U6>
bool operator!=(const Unit<U1, U2, U3, U4, U5, U6>& lhs, const NT& rhs) {
    return !(lhs() == rhs);
}

template <int U1, int U2, int U3, int U4, int U5, int U6>
bool operator!=(const NT& lhs, const Unit<U1, U2, U3, U4, U5, U6>& rhs) {
    return !(lhs == rhs());
}

// Ordering
template <int U1, int U2, int U3, int U4, int U5, int U6>
bool operator<=(const Unit<U1, U2, U3, U4, U5, U6>& lhs, const Unit<U1, U2, U3, U4, U5, U6>& rhs) {
    return lhs() <= rhs() || lhs == rhs;
}

template <int U1, int U2, int U3, int U4, int U5, int U6>
bool operator<=(const Unit<U1, U2, U3, U4, U5, U6>& lhs, const NT& rhs) {
    return (lhs() <= rhs) || lhs == rhs;
}

template <int U1, int U2, int U3, int U4, int U5, int U6>
bool operator<=(const NT& lhs, const Unit<U1, U2, U3, U4, U5, U6>& rhs) {
    return (lhs <= rhs()) || lhs == rhs;
}

template <int U1, int U2, int U3, int U4, int U5, int U6>
bool operator>=(const Unit<U1, U2, U3, U4, U5, U6>& lhs, const Unit<U1, U2, U3, U4, U5, U6>& rhs) {
    return lhs() >= rhs() || lhs == rhs;
}

template <int U1, int U2, int U3, int U4, int U5, int U6>
bool operator>=(const Unit<U1, U2, U3, U4, U5, U6>& lhs, const NT& rhs) {
    return (lhs() >= rhs) || lhs == rhs;
}

template <int U1, int U2, int U3, int U4, int U5, int U6>
bool operator>=(const NT& lhs, const Unit<U1, U2, U3, U4, U5, U6>& rhs) {
    return (lhs >= rhs()) || lhs == rhs;
}

template <int U1, int U2, int U3, int U4, int U5, int U6>
bool operator<(const Unit<U1, U2, U3, U4, U5, U6>& lhs, const Unit<U1, U2, U3, U4, U5, U6>& rhs) {
    return lhs() < rhs();
}

template <int U1, int U2, int U3, int U4, int U5, int U6>
bool operator<(const Unit<U1, U2, U3, U4, U5, U6>& lhs, const NT& rhs) {
    return (lhs() < rhs);
}

template <int U1, int U2, int U3, int U4, int U5, int U6>
bool operator<(const NT& lhs, const Unit<U1, U2, U3, U4, U5, U6>& rhs) {
    return (lhs < rhs());
}

template <int U1, int U2, int U3, int U4, int U5, int U6>
bool operator>(const Unit<U1, U2, U3, U4, U5, U6>& lhs, const Unit<U1, U2, U3, U4, U5, U6>& rhs) {
    return lhs() > rhs();
}

template <int U1, int U2, int U3, int U4, int U5, int U6>
bool operator>(const Unit<U1, U2, U3, U4, U5, U6>& lhs, const NT& rhs) {
    return (lhs() > rhs);
}

template <int U1, int U2, int U3, int U4, int U5, int U6>
bool operator>(const NT& lhs, const Unit<U1, U2, U3, U4, U5, U6>& rhs) {
    return (lhs > rhs());
}

template <int U1, int U2, int U3, int U4, int U5, int U6>
std::ostream& operator<<(std::ostream& s, const Unit<U1, U2, U3, U4, U5, U6>& rhs) {
    return s << rhs();
}

// math operations

template <int U1, int U2, int U3, int U4, int U5, int U6>
Unit<U1, U2, U3, U4, U5, U6> max(const Unit<U1, U2, U3, U4, U5, U6>& lhs, const Unit<U1, U2, U3, U4, U5, U6>& rhs) {
    return std::max(lhs(), rhs());
}

template <int U1, int U2, int U3, int U4, int U5, int U6, typename... Args>
typename std::enable_if<1 < sizeof...(Args), Unit<U1, U2, U3, U4, U5, U6>>::type
max(const Unit<U1, U2, U3, U4, U5, U6>& lhs, Args... rhs) {
    return ORNL::max(lhs, ORNL::max(rhs...));
}

template <int U1, int U2, int U3, int U4, int U5, int U6>
Unit<U1, U2, U3, U4, U5, U6> min(const Unit<U1, U2, U3, U4, U5, U6>& lhs, const Unit<U1, U2, U3, U4, U5, U6>& rhs) {
    return std::min(lhs(), rhs());
}

template <int U1, int U2, int U3, int U4, int U5, int U6, typename... Args>
typename std::enable_if<1 < sizeof...(Args), Unit<U1, U2, U3, U4, U5, U6>>::type
min(const Unit<U1, U2, U3, U4, U5, U6>& lhs, Args... rhs) {
    return ORNL::min(lhs, ORNL::min(rhs...));
}

template <int U1, int U2, int U3, int U4, int U5, int U6>
Unit<U1 / 2, U2 / 2, U3 / 2, U4 / 2, U5 / 2, U6 / 2> sqrt(const Unit<U1, U2, U3, U4, U5, U6>& lhs) {
    return std::sqrt(lhs());
}

template <int exponent, int U1, int U2, int U3, int U4, int U5, int U6>
Unit<U1 * exponent, U2 * exponent, U3 * exponent, U4 * exponent, U5 * exponent, U6 * exponent>
pow(const Unit<U1, U2, U3, U4, U5, U6>& base) {
    return std::pow(base(), exponent);
}

template <int U1, int U2, int U3, int U4, int U5, int U6>
Unit<U1, U2, U3, U4, U5, U6> abs(const Unit<U1, U2, U3, U4, U5, U6>& lhs) {
    return std::fabs(lhs());
}

/*!
 * \class Distance
 *
 * \brief Unit class for distance
 */
class Distance : public Unit<1, 0, 0, 0, 0, 0> {
  public:
    //! \brief Default Constructor
    Distance() = default;

    //! \brief Constructor
    Distance(NT value);

    //! \brief Conversion Constructor
    Distance(const Unit<1, 0, 0, 0, 0, 0>& u);

    //! \brief Returns the string representation of this unit (e.g. Inch)
    QString toString();

    //! \brief Returns the Distance object for the given string
    static Distance fromString(QString str);
};

void to_json(json& j, const Distance& d);

void from_json(const json& j, Distance& d);

/*!
 * \class Time
 *
 * \brief Unit class for time
 */
class Time : public Unit<0, 1, 0, 0, 0, 0> {
  public:
    //! \brief Default Constructor
    Time() = default;

    //! \brief Constructor
    Time(NT value);

    Time(const Unit<0, 1, 0, 0, 0, 0>& u);

    //! \brief Returns the string representation of this unit
    QString toString();

    //! \brief Returns the Distance object for the given string
    static Time fromString(QString str);
};

void to_json(json& j, const Time& t);

void from_json(const json& j, Time& t);

/*!
 * \class Mass
 *
 * \brief Unit class for mass
 */
class Mass : public Unit<0, 0, 1, 0, 0, 0> {
  public:
    //! \brief Default Constructor
    Mass() = default;

    //! \brief Constructor
    Mass(NT value);

    //! \brief Conversion Constructor
    Mass(const Unit<0, 0, 1, 0, 0, 0>& u);

    //! \brief Returns the string representation of this unit
    QString toString();

    //! \brief Returns the Distance object for the given string
    static Mass fromString(QString str);
};

void to_json(json& j, const Mass& m);

void from_json(const json& j, Mass& m);

// Typedefs for derived units
/*!
 * \class Velocity
 *
 * \brief Unit class for velocity
 */
class Velocity : public Unit<1, -1, 0, 0, 0, 0> {
  public:
    //! \brief Default Constructor
    Velocity() = default;

    //! \brief Constructor
    Velocity(NT value);

    //! \brief Conversion Constructor
    Velocity(const Unit<1, -1, 0, 0, 0, 0>& u);

    //! \brief Returns the string representation of this unit
    QString toString();

    //! \brief Returns the Distance object for the given string
    static Velocity fromString(QString str);
};

void to_json(json& j, const Velocity& v);

void from_json(const json& j, Velocity& v);

/*!
 * \class Acceleration
 *
 * \brief Unit class for acceleration
 */
class Acceleration : public Unit<1, -2, 0, 0, 0, 0> {
  public:
    //! \brief Default Constructor
    Acceleration() = default;

    //! \brief Constructor
    Acceleration(NT value);

    //! \brief Conversion Constructor
    Acceleration(const Unit<1, -2, 0, 0, 0, 0>& u);

    //! \brief Returns the string representation of this unit
    QString toString();

    //! \brief Returns the Distance object for the given string
    static Acceleration fromString(QString str);
};

void to_json(json& j, const Acceleration& a);

void from_json(const json& j, Acceleration& a);

/*!
 * \class Density
 *
 * \brief Unit class for density
 */
class Density : public Unit<-3, 0, 1, 0, 0, 0> {
  public:
    //! \brief Default Constructor
    Density() = default;

    //! \brief Constructor
    Density(NT value);

    //! \brief Conversion Constructor
    Density(const Unit<-3, 0, 1, 0, 0, 0>& u);

    //! \brief Returns the string representation of this unit
    QString toString();

    //! \brief Returns the density object for the given string
    static Density fromString(QString str);
};

void to_json(json& j, const Density& a);

void from_json(const json& j, Density& a);

/*!
 * \class Angle
 *
 * \brief Angle is a special unit that implements a wrap around feature
 * keeping it between 0 and 2 pi
 */
class Angle : public Unit<0, 0, 0, 0, 0, 1> {
  public:
    //! \brief Default Constructor
    Angle() = default;

    //! \brief Constructor
    Angle(NT value);

    //! \brief Conversion Constructor
    Angle(const Unit<0, 0, 0, 0, 0, 1>& u);

    //! \brief Returns the string representation of this unit (e.g. Degree)
    QString toString();

    //! \brief Returns the Distance object for the given string
    static Angle fromString(QString str);
};

void to_json(json& j, const Angle& a);

void from_json(const json& j, Angle& a);

class Temperature : public Unit<0, 0, 0, 0, 1, 0> {
  public:
    Temperature() = default;

    Temperature(NT value);

    Temperature(const Unit<0, 0, 0, 0, 1, 0>& u);

    QString toString();

    static Temperature fromString(QString str);

    NT to(const Unit& u) const;

    NT from(const NT& value, const Unit& u);
};

void to_json(json& j, const Temperature& t);

void from_json(const json& j, Temperature& t);

//! \typedef AngularVelocity
typedef Unit<0, -1, 0, 0, 0, 1> AngularVelocity;

void to_json(json& j, const AngularVelocity& v);

void from_json(const json& j, AngularVelocity& v);

//! \typedef AngularAcceleration
typedef Unit<0, -2, 0, 0, 0, 1> AngularAcceleration;

void to_json(json& j, const AngularAcceleration& a);

void from_json(const json& j, AngularAcceleration& a);

//! \typedef Area
class Area : public Unit<2, 0, 0, 0, 0, 0> {
  public:
    //! \brief Default Constructor
    Area() = default;

    //! \brief Constructor
    Area(NT value);

    //! \brief Conversion Constructor
    Area(Unit<2, 0, 0, 0, 0, 0> u);
};

void to_json(json& j, const Area& a);

void from_json(const json& j, Area& a);

/*!
 * \class Voltage
 *
 * \brief Unit class for voltage
 */
class Voltage : public Unit<2, -3, 1, -1, 0, 0> {
  public:
    //! \brief Default Constructor
    Voltage() = default;

    //! \brief Constructor
    Voltage(NT value);

    //! \brief Conversion Constructor
    Voltage(const Unit<2, -3, 1, -1, 0, 0>& u);

    //! \brief Returns the string representation of this unit (e.g. Inch)
    QString toString();

    //! \brief Returns the Distance object for the given string
    static Voltage fromString(QString str);
};

//! \typedef Volume
typedef Unit<3, 0, 0, 0, 0, 0> Volume;

void to_json(json& j, const Volume& v);

void from_json(const json& j, Volume& v);

typedef Unit<0, 0, 0, 1, 0, 0> Current;

typedef Unit<1, -3, 0, 0, 0, 0> Jerk;
typedef Unit<1, -2, 1, 0, 0, 0> Force;
typedef Unit<0, -1, 0, 0, 0, 0> Frequency;
typedef Unit<-1, -2, 1, 0, 0, 0> Pressure;
typedef Unit<2, -2, 1, 0, 0, 0> Torque;
typedef Unit<1, -2, 1, 0, 0, 0> Weight;
typedef Unit<2, -2, 1, 0, 0, 0> Energy;
typedef Unit<2, -2, 1, 0, 0, 0> Work;
typedef Unit<0, 1, 0, 1, 0, 0> Charge;

typedef Unit<2, -3, 1, 0, 0, 0> Power;

void to_json(json& j, const Power& v);

void from_json(const json& j, Power& v);

void to_json(json& j, const Voltage& v);

void from_json(const json& j, Voltage& v);

typedef Unit<2, -3, 1, -2, 0, 0> Resistance;
typedef Unit<-2, 3, -1, 2, 0, 0> Conductance;
typedef Unit<-2, 4, -1, 2, 0, 0> Capacitance;

//! \typedef Unitless
typedef Unit<0, 0, 0, 0, 0, 0> Unitless;

// Unit constants
const NT tera = 1e12f;
const NT giga = 1e9f;
const NT mega = 1e6f;
const NT kilo = 1e3f;
const NT deci = 1e-1f;
const NT centi = 1e-2f;
const NT milli = 1e-3f;
const NT micro = 1e-6f;
const NT nano = 1e-9f;
const NT pico = 1e-12f;
const NT femto = 1e-15f;
const NT atto = 1e-18f;
const Distance micron = 1.0f;
const Distance tensOfMicrons = micron * 0.1f;
const Distance m = mega * micron;
const Distance km = kilo * m;
const Distance cm = centi * m;
const Distance mm = milli * m;
const Distance in = 2.54f * cm;
const Distance inch = in;
const Distance inches = in;
const Distance ft = 12.0f * in;
const Distance foot = ft;
const Distance feet = ft;
const Area m2 = 1.0f * m * m;
const Area mm2 = 1.0f * mm * mm;
const Area cm2 = 1.0f * cm * cm;
const Area in2 = 1.0f * in * in;
const Area ft2 = 1.0f * ft * ft;
const Angle radian = 1.0f;
const Angle rad = 1.0f;
const Angle pi = static_cast<float>(M_PI) * rad;
const Angle deg = 1.74532925e-2f * rad;
const Angle degree = deg;
const Angle degrees = deg;
const Angle rev = 2.0f * pi;
const Mass kg = 1.0f;
const Mass g = milli * kg;
const Mass mg = milli * g;
const Mass lbm = 0.45359237f * kg;
const Time s = 1.0f;
const Time ms = milli * s;
const Time hr = 3600.0f * s;
const Time hour = hr;
const Time minute = 60.0f * s;
const Force N = 1.0f * kg * m / (s * s);
const Force lbf = 4.4482216f * N;
const Force oz = lbf / 16.0f;
const Force ounce = oz;
const Energy J = 1.0f * N * m;
const Energy cal = 4.1868f * J;
const Energy kcal = kilo * cal;
const Energy eV = 1.6021765e-19f * J;
const Energy keV = kilo * eV;
const Energy MeV = mega * eV;
const Energy GeV = giga * eV;
const Energy btu = 1055.0559f * J;
const Power W = 1.0f * J / s;
const Current A = 1.0f;
const Current MA = mega * A;
const Current kA = kilo * A;
const Current mA = milli * A;
const Current uA = micro * A;
const Current nA = nano * A;
const Current pA = pico * A;
const Charge C = 1.0f * A * s;
const Voltage V = 1.0f * J / C;
const Voltage uV = micro * V;
const Voltage mV = milli * V;
const Resistance ohm = 1.0f * V / A;
const Conductance S = 1.0f / ohm;
const Capacitance F = 1.0f * C / V;
const Capacitance pF = pico * F;
const Capacitance nF = nano * F;
const Capacitance uF = micro * F;
const Capacitance mF = milli * F;
const Pressure Pa = 1.0f;
const Pressure kPa = 1e3f * Pa;
const Pressure bar = Pa * 100000.0f;
const Pressure millibar = 1e-3f * bar;
const Pressure psi = lbf / (inch * inch);
const Pressure atm = 101325.0f * Pa;
const Temperature K = 1.0f;
// Since K and degC are the same factor with different offsets, the quick way to make it work in this
// system is to give degC a tiny difference in the factor. It's not enough to make noticble difference
// so it should work for now. Not that it isn't stupid/bad to do so.
const Temperature degC = 1.000001f * K;
const Temperature degF = 5.0f / 9.0f * K;

const Acceleration AccelerationOfGravity = 9.80665f * m / (s * s);
const Velocity SpeedOfLight = 2.9979246e8f * m / s;
const Velocity SpeedOfSound = 331.46f * m / s;

const Unitless none = 1.0f;

double cos(const Angle& lhs);
double sin(const Angle& lhs);
double tan(const Angle& lhs);
#undef UNITS_FUNC
#define UNITS_FUNC false

} // namespace ORNL
