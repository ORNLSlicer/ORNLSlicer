#pragma once

#include <QImage>
#include <QList>
#include <QObject>
#include <QPair>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QVector2D>
#include <QVector3D>
#include <QVector>
#include <functional>
#include <optional>
#include <stdexcept>
#include <typeinfo>

#include "utilities/enums.h"

class QWidget;

namespace ORNL {

class MainWindow;
class MeshBase;
class PartMetaItem;

using CaptureCallback = std::function<void(const QString&, QWidget*)>;

template <typename T>
T* requireNonNull(T* ptr, const QString& context) {
    if (ptr == nullptr) { throw std::runtime_error(("Null pointer for " + context).toStdString()); }
    return ptr;
}

template <typename T>
QSharedPointer<T> requireNonNull(const QSharedPointer<T>& ptr, const QString& context) {
    if (ptr.isNull()) { throw std::runtime_error(("Null pointer for " + context).toStdString()); }
    return ptr;
}

template <typename T>
T* requireChild(const QObject* parent, const QString& objectName = QString()) {
    const QObject* checkedParent = requireNonNull(parent, QString("parent of %1").arg(typeid(T).name()));
    T* child = objectName.isEmpty() ? checkedParent->findChild<T*>() : checkedParent->findChild<T*>(objectName);
    if (child == nullptr) {
        const QString target = objectName.isEmpty() ? QString::fromUtf8(typeid(T).name())
                                                    : QString("%1('%2')").arg(typeid(T).name(), objectName);
        throw std::runtime_error(("Missing child widget/object: " + target).toStdString());
    }
    return child;
}

QString toSlug(const QString& name);
QImage captureWidget(QWidget* widget);
void saveWidget(QWidget* widget, const QString& filePath);
void clearScene(MainWindow* window);
QSharedPointer<PartMetaItem> addScenePart(MainWindow* window, const QSharedPointer<MeshBase>& mesh, const QString& name,
                                          MeshType mt = MeshType::kBuild);
QVector3D offsetFrom(const QSharedPointer<PartMetaItem>& baseItem, float dx_mm, float dy_mm, float dz_mm = 0.0f);
void ensureSampleParts(MainWindow* window);
void setupCaptureCameras(MainWindow* window, const QVector2D& angle = QVector2D(-35.0f, -42.0f),
                         float zoomDelta = 1400.0f, std::optional<QVector3D> panTarget = std::nullopt);
void setGCodeLayerRange(MainWindow* window, int low, int high);
void sliceAndWait(MainWindow* window);
void sliceSampleParts(MainWindow* window);
QWidget* selectSettingsTab(MainWindow* window, int tabIndex);
QWidget* preparePreferencesWindow(MainWindow* window);
QWidget* prepareSliceDialog(MainWindow* w);
QWidget* prepareTemplateSaveDialog(MainWindow* w);
QWidget* prepareGcodeToS2CDialog(MainWindow* w);
QWidget* prepareSettingsFileCompareDialog(MainWindow* w);

void captureAllSceneDemos(MainWindow* window, const CaptureCallback& capture);
QVector<QPair<QString, QString>> captureAllGuiElements(MainWindow* window, const QString& outputDirectory,
                                                       bool includeGui, bool includeDemos);
QStringList captureAllMenuBarMenus(MainWindow* window, const QString& outputDirectory);
QStringList captureAllPreferencesTabs(MainWindow* window, const QString& outputDirectory);
QStringList captureAllSettingsCategories(MainWindow* window, const QString& outputDirectory,
                                         const QStringList& categoryFilters = {});

}  // namespace ORNL
