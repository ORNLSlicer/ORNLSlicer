#include "threading/gcode_arc_specialties_saver.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringBuilder>
#include <QTextStream>
#include <QVector>

#include "managers/preferences_manager.h"
#include "managers/settings/settings_manager.h"
#include "slicing/region_speed.h"
#include "threading/gcode_layer_saver.h"
#include "units/unit.h"
#include "utilities/constants.h"

namespace ORNL {
namespace {
enum class ScheduleValueType { kInteger, kSpeed, kWireFeedSpeed, kTime, kVoltage };

struct ScheduleField {
    QString assignment;
    QString setting_key;
    ScheduleValueType value_type;
    QString comment;
};

struct ScheduleRegion {
    int schedule_number;
    QString label;
    bool default_schedule;
    RegionType region_type;
    QVector<ScheduleField> fields;
};

QString formatDecimal(double value) {
    QString text = QString::number(value, 'f', 4);
    while (text.contains('.') && text.endsWith('0')) { text.chop(1); }
    if (text.endsWith('.')) { text.chop(1); }
    if (text == "-0") { text = "0"; }

    return text;
}

QString formatScheduleValue(const QString& setting_key, ScheduleValueType value_type, Velocity velocity_unit,
                            Time time_unit, Voltage voltage_unit) {
    switch (value_type) {
        case ScheduleValueType::kInteger:
            return QString::number(GSM->getGlobal()->setting<int>(setting_key));
        case ScheduleValueType::kWireFeedSpeed:
        case ScheduleValueType::kSpeed: {
            const Velocity value = GSM->getGlobal()->setting<Velocity>(setting_key);
            return formatDecimal(
                value.to(value_type == ScheduleValueType::kWireFeedSpeed ? inch / minute : velocity_unit));
        }
        case ScheduleValueType::kTime: {
            const Time value = GSM->getGlobal()->setting<Time>(setting_key);
            return formatDecimal(value.to(time_unit));
        }
        case ScheduleValueType::kVoltage: {
            const Voltage value = GSM->getGlobal()->setting<Voltage>(setting_key);
            return formatDecimal(value.to(voltage_unit));
        }
    }

    return {};
}

void writeScheduleField(QTextStream& out, const ScheduleField& field, Velocity velocity_unit, Time time_unit,
                        Voltage voltage_unit) {
    out << field.assignment << " = "
        << formatScheduleValue(field.setting_key, field.value_type, velocity_unit, time_unit, voltage_unit);
    if (!field.comment.isEmpty()) {
        out << "      " << field.comment;
        switch (field.value_type) {
            case ScheduleValueType::kInteger:
                out << " (unitless)";
                break;
            case ScheduleValueType::kWireFeedSpeed:
                out << " (in/min)";
                break;
            case ScheduleValueType::kSpeed:
                out << " (" << velocity_unit.toString() << ')';
                break;
            case ScheduleValueType::kTime:
                out << " (" << time_unit.toString() << ')';
                break;
            case ScheduleValueType::kVoltage:
                out << " (" << voltage_unit.toString() << ')';
                break;
        }
    }
    out << '\n';
}

void writeScheduleBlock(QTextStream& out, const ScheduleRegion& region, Velocity velocity_unit, Time time_unit,
                        Voltage voltage_unit) {
    if (region.default_schedule) {
        out << "$DEFAULT" << '\n';
        out << "(Schedule " << region.schedule_number << " / Default Schedule Start, " << region.label << ")" << '\n';
    }
    else {
        out << "(Schedule " << region.schedule_number << " Start, " << region.label << ")" << '\n';
        out << "$CASE " << region.schedule_number << '\n';
    }

    for (const ScheduleField& field : region.fields) {
        writeScheduleField(out, field, velocity_unit, time_unit, voltage_unit);
    }

    out << "$IF EXIST[V.S.SPEED]" << '\n';
    out << "   V.S.SPEED = " << formatDecimal(regionPrintSpeed(*GSM->getGlobal(), region.region_type).to(velocity_unit))
        << "            ;Set speed value if it exists (" << velocity_unit.toString() << ')' << '\n';
    out << "$ENDIF" << '\n';
    out << "$BREAK" << '\n';
    out << "(Schedule " << region.schedule_number << " End)" << '\n';
}
}  // namespace

GCodeArcSpecialtiesSaver::GCodeArcSpecialtiesSaver(QString filename, QString text, QVector<GcodeLayer> layers)
    : m_filename(filename),
      m_text(text),
      m_layers(std::move(layers)),
      m_time_unit(PreferencesManager::getInstance()->getTimeUnit()),
      m_voltage_unit(PreferencesManager::getInstance()->getVoltageUnit()) {
    // NOP
}

void GCodeArcSpecialtiesSaver::run() {
    emit completed(save());
}

QString GCodeArcSpecialtiesSaver::save() {
    QFileInfo file_info(m_filename);
    const QString schedule_file_name = file_info.completeBaseName() % "_sch.nc";
    const QString schedule_file_path = file_info.absolutePath() % QDir::separator() % schedule_file_name;

    const QString error = writeWeldScheduleFile(schedule_file_path);
    if (!error.isEmpty()) return error;

    if (!m_layers.isEmpty()) {
        for (auto& layer : m_layers) { updateScheduleReference(layer.text, schedule_file_name); }
        GCodeLayerSaver saver(file_info.absolutePath(), file_info.completeBaseName(), m_layers,
                              "." % file_info.suffix());
        QString layer_error;
        connect(&saver, &GCodeLayerSaver::completed, [&layer_error](const QString& error) { layer_error = error; });
        saver.run();
        return layer_error;
    }

    updateScheduleReference(m_text, schedule_file_name);

    QSaveFile file(m_filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return "Could not open G-Code file: " % m_filename % "\n" % file.errorString();
    }
    QTextStream out(&file);
    out << m_text;
    out.flush();
    if (out.status() != QTextStream::Ok || !file.commit()) {
        return "Could not write G-Code file: " % m_filename % "\n" % file.errorString();
    }

    return {};
}

void GCodeArcSpecialtiesSaver::updateScheduleReference(QString& text, const QString& schedule_file_name) {
    const QRegularExpression g80_file_line("^#FILE NAME\\[ G80=\"[^\"]*\" \\]$", QRegularExpression::MultilineOption);
    const QString replacement = "#FILE NAME[ G80=\"" % schedule_file_name % "\" ]";

    if (text.contains(g80_file_line)) { text.replace(g80_file_line, replacement); }
}

QString GCodeArcSpecialtiesSaver::writeWeldScheduleFile(const QString& file_path) {
    const QVector<ScheduleRegion> schedule_regions {
        {1,
         Constants::RegionTypeStrings::kPerimeter.toLower(),
         false,
         RegionType::kPerimeter,
         {{"V.E.Sch.Preflow", PS::ArcSpecialties::kPerimeterPreflow, ScheduleValueType::kTime, ";Preflow Time"},
          {"V.E.Sch.TravelDelay", PS::ArcSpecialties::kPerimeterTravelDelay, ScheduleValueType::kTime,
           ";Travel Start Delay"},
          {"V.E.Sch.Postflow", PS::ArcSpecialties::kPerimeterPostflow, ScheduleValueType::kTime,
           ";Stationary Postflow Time"},
          {"V.E.Sch.PostPurge", PS::ArcSpecialties::kPerimeterPostpurge, ScheduleValueType::kTime,
           ";Postflow Time after moving away (can reduce preflow delay)"},
          {"V.E.Sch.Start1.Program", PS::ArcSpecialties::kPerimeterStartProgram, ScheduleValueType::kInteger,
           ";Program/Mode in Startup. = 0 to disable"},
          {"V.E.Sch.Start1.WFS", PS::ArcSpecialties::kPerimeterStartWireFeedSpeed, ScheduleValueType::kWireFeedSpeed,
           ";Wire Feed Speed in Startup"},
          {"V.E.Sch.Start1.Volts", PS::ArcSpecialties::kPerimeterStartVoltage, ScheduleValueType::kVoltage,
           ";Trim or Volts in Startup"},
          {"V.E.Sch.Start1.Control", PS::ArcSpecialties::kPerimeterStartControl, ScheduleValueType::kInteger,
           ";Arc Control in Startup"},
          {"V.E.Sch.Start1.TSpd", PS::ArcSpecialties::kPerimeterStartTSpd, ScheduleValueType::kSpeed,
           ";Travel speed override during startup (currently unused)"},
          {"V.E.Sch.Start1.Time", PS::ArcSpecialties::kPerimeterStartTime, ScheduleValueType::kTime,
           ";Time to use start parameters"},
          {"V.E.Sch.Weld.Program", PS::ArcSpecialties::kPerimeterWeldProgram, ScheduleValueType::kInteger,
           ";Program/Mode in Main Weld"},
          {"V.E.Sch.Weld.WFS", PS::ArcSpecialties::kPerimeterWeldWireFeedSpeed, ScheduleValueType::kWireFeedSpeed,
           ";Wire Feed Speed in Main Weld"},
          {"V.E.Sch.Weld.Volts", PS::ArcSpecialties::kPerimeterWeldVoltage, ScheduleValueType::kVoltage,
           ";Trim or Volts in Main Weld"},
          {"V.E.Sch.Weld.Control", PS::ArcSpecialties::kPerimeterWeldControl, ScheduleValueType::kInteger,
           ";Arc Control in Main Weld"},
          {"V.E.Sch.Crater.Program", PS::ArcSpecialties::kPerimeterCraterProgram, ScheduleValueType::kInteger,
           ";Program/Mode in Crater Fill"},
          {"V.E.Sch.Crater.WFS", PS::ArcSpecialties::kPerimeterCraterWireFeedSpeed, ScheduleValueType::kWireFeedSpeed,
           ";Wire Feed Speed in Crater Fill"},
          {"V.E.Sch.Crater.Volts", PS::ArcSpecialties::kPerimeterCraterVoltage, ScheduleValueType::kVoltage,
           ";Trim or Volts in Crater Fill"},
          {"V.E.Sch.Crater.Control", PS::ArcSpecialties::kPerimeterCraterControl, ScheduleValueType::kInteger,
           ";Arc Control in Crater Fill"},
          {"V.E.Sch.Crater.Time", PS::ArcSpecialties::kPerimeterCraterTime, ScheduleValueType::kTime,
           ";Crater Fill Time"}}},
        {2,
         Constants::RegionTypeStrings::kInset.toLower(),
         false,
         RegionType::kInset,
         {{"V.E.Sch.Preflow", PS::ArcSpecialties::kInsetPreflow, ScheduleValueType::kTime, ";Preflow Time"},
          {"V.E.Sch.TravelDelay", PS::ArcSpecialties::kInsetTravelDelay, ScheduleValueType::kTime,
           ";Travel Start Delay"},
          {"V.E.Sch.Postflow", PS::ArcSpecialties::kInsetPostflow, ScheduleValueType::kTime,
           ";Stationary Postflow Time"},
          {"V.E.Sch.PostPurge", PS::ArcSpecialties::kInsetPostpurge, ScheduleValueType::kTime,
           ";Postflow Time after moving away (can reduce preflow delay)"},
          {"V.E.Sch.Start1.Program", PS::ArcSpecialties::kInsetStartProgram, ScheduleValueType::kInteger,
           ";Program/Mode in Startup. = 0 to disable"},
          {"V.E.Sch.Start1.WFS", PS::ArcSpecialties::kInsetStartWireFeedSpeed, ScheduleValueType::kWireFeedSpeed,
           ";Wire Feed Speed in Startup"},
          {"V.E.Sch.Start1.Volts", PS::ArcSpecialties::kInsetStartVoltage, ScheduleValueType::kVoltage,
           ";Trim or Volts in Startup"},
          {"V.E.Sch.Start1.Control", PS::ArcSpecialties::kInsetStartControl, ScheduleValueType::kInteger,
           ";Arc Control in Startup"},
          {"V.E.Sch.Start1.TSpd", PS::ArcSpecialties::kInsetStartTSpd, ScheduleValueType::kSpeed,
           ";Travel speed override during startup (currently unused)"},
          {"V.E.Sch.Start1.Time", PS::ArcSpecialties::kInsetStartTime, ScheduleValueType::kTime,
           ";Time to use start parameters"},
          {"V.E.Sch.Weld.Program", PS::ArcSpecialties::kInsetWeldProgram, ScheduleValueType::kInteger,
           ";Program/Mode in Main Weld"},
          {"V.E.Sch.Weld.WFS", PS::ArcSpecialties::kInsetWeldWireFeedSpeed, ScheduleValueType::kWireFeedSpeed,
           ";Wire Feed Speed in Main Weld"},
          {"V.E.Sch.Weld.Volts", PS::ArcSpecialties::kInsetWeldVoltage, ScheduleValueType::kVoltage,
           ";Trim or Volts in Main Weld"},
          {"V.E.Sch.Weld.Control", PS::ArcSpecialties::kInsetWeldControl, ScheduleValueType::kInteger,
           ";Arc Control in Main Weld"},
          {"V.E.Sch.Crater.Program", PS::ArcSpecialties::kInsetCraterProgram, ScheduleValueType::kInteger,
           ";Program/Mode in Crater Fill"},
          {"V.E.Sch.Crater.WFS", PS::ArcSpecialties::kInsetCraterWireFeedSpeed, ScheduleValueType::kWireFeedSpeed,
           ";Wire Feed Speed in Crater Fill"},
          {"V.E.Sch.Crater.Volts", PS::ArcSpecialties::kInsetCraterVoltage, ScheduleValueType::kVoltage,
           ";Trim or Volts in Crater Fill"},
          {"V.E.Sch.Crater.Control", PS::ArcSpecialties::kInsetCraterControl, ScheduleValueType::kInteger,
           ";Arc Control in Crater Fill"},
          {"V.E.Sch.Crater.Time", PS::ArcSpecialties::kInsetCraterTime, ScheduleValueType::kTime,
           ";Crater Fill Time"}}},
        {3,
         Constants::RegionTypeStrings::kSkeleton.toLower(),
         false,
         RegionType::kSkeleton,
         {{"V.E.Sch.Preflow", PS::ArcSpecialties::kSkeletonPreflow, ScheduleValueType::kTime, ";Preflow Time"},
          {"V.E.Sch.TravelDelay", PS::ArcSpecialties::kSkeletonTravelDelay, ScheduleValueType::kTime,
           ";Travel Start Delay"},
          {"V.E.Sch.Postflow", PS::ArcSpecialties::kSkeletonPostflow, ScheduleValueType::kTime,
           ";Stationary Postflow Time"},
          {"V.E.Sch.PostPurge", PS::ArcSpecialties::kSkeletonPostpurge, ScheduleValueType::kTime,
           ";Postflow Time after moving away (can reduce preflow delay)"},
          {"V.E.Sch.Start1.Program", PS::ArcSpecialties::kSkeletonStartProgram, ScheduleValueType::kInteger,
           ";Program/Mode in Startup. = 0 to disable"},
          {"V.E.Sch.Start1.WFS", PS::ArcSpecialties::kSkeletonStartWireFeedSpeed, ScheduleValueType::kWireFeedSpeed,
           ";Wire Feed Speed in Startup"},
          {"V.E.Sch.Start1.Volts", PS::ArcSpecialties::kSkeletonStartVoltage, ScheduleValueType::kVoltage,
           ";Trim or Volts in Startup"},
          {"V.E.Sch.Start1.Control", PS::ArcSpecialties::kSkeletonStartControl, ScheduleValueType::kInteger,
           ";Arc Control in Startup"},
          {"V.E.Sch.Start1.TSpd", PS::ArcSpecialties::kSkeletonStartTSpd, ScheduleValueType::kSpeed,
           ";Travel speed override during startup (currently unused)"},
          {"V.E.Sch.Start1.Time", PS::ArcSpecialties::kSkeletonStartTime, ScheduleValueType::kTime,
           ";Time to use start parameters"},
          {"V.E.Sch.Weld.Program", PS::ArcSpecialties::kSkeletonWeldProgram, ScheduleValueType::kInteger,
           ";Program/Mode in Main Weld"},
          {"V.E.Sch.Weld.WFS", PS::ArcSpecialties::kSkeletonWeldWireFeedSpeed, ScheduleValueType::kWireFeedSpeed,
           ";Wire Feed Speed in Main Weld"},
          {"V.E.Sch.Weld.Volts", PS::ArcSpecialties::kSkeletonWeldVoltage, ScheduleValueType::kVoltage,
           ";Trim or Volts in Main Weld"},
          {"V.E.Sch.Weld.Control", PS::ArcSpecialties::kSkeletonWeldControl, ScheduleValueType::kInteger,
           ";Arc Control in Main Weld"},
          {"V.E.Sch.Crater.Program", PS::ArcSpecialties::kSkeletonCraterProgram, ScheduleValueType::kInteger,
           ";Program/Mode in Crater Fill"},
          {"V.E.Sch.Crater.WFS", PS::ArcSpecialties::kSkeletonCraterWireFeedSpeed, ScheduleValueType::kWireFeedSpeed,
           ";Wire Feed Speed in Crater Fill"},
          {"V.E.Sch.Crater.Volts", PS::ArcSpecialties::kSkeletonCraterVoltage, ScheduleValueType::kVoltage,
           ";Trim or Volts in Crater Fill"},
          {"V.E.Sch.Crater.Control", PS::ArcSpecialties::kSkeletonCraterControl, ScheduleValueType::kInteger,
           ";Arc Control in Crater Fill"},
          {"V.E.Sch.Crater.Time", PS::ArcSpecialties::kSkeletonCraterTime, ScheduleValueType::kTime,
           ";Crater Fill Time"}}},
        {0,
         Constants::RegionTypeStrings::kInfill.toLower(),
         true,
         RegionType::kInfill,
         {{"V.E.Sch.Preflow", PS::ArcSpecialties::kInfillPreflow, ScheduleValueType::kTime, ";Preflow Time"},
          {"V.E.Sch.TravelDelay", PS::ArcSpecialties::kInfillTravelDelay, ScheduleValueType::kTime,
           ";Travel Start Delay"},
          {"V.E.Sch.Postflow", PS::ArcSpecialties::kInfillPostflow, ScheduleValueType::kTime,
           ";Stationary Postflow Time"},
          {"V.E.Sch.PostPurge", PS::ArcSpecialties::kInfillPostpurge, ScheduleValueType::kTime,
           ";Postflow Time after moving away (can reduce preflow delay)"},
          {"V.E.Sch.Start1.Program", PS::ArcSpecialties::kInfillStartProgram, ScheduleValueType::kInteger,
           ";Program/Mode in Startup. = 0 to disable"},
          {"V.E.Sch.Start1.WFS", PS::ArcSpecialties::kInfillStartWireFeedSpeed, ScheduleValueType::kWireFeedSpeed,
           ";Wire Feed Speed in Startup"},
          {"V.E.Sch.Start1.Volts", PS::ArcSpecialties::kInfillStartVoltage, ScheduleValueType::kVoltage,
           ";Trim or Volts in Startup"},
          {"V.E.Sch.Start1.Control", PS::ArcSpecialties::kInfillStartControl, ScheduleValueType::kInteger,
           ";Arc Control in Startup"},
          {"V.E.Sch.Start1.TSpd", PS::ArcSpecialties::kInfillStartTSpd, ScheduleValueType::kSpeed,
           ";Travel speed override during startup (currently unused)"},
          {"V.E.Sch.Start1.Time", PS::ArcSpecialties::kInfillStartTime, ScheduleValueType::kTime,
           ";Time to use start parameters"},
          {"V.E.Sch.Weld.Program", PS::ArcSpecialties::kInfillWeldProgram, ScheduleValueType::kInteger,
           ";Program/Mode in Main Weld"},
          {"V.E.Sch.Weld.WFS", PS::ArcSpecialties::kInfillWeldWireFeedSpeed, ScheduleValueType::kWireFeedSpeed,
           ";Wire Feed Speed in Main Weld"},
          {"V.E.Sch.Weld.Volts", PS::ArcSpecialties::kInfillWeldVoltage, ScheduleValueType::kVoltage,
           ";Trim or Volts in Main Weld"},
          {"V.E.Sch.Weld.Control", PS::ArcSpecialties::kInfillWeldControl, ScheduleValueType::kInteger,
           ";Arc Control in Main Weld"},
          {"V.E.Sch.Crater.Program", PS::ArcSpecialties::kInfillCraterProgram, ScheduleValueType::kInteger,
           ";Program/Mode in Crater Fill"},
          {"V.E.Sch.Crater.WFS", PS::ArcSpecialties::kInfillCraterWireFeedSpeed, ScheduleValueType::kWireFeedSpeed,
           ";Wire Feed Speed in Crater Fill"},
          {"V.E.Sch.Crater.Volts", PS::ArcSpecialties::kInfillCraterVoltage, ScheduleValueType::kVoltage,
           ";Trim or Volts in Crater Fill"},
          {"V.E.Sch.Crater.Control", PS::ArcSpecialties::kInfillCraterControl, ScheduleValueType::kInteger,
           ";Arc Control in Crater Fill"},
          {"V.E.Sch.Crater.Time", PS::ArcSpecialties::kInfillCraterTime, ScheduleValueType::kTime,
           ";Crater Fill Time"}}},
    };

    QSaveFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return "Could not open weld schedule file: " % file_path % "\n" % file.errorString();
    }

    QTextStream out(&file);
    out << "(DO NOT EDIT THIS HEADER SECTION)" << '\n';
    out << "#VAR" << '\n';
    out << "   V.L.P1 : UNS32 = 0   (Variable for passing in the schedule number, default is 0)" << '\n';
    out << "#ENDVAR" << '\n';
    out << "$IF V.G.CYCLE_ACTIVE    (Check if we can read the passed schedule number)" << '\n';
    out << "   $IF V.G.@P[1].VALID" << '\n';
    out << "      V.L.P1 = ROUND[@P1]" << '\n';
    out << "   $ENDIF" << '\n';
    out << "$ENDIF" << '\n';
    out << '\n';
    out << "$SWITCH V.L.P1" << '\n';
    out << '\n';

    const auto settings = GSM->getGlobal();
    const bool helical =
        static_cast<SlicingMode>(settings->setting<int>(PS::Slicing::kSlicingMode)) == SlicingMode::kCylindrical &&
        settings->setting<CylindricalPathPattern>(PS::Slicing::kCylindricalPathPattern) ==
            CylindricalPathPattern::kHelical;
    for (const ScheduleRegion& region : schedule_regions) {
        if (helical && (region.region_type == RegionType::kSkeleton ||
                        (region.region_type == RegionType::kPerimeter &&
                         settings->setting<int>(PS::Helical::kHelicalPerimeterRevolutions) <= 0) ||
                        (region.region_type == RegionType::kInset &&
                         settings->setting<int>(PS::Helical::kHelicalInsetRevolutions) <= 0)))
            continue;
        writeScheduleBlock(out, region, mm / minute, m_time_unit, m_voltage_unit);
        out << '\n';
    }

    out << "$ENDSWITCH" << '\n';
    out << "  " << '\n';
    out << "M29 (End Subroutine)" << '\n';
    out.flush();
    if (out.status() != QTextStream::Ok || !file.commit()) {
        return "Could not write weld schedule file: " % file_path % "\n" % file.errorString();
    }
    return {};
}
}  // namespace ORNL
