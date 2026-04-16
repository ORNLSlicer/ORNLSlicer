#include "utilities/qt_json_conversion.h"

#include <string>

#include <qhashfunctions.h>
#include <qmatrix4x4.h>
#include <qquaternion.h>
#include <qvectornd.h>

void to_json(fifojson& j, const QString& s) { j = s.toStdString(); }

void from_json(const fifojson& j, QString& s) { s = j.get<std::string>().c_str(); }

void to_json(fifojson& j, const QQuaternion& q) {
    QVector4D v = q.toVector4D();
    j = fifojson {{"w", v.w()}, {"x", v.x()}, {"y", v.y()}, {"z", v.z()}};
}

void from_json(const fifojson& j, QQuaternion& q) {
    q.setScalar(j["w"]);
    q.setX(j["x"]);
    q.setY(j["y"]);
    q.setZ(j["z"]);
}

void to_json(fifojson& j, const QVector3D& v) { j = fifojson {{"x", v.x()}, {"y", v.y()}, {"z", v.z()}}; }

void from_json(const fifojson& j, QVector3D& v) {
    v.setX(j["x"]);
    v.setY(j["y"]);
    v.setZ(j["z"]);
}

void to_json(fifojson& j, const QMatrix4x4& m) {
    j = fifojson::array();
    const float* data = m.data();

    for (int i = 0; i < 16; i++) {
        j[i] = data[i];
    }
}

void from_json(const fifojson& j, QMatrix4x4& m) {
    for (int i = 0; i < 16; i++) {
        m.data()[i] = j[i];
    }
}
