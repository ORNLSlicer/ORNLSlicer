#include "windows/dialogs/slice_dialog.h"

#include <QApplication>
#include <QColor>
#include <QGridLayout>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <qdialog.h>
#include <qfont.h>
#include <qnamespace.h>
#include <qprogressbar.h>
#include <qtmetamacros.h>
#include <qwidget.h>

#include "managers/preferences_manager.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace {

QColor progressBarFillColor() {
    return ORNL::PreferencesManager::getInstance()->getThemeText() == ORNL::Constants::UI::Themes::kDarkMode
               ? QColor(132, 196, 70)
               : QColor(0, 102, 44);
}

} // namespace

namespace ORNL {

SliceDialog::SliceDialog(QWidget* parent) : QDialog(parent) { this->setupUi(); }

void SliceDialog::setupUi() {
    this->setWindowTitle("Slice Progress");
    QGridLayout* layout = new QGridLayout(this);
    this->setLayout(layout);

    QFont activeFont = QApplication::font();
    activeFont.setPointSize(16);

    QLabel* stages = new QLabel("Slicing Stages");
    stages->setFont(activeFont);
    layout->addWidget(stages, 0, 0, 1, 3, Qt::AlignCenter);

    // currently 6 step types
    QColor fill_color = progressBarFillColor();
    for (int i = 0; i < 6; ++i) {
        QProgressBar* bar = new QProgressBar(this);
        bar->setMinimum(0);
        bar->setMaximum(100);
        bar->setMinimumWidth(300);
        QPalette palette = bar->palette();
        palette.setColor(QPalette::Active, QPalette::Highlight, fill_color);
        palette.setColor(QPalette::Inactive, QPalette::Highlight, fill_color);
        palette.setColor(QPalette::Disabled, QPalette::Highlight, fill_color);
        bar->setPalette(palette);

        m_progress_bars.push_back(bar);
        layout->addWidget(new QLabel(toString(static_cast<StatusUpdateStepType>(i))), i + 2, 0);
        layout->addWidget(bar, i + 2, 1);
    }
    QPushButton* cancelButton = new QPushButton("Cancel Slice", this);
    connect(cancelButton, &QPushButton::clicked, this, [this] {
        emit cancelSlice();
        this->close();
    });
    layout->addWidget(cancelButton, 8, 2);
}

void SliceDialog::updateStatus(StatusUpdateStepType type, int percentage) {
    m_progress_bars[(int)type]->setValue(percentage);
}

} // namespace ORNL
