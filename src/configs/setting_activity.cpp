#include "configs/setting_activity.h"

#include <QStringList>
#include <algorithm>

#include "utilities/constants.h"

namespace ORNL {
namespace {
struct Evaluation {
    bool matches;
    QString requirement;
    QStringList current_values;
};

QString valueText(const fifojson& value, const fifojson& metadata) {
    if (value.is_boolean()) return value.get<bool>() ? "true" : "false";
    if (value.is_string()) return value.get<QString>();
    if (metadata.value(Constants::Settings::Master::kType, std::string()) == "enumeration" && value.is_number()) {
        const int index = value.get<int>();
        const auto options =
            QString::fromStdString(metadata.value(Constants::Settings::Master::kOptions, std::string())).split(',');
        if (value == index && index >= 0 && index < options.size()) return options.at(index).trimmed();
    }
    return QString::fromStdString(value.dump());
}

Evaluation evaluate(const fifojson& expression, const SettingsBase& effective, const fifojson& master) {
    if (expression.empty() || expression == "") return {true, {}, {}};
    const auto node      = expression.begin();
    const auto& children = node.value();
    if (node.key() == "NOT") {
        auto child = evaluate(children.at(0), effective, master);
        return {!child.matches, "not (" + child.requirement + ")", child.current_values};
    }
    if (node.key() == "AND" || node.key() == "OR") {
        const auto first  = evaluate(children.at(0), effective, master);
        const auto second = evaluate(children.at(1), effective, master);
        const bool both   = node.key() == "AND";
        auto values       = first.current_values + second.current_values;
        values.removeDuplicates();
        return {both ? first.matches && second.matches : first.matches || second.matches,
                "(" + first.requirement + (both ? " and " : " or ") + second.requirement + ")", values};
    }

    const auto& metadata = master.at(node.key());
    const auto key       = QString::fromStdString(node.key());
    fifojson current =
        effective.contains(key) ? effective.setting<fifojson>(key) : metadata.at(Constants::Settings::Master::kDefault);
    const auto type = metadata.at(Constants::Settings::Master::kType).get<std::string>();
    bool matches    = false;
    // Match the checkbox, combo index, and integer editor values used by SettingRowBase::checkLogic.
    if (type == "boolean") {
        current = current.get<bool>();
        matches = current.get<bool>() == children.get<bool>();
    }
    else if (type == "enumeration") {
        const int index = current.get<int>();
        const auto options =
            QString::fromStdString(metadata.at(Constants::Settings::Master::kOptions).get<std::string>()).split(',');
        current = index >= 0 && index < options.size() ? index : -1;
        matches = current == children;
    }
    else if (type == "number" || type == "non_negative_int" || type == "positive_int" || type == "power") {
        current = std::max(type == "positive_int" || type == "power" ? 1 : 0, current.get<int>());
        matches = current == children;
    }
    const auto label = QString::fromStdString(metadata.value(Constants::Settings::Master::kDisplay, node.key()));
    return {matches, label + " = " + valueText(children, metadata), {label + " = " + valueText(current, metadata)}};
}
}  // namespace

QString SettingActivity::inactiveReason(const QString& key, const SettingsBase& effective, const fifojson& master,
                                        const fifojson& input) {
    const auto setting = master.find(key.toStdString());
    if (setting == master.end()) return {};
    fifojson metadata = *setting;
    if (!input.is_null()) {
        for (const auto& field : {Constants::Settings::Master::kDepends, Constants::Settings::Master::kVisibleIf})
            if (input.contains(field)) metadata[field] = input.at(field);
    }
    QStringList reasons;
    for (const auto& field : {Constants::Settings::Master::kVisibleIf, Constants::Settings::Master::kDepends}) {
        const auto condition = metadata.find(field);
        if (condition == metadata.end()) continue;
        const auto result = evaluate(*condition, effective, master);
        if (!result.matches)
            reasons.append("Requires " + result.requirement + ". Current values: " + result.current_values.join("; ") +
                           ".");
    }
    return reasons.join('\n');
}
}  // namespace ORNL
