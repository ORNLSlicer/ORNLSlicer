#include "windows/settings_overrides.h"

#include <QApplication>
#include <QClipboard>
#include <QCollator>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollBar>
#include <QSet>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

#include "configs/setting_activity.h"
#include "managers/preferences_manager.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "utilities/constants.h"

namespace ORNL {
namespace {
enum RowData {
    PartIdRole = Qt::UserRole + 1,
    LowRole,
    HighRole,
    KnownSettingRole,
    IdentityRole,
    SortKindRole,
    SortNumberRole
};
enum SortKind { MissingValue, NumericValue, TextValue };

const QCollator& naturalCollator() {
    static const QCollator collator = [] {
        // The C locale does not support numeric collation.
        QCollator result(QLocale().language() == QLocale::C ? QLocale(QLocale::English) : QLocale());
        result.setNumericMode(true);
        result.setCaseSensitivity(Qt::CaseInsensitive);
        return result;
    }();
    return collator;
}

class OverrideItem : public QTableWidgetItem {
   public:
    explicit OverrideItem(const QString& text) : QTableWidgetItem(text) {
        setData(SortKindRole, TextValue);
    }

    bool operator<(const QTableWidgetItem& other) const override {
        if (column() == 1) {
            const int low       = data(LowRole).toInt();
            const int other_low = other.data(LowRole).toInt();
            if (low != other_low) return low < other_low;
            const int high       = data(HighRole).toInt();
            const int other_high = other.data(HighRole).toInt();
            if (high != other_high) return high < other_high;
        }
        const int kind       = data(SortKindRole).toInt();
        const int other_kind = other.data(SortKindRole).toInt();
        if (kind != other_kind) return kind < other_kind;
        if (kind == NumericValue) {
            const double number       = data(SortNumberRole).toDouble();
            const double other_number = other.data(SortNumberRole).toDouble();
            if (number != other_number) return number < other_number;
        }
        return naturalCollator().compare(text(), other.text()) < 0;
    }
};

QString metadataText(const fifojson& metadata, const std::string& key, const QString& fallback = {}) {
    const auto value = metadata.find(key);
    return value != metadata.end() && value->is_string() ? value->get<QString>() : fallback;
}

QString valueText(const fifojson& value, const fifojson& metadata, QVariant* sort_number) {
    if (value.is_null()) return "Not set";
    if (value.is_string()) return value.get<QString>();
    if (value.is_boolean()) {
        *sort_number = value.get<bool>() ? 1.0 : 0.0;
        return value.get<bool>() ? "true" : "false";
    }

    const QString type = metadataText(metadata, Constants::Settings::Master::kType);
    if (type == "enumeration" && value.is_number_integer()) {
        const QStringList options = metadataText(metadata, Constants::Settings::Master::kOptions).split(',');
        const int index           = value.get<int>();
        if (index >= 0 && index < options.size()) return options.at(index).trimmed();
    }
    if (!value.is_number()) return QString::fromStdString(value.dump());

    const auto preferences = PreferencesManager::getInstance();
    double number          = value.get<double>();
    QString suffix;
    if (type == "distance" || type == "location") {
        number = Distance(number).to(preferences->getDistanceUnit());
        suffix = preferences->getDistanceUnitText();
    }
    else if (type == "speed") {
        number = Velocity(number).to(preferences->getVelocityUnit());
        suffix = preferences->getVelocityUnitText();
    }
    else if (type == "accel") {
        number = Acceleration(number).to(preferences->getAccelerationUnit());
        suffix = preferences->getAccelerationUnitText();
    }
    else if (type == "density") {
        number = Density(number).to(preferences->getDensityUnit());
        suffix = preferences->getDensityUnitText();
    }
    else if (type == "angle") {
        number = Angle(number).to(preferences->getAngleUnit());
        suffix = preferences->getAngleUnitText();
    }
    else if (type == "time") {
        number = Time(number).to(preferences->getTimeUnit());
        suffix = preferences->getTimeUnitText();
    }
    else if (type == "temperature") {
        number = Temperature(number).to(preferences->getTemperatureUnit());
        suffix = preferences->getTemperatureUnitText();
    }
    else if (type == "voltage") {
        number = Voltage(number).to(preferences->getVoltageUnit());
        suffix = preferences->getVoltageUnitText();
    }
    else if (type == "area") {
        number = Area(number).to(preferences->getDistanceUnit() * preferences->getDistanceUnit());
        suffix = preferences->getDistanceUnitText() + "²";
    }
    else if (type == "ang_vel") {
        number = AngularVelocity(number).to(preferences->getAngleUnit() / preferences->getTimeUnit());
        suffix = preferences->getAngleUnitText() + "/" + preferences->getTimeUnitText();
    }
    else if (type == "rpm") { suffix = "rpm"; }
    else if (type == "percentage" || type == "percentage100") { suffix = "%"; }

    if (std::isfinite(number)) *sort_number = number;
    return QString::number(number, 'g', 12) + (suffix.isEmpty() ? QString() : " " + suffix);
}

QString escapedCell(QString text, QChar delimiter) {
    if (!text.contains(delimiter) && !text.contains('"') && !text.contains('\r') && !text.contains('\n')) return text;
    text.replace("\"", "\"\"");
    return '"' + text + '"';
}
}  // namespace

SettingsOverridesDialog::SettingsOverridesDialog(QWidget* parent) : QDialog(parent, Qt::Window) {
    setWindowTitle(QApplication::applicationDisplayName() + ": Settings Overrides");
    setWindowIcon(QIcon(":/icons/ornlslicer_logo.png"));
    setMinimumSize(850, 400);
    resize(1200, 650);

    auto* description = new QLabel(
        "Part overrides replace global values. Layer overrides replace part values. "
        "Select an override to show it in Settings. This overview updates automatically.",
        this);
    description->setWordWrap(true);
    m_filter = new QLineEdit(this);
    m_filter->setObjectName("settings_overrides_filter");
    m_filter->setPlaceholderText("Search parts, layers, categories, settings, or values...");
    m_filter->setClearButtonEnabled(true);
    m_part_filter = new QComboBox(this);
    m_part_filter->setObjectName("settings_overrides_part_filter");
    m_part_filter->addItem("All Parts", QUuid());
    m_scope_filter = new QComboBox(this);
    m_scope_filter->setObjectName("settings_overrides_scope_filter");
    m_scope_filter->addItems({"All", "Part", "Layer"});
    auto* filters = new QHBoxLayout();
    filters->addWidget(new QLabel("Part:", this));
    filters->addWidget(m_part_filter, 1);
    filters->addWidget(new QLabel("Scope:", this));
    filters->addWidget(m_scope_filter);
    m_summary = new QLabel(this);
    m_summary->setObjectName("settings_overrides_summary");
    m_summary->setWordWrap(true);

    m_table = new QTableWidget(0, 8, this);
    m_table->setObjectName("settings_overrides_table");
    m_table->setHorizontalHeaderLabels(
        {"Part", "Scope", "Category", "Setting", "Global Value", "Value Being Replaced", "Override Value", "Status"});
    m_table->horizontalHeaderItem(5)->setToolTip(
        "The value used without this override. Hover over a value to see its source.");
    m_table->setAlternatingRowColors(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->horizontalHeaderItem(7)->setToolTip(
        "Feature conditions for this part or layer scope, evaluated before slicing. Hover for the reason.");
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->verticalHeader()->hide();
    m_table->setWordWrap(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setSortIndicator(0, Qt::AscendingOrder);
    m_table->installEventFilter(this);
    m_table->viewport()->installEventFilter(this);

    auto* buttons         = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_show_setting_button = buttons->addButton("Show in Settings", QDialogButtonBox::ActionRole);
    m_show_setting_button->setObjectName("settings_overrides_show_setting");
    m_show_setting_button->setAutoDefault(false);
    m_copy_button = buttons->addButton("Copy Selected", QDialogButtonBox::ActionRole);
    m_copy_button->setObjectName("settings_overrides_copy");
    m_copy_button->setAutoDefault(false);
    m_export_button = buttons->addButton("Export CSV...", QDialogButtonBox::ActionRole);
    m_export_button->setObjectName("settings_overrides_export");
    m_export_button->setAutoDefault(false);
    m_refresh_button = buttons->addButton("Refresh", QDialogButtonBox::ActionRole);
    m_refresh_button->setAutoDefault(false);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(description);
    layout->addWidget(m_filter);
    layout->addLayout(filters);
    layout->addWidget(m_summary);
    layout->addWidget(m_table, 1);
    layout->addWidget(buttons);

    connect(m_filter, &QLineEdit::textChanged, this, &SettingsOverridesDialog::filterRows);
    connect(m_part_filter, &QComboBox::currentIndexChanged, this, &SettingsOverridesDialog::filterRows);
    connect(m_scope_filter, &QComboBox::currentIndexChanged, this, &SettingsOverridesDialog::filterRows);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &SettingsOverridesDialog::updateNavigation);
    connect(m_table, &QTableWidget::currentItemChanged, this, &SettingsOverridesDialog::updateNavigation);
    connect(m_table, &QTableWidget::itemDoubleClicked, this, [this](QTableWidgetItem* item) {
        if (m_table->isRowHidden(item->row())) return;
        m_table->clearSelection();
        m_table->setCurrentCell(item->row(), 3);
        m_table->selectRow(item->row());
        showSelectedSetting();
    });
    connect(m_show_setting_button, &QPushButton::clicked, this, &SettingsOverridesDialog::showSelectedSetting);
    connect(m_copy_button, &QPushButton::clicked, this, &SettingsOverridesDialog::copySelectedRows);
    connect(m_export_button, &QPushButton::clicked, this, &SettingsOverridesDialog::chooseCsvFile);
    connect(m_refresh_button, &QPushButton::clicked, this, &SettingsOverridesDialog::refresh);
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsOverridesDialog::close);
    m_refresh_timer = new QTimer(this);
    m_refresh_timer->setSingleShot(true);
    connect(m_refresh_timer, &QTimer::timeout, this, [this] {
        if (isVisible()) refresh();
    });
    connect(PreferencesManager::getInstance().get(), &PreferencesManager::anyUnitChanged, this,
            &SettingsOverridesDialog::scheduleRefresh);
    connect(CSM.get(), &SessionManager::partAdded, this, &SettingsOverridesDialog::scheduleRefresh,
            Qt::QueuedConnection);
    connect(CSM.get(), &SessionManager::partRemoved, this, &SettingsOverridesDialog::scheduleRefresh);
    connect(CSM.get(), &SessionManager::partsCleared, this, &SettingsOverridesDialog::scheduleRefresh);
    connect(GSM.get(), &SettingsManager::globalLoaded, this, &SettingsOverridesDialog::scheduleRefresh);
    filterRows();
}

void SettingsOverridesDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    refresh();
}

bool SettingsOverridesDialog::eventFilter(QObject* watched, QEvent* event) {
    if ((watched == m_table || watched == m_table->viewport()) &&
        (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->matches(QKeySequence::Copy)) {
            event->accept();
            if (event->type() == QEvent::KeyPress) copySelectedRows();
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void SettingsOverridesDialog::setRefreshSuspended(bool suspended) {
    m_refresh_suspended = suspended;
    m_refresh_button->setEnabled(!suspended);
    if (suspended) {
        m_refresh_timer->stop();
        m_table->setRowCount(0);
        m_summary->setText("Loading project settings...");
    }
    else if (isVisible()) { refresh(); }
    updateNavigation();
}

void SettingsOverridesDialog::scheduleRefresh() {
    if (isVisible() && !m_refresh_suspended && !m_refresh_timer->isActive()) m_refresh_timer->start(0);
}

void SettingsOverridesDialog::refresh() {
    if (m_refresh_suspended) return;

    m_refresh_timer->stop();
    QSet<QString> selected_rows;
    for (const auto& index : m_table->selectionModel()->selectedRows(3))
        selected_rows.insert(index.data(IdentityRole).toString());
    const auto* current            = m_table->item(m_table->currentRow(), 3);
    const QString current_identity = current == nullptr ? QString() : current->data(IdentityRole).toString();
    const int vertical_scroll      = m_table->verticalScrollBar()->value();
    const int horizontal_scroll    = m_table->horizontalScrollBar()->value();
    auto parts                     = CSM->partsSnapshot().values();
    std::sort(parts.begin(), parts.end(), [](const auto& first, const auto& second) {
        return naturalCollator().compare(first->name(), second->name()) < 0;
    });
    {
        const QSignalBlocker blocker(m_part_filter);
        const QUuid selected_filter = m_part_filter->currentData().toUuid();
        m_part_filter->clear();
        m_part_filter->addItem("All Parts", QUuid());
        for (const auto& part : parts) m_part_filter->addItem(part->name(), part->getId());
        m_part_filter->setCurrentIndex(std::max(0, m_part_filter->findData(selected_filter)));
    }
    m_table->setSortingEnabled(false);
    m_table->setRowCount(0);
    const auto global      = GSM->getGlobal();
    const fifojson& master = GSM->getMaster()->json();
    for (const auto& part : parts) {
        const auto ranges      = part->getSettingsRanges().values();
        const auto layer_scope = [](int low, int high) {
            return low == high ? QString("Layer %1").arg(low + 1) : QString("Layers %1-%2").arg(low + 1).arg(high + 1);
        };
        const auto add_rows = [&](const QSharedPointer<SettingsBase>& settings, const QString& scope, int low,
                                  int high) {
            if (settings->json().empty()) return;
            struct Context {
                int low;
                int high;
                SettingsBase inherited;
                SettingsBase effective;
            };
            QList<int> boundaries {low, high + 1};
            if (low >= 0) {
                for (const auto& range : ranges) {
                    if (range->low() > high || range->high() < low) continue;
                    boundaries.append(std::max(low, range->low()));
                    boundaries.append(std::min(high, range->high()) + 1);
                }
                std::sort(boundaries.begin(), boundaries.end());
                boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());
            }
            QList<Context> contexts;
            for (int index = 0; index + 1 < boundaries.size(); ++index) {
                Context context {boundaries.at(index), boundaries.at(index + 1) - 1, *global, *global};
                context.effective.populate(part->getSb());
                if (low >= 0) {
                    context.inherited.populate(part->getSb());
                    bool before_owner = true;
                    // Preserve the same QMap order used by BufferedSlicer::layerSettingsForSlice.
                    for (const auto& range : ranges) {
                        if (!range->includesIndex(context.low)) continue;
                        if (range->low() == low && range->high() == high) before_owner = false;
                        if (before_owner) context.inherited.populate(range->getSb());
                        context.effective.populate(range->getSb());
                    }
                }
                contexts.append(context);
            }
            const bool modifier_disabled = !part->modifierEnabled() && (part->getMeshType() == MeshType::kSettings ||
                                                                        part->getMeshType() == MeshType::kClipping);
            for (const auto& entry : settings->json().at(0).items()) {
                const QString key           = QString::fromStdString(entry.key());
                const auto metadata_it      = master.find(entry.key());
                const fifojson metadata     = metadata_it == master.end() ? fifojson::object() : *metadata_it;
                const fifojson global_value = global->contains(key)
                                                  ? global->setting<fifojson>(key)
                                                  : metadata.value(Constants::Settings::Master::kDefault, fifojson());
                QVariant global_number;
                QVariant inherited_number;
                QVariant override_number;
                fifojson inherited;
                QStringList inherited_values;
                QStringList inherited_details;
                QStringList activity_details;
                QString inactive_reason;
                bool overrides_inherited = false;
                int inactive_contexts    = 0;
                for (const auto& context : contexts) {
                    const fifojson value = context.inherited.contains(key)
                                               ? context.inherited.setting<fifojson>(key)
                                               : metadata.value(Constants::Settings::Master::kDefault, fifojson());
                    overrides_inherited |= entry.value() != value;
                    QVariant number;
                    const QString text = valueText(value, metadata, &number);
                    if (inherited_values.isEmpty()) {
                        inherited        = value;
                        inherited_number = number;
                    }
                    if (!inherited_values.contains(text)) inherited_values.append(text);
                    QString source = low >= 0 && part->getSb()->contains(key) ? "Part: " + part->name() : "Global";
                    QString later_source;
                    if (low >= 0) {
                        bool before_owner = true;
                        for (const auto& range : ranges) {
                            if (!range->includesIndex(context.low)) continue;
                            if (range->low() == low && range->high() == high) {
                                before_owner = false;
                                continue;
                            }
                            if (!range->getSb()->contains(key)) continue;
                            if (before_owner)
                                source = layer_scope(range->low(), range->high());
                            else
                                later_source = layer_scope(range->low(), range->high());
                        }
                    }
                    const QString context_scope = low < 0 ? "Part" : layer_scope(context.low, context.high);
                    inherited_details.append((contexts.size() > 1 ? context_scope + ": " : QString()) +
                                             "Source: " + source + "\nValue: " + text);
                    QString reason;
                    if (modifier_disabled)
                        reason = "The owning modifier is disabled.";
                    else if (!later_source.isEmpty() && context.effective.setting<fifojson>(key) != entry.value())
                        reason = "Overridden by " + later_source + ".";
                    else
                        reason =
                            SettingActivity::inactiveReason(key, context.effective, master, GSM->getSettingInput(key));
                    inactive_contexts += !reason.isEmpty();
                    inactive_reason = reason;
                    activity_details.append(
                        context_scope + ": " +
                        (reason.isEmpty() ? "The setting's feature conditions are satisfied." : reason));
                }
                // Copied values are redundant only when they match every inherited layer context.
                if (!overrides_inherited) continue;
                const bool mixed_activity = inactive_contexts > 0 && inactive_contexts < contexts.size();
                const QString status      = mixed_activity                ? "Unknown"
                                            : inactive_contexts > 0       ? "Inactive"
                                            : metadata_it == master.end() ? "Unknown"
                                                                          : "Active";
                if (mixed_activity)
                    inactive_reason = "Activity varies across this range.\n" + activity_details.join('\n');
                else if (inactive_contexts > 0 && contexts.size() > 1)
                    inactive_reason = activity_details.join('\n');
                const QString category = metadataText(metadata, Constants::Settings::Master::kMajor) + " / " +
                                         metadataText(metadata, Constants::Settings::Master::kMinor);
                const QString inherited_text =
                    inherited_values.size() > 1 ? "Varies: " + inherited_values.join("; ") : inherited_values.first();
                if (inherited_values.size() > 1) inherited_number.clear();
                const QStringList values {part->name(),
                                          scope,
                                          metadata.empty() ? "Unknown" : category,
                                          metadataText(metadata, Constants::Settings::Master::kDisplay, key),
                                          valueText(global_value, metadata, &global_number),
                                          inherited_text,
                                          valueText(entry.value(), metadata, &override_number),
                                          status};
                const int row = m_table->rowCount();
                m_table->insertRow(row);
                for (int column = 0; column < values.size(); ++column) {
                    auto* item = new OverrideItem(values.at(column));
                    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                    item->setToolTip(values.at(column));
                    m_table->setItem(row, column, item);
                }
                m_table->item(row, 1)->setData(LowRole, low);
                m_table->item(row, 1)->setData(HighRole, high);
                const QList<QVariant> numbers {global_number, inherited_number, override_number};
                const QList<fifojson> raw_values {global_value, inherited, entry.value()};
                for (int index = 0; index < 3; ++index) {
                    auto* item = m_table->item(row, index + 4);
                    item->setData(SortKindRole, index == 1 && inherited_values.size() > 1 ? TextValue
                                                : raw_values.at(index).is_null()          ? MissingValue
                                                : numbers.at(index).isValid()             ? NumericValue
                                                                                          : TextValue);
                    item->setData(SortNumberRole, numbers.at(index));
                }
                m_table->item(row, 3)->setData(Qt::UserRole, key);
                m_table->item(row, 3)->setData(PartIdRole, part->getId());
                m_table->item(row, 3)->setData(LowRole, low);
                m_table->item(row, 3)->setData(HighRole, high);
                m_table->item(row, 3)->setData(KnownSettingRole, metadata_it != master.end());
                m_table->item(row, 3)->setData(IdentityRole, part->getId().toString() + "/" + QString::number(low) +
                                                                 "/" + QString::number(high) + "/" + key);
                m_table->item(row, 3)->setToolTip(values.at(3) + "\n" + key);
                m_table->item(row, 5)->setToolTip(inherited_details.join('\n'));
                m_table->item(row, 7)->setToolTip(
                    inactive_reason.isEmpty() ? status == "Unknown"
                                                    ? "No setting metadata is available to determine activity."
                                                    : "The setting's feature conditions are satisfied in this scope."
                                              : inactive_reason);
            }
        };
        add_rows(part->getSb(), "Part", -1, -1);
        auto display_ranges = ranges;
        std::sort(display_ranges.begin(), display_ranges.end(), [](const auto& first, const auto& second) {
            return first->low() == second->low() ? first->high() < second->high() : first->low() < second->low();
        });
        for (const auto& range : display_ranges) {
            QString scope = layer_scope(range->low(), range->high());
            if (!range->groupName().isEmpty()) scope += " (" + range->groupName() + ")";
            add_rows(range->getSb(), scope, range->low(), range->high());
        }
    }
    m_table->setSortingEnabled(true);
    if (!m_columns_sized && m_table->rowCount() > 0) {
        m_table->resizeColumnsToContents();
        for (int column = 0; column < m_table->columnCount(); ++column)
            m_table->setColumnWidth(column, std::clamp(m_table->columnWidth(column), 100, 240));
        m_columns_sized = true;
    }
    QItemSelection selection;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const auto* item       = m_table->item(row, 3);
        const QString identity = item->data(IdentityRole).toString();
        if (identity == current_identity) m_table->setCurrentCell(row, 3, QItemSelectionModel::NoUpdate);
        if (selected_rows.contains(identity))
            selection.select(m_table->model()->index(row, 0), m_table->model()->index(row, m_table->columnCount() - 1));
    }
    m_table->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    filterRows();
    m_table->verticalScrollBar()->setValue(vertical_scroll);
    m_table->horizontalScrollBar()->setValue(horizontal_scroll);
}

void SettingsOverridesDialog::filterRows() {
    if (m_refresh_suspended) return;
    const QString search = m_filter->text().trimmed();
    int visible          = 0;
    int part_count       = 0;
    const QUuid part_id  = m_part_filter->currentData().toUuid();
    const int scope      = m_scope_filter->currentIndex();
    for (int row = 0; row < m_table->rowCount(); ++row) {
        bool matches = m_table->item(row, 3)->data(Qt::UserRole).toString().contains(search, Qt::CaseInsensitive);
        for (int column = 0; column < m_table->columnCount(); ++column)
            matches |= m_table->item(row, column)->text().contains(search, Qt::CaseInsensitive);
        matches |= m_table->item(row, 7)->toolTip().contains(search, Qt::CaseInsensitive);
        const bool part_scope = m_table->item(row, 3)->data(LowRole).toInt() < 0;
        matches &= part_id.isNull() || m_table->item(row, 3)->data(PartIdRole).toUuid() == part_id;
        matches &= scope == 0 || (scope == 1 && part_scope) || (scope == 2 && !part_scope);
        m_table->setRowHidden(row, !matches);
        visible += matches;
        part_count += matches && part_scope;
    }
    if (m_table->rowCount() == 0) { m_summary->setText("No part or layer overrides in the current project."); }
    else {
        m_summary->setText(QString("%1 of %2 overrides shown (%3 part, %4 layer).")
                               .arg(visible)
                               .arg(m_table->rowCount())
                               .arg(part_count)
                               .arg(visible - part_count));
    }
    updateNavigation();
}

void SettingsOverridesDialog::updateNavigation() {
    const int row      = m_table->currentRow();
    const auto* item   = m_table->item(row, 3);
    int selected_count = 0;
    bool visible_rows  = false;
    for (int index = 0; index < m_table->rowCount(); ++index) {
        if (m_table->isRowHidden(index)) continue;
        visible_rows = true;
        selected_count += m_table->item(index, 3)->isSelected();
    }
    m_show_setting_button->setEnabled(!m_refresh_suspended && selected_count == 1 && item != nullptr &&
                                      !m_table->isRowHidden(row) && item->isSelected() &&
                                      item->data(KnownSettingRole).toBool());
    m_copy_button->setEnabled(!m_refresh_suspended && selected_count > 0);
    m_export_button->setEnabled(!m_refresh_suspended && visible_rows);
}

QString SettingsOverridesDialog::tableText(QChar delimiter, bool selected_only) const {
    QStringList lines;
    QStringList headers;
    for (int column = 0; column < m_table->columnCount(); ++column)
        headers.append(escapedCell(m_table->horizontalHeaderItem(column)->text(), delimiter));
    lines.append(headers.join(delimiter));
    for (int row = 0; row < m_table->rowCount(); ++row) {
        if (m_table->isRowHidden(row) || (selected_only && !m_table->item(row, 3)->isSelected())) continue;
        QStringList cells;
        for (int column = 0; column < m_table->columnCount(); ++column)
            cells.append(escapedCell(m_table->item(row, column)->text(), delimiter));
        lines.append(cells.join(delimiter));
    }
    return lines.join('\n') + '\n';
}

void SettingsOverridesDialog::copySelectedRows() {
    if (m_copy_button->isEnabled()) QApplication::clipboard()->setText(tableText('\t', true));
}

bool SettingsOverridesDialog::exportCsv(const QString& path, QString* error) const {
    if (error != nullptr) error->clear();
    if (m_refresh_suspended) {
        if (error != nullptr) *error = "Project settings are loading.";
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error != nullptr) *error = file.errorString();
        return false;
    }
    const QByteArray contents = tableText(',', false).toUtf8();
    if (file.write(contents) != contents.size() || !file.commit()) {
        if (error != nullptr) *error = file.errorString();
        return false;
    }
    return true;
}

void SettingsOverridesDialog::chooseCsvFile() {
    QFileDialog dialog(this, "Export Settings Overrides");
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setNameFilter("CSV files (*.csv)");
    dialog.setDefaultSuffix("csv");
    dialog.selectFile("settings-overrides.csv");
    if (dialog.exec() != QDialog::Accepted) return;
    QString error;
    if (!exportCsv(dialog.selectedFiles().first(), &error))
        QMessageBox::warning(this, "Export Settings Overrides", "Could not export settings overrides:\n" + error);
}

void SettingsOverridesDialog::showSelectedSetting() {
    updateNavigation();
    if (!m_show_setting_button->isEnabled()) return;
    const auto* item    = m_table->item(m_table->currentRow(), 3);
    const QUuid part_id = item->data(PartIdRole).toUuid();
    const int low       = item->data(LowRole).toInt();
    const int high      = item->data(HighRole).toInt();
    const QString key   = item->data(Qt::UserRole).toString();
    for (const auto& part : CSM->partsSnapshot()) {
        if (part->getId() != part_id) continue;
        const auto range    = low < 0 ? QSharedPointer<SettingsRange>() : part->getSettingsRange(low, high);
        const auto settings = low < 0          ? part->getSb()
                              : range.isNull() ? QSharedPointer<SettingsBase>()
                                               : range->getSb();
        if (!settings.isNull() && settings->contains(key)) {
            emit showSettingRequested(part_id, low, high, key);
            return;
        }
        break;
    }
    scheduleRefresh();
}
}  // namespace ORNL
