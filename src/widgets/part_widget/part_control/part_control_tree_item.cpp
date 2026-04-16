#include "widgets/part_widget/part_control/part_control_tree_item.h"

namespace ORNL {
PartControlTreeItem::PartControlTreeItem(QSharedPointer<PartMetaItem> pm) {
    m_part = pm->part();

    this->setText(0, pm->part()->name());
    this->updateMeshType(m_part->rootMesh()->type());
}

QSharedPointer<Part> PartControlTreeItem::getPart() { return m_part; }

void PartControlTreeItem::updateMeshType(MeshType type) {
    m_mesh_type = type;
    switch (type) {
        case (MeshType::kSupport):
        case (MeshType::kBuild):
            this->setIcon(0, QIcon(":/icons/print_head.png"));
            updateToolTip();
            break;
        case (MeshType::kClipping):
            this->setIcon(0, QIcon(":/icons/clip.png"));
            updateToolTip();
            break;
        case (MeshType::kSettings):
            this->setIcon(0, QIcon(":/icons/gear.png"));
            this->setToolTip(0, "Settings Mesh");
            break;
    };
}

void PartControlTreeItem::addContainer(QTreeWidget* tree, int item_index) {
    m_container = new Container(m_part->name(), m_part->rootMesh()->isClosed());
    tree->setItemWidget(this, item_index, m_container);

    m_is_mesh_closed = m_part->rootMesh()->isClosed();
    updateToolTip();
}

void PartControlTreeItem::setFloating(bool floating) {
    m_container->showFloatingBadge(floating);
    m_is_mesh_floating = floating;
    updateToolTip();
}

void PartControlTreeItem::setOutsideVolume(bool outside) {
    m_container->showOutsideVolumeBadge(outside);
    m_is_mesh_inside_volume = !outside;
    updateToolTip();
}

void PartControlTreeItem::updateToolTip() {
    QString tooltip;

    switch (m_mesh_type) {
        case (MeshType::kSupport):
        case (MeshType::kBuild):
            tooltip.append("Build Mesh");
            break;
        case (MeshType::kClipping):
            tooltip.append("Clipping Mesh");
            break;
        case (MeshType::kSettings):
            tooltip.append("Settings Mesh");
            break;
    };

    if (!m_is_mesh_closed)
        tooltip.append(
            "\nThis model contains errors and is not a closed volume. Some ORNLSlicer features may be unavailable.");

    if (!m_is_mesh_inside_volume)
        tooltip.append("\nThis model is outside the print volume.");

    if (m_is_mesh_floating)
        tooltip.append("\nThis model is floating.");

    this->setToolTip(0, tooltip);
}

PartControlTreeItem::Container::Container(QString name, bool closed, QWidget* parent) : QWidget(parent) {
    this->setStyleSheet("border: 0px");
    this->setWindowFlags(Qt::FramelessWindowHint);
    this->setAttribute(Qt::WA_NoSystemBackground);
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto layout = new QHBoxLayout(this);
    layout->setSpacing(1);
    this->setLayout(layout);

    layout->addStretch();

    QIcon error_icon(":/icons/alert_circle_red.png");
    auto error_label = new QLabel(this);
    error_label->setAttribute(Qt::WA_TranslucentBackground);
    error_label->setToolTip(
        "This model contains errors is not a closed volume. Some ORNLSlicer features may be unavailable.");
    error_label->setPixmap(error_icon.pixmap(QSize(20, 20)));
    layout->addWidget(error_label);

    if (closed)
        error_label->setVisible(false);

    QIcon align_icon(":/icons/alert_outline_orange.png");
    m_alignment_badge = new QLabel(this);
    m_alignment_badge->setAttribute(Qt::WA_TranslucentBackground);
    m_alignment_badge->setToolTip("This model is outside the print volume.");
    m_alignment_badge->setPixmap(align_icon.pixmap(QSize(20, 20)));
    layout->addWidget(m_alignment_badge);
    m_alignment_badge->setVisible(false);
}

void PartControlTreeItem::Container::showOutsideVolumeBadge(bool show) {
    if (m_alignment_badge != nullptr)
        m_alignment_badge->setVisible(show);
}

void PartControlTreeItem::Container::showFloatingBadge(bool show) {
    if (m_alignment_badge != nullptr)
        m_alignment_badge->setVisible(show);
}
} // namespace ORNL
