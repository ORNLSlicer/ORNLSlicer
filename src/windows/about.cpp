#include "windows/about.h"

#include "QApplication"
#include "QDesktopServices"
#include "QFile"
#include "QGridLayout"
#include "QIcon"
#include "QLabel"
#include "QUrl"
#include "qdatetime.h"

namespace ORNL {
AboutWindow::AboutWindow(QWidget* parent) : QWidget() {
    // make it behave the same as "About Qt" window: The window is modal to the application and blocks input to all
    // windows
    setWindowModality(Qt::ApplicationModal);

    setWindowTitle("About " + QApplication::applicationDisplayName());
    setWindowIcon(QIcon(":/icons/slicer-2_logo.png"));

    QGridLayout* layout = new QGridLayout();

    QLabel* mdf = new QLabel();
    QPixmap mdfIcon(":/icons/mdf_color.png");
    mdf->setPixmap(mdfIcon.scaled(450, 100, Qt::KeepAspectRatio));
    layout->addWidget(mdf, 0, 0, 1, 2);

    QFont f = QApplication::font();
    f.setPointSize(10);

    QString version = QApplication::applicationDisplayName();

    QLabel* lblVersion = new QLabel(version);
    lblVersion->setFont(f);
    layout->addWidget(lblVersion, 1, 0, 1, 2, Qt::AlignCenter);

    QGridLayout* gpu_layout = new QGridLayout();
    layout->addLayout(gpu_layout, 3, 0, 1, 2, Qt::AlignCenter);

    QLabel* lblContact =
        new QLabel("Questions or comments? Email us at: <a href='mailto:slicer@ornl.gov'>slicer@ornl.gov</a>");
    connect(lblContact, &QLabel::linkActivated, [](const QString& link) { QDesktopServices::openUrl(QUrl(link)); });
    layout->addWidget(lblContact, 4, 0, 1, 2, Qt::AlignCenter);

    QLabel* bugReport = new QLabel("<a href='https://github.com/ORNLSlicer/ORNLSlicer/issues'>Bug report?</a>");
    connect(bugReport, &QLabel::linkActivated, [](const QString& link) { QDesktopServices::openUrl(QUrl(link)); });
    layout->addWidget(bugReport, 6, 0, 1, 2, Qt::AlignCenter);

    int curr_year = QDate::currentDate().year();
    layout->addWidget(new QLabel("Copyright © " + QString::number(curr_year)), 7, 0, 1, 2, Qt::AlignCenter);

    this->setLayout(layout);
}

AboutWindow::~AboutWindow() {}
} // namespace ORNL
