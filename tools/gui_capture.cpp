#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QHash>
#include <QList>
#include <QPair>
#include <QSharedPointer>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QStyleFactory>
#include <QTextCharFormat>
#include <QVector>
#include <exception>
#include <iostream>

#include "capture_common.h"
#include "gcode/gcode_command.h"
#include "gcode/gcode_meta.h"
#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/mesh_base.h"
#include "geometry/mesh/open_mesh.h"
#include "geometry/segment_base.h"
#include "ornlslicer/build_info.h"
#include "part/part.h"
#include "threading/mesh_loader.h"
#include "units/unit.h"
#include "utilities/enums.h"
#include "utilities/qt_json_conversion.h"
#include "utilities/theme_tool.h"
#include "windows/main_window.h"

int main(int argc, char* argv[]) {
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
    QStandardPaths::setTestModeEnabled(true);

    qRegisterMetaType<QSharedPointer<ORNL::Part>>("QSharedPointer<Part>");
    qRegisterMetaType<QSharedPointer<ORNL::ClosedMesh>>("QSharedPointer<ClosedMesh>");
    qRegisterMetaType<QSharedPointer<ORNL::OpenMesh>>("QSharedPointer<OpenMesh>");
    qRegisterMetaType<QSharedPointer<ORNL::MeshBase>>("QSharedPointer<MeshBase>");
    qRegisterMetaType<QVector<QVector<QSharedPointer<ORNL::SegmentBase>>>>(
        "QVector<QVector<QSharedPointer<SegmentBase>>>");
    qRegisterMetaType<ORNL::Distance>("Distance");
    qRegisterMetaType<ORNL::Velocity>("Velocity");
    qRegisterMetaType<ORNL::Acceleration>("Acceleration");
    qRegisterMetaType<ORNL::Angle>("Angle");
    qRegisterMetaType<ORNL::Time>("Time");
    qRegisterMetaType<ORNL::Temperature>("Temperature");
    qRegisterMetaType<ORNL::Voltage>("Voltage");
    qRegisterMetaType<ORNL::Mass>("Mass");
    qRegisterMetaType<QHash<QString, QTextCharFormat>>("QHash<QString,QTextCharFormat>");
    qRegisterMetaType<QList<ORNL::Time>>("QList<Time>");
    qRegisterMetaType<QList<int>>("QList<int>");
    qRegisterMetaType<QList<double>>("QList<double>");
    qRegisterMetaType<ORNL::StatusUpdateStepType>("StatusUpdateStepType");
    qRegisterMetaType<ORNL::GcodeCommand>("GcodeCommand");
    qRegisterMetaType<ORNL::GcodeMeta>("GcodeMeta");
    qRegisterMetaType<fifojson>("fifojson");
    qRegisterMetaType<nlohmann::json>("nlohmann::json");
    qRegisterMetaType<quintptr>("quintptr");
    qRegisterMetaType<ORNL::MeshLoader::MeshData>("MeshData");
    qRegisterMetaType<qintptr>("qintptr");

    QApplication app(argc, argv);
    QApplication::setStyle(QStyleFactory::create("fusion"));
    QApplication::setApplicationName("ornlslicer");
    QApplication::setOrganizationName("ornl");
    QApplication::setApplicationVersion(QString::fromUtf8(ORNL::BuildInfo::Version));
    QApplication::setApplicationDisplayName(QString("ORNLSlicer-") + QString::fromUtf8(ORNL::BuildInfo::Version));

    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(shaders);
    Q_INIT_RESOURCE(styles);
    Q_INIT_RESOURCE(configs);
    Q_INIT_RESOURCE(docs);

    ORNL::Theme::initializeFonts();

    QCommandLineParser parser;
    parser.setApplicationDescription("Captures designated GUI elements with semantic names for documentation.");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption sectionOption(
        {"s", "section"},
        "Capture section(s): all, gui, demos, menus, preferences, settings (comma-separated or repeatable).", "section",
        "all");
    QCommandLineOption categoryOption(
        {"c", "settings-category"},
        "Capture only matching settings category slug(s), e.g. 'helical' or 'profile/helical'. Implies --section "
        "settings unless --section is also specified.",
        "category");
    parser.addOption(sectionOption);
    parser.addOption(categoryOption);
    parser.process(app);

    const QStringList categoryFilters = parser.values(categoryOption);
    QStringList sections;
    if (parser.isSet(sectionOption)) {
        for (const QString& val : parser.values(sectionOption)) {
            for (const QString& part : val.split(',', Qt::SkipEmptyParts)) { sections << part.trimmed().toLower(); }
        }
    }
    else if (!categoryFilters.isEmpty()) { sections << "settings"; }
    else { sections << "all"; }

    const bool runAll   = sections.contains("all");
    const bool runGui   = runAll || sections.contains("gui");
    const bool runDemos = runAll || sections.contains("demos");
    const bool runMenus = runAll || sections.contains("menus");
    const bool runPrefs = runAll || sections.contains("preferences");
    const bool runSets  = runAll || sections.contains("settings") || !categoryFilters.isEmpty();

    const QString outputDir = "docs/user-guide-images";

    std::cout << "Starting ORNLSlicer GUI Capture...\n";
    std::cout << "Output directory: " << outputDir.toStdString() << "\n\n";

    ORNL::MainWindow* window = ORNL::requireNonNull(ORNL::MainWindow::getInstance(), "MainWindow::getInstance");
    window->show();

    try {
        ORNL::ensureSampleParts(window);
        if (runGui || runDemos) { ORNL::sliceSampleParts(window); }

        if (runGui || runDemos) {
            const auto savedFiles = ORNL::captureAllGuiElements(window, outputDir, runGui, runDemos);
            std::cout << "Captured " << savedFiles.size() << " GUI/demo elements:\n";
            for (const auto& file : savedFiles) {
                std::cout << "  - " << file.first.toStdString() << ": " << file.second.toStdString() << "\n";
            }
            std::cout << "\n";
        }

        if (runMenus) {
            std::cout << "Capturing menu bar dropdown menus to menu_bar/<menu>_menu.png...\n";
            const auto menuPaths = ORNL::captureAllMenuBarMenus(window, outputDir);
            std::cout << "Captured " << menuPaths.size() << " menu bar dropdowns:\n";
            for (const auto& path : menuPaths) { std::cout << "  - " << path.toStdString() << "\n"; }
            std::cout << "\n";
        }

        if (runPrefs) {
            std::cout << "Capturing preferences tabs to preferences/<tab>_tab.png...\n";
            const auto prefPaths = ORNL::captureAllPreferencesTabs(window, outputDir);
            std::cout << "Captured " << prefPaths.size() << " preferences tabs:\n";
            for (const auto& path : prefPaths) { std::cout << "  - " << path.toStdString() << "\n"; }
            std::cout << "\n";
        }

        if (runSets) {
            std::cout << "Capturing settings dropdown categories to settings/<major>/<minor>_options.png...\n";
            const auto settingsPaths = ORNL::captureAllSettingsCategories(window, outputDir, categoryFilters);
            std::cout << "Captured " << settingsPaths.size() << " setting category options:\n";
            for (const auto& path : settingsPaths) { std::cout << "  - " << path.toStdString() << "\n"; }
            std::cout << "\n";
        }
    } catch (const std::exception& ex) {
        std::cerr << "GUI capture failed: " << ex.what() << "\n";
        return 1;
    }

    std::cout << "GUI capture complete.\n";
    return 0;
}
