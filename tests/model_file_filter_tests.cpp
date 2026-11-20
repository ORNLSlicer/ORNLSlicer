#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdlib>

#include "test_utils.h"
#include "threading/mesh_loader.h"
#include "utilities/dialog_utils.h"
#include "utilities/theme_tool.h"

int main(int argc, char* argv[]) {
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "offscreen");
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setApplicationName("ornlslicer-model-file-filter-tests");
    QApplication::setStyle(QStyleFactory::create("fusion"));
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(styles);
    if (!ORNL::Testing::expect(ORNL::Theme::initializeFonts(), "Could not initialize application fonts."))
        return EXIT_FAILURE;

    QTemporaryDir directory;
    if (!ORNL::Testing::expect(directory.isValid(), "Could not create a temporary directory.")) return EXIT_FAILURE;
    const QStringList extensions = {"stl", "STL", "StL", "3mf",  "3MF",  "3mF",  "obj", "OBJ", "ObJ",
                                    "amf", "AMF", "AmF", "step", "STEP", "StEp", "stp", "STP", "StP"};
    QStringList supported;
    for (const auto& extension : extensions)
        supported.append(QString("model-%1.%2").arg(supported.size()).arg(extension));
    const QStringList unsupported = {"model.txt", "model.stl.bak", "model.stepx", "model"};
    const QByteArray stl_data =
        "solid triangle\nfacet normal 0 0 1\nouter loop\n"
        "vertex 0 0 0\nvertex 1 0 0\nvertex 0 1 0\n"
        "endloop\nendfacet\nendsolid triangle\n";
    const QByteArray obj_data = "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf 1 2 3 4\n";
    for (const auto& name : supported + unsupported) {
        QFile file(directory.filePath(name));
        if (!ORNL::Testing::expect(file.open(QIODevice::WriteOnly), "Could not create " + name.toStdString()))
            return EXIT_FAILURE;
        const QString extension = QFileInfo(name).suffix().toLower();
        const QByteArray data   = extension == "stl" ? stl_data : extension == "obj" ? obj_data : QByteArray();
        if (!ORNL::Testing::expect(file.write(data) == data.size(), "Could not write " + name.toStdString()))
            return EXIT_FAILURE;
    }

    bool passed = true;
    for (int theme_number : {0, 1}) {
        ORNL::Theme theme(theme_number);
        QApplication::setPalette(theme.getPalette());
        QFile style(theme.getFolderPath() + "style.qss");
        if (!ORNL::Testing::expect(style.open(QIODevice::ReadOnly), "Could not load the application stylesheet."))
            return EXIT_FAILURE;

        QFileDialog dialog;
        dialog.setOption(QFileDialog::DontUseNativeDialog);
        dialog.setStyleSheet(style.readAll());
        dialog.setWindowTitle("Open model part file");
        dialog.setFileMode(QFileDialog::ExistingFiles);
        dialog.setNameFilter(ORNL::DialogUtils::modelFileFilter());
        // Native Linux dialogs may match globs case-sensitively even when Qt's own dialog does not.
#ifdef Q_OS_LINUX
        dialog.setFilter(dialog.filter() | QDir::CaseSensitive);
#endif
        auto* model = dialog.findChild<QFileSystemModel*>();
        if (!ORNL::Testing::expect(model != nullptr, "The file dialog must expose its filesystem model."))
            return EXIT_FAILURE;

        QEventLoop loop;
        bool loaded = false;
        QObject::connect(model, &QFileSystemModel::directoryLoaded, &loop, [&](const QString& path) {
            if (path == directory.path()) {
                loaded = true;
                loop.quit();
            }
        });
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        dialog.setDirectory(directory.path());
        dialog.resize(1000, 750);
        dialog.show();
        if (!loaded) loop.exec();
        app.processEvents();
        passed &= ORNL::Testing::expect(loaded, "Timed out waiting for the file dialog directory.");

        QStringList visible;
        const auto root = model->index(directory.path());
        for (int row = 0; row < model->rowCount(root); ++row)
            visible.append(model->fileName(model->index(row, 0, root)));
        for (const auto& name : supported)
            passed &= ORNL::Testing::expect(visible.contains(name), "The file dialog hid " + name.toStdString());
        for (const auto& name : unsupported)
            passed &= ORNL::Testing::expect(!visible.contains(name), "The file dialog showed " + name.toStdString());

        if (argc > 1)
            passed &= ORNL::Testing::expect(
                dialog.grab().save(QString(argv[1]) + QString("/model-filter-%1.png").arg(theme_number)),
                "Could not save the file dialog screenshot.");
    }

    for (const auto& name : supported) {
        const QString extension = QFileInfo(name).suffix().toLower();
        if (extension != "stl" && extension != "obj") continue;
        auto meshes = ORNL::MeshLoader::LoadMeshes(directory.filePath(name));
        passed &= ORNL::Testing::expect(meshes.size() == 1, "Could not import " + name.toStdString());
        if (meshes.size() == 1) {
            const int expected_faces = extension == "obj" ? 2 : 1;
            passed &= ORNL::Testing::expect(meshes.front().mesh->faces().size() == expected_faces,
                                            "Incorrect triangulation when importing " + name.toStdString());
        }
        for (auto& mesh : meshes) free(mesh.raw_data);
    }
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
