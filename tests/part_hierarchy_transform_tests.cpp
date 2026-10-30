#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QImage>
#include <QStandardPaths>
#include <QSurfaceFormat>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QThread>
#include <QUndoStack>
#include <cmath>
#include <cstdlib>
#include <iostream>

#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/mesh_factory.h"
#include "graphics/objects/arrow_object.h"
#include "managers/preferences_manager.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "test_utils.h"
#include "utilities/constants.h"
#include "widgets/gcode_widget.h"
#include "widgets/part_widget/part_widget.h"
#include "windows/main_window.h"

namespace {
bool sameTransform(const QMatrix4x4& actual, const QMatrix4x4& expected, float tolerance = 0.0001f) {
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            if (std::fabs(actual(row, column) - expected(row, column)) > tolerance) return false;
        }
    }
    return true;
}

QSharedPointer<ORNL::Part> makePart(const QString& name, ORNL::MeshType type) {
    auto mesh = QSharedPointer<ORNL::ClosedMesh>::create(ORNL::MeshFactory::CreateBoxMesh(20000, 15000, 10000));
    mesh->setName(name);
    mesh->setType(type);
    return QSharedPointer<ORNL::Part>::create(mesh, QString(), type);
}
}  // namespace

int main(int argc, char* argv[]) {
    QTemporaryDir temp_dir;
    if (!ORNL::Testing::expect(temp_dir.isValid(), "Could not create isolated GUI test storage.")) return EXIT_FAILURE;
    qputenv("XDG_CONFIG_HOME", (temp_dir.path() + "/config").toUtf8());
    qputenv("XDG_DATA_HOME", (temp_dir.path() + "/data").toUtf8());
    qputenv("XDG_CACHE_HOME", (temp_dir.path() + "/cache").toUtf8());
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", qEnvironmentVariableIsEmpty("DISPLAY") ? "offscreen" : "xcb");
    }
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    if (QApplication::platformName() == "offscreen" || QApplication::platformName() == "minimal") {
        std::cerr << "A window-system OpenGL context is required for the part hierarchy GUI regression.\n";
        return 77;
    }
    Q_INIT_RESOURCE(configs);
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(styles);
    Q_INIT_RESOURCE(shaders);
    Q_INIT_RESOURCE(textures);

    auto preferences = ORNL::PreferencesManager::getInstance();
    preferences->setFileShiftPreference(ORNL::PreferenceChoice::kSkipAutomatically);
    preferences->setShouldAlwaysDrop(false);
    auto global = ORNL::GSM->getGlobal();
    for (const auto& key : {ORNL::PRS::Dimensions::kXMax, ORNL::PRS::Dimensions::kYMax, ORNL::PRS::Dimensions::kZMax}) {
        global->setSetting(key, 300000.0);
    }

    ORNL::PartWidget widget;
    auto* view = widget.view();
    view->setParent(&widget);
    QSurfaceFormat format;
    format.setVersion(4, 4);
    format.setProfile(QSurfaceFormat::CoreProfile);
    view->setFormat(format);
    widget.resize(1000, 800);
    view->resize(widget.size());
    view->lower();
    widget.show();
    view->show();
    QElapsedTimer timer;
    timer.start();
    while (!view->isValid() && timer.elapsed() < 5000) {
        app.processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(10);
    }
    if (!view->isValid()) {
        std::cerr << "An OpenGL context is unavailable for the part hierarchy GUI regression.\n";
        return 77;
    }

    auto parent     = makePart("parent", ORNL::MeshType::kBuild);
    auto child      = makePart("child", ORNL::MeshType::kSettings);
    auto grandchild = makePart("grandchild", ORNL::MeshType::kClipping);
    for (const auto& part : {parent, child, grandchild}) {
        ORNL::CSM->addPart(part, false);
        widget.add(part);
    }
    auto model           = widget.getPartMeta();
    auto parent_meta     = model->lookupByPointer(parent);
    auto child_meta      = model->lookupByPointer(child);
    auto grandchild_meta = model->lookupByPointer(grandchild);
    parent_meta->setTranslation(QVector3D(0.7f, 0.7f, 0.1f));
    child_meta->setTranslation(QVector3D(1.0f, 0.8f, 0.1f));
    grandchild_meta->setTranslation(QVector3D(1.1f, 0.9f, 0.1f));
    parent_meta->adoptChild(child_meta);
    child_meta->adoptChild(grandchild_meta);
    child_meta->setRotation(QQuaternion::fromEulerAngles(0, 0, 20));

    bool passed       = true;
    const auto expect = [&passed](bool condition, const QString& message) {
        passed &= ORNL::Testing::expect(condition, message.toStdString());
    };
    expect(child->parent() == parent && parent->children().contains(child),
           "GUI adoption must update the hierarchy saved by the Part objects.");
    expect(grandchild->parent() == child && child->children().contains(grandchild),
           "Nested mixed-role GUI adoption must update the saved hierarchy.");
    const auto checkSynchronized = [&](const QString& stage) {
        expect(child_meta->graphicsPart()->parent() == parent_meta->graphicsPart() &&
                   grandchild_meta->graphicsPart()->parent() == child_meta->graphicsPart(),
               stage + ": graphics must retain the complete hierarchy.");
        for (const auto& item : {parent_meta, child_meta, grandchild_meta}) {
            expect(sameTransform(item->transformation(), item->graphicsPart()->transformation()),
                   stage + ": metadata must match the visible transform for " + item->part()->name());
        }
        const auto checkPartTransforms = [&](const QString& operation) {
            for (const auto& item : {parent_meta, child_meta, grandchild_meta}) {
                QMatrix4x4 expected = item->graphicsPart()->transformation();
                for (int axis = 0; axis < 3; ++axis) expected(axis, 3) *= ORNL::Constants::OpenGL::kViewToObject;
                expect(sameTransform(item->part()->rootMesh()->transformation(), expected, 0.05f),
                       stage + ": " + operation + " geometry must match the visible transform for " +
                           item->part()->name());
            }
        };
        widget.updatePartTransformations();
        checkPartTransforms("saved");
        expect(widget.parts().size() == 3, "Slicing must retain all three hierarchy members.");
        checkPartTransforms("sliced");
    };
    const auto screenshot = [&](const QString& name) {
        if (argc <= 1) return;
        QDir().mkpath(QString::fromLocal8Bit(argv[1]));
        app.processEvents();
        expect(view->grabFramebuffer().save(QString::fromLocal8Bit(argv[1]) + "/" + name + "-scene.png"),
               "Could not save the rendered hierarchy screenshot: " + name);
        expect(widget.grab().save(QString::fromLocal8Bit(argv[1]) + "/" + name + ".png"),
               "Could not save the hierarchy screenshot: " + name);
    };
    parent_meta->setSelected(true);
    screenshot("before");
    parent_meta->setTranslation(QVector3D(1.2f, 1.0f, 0.3f));
    expect((child_meta->graphicsPart()->translation() - QVector3D(1.5f, 1.1f, 0.3f)).length() < 0.0001f,
           "A numeric parent translation must move its child by the same offset.");
    checkSynchronized("numeric translation");
    parent_meta->setRotation(QQuaternion::fromEulerAngles(0, 0, 35));
    checkSynchronized("numeric rotation");
    parent_meta->setScale(QVector3D(1.5f, 0.75f, 1.25f));
    checkSynchronized("numeric scale");
    screenshot("after-numeric-transforms");
    view->centerSelectedParts();
    checkSynchronized("center group");
    view->dropSelectedParts();
    checkSynchronized("drop group");
    screenshot("after-center-and-drop");

    QFile source(temp_dir.filePath("hierarchy-source.stl"));
    const QByteArray stl_source = R"(solid hierarchy
facet normal 0 0 -1
outer loop
vertex 0 0 0
vertex 0 2 0
vertex 1 0 0
endloop
endfacet
facet normal 0 -1 0
outer loop
vertex 0 0 0
vertex 1 0 0
vertex 0 0 3
endloop
endfacet
facet normal -1 0 0
outer loop
vertex 0 0 0
vertex 0 0 3
vertex 0 2 0
endloop
endfacet
facet normal 1 1 1
outer loop
vertex 1 0 0
vertex 0 2 0
vertex 0 0 3
endloop
endfacet
endsolid hierarchy
)";
    expect(source.open(QIODevice::WriteOnly) && source.write(stl_source) == stl_source.size(),
           "Could not write the reload/replace hierarchy fixture.");
    source.close();
    for (const auto& item : {parent_meta, child_meta}) {
        for (bool replace : {false, true}) {
            const QString stage = (replace ? "replace " : "reload ") + item->part()->name();
            auto old_graphics   = item->graphicsPart();
            auto old_parent     = old_graphics->parent();
            auto descendant_graphics =
                item == parent_meta ? child_meta->graphicsPart() : grandchild_meta->graphicsPart();
            auto old_arrow             = descendant_graphics->arrow();
            const auto pose            = item->transformation();
            const auto child_pose      = child_meta->transformation();
            const auto grandchild_pose = grandchild_meta->transformation();
            bool reloaded              = false;
            const auto connection =
                QObject::connect(ORNL::CSM.get(), &ORNL::SessionManager::partReloaded, &widget,
                                 [&](QSharedPointer<ORNL::PartMetaItem> changed) { reloaded |= changed == item; });
            if (replace)
                ORNL::CSM->replacePart(item, source.fileName());
            else {
                item->part()->setSourceFile(source.fileName());
                ORNL::CSM->reloadPart(item);
            }
            timer.restart();
            while (!reloaded && timer.elapsed() < 5000) {
                app.processEvents(QEventLoop::AllEvents, 50);
                QThread::msleep(10);
            }
            QObject::disconnect(connection);
            expect(reloaded && item->graphicsPart() != old_graphics, stage + ": source mesh must be reloaded.");
            expect(!old_parent->children().contains(old_graphics) && old_graphics->parent().isNull(),
                   stage + ": discarded graphics must be removed from their actual parent.");
            expect(!descendant_graphics->children().contains(old_arrow) && old_arrow->parent().isNull(),
                   stage + ": discarded parenting arrows must not remain in the rendered hierarchy.");
            expect(sameTransform(item->transformation(), pose) &&
                       sameTransform(child_meta->transformation(), child_pose) &&
                       sameTransform(grandchild_meta->transformation(), grandchild_pose),
                   stage + ": all world poses must survive mesh replacement.");
            checkSynchronized(stage);
            const QVector3D offset(0.1f, 0.1f, 0.0f);
            item->setTranslation(item->translation() + offset);
            expect(
                (grandchild_meta->translation() - grandchild_pose.column(3).toVector3D() - offset).length() < 0.0001f,
                stage + ": subsequent movement must propagate to descendants.");
            checkSynchronized(stage + " movement");
        }
        auto old_graphics          = item->graphicsPart();
        auto old_parent            = old_graphics->parent();
        const auto pose            = item->transformation();
        const auto grandchild_pose = grandchild_meta->transformation();
        item->resetTransformation();
        expect(item->graphicsPart() != old_graphics && !old_parent->children().contains(old_graphics),
               "Reset must remove the replaced graphics node from its actual parent.");
        expect(sameTransform(grandchild_meta->transformation(),
                             item->transformation() * pose.inverted() * grandchild_pose),
               "Reset must preserve descendant poses relative to the reset object.");
        checkSynchronized("reset " + item->part()->name());
    }
    parent_meta->setSelected(true);
    screenshot("after-reload-replace-and-reset");

    widget.copy();
    widget.paste();
    for (const auto& original : {parent_meta, child_meta, grandchild_meta}) {
        auto copied_part = ORNL::CSM->getPart(original->part()->name() + "_copy");
        auto copied      = model->lookupByPointer(copied_part);
        expect(copied && sameTransform(copied->graphicsPart()->transformation(), original->transformation()),
               "Pasted hierarchy geometry must preserve the visible position, rotation, scale, and shear.");
    }

    parent_meta->orphanChild(child_meta);
    expect(child->parent().isNull() && !parent->children().contains(child),
           "GUI detachment must remove the saved hierarchy relationship.");
    child_meta->orphanChild(grandchild_meta);
    widget.clear();
    ORNL::CSM->clearParts();
    widget.hide();

    auto* window      = ORNL::MainWindow::getInstance();
    auto* undo_widget = window->findChild<ORNL::PartWidget*>("m_part_widget");
    auto* undo_stack  = window->findChild<QUndoStack*>();
    expect(undo_widget && undo_stack, "The main window must expose its part widget and undo stack.");
    if (!undo_widget || !undo_stack) {
        delete window;
        return EXIT_FAILURE;
    }
    preferences->setFileShiftPreference(ORNL::PreferenceChoice::kSkipAutomatically);
    preferences->setShouldAlwaysDrop(false);
    auto* undo_view = undo_widget->view();
    undo_view->setFormat(format);
    window->show();
    timer.restart();
    while (!undo_view->isValid() && timer.elapsed() < 5000) {
        app.processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(10);
    }
    expect(undo_view->isValid(), "The main-window undo regression requires a valid OpenGL context.");
    if (!undo_view->isValid()) {
        delete window;
        return EXIT_FAILURE;
    }

    auto* tabs         = window->findChild<QTabWidget*>("m_tab_widget");
    auto* gcode_widget = window->findChild<ORNL::GCodeWidget*>("m_gcode");
    expect(tabs && gcode_widget, "The main-window fixture must expose both graphics tabs.");
    if (!tabs || !gcode_widget) {
        delete window;
        return EXIT_FAILURE;
    }
    auto* gcode_view = gcode_widget->view();
    gcode_view->setFormat(format);
    tabs->setCurrentWidget(gcode_widget);
    timer.restart();
    while (!gcode_view->isValid() && timer.elapsed() < 5000) {
        app.processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(10);
    }
    expect(gcode_view->isValid(), "Selection updates require the GCode view's OpenGL context to be initialized.");
    if (!gcode_view->isValid()) {
        delete window;
        return EXIT_FAILURE;
    }
    tabs->setCurrentWidget(undo_widget);
    app.processEvents();

    auto undo_parent = makePart("undo-parent", ORNL::MeshType::kBuild);
    auto undo_child  = makePart("undo-child", ORNL::MeshType::kSettings);
    ORNL::CSM->addPart(undo_parent);
    ORNL::CSM->addPart(undo_child);
    auto undo_model       = undo_widget->getPartMeta();
    auto undo_parent_meta = undo_model->lookupByPointer(undo_parent);
    auto undo_child_meta  = undo_model->lookupByPointer(undo_child);
    undo_parent_meta->setTranslation(QVector3D(0.6f, 0.6f, 0.1f));
    undo_child_meta->setTranslation(QVector3D(0.9f, 0.7f, 0.1f));
    undo_parent_meta->adoptChild(undo_child_meta);
    undo_stack->clear();
    const QMatrix4x4 initial_parent = undo_parent_meta->transformation();
    const QMatrix4x4 initial_child  = undo_child_meta->transformation();
    undo_parent_meta->setTranslation(QVector3D(1.0f, 0.7f, 0.1f));
    const QMatrix4x4 inherited_child = undo_child_meta->graphicsPart()->transformation();
    expect(undo_stack->count() == 1, "Moving a parent must create one undo command for the entire hierarchy.");
    undo_child_meta->setTranslation(undo_child_meta->translation() + QVector3D(0.2f, 0.1f, 0.0f));
    const QMatrix4x4 moved_child = undo_child_meta->transformation();
    expect(undo_stack->count() == 2, "Moving the child separately must create one additional undo command.");
    undo_stack->undo();
    expect(sameTransform(undo_child_meta->transformation(), inherited_child),
           "Undoing a child edit must restore its inherited post-parent pose.");
    undo_stack->undo();
    expect(sameTransform(undo_parent_meta->transformation(), initial_parent) &&
               sameTransform(undo_child_meta->transformation(), initial_child),
           "Undoing the parent edit must restore the complete hierarchy.");
    undo_stack->redo();
    expect(sameTransform(undo_child_meta->transformation(), inherited_child),
           "Redoing the parent edit must restore the inherited child pose.");
    undo_stack->redo();
    expect(sameTransform(undo_child_meta->transformation(), moved_child),
           "Redoing the child edit must restore its separate movement.");
    undo_stack->undo();
    undo_stack->undo();
    undo_child_meta->setTranslation(undo_child_meta->translation() + QVector3D(0.1f, 0.0f, 0.0f));
    undo_stack->undo();
    expect(sameTransform(undo_child_meta->transformation(), initial_child),
           "A child edit after undoing parent motion must use the restored pose as its undo baseline.");
    undo_parent_meta->orphanChild(undo_child_meta);
    undo_widget->clear();
    ORNL::CSM->clearParts();
    delete window;
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
