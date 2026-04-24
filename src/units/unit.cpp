#include "units/unit.h"

#include <math.h>

#include <qmath.h>

#include "exceptions/exceptions.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
void to_json(json& j, const Distance& d) { j = d(); }

void from_json(const json& j, Distance& d) { d = j.get<NT>(); }

void to_json(json& j, const Time& t) { j = t(); }

void from_json(const json& j, Time& t) { t = j.get<NT>(); }

void to_json(json& j, const Mass& m) { j = m(); }

void from_json(const json& j, Mass& m) { m = j.get<NT>(); }

void to_json(json& j, const Velocity& v) { j = v(); }

void from_json(const json& j, Velocity& v) { v = j.get<NT>(); }

void to_json(json& j, const Acceleration& a) { j = a(); }

void from_json(const json& j, Acceleration& a) { a = j.get<NT>(); }

void to_json(json& j, const Density& a) { j = a(); }

void from_json(const json& j, Density& a) { a = j.get<NT>(); }

void to_json(json& j, const Angle& a) { j = a(); }

void from_json(const json& j, Angle& a) { a = j.get<NT>(); }

void to_json(json& j, const AngularVelocity& v) { j = v(); }

void from_json(const json& j, AngularVelocity& v) { v = j.get<NT>(); }

void to_json(json& j, const AngularAcceleration& a) { j = a(); }

void from_json(const json& j, AngularAcceleration& a) { a = j.get<NT>(); }

void to_json(json& j, const Area& a) { j = a(); }

void from_json(const json& j, Area& a) { a = j.get<NT>(); }

void to_json(json& j, const Volume& v) { j = v(); }

void from_json(const json& j, Volume& v) { v = j.get<NT>(); }

void to_json(json& j, const Temperature& t) { j = t(); }

void from_json(const json& j, Temperature& t) { t = j.get<NT>(); }

void to_json(json& j, const Voltage& v) { j = v(); }

void from_json(const json& j, Voltage& v) { v = j.get<NT>(); }

void to_json(json& j, const Power& p) { j = p(); }

void from_json(const json& j, Power& p) { p = j.get<NT>(); }

Distance::Distance(NT value) : Unit<1, 0, 0, 0, 0, 0>(value) {}

Distance::Distance(const Unit<1, 0, 0, 0, 0, 0>& u) : Unit<1, 0, 0, 0, 0, 0>(u) {}

QString Distance::toString() {
    if (operator==(*this, in)) {
        return Constants::Units::kInch;
    }
    else if (operator==(*this, ft)) {
        return Constants::Units::kFeet;
    }
    else if (operator==(*this, mm)) {
        return Constants::Units::kMm;
    }
    else if (operator==(*this, cm)) {
        return Constants::Units::kCm;
    }
    else if (operator==(*this, m)) {
        return Constants::Units::kM;
    }
    else if (operator==(*this, micron)) {
        return Constants::Units::kMicron;
    }
    else if (operator==(*this, tensOfMicrons)) {
        return Constants::Units::kTensOfMicrons;
    }
    else {
        throw UnknownUnitException("Unknown distance unit");
    }
}

Distance Distance::fromString(QString str) {
    if (str == Constants::Units::kInch) {
        return in;
    }
    else if (str == Constants::Units::kFeet) {
        return ft;
    }
    else if (str == Constants::Units::kMm) {
        return mm;
    }
    else if (str == Constants::Units::kCm) {
        return cm;
    }
    else if (str == Constants::Units::kM) {
        return m;
    }
    else if (str == Constants::Units::kMicron) {
        return micron;
    }
    else {
        throw UnknownUnitException("Unknown distance unit");
    }
}

Time::Time(NT value) : Unit<0, 1, 0, 0, 0, 0>(value) {}

Time::Time(const Unit<0, 1, 0, 0, 0, 0>& u) : Unit<0, 1, 0, 0, 0, 0>(u) {}

QString Time::toString() {
    if (operator==(*this, s)) {
        return Constants::Units::kSecond;
    }
    else if (operator==(*this, ms)) {
        return Constants::Units::kMillisecond;
    }
    else if (operator==(*this, minute)) {
        return Constants::Units::kMinute;
    }
    else {
        throw UnknownUnitException("Unknown time unit");
    }
}

Time Time::fromString(QString str) {
    if (str == Constants::Units::kSecond) {
        return s;
    }
    else if (str == Constants::Units::kMillisecond) {
        return ms;
    }
    else if (str == Constants::Units::kMinute) {
        return minute;
    }
    else {
        throw UnknownUnitException("Unknown time unit");
    }
}

Mass::Mass(NT value) : Unit<0, 0, 1, 0, 0, 0>(value) {}

Mass::Mass(const Unit<0, 0, 1, 0, 0, 0>& u) : Unit<0, 0, 1, 0, 0, 0>(u) {}

QString Mass::toString() {
    if (operator==(*this, mg)) {
        return Constants::Units::kMg;
    }
    else if (operator==(*this, g)) {
        return Constants::Units::kG;
    }
    else if (operator==(*this, kg)) {
        return Constants::Units::kKg;
    }
    else if (operator==(*this, lbm)) {
        return Constants::Units::kLb;
    }
    else {
        throw UnknownUnitException("Unknown mass unit");
    }
}

Mass Mass::fromString(QString str) {
    if (str == Constants::Units::kMg) {
        return mg;
    }
    else if (str == Constants::Units::kG) {
        return g;
    }
    else if (str == Constants::Units::kKg) {
        return kg;
    }
    else if (str == Constants::Units::kLb) {
        return lbm;
    }
    else {
        throw UnknownUnitException("Unknown mass unit");
    }
}

Velocity::Velocity(NT value) : Unit<1, -1, 0, 0, 0, 0>(value) {}

Velocity::Velocity(const Unit<1, -1, 0, 0, 0, 0>& u) : Unit<1, -1, 0, 0, 0, 0>(u) {}

QString Velocity::toString() {
    if (operator==(*this, in / s)) {
        return Constants::Units::kInchPerSec;
    }
    else if (operator==(*this, in / minute)) {
        return Constants::Units::kInchPerMin;
    }
    else if (operator==(*this, ft / s)) {
        return Constants::Units::kFeetPerSec;
    }
    else if (operator==(*this, mm / s)) {
        return Constants::Units::kMmPerSec;
    }
    else if (operator==(*this, mm / minute)) {
        return Constants::Units::kMmPerMin;
    }
    else if (operator==(*this, cm / s)) {
        return Constants::Units::kCmPerSec;
    }
    else if (operator==(*this, m / s)) {
        return Constants::Units::kMPerSec;
    }
    else if (operator==(*this, micron / s)) {
        return Constants::Units::kMicronPerSec;
    }
    else {
        throw UnknownUnitException("Unknown velocity unit");
    }
}

Velocity Velocity::fromString(QString str) {
    if (str == Constants::Units::kInchPerSec) {
        return in / s;
    }
    if (str == Constants::Units::kInchPerMin) {
        return in / minute;
    }
    else if (str == Constants::Units::kFeetPerSec) {
        return ft / s;
    }
    else if (str == Constants::Units::kMmPerSec) {
        return mm / s;
    }
    else if (str == Constants::Units::kMmPerMin) {
        return mm / minute;
    }
    else if (str == Constants::Units::kCmPerSec) {
        return cm / s;
    }
    else if (str == Constants::Units::kMPerSec) {
        return m / s;
    }
    else if (str == Constants::Units::kMicronPerSec) {
        return micron / s;
    }
    else {
        throw UnknownUnitException("Unknown velocity unit");
    }
}

Acceleration::Acceleration(NT value) : Unit<1, -2, 0, 0, 0, 0>(value) {}

Acceleration::Acceleration(const Unit<1, -2, 0, 0, 0, 0>& u) : Unit<1, -2, 0, 0, 0, 0>(u) {}

QString Acceleration::toString() {
    Acceleration acc = mm / s / s;
    if (operator==(*this, in / s / s)) {
        return Constants::Units::kInchPerSec2;
    }
    else if (operator==(*this, ft / s / s)) {
        return Constants::Units::kFeetPerSec2;
    }
    else if (operator==(*this, mm / s / s)) {
        return Constants::Units::kMmPerSec2;
    }
    else if (operator==(*this, cm / s / s)) {
        return Constants::Units::kCmPerSec2;
    }
    else if (operator==(*this, m / s / s)) {
        return Constants::Units::kMPerSec2;
    }
    else if (operator==(*this, micron / s / s)) {
        return Constants::Units::kMicronPerSec2;
    }
    else {
        throw UnknownUnitException("Unknown acceleration unit");
    }
}

Acceleration Acceleration::fromString(QString str) {
    if (str == Constants::Units::kInchPerSec2) {
        return in / s / s;
    }
    else if (str == Constants::Units::kFeetPerSec2) {
        return ft / s / s;
    }
    else if (str == Constants::Units::kMmPerSec2) {
        return mm / s / s;
    }
    else if (str == Constants::Units::kCmPerSec2) {
        return cm / s / s;
    }
    else if (str == Constants::Units::kMPerSec2) {
        return m / s / s;
    }
    else if (str == Constants::Units::kMicronPerSec2) {
        return micron / s / s;
    }
    else {
        throw UnknownUnitException("Unknown acceleration unit");
    }
}

Density::Density(NT value) : Unit<-3, 0, 1, 0, 0, 0>(value) {}

Density::Density(const Unit<-3, 0, 1, 0, 0, 0>& u) : Unit<-3, 0, 1, 0, 0, 0>(u) {}

QString Density::toString() {
    if (operator==(*this, lbm / in / in / in)) {
        return Constants::Units::kLbPerInch3;
    }
    else if (operator==(*this, g / cm / cm / cm)) {
        return Constants::Units::kGPerCm3;
    }
    else {
        throw UnknownUnitException("Unknown density unit");
    }
}

Density Density::fromString(QString str) {
    if (str == Constants::Units::kLbPerInch3) {
        return lbm / in / in / in;
    }
    else if (str == Constants::Units::kGPerCm3) {
        return g / cm / cm / cm;
    }
    else {
        throw UnknownUnitException("Unknown density unit");
    }
}

Angle::Angle(NT value) : Unit<0, 0, 0, 0, 0, 1>(value) {
    // Wrap value
    while (m_value > Constants::Limits::Maximums::kMaxAngle) {
        m_value -= 2.0f * static_cast<float>(M_PI);
    }

    while (m_value < Constants::Limits::Minimums::kMinAngle) {
        m_value += 2.0f * static_cast<float>(M_PI);
    }
}

Angle::Angle(const Unit<0, 0, 0, 0, 0, 1>& u) : Unit<0, 0, 0, 0, 0, 1>(u) {}

QString Angle::toString() {
    if (operator==(*this, deg)) {
        return Constants::Units::kDegree;
    }
    else if (operator==(*this, rad)) {
        return Constants::Units::kRadian;
    }
    else if (operator==(*this, rev)) {
        return Constants::Units::kRevolution;
    }
    else {
        throw UnknownUnitException("Unknown angle unit");
    }
}

Angle Angle::fromString(QString str) {
    if (str == Constants::Units::kDegree) {
        return deg;
    }
    else if (str == Constants::Units::kRadian) {
        return rad;
    }
    else if (str == Constants::Units::kRevolution) {
        return rev;
    }
    else {
        throw UnknownUnitException("Unknown angle unit");
    }
}

double cos(const Angle& lhs) { return qCos(lhs()); }

double sin(const Angle& lhs) { return qSin(lhs()); }

double tan(const Angle& lhs) { return qTan(lhs()); }

Area::Area(NT value) : Unit<2, 0, 0, 0, 0, 0>(value) {}

Area::Area(Unit<2, 0, 0, 0, 0, 0> u) : Unit<2, 0, 0, 0, 0, 0>(u) {}

Temperature::Temperature(NT value) : Unit<0, 0, 0, 0, 1, 0>(value) {}

Temperature::Temperature(const Unit<0, 0, 0, 0, 1, 0>& u) : Unit<0, 0, 0, 0, 1, 0>(u) {}

NT Temperature::to(const Unit& u) const {
    NT offset = 0.0;
    if (u == degC)
        offset = -273.15f;
    else if (u == degF)
        offset = -459.67f;

    float retVal = (m_value / u()) + offset;
    retVal = QString::number(retVal, 'f', 3).toFloat();
    return retVal;
}

NT Temperature::from(const NT& value, const Unit& u) {
    NT result = 0.0;
    NT offset;
    if (u == degC) {
        offset = 273.15f;
        result = value + offset;
    }
    else if (u == degF) {
        offset = 459.67f;
        result = (value + offset) * degF();
    }
    else if (u == K) {
        offset = 0.0f;
        result = value;
    }
    m_value = QString::number(result, 'f', 3).toFloat();
    return m_value;
}

QString Temperature::toString() {
    if (operator==(*this, K)) {
        return Constants::Units::kKelvin;
    }
    else if (operator==(*this, degC)) {
        return Constants::Units::kCelsius;
    }
    else if (operator==(*this, degF)) {
        return Constants::Units::kFahrenheit;
    }
    else {
        throw UnknownUnitException("Unknown temperature unit");
    }
}

Temperature Temperature::fromString(QString str) {
    if (str == Constants::Units::kKelvin) {
        return K;
    }
    else if (str == Constants::Units::kCelsius) {
        return degC;
    }
    else if (str == Constants::Units::kFahrenheit) {
        return degF;
    }
    else {
        throw UnknownUnitException("Unknown temperature unit");
    }
}

Voltage::Voltage(NT value) : Unit<2, -3, 1, -1, 0, 0>(value) {}

Voltage::Voltage(const Unit<2, -3, 1, -1, 0, 0>& u) : Unit<2, -3, 1, -1, 0, 0>(u) {}

QString Voltage::toString() {
    if (operator==(*this, uV)) {
        return Constants::Units::kmuV;
    }
    else if (operator==(*this, mV)) {
        return Constants::Units::kmV;
    }
    else if (operator==(*this, V)) {
        return Constants::Units::kV;
    }
    else {
        throw UnknownUnitException("Unknown voltage unit");
    }
}

Voltage Voltage::fromString(QString str) {
    if (str == Constants::Units::kmuV) {
        return uV;
    }
    else if (str == Constants::Units::kmV) {
        return mV;
    }
    else if (str == Constants::Units::kV) {
        return V;
    }
    else {
        throw UnknownUnitException("Unknown voltage unit");
    }
}
} // namespace ORNL
