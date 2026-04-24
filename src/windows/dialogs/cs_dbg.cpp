#include "windows/dialogs/cs_dbg.h"

#include <QComboBox>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QScrollBar>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolTip>
#include <qbrush.h>
#include <qcontainerfwd.h>
#include <qcursor.h>
#include <qdialog.h>
#include <qgraphicsscene.h>
#include <qmatrix4x4.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qoverload.h>
#include <qpen.h>
#include <qpolygon.h>
#include <qquaternion.h>
#include <qsharedpointer.h>
#include <qwidget.h>

#include "cross_section/cross_section.h"
#include "geometry/plane.h"
#include "geometry/polygon_list.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "part/part.h"

namespace ORNL {
CsDebugDialog::CsDebugDialog(QWidget* parent) : QDialog(parent) { this->setupUi(); }

void CsDebugDialog::paintGraphicsView(int height) {

    // Each part is maintained such that the cross section the user selects will appear parallel
    // to the XZ plane, as this aligns with the wall. However, the cross section algorithm only takes
    // horizontal (XY) cross sections. In order to display the correct cross section, we need to
    // rotate the XZ plane to the XY plane and then rotate back.

    // Rotate to XY while keeping old rotation around
    QMatrix4x4 part_rotation = m_part->rootMesh()->transformation();
    QMatrix4x4 csxn_rotation = part_rotation;
    csxn_rotation.rotate(-90.0f, QVector3D(1, 0, 0));
    m_part->setTransformation(csxn_rotation);

    Plane slicing_plane = Plane(Point(0, 0, height), QVector3D(0, 0, 1));
    Point shift;
    QVector3D tmp_vec;
    PolygonList polylist =
        CrossSection::doCrossSection(m_part->rootMesh(), slicing_plane, shift, tmp_vec, GSM->getGlobal());
    QVector<PolygonList> splitpoly = polylist.splitIntoParts();

    QGraphicsScene* gs = m_view->scene();
    gs->clear();

    // For each list in the split polygons, construct a QPolygon.
    for (const PolygonList& poly : splitpoly) {
        QVector<QPolygon> qpolylist = poly.toQPolygons();
        QPolygon qpoly = qpolylist.front();

        // Starting at one, subtract all holes.
        for (int i = 1; i < qpolylist.size(); i++)
            qpoly = qpoly.subtracted(qpolylist[i]);

        gs->addPolygon(qpoly, QPen(Qt::black), QBrush(Qt::red));
    }

    m_scroll_min = m_part->rootMesh()->min().z();
    m_scroll_max = m_part->rootMesh()->max().z();

    gs->setSceneRect(polylist.boundingRect());
    m_view->fitInView(gs->sceneRect(), Qt::KeepAspectRatio);
    m_view->setScene(gs);

    // Restore old rotation of part
    m_part->setTransformation(part_rotation);
}

void CsDebugDialog::updateAxis(int idx) {
    QSharedPointer<Part> part = m_part_lookup[dynamic_cast<QComboBox*>(QObject::sender())];

    // Set the transform by switching on an int and selecting the corresponding axis.
    // This relies on the fact that the combo box goes through the planes in this order: YZ, XZ, XY;

    QMatrix4x4 rotation;
    switch (idx) {
        case 0: // YZ
            rotation.rotate(QQuaternion::fromAxisAndAngle(QVector3D(0, 0, 1), 90.0f));
            break;
        case 2: // XY
            rotation.rotate(QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), 90.0f));
            break;
        default:
            break;
    }
    this->changePart(part);
    part->setTransformation(rotation);

    this->paintGraphicsView();

    this->updateScroll();
}

void CsDebugDialog::selectFromRow(int row, int col) {
    QSharedPointer<Part> part = CSM->getPart(m_table->item(row, 0)->text());
    this->changePart(part);

    this->paintGraphicsView();

    this->updateScroll();
}

void CsDebugDialog::changeLayer(int height) {
    if (m_part.isNull())
        return;
    this->paintGraphicsView(height);

    QToolTip::showText(QCursor::pos(), QString::number(height));
}

void CsDebugDialog::changePart(QSharedPointer<Part> part) { m_part = part; }

void CsDebugDialog::updateScroll() {
    m_scrollbar->setMinimum(m_scroll_min);
    m_scrollbar->setMaximum(m_scroll_max);

    m_spinbox->setMinimum(m_scroll_min);
    m_spinbox->setMaximum(m_scroll_max);
}

void CsDebugDialog::setupUi() {
    this->setupWindow();
    this->setupWidgets();
    this->setupLayouts();
    this->setupTable();
    this->setupInsert();
    this->setupEvents();
}

void CsDebugDialog::setupWindow() {
    this->setWindowTitle("Cross-Section Debug");
    this->setMinimumSize(1280, 720);
}

void CsDebugDialog::setupWidgets() {
    m_table = new QTableWidget(this);

    m_view = new QGraphicsView(this);
    m_view->setScene(new QGraphicsScene(m_view));

    m_scrollbar = new QScrollBar(this);
    m_scrollbar->setMinimum(0);
    m_scrollbar->setMaximum(0);
    m_scrollbar->setInvertedAppearance(true);
    m_scrollbar->setInvertedControls(true);

    m_spinbox = new QSpinBox(this);
}

void CsDebugDialog::setupLayouts() { m_layout = new QHBoxLayout(this); }

void CsDebugDialog::setupTable() {
    // Create the table with our desired dimensions.
    m_table->setColumnCount(2);

    // Setup the header.
    m_table->setHorizontalHeaderLabels(QStringList() << "Name" << "Cross-Section Plane");
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    m_table->setSelectionMode(QAbstractItemView::SingleSelection);

    // For each part in the session, add it to our table.
    for (QSharedPointer<Part> part : CSM->parts()) {
        QTableWidgetItem* name = new QTableWidgetItem(part->name());

        name->setFlags(Qt::ItemFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable));

        QComboBox* combo = new QComboBox(this);
        combo->addItems(QStringList() << "YZ" << "XZ" << "XY");
        combo->setCurrentIndex(2);

        int row_nr = m_table->rowCount();

        m_table->insertRow(row_nr);

        m_table->setItem(row_nr, 0, name);
        m_table->setCellWidget(row_nr, 1, combo);

        m_table->setCurrentCell(row_nr, 0);

        this->changePart(part);

        // We will start with the XY plane of the part being parallel to the wall which is parallel to
        // the XZ plane. In order to align the XY plane with the XZ plane, we apply a 90 degree rotation
        // about the x-axis
        QMatrix4x4 rotation;
        rotation.rotate(QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), 90.0f));
        part->setTransformation(rotation);
        this->paintGraphicsView();

        this->updateScroll();

        QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CsDebugDialog::updateAxis);
        m_part_lookup[combo] = part;
    }
}

void CsDebugDialog::setupInsert() {
    m_layout->addWidget(m_table);
    m_layout->addWidget(m_view);
    m_layout->addWidget(m_scrollbar);
    m_layout->addWidget(m_spinbox);
}

void CsDebugDialog::setupEvents() {
    // Connect table to show function.
    connect(m_table, &QTableWidget::cellClicked, this, &CsDebugDialog::selectFromRow);
    connect(m_scrollbar, &QScrollBar::sliderMoved, this, &CsDebugDialog::changeLayer);
    connect(m_spinbox, QOverload<int>::of(&QSpinBox::valueChanged), this, &CsDebugDialog::changeLayer);

    //        connect(m_spinbox, QOverload<int>::of(&QSpinBox::valueChanged), m_scrollbar, &QScrollBar::setValue);
    //        connect(m_scrollbar, &QScrollBar::sliderMoved, m_spinbox, &QSpinBox::setValue);
}

} // namespace ORNL
