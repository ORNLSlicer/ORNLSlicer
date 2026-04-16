#pragma once

#include <QMatrix4x4>
#include <QQuaternion>
#include <QString>
#include <QVector3D>
#include <QVector>
#include <nlohmann/json.hpp>

// Needed for cross compile - includes defintions that qt is looking
// for from internal forward declarations (e.g. tagMSG).
#ifdef Q_OS_WIN
    #include <QtCore/qt_windows.h>
#endif

// using json = nlohmann::json;
using fifojson = nlohmann::ordered_json;

//! \brief Function for going from json to QString
void to_json(fifojson& j, const QString& s);
//! \brief Function for going from QString to json
void from_json(const fifojson& j, QString& s);

//! \brief Function for going from json to QQuaternion
void to_json(fifojson& j, const QQuaternion& q);
//! \brief Function for going from QQuaternion to json
void from_json(const fifojson& j, QQuaternion& q);

//! \brief Function for going from json to QVector3D
void to_json(fifojson& j, const QVector3D& v);
//! \brief Function for going from QVector3D to json
void from_json(const fifojson& j, QVector3D& v);

//! \brief To Json for QMatrix4x4.
void to_json(fifojson& j, const QMatrix4x4& m);
//! \brief From Json for QMatrix4x4.
void from_json(const fifojson& j, QMatrix4x4& m);
