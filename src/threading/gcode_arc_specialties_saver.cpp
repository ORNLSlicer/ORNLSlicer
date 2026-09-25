#include "threading/gcode_arc_specialties_saver.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringBuilder>
#include <QTextStream>
#include <QVector>

#include "managers/settings/settings_manager.h"
#include "units/unit.h"
#include "utilities/constants.h"

namespace ORNL {
namespace {
enum class ScheduleValueType { kInteger, kSpeed, kTime, kVoltage };

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
    QString speed_setting_key;
    QVector<ScheduleField> fields;
};

QString formatDecimal(double value) {
    QString text = QString::number(value, 'f', 4);
    while (text.contains('.') && text.endsWith('0')) { text.chop(1); }
    if (text.endsWith('.')) { text.chop(1); }
    if (text == "-0") { text = "0"; }

    return text;
}

QString formatScheduleValue(const QString& setting_key, ScheduleValueType value_type, GcodeMeta meta) {
    switch (value_type) {
        case ScheduleValueType::kInteger:
            return QString::number(GSM->getGlobal()->setting<int>(setting_key));
        case ScheduleValueType::kSpeed: {
            const Velocity value = GSM->getGlobal()->setting<Velocity>(setting_key);
            return formatDecimal(value.to(meta.m_velocity_unit));
        }
        case ScheduleValueType::kTime: {
            const Time value = GSM->getGlobal()->setting<Time>(setting_key);
            return formatDecimal(value.to(meta.m_time_unit));
        }
        case ScheduleValueType::kVoltage: {
            const Voltage value = GSM->getGlobal()->setting<Voltage>(setting_key);
            return formatDecimal(value.to(V));
        }
    }

    return {};
}

void writeScheduleField(QTextStream& out, const ScheduleField& field, GcodeMeta meta) {
    out << field.assignment << " = " << formatScheduleValue(field.setting_key, field.value_type, meta);
    if (!field.comment.isEmpty()) { out << "      " << field.comment; }
    out << '\n';
}

void writeScheduleBlock(QTextStream& out, const ScheduleRegion& region, GcodeMeta meta) {
    if (region.default_schedule) {
        out << "$DEFAULT" << '\n';
        out << "(Schedule " << region.schedule_number << " / Default Schedule Start, " << region.label << ")" << '\n';
    }
    else {
        out << "(Schedule " << region.schedule_number << " Start, " << region.label << ")" << '\n';
        out << "$CASE " << region.schedule_number << '\n';
    }

    for (const ScheduleField& field : region.fields) { writeScheduleField(out, field, meta); }

    out << "$IF EXIST[V.S.SPEED]" << '\n';
    out << "   V.S.SPEED = " << formatScheduleValue(region.speed_setting_key, ScheduleValueType::kSpeed, meta)
        << "            ;Set speed value if it exists" << '\n';
    out << "$ENDIF" << '\n';
    out << "$BREAK" << '\n';
    out << "(Schedule " << region.schedule_number << " End)" << '\n';
}
}  // namespace

GCodeArcSpecialtiesSaver::GCodeArcSpecialtiesSaver(QString tempLocation, QString path, QString filename, QString text,
                                                   GcodeMeta meta)
    : m_temp_location(tempLocation), m_path(path), m_filename(filename), m_text(text), m_selected_meta(meta) {
    // NOP
}

void GCodeArcSpecialtiesSaver::run() {
    QFileInfo file_info(m_filename);
    const QString schedule_file_name = file_info.completeBaseName() % "_sch.nc";
    const QString schedule_file_path = file_info.absolutePath() % QDir::separator() % schedule_file_name;

    updateScheduleReference(schedule_file_name);

    QFile temp_file(m_temp_location % "temp");
    if (temp_file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream out(&temp_file);
        out << m_text;
        temp_file.close();

        QFile::rename(temp_file.fileName(), m_filename);
    }

    writeWeldScheduleFile(schedule_file_path);
}

void GCodeArcSpecialtiesSaver::updateScheduleReference(const QString& schedule_file_name) {
    const QRegularExpression g80_file_line("^#FILE NAME\\[ G80=\"[^\"]*\" \\]$", QRegularExpression::MultilineOption);
    const QString replacement = "#FILE NAME[ G80=\"" % schedule_file_name % "\" ]";

    if (m_text.contains(g80_file_line)) { m_text.replace(g80_file_line, replacement); }
}

void GCodeArcSpecialtiesSaver::writeWeldScheduleFile(const QString& file_path) {
    const QVector<ScheduleRegion> schedule_regions {
        {1,
         Constants::RegionTypeStrings::kPerimeter.toLower(),
         false,
         PS::Perimeter::kSpeed,
         {{"V.E.Sch.Preflow", PS::ArcSpecialties::kPerimeterPreflow, ScheduleValueType::kTime,
           ";Preflow Time in Seconds"},
          {"V.E.Sch.TravelDelay", PS::ArcSpecialties::kPerimeterTravelDelay, ScheduleValueType::kTime,
           ";Travel Start Delay in Seconds"},
          {"V.E.Sch.Postflow", PS::ArcSpecialties::kPerimeterPostflow, ScheduleValueType::kTime,
           ";Stationary Postflow Time"},
          {"V.E.Sch.PostPurge", PS::ArcSpecialties::kPerimeterPostpurge, ScheduleValueType::kTime,
           ";Postflow Time after moving away (can reduce preflow delay)"},
          {"V.E.Sch.Start1.Program", PS::ArcSpecialties::kPerimeterStartProgram, ScheduleValueType::kInteger,
           ";Program/Mode in Startup. = 0 to disable"},
          {"V.E.Sch.Start1.WFS", PS::ArcSpecialties::kPerimeterStartWireFeedSpeed, ScheduleValueType::kSpeed,
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
          {"V.E.Sch.Weld.WFS", PS::ArcSpecialties::kPerimeterWeldWireFeedSpeed, ScheduleValueType::kSpeed,
           ";Wire Feed Speed in Main Weld"},
          {"V.E.Sch.Weld.Volts", PS::ArcSpecialties::kPerimeterWeldVoltage, ScheduleValueType::kVoltage,
           ";Trim or Volts in Main Weld"},
          {"V.E.Sch.Weld.Control", PS::ArcSpecialties::kPerimeterWeldControl, ScheduleValueType::kInteger,
           ";Arc Control in Main Weld"},
          {"V.E.Sch.Crater.Program", PS::ArcSpecialties::kPerimeterCraterProgram, ScheduleValueType::kInteger,
           ";Program/Mode in Crater Fill"},
          {"V.E.Sch.Crater.WFS", PS::ArcSpecialties::kPerimeterCraterWireFeedSpeed, ScheduleValueType::kSpeed,
           ";Wire Feed Speed in Crater Fill"},
          {"V.E.Sch.Crater.Volts", PS::ArcSpecialties::kPerimeterCraterVoltage, ScheduleValueType::kVoltage,
           ";Trim or Volts in Crater Fill"},
          {"V.E.Sch.Crater.Control", PS::ArcSpecialties::kPerimeterCraterControl, ScheduleValueType::kInteger,
           ";Arc Control in Crater Fill"},
          {"V.E.Sch.Crater.Time", PS::ArcSpecialties::kPerimeterCraterTime, ScheduleValueType::kTime,
           ";Crater Fill Time in Seconds"}}},
        {2,
         Constants::RegionTypeStrings::kInset.toLower(),
         false,
         PS::Inset::kSpeed,
         {{"V.E.Sch.Preflow", PS::ArcSpecialties::kInsetPreflow, ScheduleValueType::kTime, ";Preflow Time in Seconds"},
          {"V.E.Sch.TravelDelay", PS::ArcSpecialties::kInsetTravelDelay, ScheduleValueType::kTime,
           ";Travel Start Delay in Seconds"},
          {"V.E.Sch.Postflow", PS::ArcSpecialties::kInsetPostflow, ScheduleValueType::kTime,
           ";Stationary Postflow Time"},
          {"V.E.Sch.PostPurge", PS::ArcSpecialties::kInsetPostpurge, ScheduleValueType::kTime,
           ";Postflow Time after moving away (can reduce preflow delay)"},
          {"V.E.Sch.Start1.Program", PS::ArcSpecialties::kInsetStartProgram, ScheduleValueType::kInteger,
           ";Program/Mode in Startup. = 0 to disable"},
          {"V.E.Sch.Start1.WFS", PS::ArcSpecialties::kInsetStartWireFeedSpeed, ScheduleValueType::kSpeed,
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
          {"V.E.Sch.Weld.WFS", PS::ArcSpecialties::kInsetWeldWireFeedSpeed, ScheduleValueType::kSpeed,
           ";Wire Feed Speed in Main Weld"},
          {"V.E.Sch.Weld.Volts", PS::ArcSpecialties::kInsetWeldVoltage, ScheduleValueType::kVoltage,
           ";Trim or Volts in Main Weld"},
          {"V.E.Sch.Weld.Control", PS::ArcSpecialties::kInsetWeldControl, ScheduleValueType::kInteger,
           ";Arc Control in Main Weld"},
          {"V.E.Sch.Crater.Program", PS::ArcSpecialties::kInsetCraterProgram, ScheduleValueType::kInteger,
           ";Program/Mode in Crater Fill"},
          {"V.E.Sch.Crater.WFS", PS::ArcSpecialties::kInsetCraterWireFeedSpeed, ScheduleValueType::kSpeed,
           ";Wire Feed Speed in Crater Fill"},
          {"V.E.Sch.Crater.Volts", PS::ArcSpecialties::kInsetCraterVoltage, ScheduleValueType::kVoltage,
           ";Trim or Volts in Crater Fill"},
          {"V.E.Sch.Crater.Control", PS::ArcSpecialties::kInsetCraterControl, ScheduleValueType::kInteger,
           ";Arc Control in Crater Fill"},
          {"V.E.Sch.Crater.Time", PS::ArcSpecialties::kInsetCraterTime, ScheduleValueType::kTime,
           ";Crater Fill Time in Seconds"}}},
        {3,
         Constants::RegionTypeStrings::kSkeleton.toLower(),
         false,
         PS::Skeleton::kSpeed,
         {{"V.E.Sch.Preflow", PS::ArcSpecialties::kSkeletonPreflow, ScheduleValueType::kTime,
           ";Preflow Time in Seconds"},
          {"V.E.Sch.TravelDelay", PS::ArcSpecialties::kSkeletonTravelDelay, ScheduleValueType::kTime,
           ";Travel Start Delay in Seconds"},
          {"V.E.Sch.Postflow", PS::ArcSpecialties::kSkeletonPostflow, ScheduleValueType::kTime,
           ";Stationary Postflow Time"},
          {"V.E.Sch.PostPurge", PS::ArcSpecialties::kSkeletonPostpurge, ScheduleValueType::kTime,
           ";Postflow Time after moving away (can reduce preflow delay)"},
          {"V.E.Sch.Start1.Program", PS::ArcSpecialties::kSkeletonStartProgram, ScheduleValueType::kInteger,
           ";Program/Mode in Startup. = 0 to disable"},
          {"V.E.Sch.Start1.WFS", PS::ArcSpecialties::kSkeletonStartWireFeedSpeed, ScheduleValueType::kSpeed,
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
          {"V.E.Sch.Weld.WFS", PS::ArcSpecialties::kSkeletonWeldWireFeedSpeed, ScheduleValueType::kSpeed,
           ";Wire Feed Speed in Main Weld"},
          {"V.E.Sch.Weld.Volts", PS::ArcSpecialties::kSkeletonWeldVoltage, ScheduleValueType::kVoltage,
           ";Trim or Volts in Main Weld"},
          {"V.E.Sch.Weld.Control", PS::ArcSpecialties::kSkeletonWeldControl, ScheduleValueType::kInteger,
           ";Arc Control in Main Weld"},
          {"V.E.Sch.Crater.Program", PS::ArcSpecialties::kSkeletonCraterProgram, ScheduleValueType::kInteger,
           ";Program/Mode in Crater Fill"},
          {"V.E.Sch.Crater.WFS", PS::ArcSpecialties::kSkeletonCraterWireFeedSpeed, ScheduleValueType::kSpeed,
           ";Wire Feed Speed in Crater Fill"},
          {"V.E.Sch.Crater.Volts", PS::ArcSpecialties::kSkeletonCraterVoltage, ScheduleValueType::kVoltage,
           ";Trim or Volts in Crater Fill"},
          {"V.E.Sch.Crater.Control", PS::ArcSpecialties::kSkeletonCraterControl, ScheduleValueType::kInteger,
           ";Arc Control in Crater Fill"},
          {"V.E.Sch.Crater.Time", PS::ArcSpecialties::kSkeletonCraterTime, ScheduleValueType::kTime,
           ";Crater Fill Time in Seconds"}}},
        {0,
         Constants::RegionTypeStrings::kInfill.toLower(),
         true,
         PS::Infill::kSpeed,
         {{"V.E.Sch.Preflow", PS::ArcSpecialties::kInfillPreflow, ScheduleValueType::kTime, ";Preflow Time in Seconds"},
          {"V.E.Sch.TravelDelay", PS::ArcSpecialties::kInfillTravelDelay, ScheduleValueType::kTime,
           ";Travel Start Delay in Seconds"},
          {"V.E.Sch.Postflow", PS::ArcSpecialties::kInfillPostflow, ScheduleValueType::kTime,
           ";Stationary Postflow Time"},
          {"V.E.Sch.PostPurge", PS::ArcSpecialties::kInfillPostpurge, ScheduleValueType::kTime,
           ";Postflow Time after moving away (can reduce preflow delay)"},
          {"V.E.Sch.Start1.Program", PS::ArcSpecialties::kInfillStartProgram, ScheduleValueType::kInteger,
           ";Program/Mode in Startup. = 0 to disable"},
          {"V.E.Sch.Start1.WFS", PS::ArcSpecialties::kInfillStartWireFeedSpeed, ScheduleValueType::kSpeed,
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
          {"V.E.Sch.Weld.WFS", PS::ArcSpecialties::kInfillWeldWireFeedSpeed, ScheduleValueType::kSpeed,
           ";Wire Feed Speed in Main Weld"},
          {"V.E.Sch.Weld.Volts", PS::ArcSpecialties::kInfillWeldVoltage, ScheduleValueType::kVoltage,
           ";Trim or Volts in Main Weld"},
          {"V.E.Sch.Weld.Control", PS::ArcSpecialties::kInfillWeldControl, ScheduleValueType::kInteger,
           ";Arc Control in Main Weld"},
          {"V.E.Sch.Crater.Program", PS::ArcSpecialties::kInfillCraterProgram, ScheduleValueType::kInteger,
           ";Program/Mode in Crater Fill"},
          {"V.E.Sch.Crater.WFS", PS::ArcSpecialties::kInfillCraterWireFeedSpeed, ScheduleValueType::kSpeed,
           ";Wire Feed Speed in Crater Fill"},
          {"V.E.Sch.Crater.Volts", PS::ArcSpecialties::kInfillCraterVoltage, ScheduleValueType::kVoltage,
           ";Trim or Volts in Crater Fill"},
          {"V.E.Sch.Crater.Control", PS::ArcSpecialties::kInfillCraterControl, ScheduleValueType::kInteger,
           ";Arc Control in Crater Fill"},
          {"V.E.Sch.Crater.Time", PS::ArcSpecialties::kInfillCraterTime, ScheduleValueType::kTime,
           ";Crater Fill Time in Seconds"}}},
    };

    QFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) { return; }

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

    for (const ScheduleRegion& region : schedule_regions) {
        writeScheduleBlock(out, region, m_selected_meta);
        out << '\n';
    }

    out << "$ENDSWITCH" << '\n';
    out << "  " << '\n';
    out << "M29 (End Subroutine)" << '\n';
}
}  // namespace ORNL
