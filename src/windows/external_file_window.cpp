#include "windows/external_file_window.h"

#include "QFileDialog"
#include "QHeaderView"
#include "QIcon"
#include "external_files/parsers/external_grid_reader.h"

namespace ORNL {
ExternalFileWindow::ExternalFileWindow(QWidget* parent) {
    QIcon icon;
    icon.addFile(QStringLiteral(":/icons/slicer-2_logo.png"), QSize(), QIcon::Normal, QIcon::Off);
    setWindowIcon(icon);

    m_layout = new QGridLayout();

    m_display_label = new QLabel("Currently Specified Grid File: None");
    m_display_label->setWordWrap(true);

    m_layout->addWidget(m_display_label, 0, 0, 1, 4);

    m_load_button = new QPushButton("Load File");
    connect(m_load_button, &QPushButton::pressed, this, &ExternalFileWindow::loadFile);
    m_layout->addWidget(m_load_button, 1, 0, 1, 2);

    QPushButton* clearButton = new QPushButton("Clear Selection");
    connect(clearButton, &QPushButton::pressed, [this]() {
        m_display_label->setText("Currently Specified Grid File: None");
        emit forwardGridInfo(ExternalGridInfo());
        m_progress_bar->setValue(0);
        m_grid_info_label->clear();
        m_grid_cell_label->clear();
        m_x_cell_label->clear();
        m_y_cell_label->clear();
        m_z_cell_label->clear();

        m_grid_size_label->clear();
        m_x_size_label->clear();
        m_y_size_label->clear();
        m_z_size_label->clear();

        m_horizontal_sep2->hide();
        m_recipe_label->clear();
        m_recipe_table->hide();
    });

    m_layout->addWidget(clearButton, 1, 2, 1, 2);

    m_progress_bar = new QProgressBar();
    m_progress_bar->setMinimum(0);
    m_progress_bar->setMaximum(100);
    m_progress_bar->setValue(0);

    m_layout->addWidget(m_progress_bar, 2, 0, 1, 4);

    m_horizontal_sep1 = new QFrame();
    m_horizontal_sep1->setFrameShape(QFrame::HLine);
    m_layout->addWidget(m_horizontal_sep1, 3, 0, 1, 4);

    m_grid_info_label = new QLabel();
    m_layout->addWidget(m_grid_info_label, 5, 0, 1, 4, Qt::AlignCenter);

    m_grid_cell_label = new QLabel();
    m_layout->addWidget(m_grid_cell_label, 6, 0, 1, 2, Qt::AlignCenter);

    m_grid_size_label = new QLabel();
    m_layout->addWidget(m_grid_size_label, 6, 2, 1, 2, Qt::AlignCenter);

    m_x_cell_label = new QLabel();
    m_x_size_label = new QLabel();
    m_layout->addWidget(m_x_cell_label, 7, 0, 1, 2, Qt::AlignCenter);
    m_layout->addWidget(m_x_size_label, 7, 2, 1, 2, Qt::AlignCenter);

    m_y_cell_label = new QLabel();
    m_y_size_label = new QLabel();
    m_layout->addWidget(m_y_cell_label, 8, 0, 1, 2, Qt::AlignCenter);
    m_layout->addWidget(m_y_size_label, 8, 2, 1, 2, Qt::AlignCenter);

    m_z_cell_label = new QLabel();
    m_z_size_label = new QLabel();
    m_layout->addWidget(m_z_cell_label, 9, 0, 1, 2, Qt::AlignCenter);
    m_layout->addWidget(m_z_size_label, 9, 2, 1, 2, Qt::AlignCenter);

    m_horizontal_sep2 = new QFrame();
    m_horizontal_sep2->setFrameShape(QFrame::HLine);
    m_horizontal_sep2->hide();
    m_layout->addWidget(m_horizontal_sep2, 10, 0, 1, 4);

    m_recipe_label = new QLabel();
    m_layout->addWidget(m_recipe_label, 11, 0, 1, 4, Qt::AlignCenter);

    m_recipe_table = new QTableWidget();
    m_recipe_table->setColumnCount(3);
    QTableWidgetItem* header1 = new QTableWidgetItem();
    header1->setText("Id");
    m_recipe_table->setHorizontalHeaderItem(0, header1);
    QTableWidgetItem* header2 = new QTableWidgetItem();
    header2->setText("Min");
    m_recipe_table->setHorizontalHeaderItem(1, header2);
    QTableWidgetItem* header3 = new QTableWidgetItem();
    header3->setText("Max");
    m_recipe_table->setHorizontalHeaderItem(2, header3);
    m_recipe_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_recipe_table->verticalHeader()->hide();
    m_recipe_table->hide();

    m_layout->addWidget(m_recipe_table, 12, 0, 10, 4);

    m_layout->setRowStretch(0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12, 1);

    this->setLayout(m_layout);
}

void ExternalFileWindow::loadFile() {
    QFileDialog importDialog;
    QString filter = "Grid File (*.xlsx)";
    QString filepath = importDialog.getOpenFileName(this, "Import Grid File", QString(), filter, &filter);

    if (filepath != QString()) {
        m_load_button->setDisabled(true);
        ExternalGridReader* reader = new ExternalGridReader(filepath);
        connect(reader, &ExternalGridReader::statusUpdate, this,
                [this](int value) { m_progress_bar->setValue(value); });
        connect(reader, &ExternalGridReader::gridFileProcessed, this, [this, filepath](ExternalGridInfo gridInfo) {
            m_grid_info_label->setText("Grid Information");
            m_display_label->setText("Currently Specified Grid File: " + filepath);

            emit forwardGridInfo(gridInfo);
            Distance gridX(gridInfo.m_grid_layers[0].m_x_step);
            Distance gridY(gridInfo.m_grid_layers[0].m_y_step);
            Distance gridZ(gridInfo.m_z_step);

            m_grid_cell_label->setText("Grid Cell Dimensions (inches)");
            m_x_cell_label->setText("X: " + QString::number(gridX.to(inch), 'f', 2));
            m_y_cell_label->setText("Y: " + QString::number(gridY.to(inch), 'f', 2));
            m_z_cell_label->setText("Z: " + QString::number(gridZ.to(inch), 'f', 2));

            m_grid_size_label->setText("Grid Size (inches)");
            m_x_size_label->setText("X: " + QString::number(gridInfo.m_grid_layers[0].m_grid.size() * gridX.to(inch)));
            m_y_size_label->setText("Y: " +
                                    QString::number(gridInfo.m_grid_layers[0].m_grid[0].size() * gridY.to(inch)));
            m_z_size_label->setText("Z: " + QString::number(gridInfo.m_grid_layers.size() * gridZ.to(inch)));

            m_recipe_label->setText("Recipe Information");
            m_recipe_table->show();
            m_recipe_table->setRowCount(gridInfo.m_grid_layers[0].m_recipe_maps.size());
            for (int i = 0, end = gridInfo.m_grid_layers[0].m_recipe_maps.size(); i < end; ++i) {
                QTableWidgetItem* item =
                    new QTableWidgetItem(QString::number(gridInfo.m_grid_layers[0].m_recipe_maps[i].m_id));
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                QTableWidgetItem* item2 =
                    new QTableWidgetItem(QString::number(gridInfo.m_grid_layers[0].m_recipe_maps[i].m_min));
                item2->setFlags(item2->flags() & ~Qt::ItemIsEditable);
                QTableWidgetItem* item3 =
                    new QTableWidgetItem(QString::number(gridInfo.m_grid_layers[0].m_recipe_maps[i].m_max));
                item3->setFlags(item3->flags() & ~Qt::ItemIsEditable);

                m_recipe_table->setItem(i, 0, item);
                m_recipe_table->setItem(i, 1, item2);
                m_recipe_table->setItem(i, 2, item3);
            }
        });

        connect(reader, &ExternalGridReader::gridFailed, this, [this](QString msg) {
            m_grid_info_label->setText(msg);
            m_grid_cell_label->clear();
            m_x_cell_label->clear();
            m_y_cell_label->clear();
            m_z_cell_label->clear();

            m_grid_size_label->clear();
            m_x_size_label->clear();
            m_y_size_label->clear();
            m_z_size_label->clear();

            m_horizontal_sep2->hide();
            m_recipe_label->clear();
            m_recipe_table->hide();
        });
        connect(reader, &ExternalGridReader::finished, reader, &ExternalGridReader::deleteLater);
        connect(reader, &ExternalGridReader::finished, this, [this]() { m_load_button->setDisabled(false); });
        reader->start();
    }
}
} // namespace ORNL
