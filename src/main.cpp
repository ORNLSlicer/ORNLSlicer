#include "QApplication"
#include "QCommandLineParser"
#include "QStyleFactory"
#include "boost/preprocessor.hpp"
#include "configs/settings_base.h"
#include "console/command_line_processor.h"
#include "console/main_control.h"
#include "external_files/external_grid.h"
#include "gcode/gcode_command.h"
#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/mesh_base.h"
#include "geometry/mesh/open_mesh.h"
#include "part/part.h"
#include "threading/mesh_loader.h"
#include "units/unit.h"
#include "utilities/enums.h"
#include "utilities/qt_json_conversion.h"
#include "windows/main_window.h"

int main(int argc, char* argv[]) {
#ifdef Q_OS_WINDOWS
    qputenv("QT_QPA_PLATFORM", "windows:darkmode=0");
#endif

    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);

    // Register the meta type so we can use queued signals/slots.
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
    qRegisterMetaType<ORNL::ExternalGridInfo>("ExternalGridInfo");
    qRegisterMetaType<QList<QList<ORNL::Time>>>("QList<QList<Time>>");
    qRegisterMetaType<nlohmann::json>("nlohmann::json");
    qRegisterMetaType<QAbstractSocket::SocketError>("QAbstractSocket::SocketError");
    qRegisterMetaType<quintptr>("quintptr");
    qRegisterMetaType<ORNL::MeshLoader::MeshData>("MeshData");
    qRegisterMetaType<qintptr>("qintptr");
    qRegisterMetaType<QSet<int>>("QSet<int>");

    // Register the message handler so all output is printed in the main window as well.
    // qInstallMessageHandler(ORNL::msgHandler);

    QCommandLineParser parser;

    if (argc > 1) {
        QCoreApplication ca(argc, argv);

        QSharedPointer<ORNL::SettingsBase> options = QSharedPointer<ORNL::SettingsBase>::create();
        ORNL::CommandLineConverter clc;
        clc.setupCommandLineParser(parser);

        parser.process(ca);

        bool setupResult = clc.convertOptions(parser, options);

        if (setupResult) {
            ORNL::MainControl* control = new ORNL::MainControl(options);
            QObject::connect(control, &ORNL::MainControl::finished, &ca, &QCoreApplication::quit, Qt::QueuedConnection);

            control->run();
            int ret = ca.exec();

            delete control;
            return ret;
        }
        return 1;
    }
    else {
        QApplication a(argc, argv);
        QApplication::setStyle(QStyleFactory::create("fusion"));

        QApplication::setApplicationName("slicer2");
        QApplication::setOrganizationName("ornl");
        QApplication::setApplicationVersion(BOOST_PP_STRINGIZE(SLICER2_VERSION));
        // #ifdef WIN32
        //         HWND consoleWnd = GetConsoleWindow();
        //         DWORD dwProcessId;
        //         GetWindowThreadProcessId(consoleWnd, &dwProcessId);
        //         if (GetCurrentProcessId() == dwProcessId)
        //             ::ShowWindow(::GetConsoleWindow(), SW_HIDE);
        // #endif

        Q_INIT_RESOURCE(icons);
        Q_INIT_RESOURCE(shaders);
        Q_INIT_RESOURCE(styles);
        Q_INIT_RESOURCE(configs);

        // Get the instance of the main window and show it.
        ORNL::MainWindow* w = ORNL::MainWindow::getInstance();
        w->show();

        int ret = a.exec();
        delete w;

        return ret;
    }
}
