#include "widgets/layerbar.h"

#include <algorithm>
#include <tuple>

#include <qabstractbutton.h>
#include <qaction.h>
#include <qalgorithms.h>
#include <qbrush.h>
#include <qbuttongroup.h>
#include <qcontainerfwd.h>
#include <qdialog.h>
#include <qdialogbuttonbox.h>
#include <qevent.h>
#include <qformlayout.h>
#include <qgridlayout.h>
#include <qgroupbox.h>
#include <qguiapplication.h>
#include <qinputdialog.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qlist.h>
#include <qmenu.h>
#include <qmessagebox.h>
#include <qminmax.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qobjectdefs.h>
#include <qoverload.h>
#include <qpainter.h>
#include <qpair.h>
#include <qpoint.h>
#include <qpropertyanimation.h>
#include <qquaternion.h>
#include <qradiobutton.h>
#include <qsharedpointer.h>
#include <qsize.h>
#include <qspinbox.h>
#include <qtmetamacros.h>
#include <qtooltip.h>
#include <qtypes.h>
#include <qvectornd.h>
#include <qwidget.h>

#include "configs/settings_range.h"
#include "geometry/plane.h"
#include "geometry/point.h"
#include "managers/preferences_manager.h"
#include "managers/settings/settings_manager.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "utilities/mathutils.h"
#include "widgets/layerdot.h"
#include "widgets/part_widget/model/part_meta_item.h"
#include "widgets/part_widget/model/part_meta_model.h"

namespace ORNL {
LayerBar::LayerBar(QSharedPointer<PartMetaModel> pm, QWidget* parent) : QWidget(parent) {
    m_model = pm;
    m_layers = 0;
    m_position.resize(m_layers);
    m_part = nullptr;
    m_skip = 1;
    m_last_clicked_dot = nullptr;
    m_track_change = false;
    m_original_y = 0;

    setupActions();

    setCursor(Qt::PointingHandCursor);

    connect(m_model.get(), &PartMetaModel::selectionUpdate, this, &LayerBar::selectionUpdate);
    connect(m_model.get(), &PartMetaModel::transformUpdate, this, &LayerBar::transformUpdate);
    connect(m_model.get(), &PartMetaModel::visualUpdate, this, &LayerBar::visualUpdate);
    connect(m_model.get(), &PartMetaModel::itemRemovedUpdate, this, &LayerBar::removalUpdate);
}

LayerBar::~LayerBar() { qDeleteAll(m_position); }

QSize LayerBar::sizeHint() const { return QSize(40, 200); }

void LayerBar::addSingle(int layer) {
    if (layer < 0) {
        return;
    }

    LayerDot* new_dot = addDot(layer, false);  // visually add dot. Not from template
    m_part->createSettingsRange(layer, layer); // add range to part
    selectDot(new_dot);
    m_last_clicked_dot = new_dot;
    update();
}

void LayerBar::addSingleFromTemplate(int layer, QSharedPointer<SettingsBase> sb) {
    LayerDot* new_dot = addDot(layer, true);       // visually add dot. From template
    m_part->createSettingsRange(layer, layer, sb); // add range to part with settings base
    new_dot->isFromTemplate();
    selectDot(new_dot);
    m_last_clicked_dot = new_dot;
    update();
}

void LayerBar::addRange(int lower, int upper) {
    if (layerValid(lower) && layerValid(upper)) {
        LayerDot* a = addDot(lower, false); // Not from template
        LayerDot* b = addDot(upper, false);

        dot_range* range = new dot_range;

        range->a = a;
        range->b = b;
        a->setRange(range);
        b->setRange(range);

        m_part->createSettingsRange(lower, upper);

        update();
    }
}

void LayerBar::addRangeFromTemplate(int lower, int upper, QSharedPointer<SettingsBase> sb) {
    if (layerValid(lower) && layerValid(upper)) {
        LayerDot* a = addDot(lower, true); // From template
        LayerDot* b = addDot(upper, true);

        a->isFromTemplate();
        b->isFromTemplate();

        dot_range* range = new dot_range;

        range->a = a;
        range->b = b;
        a->setRange(range);
        b->setRange(range);

        m_part->createSettingsRange(lower, upper, sb);

        update();
    }
}

void LayerBar::changePart(QSharedPointer<PartMetaItem> item) {
    // delete all the old dots / positions
    qDeleteAll(m_position);
    for (auto& dot : m_position)
        dot = nullptr;

    m_selection.clear();

    // if it's a build part, reconstruct the layer dots from the settings_ranges. If same template applied again, return
    // all previous dots.
    if (item != nullptr && item->meshType() == MeshType::kBuild && item->isSelected() &&
        (item->part()->getCurrentPartTemplate() == GSM->getCurrentTemplate())) {
        m_part = item->part();

        // Set template name if no template selected. Otherwise, it's set when layers are added from template.
        if (GSM->getCurrentTemplate() == "<no template selected>") {
            m_part->setCurrentPartTemplate("<no template selected>");
        }
        updateLayers();

        resizeEvent(nullptr);

        // reconstruct the dots, ranges and groups
        QVector<dot_group*> dot_group_list; // keep a list of groups we've made so we don't make the same one twice

        auto ranges = m_part->getSettingsRanges();
        auto range_from_template = m_part->getRangesFromTemplate();
        for (auto range : ranges) {
            int low = range->low();
            int high = range->high();
            QString group_name = range->groupName(); // only single dots can be in groups
            int min = qMin(low, high);
            int max = qMax(low, high);

            if (low == high) // add Single
            {
                LayerDot* new_dot;

                if (range_from_template[MathUtils::cantorPair(min, max)] == false)
                    new_dot = addDot(low, false);
                else
                    new_dot = addDot(low, true);
                if (group_name.length() > 0) // dot is in a group
                {
                    dot_group* group = nullptr;

                    // look for the group in existing groups
                    for (int i = 0, end = dot_group_list.size(); i < end && group == nullptr; ++i) {
                        if (dot_group_list[i]->group_name == group_name)
                            group = dot_group_list[i];
                    }

                    // if we didn't find the group in the list, make it & add to list
                    if (group == nullptr) {
                        group = new dot_group;
                        group->group_name = group_name;
                        dot_group_list.push_back(group);
                    }

                    // add the dot to the group
                    group->grouped.append(new_dot);
                    new_dot->setGroup(group);
                }
            }
            else // add range
            {
                LayerDot* a;
                LayerDot* b;
                // If range not from template
                if (range_from_template[MathUtils::cantorPair(min, max)] == false) {
                    a = addDot(low, false);
                    b = addDot(high, false);
                }
                else { // From template
                    a = addDot(low, true);
                    b = addDot(high, true);
                }
                dot_range* range = new dot_range;
                range->a = a;
                range->b = b;

                a->setRange(range);
                b->setRange(range);
            }
        }
    }
    else // Disable on settings/ clipping mesh or no part
    {
        m_part = nullptr;
        m_layers = 0;
        m_position.resize(m_layers);
        m_selection.clear();
    }

    // Select part settings
    if (item != nullptr && item->meshType() != MeshType::kClipping && item->isSelected()) {
        // Set selected settings to be part
        QList<QSharedPointer<SettingsBase>> ranges;
        ranges.append(item->part()->getSb());
        emit setSelectedSettings(qMakePair(item->part()->name(), ranges));
    }
    else {
        emit setSelectedSettings(qMakePair(QString(""), QList<QSharedPointer<SettingsBase>>()));
    }
    // Check if new part selected and if that part's template equals the currently selected template from drop down
    // menu. If need to change part template.
    if (item != nullptr && item->meshType() == MeshType::kBuild && item->isSelected() &&
        !(item->part()->getCurrentPartTemplate() == GSM->getCurrentTemplate())) {
        m_part = item->part();
        if (GSM->getCurrentTemplate() == "<no template selected>") {
            m_part->setCurrentPartTemplate("<no template selected>");
        }
        m_part->clearSettingsRanges();
        m_deleted_ranges.clear();
        updateLayers();
        resizeEvent(nullptr);
        for (LayerDot* dot : m_position) {
            if (dot != nullptr) // Delete current layer dots and load new ones from selected template
                deleteSingle(dot);
        }
        loadTemplateLayers();
        QList<QSharedPointer<SettingsBase>> ranges;
        ranges.append(item->part()->getSb());
        emit setSelectedSettings(qMakePair(item->part()->name(), ranges));
    }
    update();
}

void LayerBar::reselectPart() { // If no part selected
    if (m_part == nullptr)
        return;
    // delete all the old dots / positions
    qDeleteAll(m_position);
    for (auto& dot : m_position)
        dot = nullptr;

    m_selection.clear();
    // Clear dots if no template selected.
    if (GSM->getCurrentTemplate() == "<no template selected>") {
        m_part->setCurrentPartTemplate("<no template selected>");
        m_part->clearSettingsRanges();
        clearTemplate();
        return;
    }

    // if it's a build part, reconstruct the layer dots from the settings_ranges
    if (m_part->getCurrentPartTemplate() == GSM->getCurrentTemplate()) {
        updateLayers();

        resizeEvent(nullptr);

        // reconstruct the dots, ranges and groups
        QVector<dot_group*> dot_group_list; // keep a list of groups we've made so we don't make the same one twice

        auto ranges = m_part->getSettingsRanges();
        auto range_from_template = m_part->getRangesFromTemplate();
        for (auto range : ranges) {
            int low = range->low();
            int high = range->high();
            QString group_name = range->groupName(); // only single dots can be in groups
            int min = qMin(low, high);
            int max = qMax(low, high);

            if (low == high) // add Single
            {
                LayerDot* new_dot;

                if (range_from_template[MathUtils::cantorPair(min, max)] == false)
                    new_dot = addDot(low, false);
                else
                    new_dot = addDot(low, true);
                if (group_name.length() > 0) // dot is in a group
                {
                    dot_group* group = nullptr;

                    // look for the group in existing groups
                    for (int i = 0, end = dot_group_list.size(); i < end && group == nullptr; ++i) {
                        if (dot_group_list[i]->group_name == group_name)
                            group = dot_group_list[i];
                    }

                    // if we didn't find the group in the list, make it & add to list
                    if (group == nullptr) {
                        group = new dot_group;
                        group->group_name = group_name;
                        dot_group_list.push_back(group);
                    }

                    // add the dot to the group
                    group->grouped.append(new_dot);
                    new_dot->setGroup(group);
                }
            }
            else // add range
            {
                LayerDot* a;
                LayerDot* b;
                // If range not from template
                if (range_from_template[MathUtils::cantorPair(min, max)] == false) {
                    a = addDot(low, false);
                    b = addDot(high, false);
                }
                else { // From template
                    a = addDot(low, true);
                    b = addDot(high, true);
                }
                dot_range* range = new dot_range;
                range->a = a;
                range->b = b;

                a->setRange(range);
                b->setRange(range);
            }
        }
    }

    // Select part settings
    if (m_part->getCurrentPartTemplate() == GSM->getCurrentTemplate()) {
        // Set selected settings to be part
        QList<QSharedPointer<SettingsBase>> ranges;
        ranges.append(m_part->getSb());
        emit setSelectedSettings(qMakePair(m_part->name(), ranges));
    }
    else
        emit setSelectedSettings(qMakePair(QString(""), QList<QSharedPointer<SettingsBase>>()));
    // Check if new part selected and if that part's template equals the currently selected template from drop down menu
    if (!(m_part->getCurrentPartTemplate() == GSM->getCurrentTemplate())) {
        m_part->clearSettingsRanges();
        m_deleted_ranges.clear();
        updateLayers();
        resizeEvent(nullptr);
        for (LayerDot* dot : m_position) {
            if (dot != nullptr) // Delete current layer dots and load new ones from selected template
                deleteSingle(dot);
        }
        loadTemplateLayers();
        QList<QSharedPointer<SettingsBase>> ranges;
        ranges.append(m_part->getSb());
        emit setSelectedSettings(qMakePair(m_part->name(), ranges));
    }
    update();
}

void LayerBar::loadTemplateLayers() {
    QVector<SettingsRange> sr = GSM->getLayerSettings();
    for (int i = 0, end = sr.size(); i < end; ++i) {
        if (sr[i].high() <= m_layers) {        // Don't load layers that are higher than part height
            if (sr[i].low() == sr[i].high()) { // If single layer
                addSingleFromTemplate(sr[i].low(), sr[i].getSb());
            }
            else { // If range
                addRangeFromTemplate(sr[i].low(), sr[i].high(), sr[i].getSb());
            }
        }
        else { // If not in range, add to deleted layers in case they need to be brought back if layerbar grows.
            m_deleted_ranges.push_back(sr[i]);
        }
    }
}

void LayerBar::removePart(QSharedPointer<Part> part) {
    clearSelection();
    qDeleteAll(m_position);
    m_part = nullptr;
}

void LayerBar::deleteSelection() {
    for (LayerDot* dot : m_selection)
        deleteSingle(dot);

    m_selection.clear();
    clearSelection();
    update();
}

void LayerBar::setLayer() {
    bool ok;
    bool moved = false;
    LayerDot* dot = m_selection.back();
    int orginal_layer = dot->getLayer();
    int new_layer = QInputDialog::getInt(this, "Layer Entry", "Enter new layer number:", dot->getLayer() + 1, 1,
                                         m_layers + 1, 1, &ok);
    new_layer--;

    moved = moveDotToLayer(dot, new_layer);

    // If we can't move the dot to that layer, ask the use for another until we successfully move the dot
    while (!moved && ok) {
        new_layer = QInputDialog::getInt(this, "Layer Entry",
                                         "Could not create new layer settings because settings already exist at that "
                                         "layer. Please enter another value:",
                                         new_layer + 1, 1, m_layers + 1, 1, &ok);
        new_layer--;

        moved = moveDotToLayer(dot, new_layer);
    }

    if (moved) {
        m_part->updateSettingsRangeLimits(orginal_layer, orginal_layer, new_layer, new_layer);
        changeSelectedSettings();
    }
}

void LayerBar::setPairLayers() {
    LayerDot* first_layer = m_selection.back();
    LayerDot* second_layer = m_selection.front();

    // Dialog prompt to get user input for range start and end
    QDialog dialog(this);
    QFormLayout form(&dialog);
    dialog.setWindowTitle("Move existing layer range");

    // Input for first and last in range
    QSpinBox* spinbox_first = new QSpinBox(&dialog);  // start of range
    QSpinBox* spinbox_second = new QSpinBox(&dialog); // end of range
    spinbox_first->setRange(1, m_layers - 1);
    spinbox_second->setRange(1, m_layers);

    spinbox_first->setValue(first_layer->getLayer() + 1);
    spinbox_second->setValue(second_layer->getLayer() + 1);

    // Add inputs into a layout and the form
    QGroupBox* groupBox = new QGroupBox(tr("Alter the layer numbers to the desired location."));
    QGridLayout* grid = new QGridLayout;
    grid->addWidget(new QLabel("First layer"), 1, 1);
    grid->addWidget(new QLabel("Last layer"), 2, 1);
    grid->addWidget(spinbox_first, 1, 2);
    grid->addWidget(spinbox_second, 2, 2);
    grid->setColumnStretch(2, 2);
    groupBox->setLayout(grid);
    groupBox->setEnabled(true);
    form.addRow(groupBox);

    // Add some standard buttons (Cancel/Ok) at the bottom of the dialog
    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    form.addRow(&buttonBox);
    QObject::connect(&buttonBox, SIGNAL(accepted()), &dialog, SLOT(accept()));
    QObject::connect(&buttonBox, SIGNAL(rejected()), &dialog, SLOT(reject()));

    bool result = dialog.exec();

    while (result == QDialog::Accepted) {
        int first_val = spinbox_first->value() - 1;
        int second_val = spinbox_second->value() - 1;

        if (first_val == first_layer->getLayer() && second_val == second_layer->getLayer()) {
            break;
        }

        if ((this->layerValid(first_val) || m_position[first_val] == first_layer) &&
            (this->layerValid(second_val) || m_position[second_val] == second_layer)) {
            m_part->updateSettingsRangeLimits(first_layer->getLayer(), second_layer->getLayer(), first_val, second_val);

            this->moveDotToLayer(first_layer, first_val);
            this->moveDotToLayer(second_layer, second_val);
            break;
        }

        groupBox->setTitle(
            tr("The selected layer numbers are not available, please enter unoccupied layers for the new locations."));

        result = dialog.exec();
    }

    this->clearSelection();
}

void LayerBar::addSelection() {
    // QAction function to add selection
    bool ok;
    int new_layer =
        QInputDialog::getInt(this, "Add New Layer Settings", "Enter the layer number:", 1, 1, m_layers, 1, &ok);
    new_layer--;

    // If the user gives a bad layer, ask for another value until we get it right.
    while (!layerValid(new_layer) && ok) {
        new_layer = QInputDialog::getInt(this, "Add New Layer Settings",
                                         "Could not create new layer settings because settings already exist at that "
                                         "layer. Please enter another value:",
                                         1, 1, m_layers, 1, &ok);
        new_layer--;
    }

    if (ok)
        addSingle(new_layer);
}

void LayerBar::addPair() {
    // Dialog prompt to get user input for range start and end
    QDialog dialog(this);
    QFormLayout form(&dialog);
    dialog.setWindowTitle("Add New Range of Layer Settings");

    // Input for first and last in range
    QDoubleSpinBox* pairFirst = new QDoubleSpinBox(&dialog); // start of range
    QDoubleSpinBox* pairLast = new QDoubleSpinBox(&dialog);  // end of range
    pairFirst->setRange(1, m_layers - 1);
    pairLast->setRange(1, m_layers);
    pairFirst->setDecimals(0);
    pairLast->setDecimals(0);

    // If a dot is selected, use that dot as the start
    if (m_selection.size() == 1)
        pairFirst->setValue(m_selection[0]->getLayer() + 1);

    // Add inputs into a layout and the form
    QGroupBox* groupBox = new QGroupBox(tr("Indicate which layers to pair"));
    QGridLayout* grid = new QGridLayout;
    grid->addWidget(new QLabel("First layer"), 1, 1);
    grid->addWidget(new QLabel("Second layer"), 2, 1);
    grid->addWidget(pairFirst, 1, 2);
    grid->addWidget(pairLast, 2, 2);
    grid->setColumnStretch(2, 2);
    groupBox->setLayout(grid);
    groupBox->setEnabled(true);
    form.addRow(groupBox);

    // Add some standard buttons (Cancel/Ok) at the bottom of the dialog
    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    form.addRow(&buttonBox);
    QObject::connect(&buttonBox, SIGNAL(accepted()), &dialog, SLOT(accept()));
    QObject::connect(&buttonBox, SIGNAL(rejected()), &dialog, SLOT(reject()));

    // Show the dialog as modal
    if (dialog.exec() == QDialog::Accepted) {
        // Initiate values and dots
        int first = pairFirst->value() - 1;
        int last = pairLast->value() - 1;
        LayerDot* a;
        LayerDot* b;
        bool ok = true;
        QInputDialog options;
        QStringList choose;
        choose << "Choose a different layer" << "Delete the existing settings";
        options.setOptions(QInputDialog::UseListViewForComboBoxItems);
        options.setComboBoxItems(choose);
        options.setWindowTitle("Could not add the settings range");
        options.setLabelText("One of the specified layers already has settings associated with it. \n"
                             "What would you like to do?");

        if (m_selection.size() == 0) {
            while (!layerValid(first) && ok) {
                options.exec();
                if (options.textValue().contains("Choose") && ok) {
                    first = QInputDialog::getInt(
                        this, "Add New Range of Layer Settings",
                        "Layer " + QString::number(first + 1) + " was invalid. New first layer:", 1, 1, m_layers, 1,
                        &ok);
                    first--;
                }
                else if (options.textValue().contains("Delete") && ok) {
                    deleteSingle(m_position[first]);
                }
            }
            while (!layerValid(last) && ok) {
                options.exec();
                if (options.textValue().contains("Choose") && ok) {
                    last = QInputDialog::getInt(
                        this, "Add New Range of Layer Settings",
                        "Layer " + QString::number(last + 1) + " was invalid. New last layer:", 1, 1, m_layers, 1, &ok);
                    last--;
                }
                else if (options.textValue().contains("Delete") && ok) {
                    deleteSingle(m_position[last]);
                }
            }
            while (first == last && ok) {
                last = QInputDialog::getInt(
                    this, "Could not add the settings range",
                    "Cannot create a range between a single layer. Please select another layer:", 1, 1, m_layers, 1,
                    &ok);
                last--;
            }
            if (ok) {
                this->addRange(first, last);
            }
        }
        else if (m_selection.size() == 1) {
            while (!layerValid(last) && ok) {
                options.exec();
                if (options.textValue().contains("Choose") && ok) {
                    last = QInputDialog::getInt(
                        this, "Add New Range of Layer Settings",
                        "Layer " + QString::number(last + 1) + " was invalid. New last layer:", 1, 1, m_layers, 1, &ok);
                    last--;
                }
                else if (options.textValue().contains("Delete") && ok) {
                    deleteSingle(m_position[last]);
                }
            }
            while (first == last && ok) {
                last = QInputDialog::getInt(
                    this, "Add New Range of Layer Settings",
                    "Could not create a range between identical layers. Please enter another value:", 1, 1, m_layers, 1,
                    &ok);
                last--;
            }
            if (ok) {
                a = m_selection[0];
                b = addDot(last, false);

                dot_range* range = new dot_range;

                range->a = a;
                range->b = b;
                a->setRange(range);
                b->setRange(range);

                // delete old single-layer ranges
                m_part->removeSettingsRange(a->getLayer(), a->getLayer());
                m_part->removeSettingsRange(b->getLayer(), b->getLayer());

                // make new combined range
                m_part->createSettingsRange(a->getLayer(), b->getLayer());

                clearSelection();
                update();
            }
        }
    }
}

void LayerBar::addGroup() {
    // Dialog prompt to get user input for adding a group of dots.
    QDialog dialog(this);
    QFormLayout form(&dialog);
    dialog.setWindowTitle("Add a Selection Group");

    // Radiobuttons for selecting odds/evens or making a custom selection group
    QRadioButton* selectOdds = new QRadioButton("Select all odd layers.", &dialog);
    QRadioButton* selectEvens = new QRadioButton("Select all even layers.", &dialog);
    QRadioButton* selectCustom = new QRadioButton("Make a custom group.", &dialog);

    // ButtonGroup to make the buttons exclusive
    QButtonGroup* chooseSelection = new QButtonGroup(&dialog);
    chooseSelection->addButton(selectOdds, 1);
    chooseSelection->addButton(selectEvens, 2);
    chooseSelection->addButton(selectCustom, 3);

    // Inputs for a custom selection group
    QDoubleSpinBox* grpStart = new QDoubleSpinBox(&dialog); // first layer in the group
    grpStart->setRange(1, m_layers - 2);                    // minimum of 1, maximum of 2nd to last layer of the part
    grpStart->setDecimals(0);                               // no decimals
    QDoubleSpinBox* grpEnd = new QDoubleSpinBox(&dialog);   // last layer in the group
    grpEnd->setRange(3, m_layers);                          // minimum of 3, maximum of last layer
    grpEnd->setValue(m_layers);                             // default is last layer
    grpEnd->setDecimals(0);                                 // no decimals
    QDoubleSpinBox* interval = new QDoubleSpinBox(&dialog); // number of layers to skip between selections
    interval->setRange(1, m_layers / 2);                    // minimum of 0, maximum of 1/2 total number of layers
    interval->setDecimals(0);                               // no decimals
    QLineEdit* name = new QLineEdit(&dialog);
    name->setText("New Group");

    // Groupbox to group the inputs together and enable/disable them based on radiobutton selection
    QGroupBox* groupBox = new QGroupBox(tr("Custom Group"));
    QGridLayout* grid = new QGridLayout;
    grid->addWidget(new QLabel("Start at layer"), 1, 1);
    grid->addWidget(new QLabel("Stop at layer"), 2, 1);
    grid->addWidget(new QLabel("Interval*"), 3, 1);
    QLabel* intDef = new QLabel(
        "*Indicate how many layers to skip between affected layers; an interval of 1 selects every other layer.");
    intDef->setWordWrap(true);
    intDef->setIndent(1);
    grid->addWidget(new QLabel("Group Name"), 5, 1);
    grid->addWidget(grpStart, 1, 2);
    grid->addWidget(grpEnd, 2, 2);
    grid->addWidget(interval, 3, 2);
    grid->addWidget(intDef, 4, 1, 1, 2);
    grid->addWidget(name, 5, 2);
    grid->setColumnStretch(2, 2);
    groupBox->setLayout(grid);
    groupBox->setEnabled(false);

    // Groupbox is only enabled if the user indicates they want to make a custom selection group
    connect(chooseSelection, QOverload<QAbstractButton*>::of(&QButtonGroup::buttonClicked),
            [=](QAbstractButton* button) {
                if (chooseSelection->checkedId() == 3)
                    groupBox->setEnabled(true);
                else
                    groupBox->setEnabled(false);
            });

    // Populate the form
    form.addRow(selectOdds);
    form.addRow(selectEvens);
    form.addRow(selectCustom);
    form.addRow(groupBox);

    // Add some standard buttons (Cancel/Ok) at the bottom of the dialog
    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    form.addRow(&buttonBox);
    QObject::connect(&buttonBox, SIGNAL(accepted()), &dialog, SLOT(accept()));
    QObject::connect(&buttonBox, SIGNAL(rejected()), &dialog, SLOT(reject()));

    // Show the dialog as modal
    if (dialog.exec() == QDialog::Accepted) {
        // If the user didn't dismiss the dialog, add dots at the appropriate layers
        // Default values: select all odd layers (checkedId == 1)
        int start = 0;
        int end = m_layers;
        int skip = 2;
        QString group_name = "Odd Layers";
        if (chooseSelection->checkedId() == 2) {
            // If user wants to select even layers
            group_name = "Even Layers";
            start = 1;
        }
        else if (chooseSelection->checkedId() == 3) {
            // If user wants to make a custom selection group
            start = grpStart->value() - 1;
            end = grpEnd->value();
            skip = interval->value() + 1;
            group_name = name->text();
        }

        // Make the dot group
        dot_group* group = new dot_group;
        group->group_name = group_name;

        for (int i = start; i < end; i = i + skip) {
            // Add dots on the appropriate layers and add them to the dot group
            if (layerValid(i)) {
                LayerDot* dot = addDot(i, false);
                group->grouped.append(dot);
                dot->setGroup(group);
                dot->show();

                m_part->createSettingsRange(dot->getLayer(), dot->getLayer(), group_name);
            }
        }
    }
}

// Group selected dots together
void LayerBar::groupDots() {
    bool ok;
    QString group_name = QInputDialog::getText(this, "Create Group", "Enter a name for the new group:");

    // Make a new group
    dot_group* group = new dot_group;
    group->group_name = group_name;

    for (LayerDot* dot : m_selection) {
        // Check that the selected dot is not already in a group or pair
        if (dot->getGroup() == nullptr && dot->getPair() == nullptr) {
            // Add selected dots to the group
            group->grouped.append(dot);
            dot->setGroup(group);

            m_part->removeSettingsRange(dot->getLayer(), dot->getLayer());
            m_part->createSettingsRange(dot->getLayer(), dot->getLayer(), group_name);
        }
    }

    clearSelection();
}

// Ungroup a group of dots
void LayerBar::ungroupDots() {
    dot_group* group;
    for (LayerDot* dot : m_selection) {
        group = dot->getGroup();
        if (group != nullptr) {
            group->grouped.removeAt(group->grouped.indexOf(dot));
            dot->setGroup(nullptr);

            auto orginal_sb = m_part->getSettingsRange(dot->getLayer(), dot->getLayer())->getSb();
            m_part->getSettingsRange(dot->getLayer(), dot->getLayer())
                ->setSb(QSharedPointer<SettingsBase>::create(*orginal_sb));
            m_part->getSettingsRange(dot->getLayer(), dot->getLayer())->setGroup("");
        }
    }

    delete group;
    clearSelection();
}

void LayerBar::selectionUpdate(QSharedPointer<PartMetaItem> item) {
    // Part selection changed, update view only if a single part is selected
    if (m_model->selectedItems().size() == 1 && item->isSelected())
        changePart(item);
    else
        changePart(nullptr);
}

void LayerBar::transformUpdate(QSharedPointer<PartMetaItem> item) {
    // Part was transformed, might need to update
    auto part = item->part();

    QVector3D part_translation, part_scale;
    QQuaternion part_rotation;
    std::tie(part_translation, part_rotation, part_scale) =
        MathUtils::decomposeTransformMatrix(part->rootMesh()->transformation());

    QVector3D gop_translation = item->translation();
    QQuaternion gop_rotation = item->rotation();
    QVector3D gop_scale = item->scale();
    gop_translation *= Constants::OpenGL::kViewToObject;

    if (gop_rotation != part_rotation || gop_scale != part_scale) // Only need to recalculate on scale/ rotation events
    {

        QMatrix4x4 object_transformation = MathUtils::composeTransformMatrix(gop_translation, gop_rotation, gop_scale);
        part->setTransformation(object_transformation);

        changePart(item);
    }
}

void LayerBar::visualUpdate(QSharedPointer<PartMetaItem> item) { changePart(item); }

void LayerBar::removalUpdate(QSharedPointer<PartMetaItem> item) {
    if (item->isSelected())
        changePart(nullptr);
}

void LayerBar::makePair() {
    // This function is only called when exactly two dots are selected
    LayerDot* a = m_selection[0];
    LayerDot* b = m_selection[1];

    dot_range* range = new dot_range;

    range->a = a;
    range->b = b;
    a->setRange(range);
    b->setRange(range);

    // delete old single-layer ranges
    m_part->removeSettingsRange(a->getLayer(), a->getLayer());
    m_part->removeSettingsRange(b->getLayer(), b->getLayer());

    // make new combined range
    m_part->createSettingsRange(a->getLayer(), b->getLayer());

    clearSelection();
    update();
}

void LayerBar::splitPair() {
    // function is called only a single pair is selected
    dot_range* range = m_selection[0]->getRange();
    splitRange(range);

    clearSelection();
    update();
}

void LayerBar::paintEvent(QPaintEvent* event) {

    QPainter painter(this);

    painter.setRenderHint(QPainter::Antialiasing);
    paintDivisions(&painter);
    paintRanges(&painter);
}

void LayerBar::mousePressEvent(QMouseEvent* event) {
    // if there's no part or no layers, do nothing
    if (m_part == nullptr || m_layers <= 0)
        return;

    // Get the dot underneath the cursor.
    LayerDot* dot = qobject_cast<LayerDot*>(this->childAt(event->pos()));

    if (dot == nullptr) // there is not a dot under the cursor
    {
        // If the left button was clicked, make a new dot if possible.
        if (event->button() == Qt::LeftButton) {
            if (m_selection.isEmpty()) {
                addSingle(getLayerFromPosition(event->y()));
                m_should_deselect = false;
            }
            else
                clearSelection();
        }
    }
    else // there is a dot under the cursor
    {
        if (event->button() == Qt::LeftButton) {
            // If SHIFT is held and there's a dot already selected,
            //   then select all dots between the last selection and this one.
            // If either CTRL or ALT is held,
            //    then select multiple dots
            // Otherwise,
            //    select/deselect the dot depending on its current status
            if (QGuiApplication::queryKeyboardModifiers() == Qt::ShiftModifier && m_selection.size() > 0) {
                int min = qMin(m_last_clicked_dot->getLayer(), dot->getLayer());
                int max = qMax(m_last_clicked_dot->getLayer(), dot->getLayer());
                selectOnInterval(min, max);
                m_should_deselect = false;
            }
            else if (QGuiApplication::queryKeyboardModifiers() == Qt::AltModifier ||
                     QGuiApplication::queryKeyboardModifiers() == Qt::ControlModifier) {
                if (dot->isSelected()) {
                    deselectDot(dot);
                    changeSelectedSettings();
                }
                else {
                    selectDot(dot);
                    changeSelectedSettings();
                    m_should_deselect = false;
                }
            }
            else // no key modifiers
            {
                if (!dot->isSelected()) {
                    clearSelection();
                    selectDot(dot);
                    changeSelectedSettings();
                    m_should_deselect = false;
                }
                else {
                    m_should_deselect = true;
                }
            }
            m_last_clicked_dot = dot;
        }
    }
}

void LayerBar::mouseReleaseEvent(QMouseEvent* event) {
    if (m_part == nullptr || m_layers <= 0)
        return;

    // this function needs to update part's ranges if the dots were moved by user
    if (!m_selection.isEmpty() && event->button() == Qt::LeftButton && m_track_change) {
        // we're handling the change, so set tracking flag to false
        m_track_change = false;

        QVector<LayerDot*> selection_copy = m_selection;
        std::sort(selection_copy.begin(), selection_copy.end(),
                  [](LayerDot* a, LayerDot* b) { return a->getLayer() < b->getLayer(); }); // sort low to high

        // Upon release, calculate each dot's new position.
        for (int i = 0; i < selection_copy.size();
             ++i) // initentionally re-evaluating array size on every iteration bc it can change
        {
            LayerDot* dot = selection_copy[i];
            // if the range has been visually moved, but not moved on the part, update the part
            if (dot->getLayer() != dot->getDisplayLayer()) {
                int old_low = dot->getLayer();
                int old_high = dot->getLayer();
                if (dot->getRange() != nullptr)
                    old_high = dot->getPair()->getLayer();

                int new_low = dot->getDisplayLayer();
                int new_high = dot->getDisplayLayer();
                if (dot->getRange() != nullptr)
                    new_high = dot->getPair()->getDisplayLayer();

                if (moveDotToLayer(dot, new_low)) // successfully moved the layer
                {
                    // need to update the ranges on the part to reflect the click/drag movement
                    m_part->updateSettingsRangeLimits(old_low, old_high, new_low, new_high);

                    // if its a range, move the paired dot now, skip it later
                    if (old_high != old_low) {
                        moveDotToLayer(dot->getPair(), new_high);
                        int index = selection_copy.indexOf(dot->getPair());
                        selection_copy.removeAt(index);
                    }
                }
            }
        }
    }
    else if (m_should_deselect && m_last_clicked_dot != nullptr && m_last_clicked_dot->isSelected()) {
        // if only one dot or group or range is selected, then deselect it
        // if multiple things are selected, deselect everything and reselect the clicked-on thing
        int selection_count = 1;
        if (m_last_clicked_dot->getRange() != nullptr)
            selection_count = 2;
        else if (m_last_clicked_dot->getGroup() != nullptr)
            selection_count = m_last_clicked_dot->getGroup()->grouped.size();

        if (m_selection.size() > selection_count) {
            clearSelection();
            selectDot(m_last_clicked_dot);
        }
        else
            deselectDot(m_last_clicked_dot);
    }
    changeSelectedSettings();
}

void LayerBar::mouseMoveEvent(QMouseEvent* event) {
    if (!m_selection.isEmpty() && event->buttons() & Qt::LeftButton &&
        QGuiApplication::keyboardModifiers() == Qt::NoModifier) {
        m_should_deselect = false;
        int relative_origin = m_last_clicked_dot->y();
        if (!m_track_change) {
            m_original_y = relative_origin;
            m_track_change = true;
        }

        // ensure that none of the selected dots are moved out of bounds when clicking & dragging dots
        LayerDot* highest_dot = m_selection[0];
        LayerDot* lowest_dot = m_selection[0];
        int min = lowest_dot->getLayer();
        int max = highest_dot->getLayer();
        for (auto d : m_selection) {
            if (d->getLayer() > max) {
                max = d->getLayer();
                highest_dot = d;
            }
            else if (d->getLayer() < min) {
                min = d->getLayer();
                lowest_dot = d;
            }
        }

        int new_low_coord = event->y() + (lowest_dot->y() - relative_origin) - 10;
        int new_high_coord = event->y() + (highest_dot->y() - relative_origin) - 10;
        int adjusted_y = event->y();

        // if the move will put a dot out-of-bounds, adjust the move amount
        if (new_low_coord > getPositionFromLayer(0))
            adjusted_y -= (new_low_coord - getPositionFromLayer(0));
        else if (new_high_coord < getPositionFromLayer(m_layers - 1))
            adjusted_y += (getPositionFromLayer(m_layers - 1) - new_high_coord);

        // For each selected dot, try to move them to the appropriate location.
        for (LayerDot* dot : m_selection) {
            int y_coord = adjusted_y + (dot->y() - relative_origin) - 10;
            int layer = getLayerFromPosition(y_coord);

            // move
            dot->move(dot->x(), y_coord);
            dot->setDisplayLayer(layer);
        }

        // If the dot is too small have a number float the number in a tooltip.
        if (m_last_clicked_dot->m_shrink >= 4)
            QToolTip::showText(event->screenPos().toPoint(),
                               QString::number(m_last_clicked_dot->getDisplayLayer() + 1));

        update();
    }
}

void LayerBar::contextMenuEvent(QContextMenuEvent* event) {
    QMenu menu(this);

    if (m_selection.size() > 1) {
        if (m_selection.size() == 2) {
            // Get access to the two dots.
            LayerDot* a = m_selection[0];
            LayerDot* b = m_selection[1];

            // If both dots have no pair or group, then they are canidates for joining.
            // If the dots are in the same pair, then add the split action.
            if (a->getPair() == nullptr && b->getPair() == nullptr && a->getGroup() == nullptr &&
                b->getGroup() == nullptr) {
                menu.addAction(m_join_act);
            }
            else if (a->getRange() == b->getRange() && a->getGroup() == nullptr && b->getGroup() == nullptr) {
                menu.addAction(m_split_act);
                menu.addAction(m_set_range_layers_act);
            }
        }

        // if any selected dot is in a group or pair, then the dots CAN'T be group
        // if all selected dots are in the same group, then the dots CAN be ungrouped
        dot_group* check_group = m_selection[0]->getGroup();
        bool can_group = (check_group == nullptr);   // assume we can group, until we find a dot in a pair or group
        bool can_ungroup = (check_group != nullptr); // assume we can ungroup if the first dot is in a group, until
                                                     //   we find a dot that isn't in that group
        for (LayerDot* dot : m_selection) {
            if (dot->getGroup() != nullptr) {
                can_group = false;
                if (check_group != dot->getGroup())
                    can_ungroup = false;
            }
            else if (dot->getPair() != nullptr)
                can_group = true;
        }

        // add the corresponding actions to the menu
        if (can_group)
            menu.addAction(m_group_dots);

        if (can_ungroup) {
            menu.addAction(m_ungroup_dots);
        }

        menu.addSeparator();
    }
    else if (m_selection.size() == 1) {
        menu.addAction(m_set_layer_act);
        menu.addAction(m_pair_from_one);
        menu.addSeparator();
    }

    if (m_selection.size() > 0) {
        menu.addAction(m_delete_act);
        menu.addAction(m_clear_act);
        menu.addSeparator();
    }

    menu.addAction(m_select_all);
    menu.addAction(m_add_act);
    menu.addAction(m_add_pair);
    menu.addAction(m_add_group);

    menu.exec(event->globalPos());
}

void LayerBar::resizeEvent(QResizeEvent* event) {
    // Recalculate the following to use in movement.
    m_px_divs = ((float)(this->height() - (m_layers * 3) - 6) / (m_layers + 1)) + 1;
    m_px_divs_inc = m_px_divs + 2;

    if (m_px_divs < 30)
        m_skip = static_cast<int>(30 / m_px_divs);

    // Update all dots to their correct position.
    for (LayerDot* dot : m_position) {
        if (dot != nullptr) {
            int y = getPositionFromLayer(dot->getLayer());

            dot->move(dot->x(), y);
        }
    }
}

// Paint the tick marks for the background.
void LayerBar::paintDivisions(QPainter* painter) {
    const int maj_hz_pad = 3;
    const int min_hz_pad = 7;
    const int bar_size = 3;

    painter->setPen(PreferencesManager::getInstance()->getTheme().getLayerbarMajorColor());

    // m_px_divs = size BETWEEN divisions, m_px_divs_inc = size INCLUDING divisions
    m_px_divs = ((float)(this->height() - (m_layers * bar_size) - (bar_size * 2)) / (m_layers + 1)) + 1;
    m_px_divs_inc = m_px_divs + 2;

    // Adjust the skip value based on the available room.
    int minor_skip = 1;
    if (m_px_divs < 60) {
        if (m_px_divs < 5) {
            minor_skip = static_cast<int>(5 / m_px_divs);
        }

        m_skip = static_cast<int>(60 / m_px_divs);
    }
    else
        m_skip = 1;

    // Draw the divisions lines.
    float loc = 2;
    for (int i = 0; i < m_layers; ++i) {
        if (i % m_skip) {
            if (i % minor_skip) {
                loc += m_px_divs + 2;
            }
            else {
                // Draw the minor line.
                painter->setPen(PreferencesManager::getInstance()->getTheme().getLayerbarMinorColor());
                loc += m_px_divs;
                painter->drawLine(min_hz_pad, loc + 0.5, this->width() - min_hz_pad, loc + 0.5);
                painter->setPen(PreferencesManager::getInstance()->getTheme().getLayerbarMinorColor());
                loc += 2;
            }
        }
        else {
            // Draw the major line.
            loc += m_px_divs;
            painter->drawLine(maj_hz_pad, loc + 0.5, this->width() - maj_hz_pad, loc + 0.5);

            // Increment to get to beginning of next section.
            loc += 2;

            // Draw the layer text.
            if ((this->height() - loc) > 15) {
                painter->setPen(Qt::darkGray);
                painter->drawText(QRect(this->width() / 2 + 2, loc + 4, this->width() / 2 - 2, 10), Qt::AlignCenter,
                                  QString::number(m_layers - i));
                painter->setPen(PreferencesManager::getInstance()->getTheme().getLayerbarMajorColor());
            }
        }
    }

    // Draw the middle.
    painter->drawLine(this->width() / 2, 0, this->width() / 2, this->height());
}

// Paint the ranges for the dots, if there are any.
void LayerBar::paintRanges(QPainter* painter) {
    if (m_part != nullptr) {
        // Setup fill pattern.
        QColor fill_color(PreferencesManager::getInstance()->getTheme().getDotPairedColor());
        fill_color.setAlpha(50);
        QBrush filler(fill_color, Qt::SolidPattern);

        // loop through all the ranges on the part
        // if the range spans multiple layers,
        // draw the connector rectangle
        for (auto settings_range : m_part->getSettingsRanges()) {
            if (!settings_range->isSingle()) {
                LayerDot* low_dot = m_position[settings_range->low()];
                dot_range* range = low_dot->getRange();

                // Determine edges of ranges.
                QPoint a_point, b_point;
                a_point.setX(range->a->x());
                a_point.setY(range->a->y() + range->a->height() / 2);
                b_point.setX(range->b->x() + range->b->width());
                b_point.setY(range->b->y() + range->b->height() / 2);
                QRect range_area(a_point, b_point);

                // Draw the range.
                painter->setBrush(filler);
                painter->setPen(Qt::NoPen);
                painter->drawRect(range_area);
            }
        }
    }
}

LayerDot* LayerBar::addDot(int layer, bool from_template) {
    LayerDot* dot =
        new LayerDot(this, layer, PreferencesManager::getInstance()->getTheme().getDotColors(), from_template);
    dot->move((this->width() / 2) - (dot->width() / 2), -20);

    if (this->moveDotToLayer(dot, layer)) // successfully moved to layer
    {
        this->clearSelection();

        // Insert the dot into the appropriate set and position.
        m_position[dot->getLayer()] = dot;
        dot->show();
    }
    else // couldn't move to layer, so backtrack
    {
        delete dot;
        dot = nullptr;
    }
    return dot;
}

void LayerBar::deleteSingle(LayerDot* dot) {
    // Remove the dot from internal containers.
    int layer = dot->getLayer();
    m_position[layer] = nullptr;

    // When the animation finishes, delete the dot.
    connect(dot->m_move_ani, &QPropertyAnimation::finished, dot, &LayerDot::deleteLater);

    dot->smoothMove(dot->x(), this->height());

    if (dot->getRange() != nullptr) // If this dot is in a range, delete the range.
    {
        dot_range* range = dot->getRange();

        // When a dot in a range is deleted, this makes a new single and deletes a range.
        // keep the settings base from the range to add to new single
        QSharedPointer<SettingsBase> sb = m_part->getSettingsRange(range->a->getLayer(), range->b->getLayer())->getSb();
        m_part->removeSettingsRange(range->a->getLayer(), range->b->getLayer());
        m_part->createSettingsRange(dot->getPair()->getLayer(), dot->getPair()->getLayer(), sb);

        // Remove the range.
        dot->getPair()->setRange(nullptr);
        delete range;
    }
    else if (dot->getGroup() != nullptr) // dot is in group, remove it from group before deleting dot
    {
        dot_group* group = dot->getGroup();
        group->grouped.removeAt(group->grouped.indexOf(dot));
        m_part->removeSettingsRange(dot->getLayer(), dot->getLayer());

        if (group->grouped.size() < 0)
            delete group;
    }
    else // just a single dot, delete it
    {
        m_part->removeSettingsRange(dot->getLayer(), dot->getLayer());
    }
}

void LayerBar::removeFromGrp(LayerDot* dot) {
    QMessageBox msgRemove;
    msgRemove.setText("Remove this layer from the group?");
    msgRemove.setInformativeText(
        "You have attempted to deselect a specific layer in a group, which will remove it from the group.");
    msgRemove.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    msgRemove.setDefaultButton(QMessageBox::Cancel);
    int ret = msgRemove.exec();
    switch (ret) {
        case QMessageBox::Ok:
            // Remove was clicked
            dot->getGroup()->grouped.removeAt(dot->getGroup()->grouped.indexOf(dot));
            dot->setGroup(nullptr);
            dot->setSelected(false);
            m_selection.removeAt(m_selection.indexOf(dot));
        case QMessageBox::Cancel:
            // Cancel was clicked
            break;
        default:
            // should never be reached
            break;
    }
}

void LayerBar::updateLayers() {
    if (m_part.isNull()) {
        m_layers = 0;
        return;
    }

    // Retrieve the slicing vector
    QVector3D slicing_vector = {GSM->getGlobal()->setting<float>(PS::SlicingVector::kSlicingVectorX),
                                GSM->getGlobal()->setting<float>(PS::SlicingVector::kSlicingVectorY),
                                GSM->getGlobal()->setting<float>(PS::SlicingVector::kSlicingVectorZ)};
    slicing_vector.normalize();

    // Retrieve the part min and max in the slicing plane normal direction
    auto [part_min, part_max] = m_part->rootMesh()->getAxisExtrema(slicing_vector);

    // Create the slicing plane
    Plane slicing_plane(part_min, slicing_vector);

    Distance global_layer_height;
    if (m_part->getSb()->contains(PS::Layer::kLayerHeight)) {
        global_layer_height = m_part->getSb()->setting<Distance>(PS::Layer::kLayerHeight);
    }
    else {
        global_layer_height = GSM->getGlobal()->setting<Distance>(PS::Layer::kLayerHeight);
    }

    int layer_count = 0;
    // if valid config ( normal and axis not perpendicular)
    if (global_layer_height > 0) {
        Point part_min, part_max;
        std::tie(part_min, part_max) = m_part->rootMesh()->getAxisExtrema(slicing_plane.normal());

        // move slicing plane to start at min on the part
        slicing_plane.point(part_min);

        Distance part_height = slicing_plane.distanceToPoint(part_max);

        Point g_direction = part_min + (slicing_vector * global_layer_height());
        global_layer_height = slicing_plane.distanceToPoint(g_direction); // height in direction normal to slicing plane

        // count the layers
        Distance current_height = 0;
        layer_count = 0;
        auto ranges = m_part->getSettingsRanges();
        bool is_in_range = false;
        uint range_id;

        while (current_height < part_height) {
            // determine if layer is in a range, and if so, which range
            is_in_range = false;
            for (auto i = ranges.begin(), end = ranges.end(); i != end; ++i) {
                auto range = i.value();
                if (range->includesIndex(layer_count)) {
                    if (!is_in_range) {
                        // this is the first range we've found that contains the layer
                        is_in_range = true;
                        range_id = i.key();
                    }
                    else {
                        // The current layer is in multiple ranges
                        // Precedenc: Use the narrowest range that this layer belongs too
                        //    - single layer range is preferred to multi-layer range
                        //    - the multi-layer range with the highest low-index is applied
                        //    - for multi-layer ranges with same low-index, the range with
                        //      the lower high index will be selected
                        auto old_range = ranges[range_id];
                        if (range->isSingle() ||               // new range is single, top priority
                            range->low() > old_range->low() || // new range has higher low than old
                            old_range->low() == range->low() &&
                                range->high() < old_range->high()) // new range is narrower
                        {
                            range_id = i.key();
                        }
                        // otherwise the old range has priority, no need to change anything
                    }
                }
            }

            // If current layer is in range that modifies the layer_height, use range height otherwise use global/part
            // height
            if (is_in_range && ranges[range_id]->getSb()->contains(PS::Layer::kLayerHeight)) {
                Distance range_height = ranges[range_id]->getSb()->setting<Distance>(PS::Layer::kLayerHeight);
                Point p = slicing_plane.point() + (slicing_vector * range_height());
                range_height = slicing_plane.distanceToPoint(p); // height in direction normal to slicing plane

                current_height += range_height;
            }
            else {
                current_height += global_layer_height;
            }

            ++layer_count;
        }

        // if the part is not an even division of the layer height, it is possible to over count
        // Subtract the last layer if its cross-section (taken at the middle of the layer) is above the part height
        Distance last_layer_height;
        if (is_in_range && ranges[range_id]->getSb()->contains(PS::Layer::kLayerHeight)) {
            Point p = slicing_plane.point() +
                      (slicing_vector * ranges[range_id]->getSb()->setting<Distance>(PS::Layer::kLayerHeight)());
            last_layer_height = slicing_plane.distanceToPoint(p);
        }
        else {
            last_layer_height = global_layer_height;
        }

        if ((current_height - part_height) >= (last_layer_height / 2.0)) {
            --layer_count;
        }
        m_layers = layer_count;
    }
    else {
        m_layers = 0;
    }
    // if the number of layers decreased, remove any dots from
    // layers that don't exist anymore
    if (layer_count < m_position.size()) {
        for (int i = m_position.size() - 1; i >= layer_count; --i) {
            if (m_position[i] != nullptr) {
                int layer_num = m_position[i]->getLayer();
                LayerDot* dot = m_position[i]->getPair();
                if (dot == nullptr) {
                    storeDeletedLayers(layer_num, layer_num);
                }
                else {
                    int pair = dot->getLayer();
                    storeDeletedLayers(layer_num, pair);
                }
                deleteSingle(m_position[i]);
            }
        }
    }
    int old_size = m_position.size();
    m_position.resize(m_layers);
    if (old_size < m_position.size()) {
        if (m_deleted_ranges.size() > 0) {
            returnDeletedLayers();
        }
    }

    double layers = (m_layers <= 30) ? m_layers : 30;
    m_px_divs = height() / layers;

    resizeEvent(nullptr);
    update();
}

bool LayerBar::sortByTop(SettingsRange a, SettingsRange b) { return a.high() > b.high(); }

QVector<SettingsRange> LayerBar::getDeletedRanges() { return m_deleted_ranges; }

void LayerBar::storeDeletedLayers(int layer_number, int pair) {
    QVector<SettingsRange> sr = GSM->getLayerSettings();
    for (int i = 0, end = sr.size(); i < end; ++i) {
        // Since deleted layers sorted by top, layer_number is high and its pair is low.
        if (sr[i].low() == pair && sr[i].high() == layer_number) {
            m_deleted_ranges.push_back(sr[i]);
            break;
        }
    }
    // Sort by top value so that layers brought back in order
    std::sort(m_deleted_ranges.begin(), m_deleted_ranges.end(), sortByTop);
}

void LayerBar::returnDeletedLayers() {
    while (m_deleted_ranges.size() > 0 && m_deleted_ranges[m_deleted_ranges.size() - 1].high() < m_layers) {
        if (m_deleted_ranges[m_deleted_ranges.size() - 1].low() ==
            m_deleted_ranges[m_deleted_ranges.size() - 1].high()) { // Bring back single
            addSingleFromTemplate(m_deleted_ranges[m_deleted_ranges.size() - 1].low(),
                                  m_deleted_ranges[m_deleted_ranges.size() - 1].getSb());
            m_deleted_ranges.pop_back();
        }
        else { // Bring back range
            addRangeFromTemplate(m_deleted_ranges[m_deleted_ranges.size() - 1].low(),
                                 m_deleted_ranges[m_deleted_ranges.size() - 1].high(),
                                 m_deleted_ranges[m_deleted_ranges.size() - 1].getSb());
            m_deleted_ranges.pop_back();
        }
    }
}

void LayerBar::clear() {
    m_selection.clear();
    m_position.clear();
    m_part = nullptr;

    update();
}

void LayerBar::clearTemplate() {
    // m_position.clear();
    m_deleted_ranges.clear();
}

void LayerBar::deleteRange(LayerBar::dot_range* range) {
    // should potentially delete range->a and range->b ...?
    range->a->setRange(nullptr);
    range->b->setRange(nullptr);

    m_part->removeSettingsRange(range->a->getLayer(), range->b->getLayer());

    delete range;
}

void LayerBar::splitRange(LayerBar::dot_range* range) {
    range->a->setRange(nullptr);
    range->b->setRange(nullptr);

    m_part->splitSettingsRange(range->a->getLayer(), range->b->getLayer());

    delete range;
}

void LayerBar::handleModifiedSetting(QString key) {
    if (key == PS::Layer::kLayerHeight || key == PS::SlicingVector::kSlicingVectorX ||
        key == PS::SlicingVector::kSlicingVectorY || key == PS::SlicingVector::kSlicingVectorZ) {
        updateLayers();
    }
}

bool LayerBar::layerValid(int layer) {
    // Check that layer is in bounds
    // and there isn't a dot there already.
    return layer >= 0 && layer < m_layers && m_position[layer] == nullptr;
}

int LayerBar::getLayerFromPosition(int y_coord) {
    int coord_layer = static_cast<int>((y_coord + (m_px_divs_inc / 2) + 10) / m_px_divs_inc);
    // The coordinate system QT uses is downward as negative.
    // Therefore, the actual layer is the total number of layers minus the calculated layer.
    return m_layers - coord_layer;
}

int LayerBar::getPositionFromLayer(int layer) {
    // Constant -10 here to shift dots 1/2 of their heights up. This allows them to fall on the correct line.
    return static_cast<int>(((m_layers - layer) * m_px_divs_inc) - 10);
}

bool LayerBar::moveDotToLayer(LayerDot* dot, int layer) {
    bool moved_dot = false;

    if (layerValid(layer)) // a valid layer is in bounds and has no dot on it
    {
        m_position[dot->getLayer()] = nullptr;
        m_position[layer] = dot;

        int x = (this->width() / 2) - (dot->width() / 2);
        int y = getPositionFromLayer(layer);

        dot->smoothMove(x, y);
        dot->setLayer(layer);

        update();

        moved_dot = true;
    }

    return moved_dot;
}

bool LayerBar::moveDotToNextLayer(LayerDot* dot) { return moveDotToNextLayer(dot, getLayerFromPosition(dot->y())); }

bool LayerBar::moveDotToNextLayer(LayerDot* dot, int layer) {
    int check_dist = 0;
    int init_layer = dot->getLayer();

    while (true) {
        // Check up then down for each check distance.
        if (moveDotToLayer(dot, layer + check_dist))
            break;

        if (moveDotToLayer(dot, layer - check_dist))
            break;

        check_dist++;
    }

    // Return true if dot moved, false otherwise
    return dot->getLayer() == init_layer;
}

bool LayerBar::onLayer(LayerDot* dot, int layer) {
    // returns true if layer is inside bounds, and the given dot sits on that layer
    // otherwise, false
    return layer > 0 && layer <= m_layers && m_position[layer] == dot;
}

void LayerBar::changeSelectedSettings() {
    QVector<QString> names;
    QList<QSharedPointer<SettingsBase>> settings_bases;

    QVector<LayerDot*> selected_copy = m_selection;
    // sort from low to high, so that low value is always found first
    std::sort(selected_copy.begin(), selected_copy.end(),
              [](LayerDot* a, LayerDot* b) { return a->getLayer() < b->getLayer(); }); // sort low to high

    for (int i = 0; i < selected_copy.size(); ++i) // intentionally re-evaluating array size bc it will change
    {
        LayerDot* dot = selected_copy[i];

        if (dot->getRange() != nullptr) {
            int low = dot->getLayer();
            int high = dot->getPair()->getLayer();

            selected_copy.removeAt(selected_copy.indexOf(dot->getPair()));

            auto range = m_part->getSettingsRange(low, high);
            QString name = "Layers " + QString::number(low + 1) + " - " + QString::number(high + 1);
            names.append(name);
            settings_bases.append(range->getSb());
        }
        else if (dot->getGroup() != nullptr) {
            // get the dot group & its name
            // add the group name to the list
            auto dot_group = dot->getGroup();
            QString name = dot_group->group_name;
            names.append(name);

            // dots in the same group share a pointer to the same settings base
            // so just get the sb of one dot
            auto range = m_part->getSettingsRange(dot->getLayer(), dot->getLayer());
            settings_bases.append(range->getSb());

            // remove all the dots in the group from selection copy
            for (int j = selected_copy.size() - 1; j > i; --j) {
                if (selected_copy[j]->getGroup() != nullptr && selected_copy[j]->getGroup()->group_name == name)
                    selected_copy.removeAt(j);
            }
        }
        else {
            int layer_num = dot->getLayer();
            auto range = m_part->getSettingsRange(layer_num, layer_num);
            QString name = "Layer " + QString::number(layer_num + 1);
            names.append(name);
            settings_bases.append(range->getSb());
        }
    }

    QString final;
    if (names.size() == 0) // no selected ranges, change back to part settings
    {
        final = m_part->name();
        settings_bases.append(m_part->getSb());
    }
    else if (names.size() == 1) // just one selected range, display it
    {
        final = names[0];
        final += " of " + m_part->name();
    }
    else if (names.size() <= 3) // several selected ranges, display their names concatenated
    {
        for (int i = 0, end = names.size(); i < end; ++i) {
            if (i > 0)
                final += ", ";
            final += names[i];
        }
        final += " of " + m_part->name();
    }
    else // too many ranges to display all the names
    {
        final = "Multiple ranges";
        final += " of " + m_part->name();
    }

    emit setSelectedSettings(qMakePair(final, settings_bases));
}

void LayerBar::clearSelection() {
    for (LayerDot* curr_dot : m_selection)
        curr_dot->setSelected(false);

    m_selection.clear();
    changeSelectedSettings();
}

void LayerBar::selectAll() {
    for (LayerDot* dot : m_position) {
        if (dot != nullptr)
            selectDot(dot);
    }
    changeSelectedSettings();
}

void LayerBar::selectOnInterval(int min, int max) {
    for (int i = min; i <= max; i++) {
        LayerDot* dot = m_position[i];
        if (dot != nullptr)
            selectDot(dot);
    }
    changeSelectedSettings();
}

void LayerBar::selectDot(LayerDot* dot) {
    if (dot->getGroup() != nullptr) // dot it group, select all dots in group
    {
        QVector<LayerDot*> grouped_dots = dot->getGroup()->grouped;
        for (auto d : grouped_dots) {
            if (!d->isSelected()) {
                d->setSelected(true);
                m_selection.append(d);
            }
        }
    }
    else if (dot->getRange() != nullptr) // dot in range, select its pair too
    {
        if (!dot->isSelected()) {
            dot->setSelected(true);
            m_selection.append(dot);
        }

        LayerDot* paired_dot = dot->getPair();
        if (!paired_dot->isSelected()) {
            paired_dot->setSelected(true);
            m_selection.append(paired_dot);
        }
    }
    else // just a single dot
    {
        if (!dot->isSelected()) {
            dot->setSelected(true);
            m_selection.append(dot);
        }
    }
}

void LayerBar::deselectDot(LayerDot* dot) {
    if (dot->getGroup() != nullptr) // dot in group, deselect all dots in group
    {
        QVector<LayerDot*> grouped_dots = dot->getGroup()->grouped;
        for (auto d : grouped_dots) {
            if (d->isSelected()) {
                d->setSelected(false);
                m_selection.removeAt(m_selection.indexOf(d));
            }
        }
    }
    else if (dot->getRange() != nullptr) // dot in range, deselect its pair too
    {
        if (dot->isSelected()) {
            dot->setSelected(false);
            m_selection.removeAt(m_selection.indexOf(dot));
        }

        LayerDot* paired_dot = dot->getPair();
        if (paired_dot->isSelected()) {
            paired_dot->setSelected(false);
            m_selection.removeAt(m_selection.indexOf(paired_dot));
        }
    }
    else // just a single dot
    {
        if (dot->isSelected()) {
            dot->setSelected(false);
            m_selection.removeAt(m_selection.indexOf(dot));
        }
    }
}

void LayerBar::setupActions() {
    // Setup actions.
    m_set_layer_act = new QAction("Set Layer Number", this);
    m_set_range_layers_act = new QAction("Set Range Layer Numbers", this);
    m_delete_act = new QAction("Delete Selected Layer Settings", this);
    m_add_act = new QAction("Add Layer Settings", this);
    m_add_group = new QAction("Add a Group of Layer Settings", this);
    m_join_act = new QAction("Pair Selected Layers", this);
    m_split_act = new QAction("Split Pair", this);
    m_add_pair = new QAction("Add a Range of Layer Settings", this);
    m_pair_from_one = new QAction("Select a Range from This Layer", this);
    m_clear_act = new QAction("Clear Selection", this);
    m_group_dots = new QAction("Group Selected Layer Settings", this);
    m_ungroup_dots = new QAction("Ungroup Selected Layer Settings", this);
    m_select_all = new QAction("Select All", this);

    // Connect our actions to our signals.
    connect(m_set_layer_act, &QAction::triggered, this, &LayerBar::setLayer);
    connect(m_set_range_layers_act, &QAction::triggered, this, &LayerBar::setPairLayers);
    connect(m_delete_act, &QAction::triggered, this, &LayerBar::deleteSelection);
    connect(m_add_act, &QAction::triggered, this, &LayerBar::addSelection);
    connect(m_add_group, &QAction::triggered, this, &LayerBar::addGroup);
    connect(m_join_act, &QAction::triggered, this, &LayerBar::makePair);
    connect(m_split_act, &QAction::triggered, this, &LayerBar::splitPair);
    connect(m_add_pair, &QAction::triggered, this, &LayerBar::addPair);
    connect(m_pair_from_one, &QAction::triggered, this, &LayerBar::addPair);
    connect(m_clear_act, &QAction::triggered, this, &LayerBar::clearSelection);
    connect(m_group_dots, &QAction::triggered, this, &LayerBar::groupDots);
    connect(m_ungroup_dots, &QAction::triggered, this, &LayerBar::ungroupDots);
    connect(m_select_all, &QAction::triggered, this, &LayerBar::selectAll);
}

} // namespace ORNL
