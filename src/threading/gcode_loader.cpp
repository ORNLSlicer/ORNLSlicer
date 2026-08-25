#include "threading/gcode_loader.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QStringBuilder>
#include <QTextStream>
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <tuple>

#include <qcolor.h>
#include <qcontainerfwd.h>
#include <qdatetime.h>
#include <qhash.h>
#include <qlist.h>
#include <qlogging.h>
#include <qmap.h>
#include <qmath.h>
#include <qminmax.h>
#include <qnumeric.h>
#include <qquaternion.h>
#include <qsharedpointer.h>
#include <qstringmatcher.h>
#include <qtextformat.h>
#include <qthread.h>
#include <qtmetamacros.h>
#include <qtypes.h>
#include <qvectornd.h>

#include "configs/settings_range.h"
#include "exceptions/exceptions.h"
#include "gcode/arc_specialties_axis_inference.h"
#include "gcode/gcode_command.h"
#include "gcode/gcode_meta.h"
#include "gcode/parsers/adamantine_parser.h"
#include "gcode/parsers/aerobasic_parser.h"
#include "gcode/parsers/arc_specialties_parser.h"
#include "gcode/parsers/beam_parser.h"
#include "gcode/parsers/cincinnati_parser.h"
#include "gcode/parsers/common_parser.h"
#include "gcode/parsers/marlin_parser.h"
#include "gcode/parsers/mazak_parser.h"
#include "gcode/parsers/mvp_parser.h"
#include "gcode/parsers/siemens_parser.h"
#include "gcode/parsers/tormach_parser.h"
#include "geometry/point.h"
#include "geometry/segment_base.h"
#include "geometry/segments/arc.h"
#include "geometry/segments/bezier.h"
#include "geometry/segments/line.h"
#include "managers/preferences_manager.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "part/part.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "utilities/mathutils.h"

namespace ORNL {
namespace {
Distance beadWidthFromRegionComment(const QString& comment, const QString& region_name, Distance fallback_width,
                                    Distance distance_unit) {
    const int region_start = comment.indexOf(region_name);
    if (region_start < 0) { return fallback_width; }

    const int width_start = comment.indexOf('-', region_start + region_name.size());

    if (width_start >= 0) {
        const int value_start = width_start + 1;
        int value_end         = comment.indexOf(' ', value_start);
        if (value_end < 0) { value_end = comment.size(); }

        bool ok                   = false;
        const double parsed_width = comment.mid(value_start, value_end - value_start).toDouble(&ok);
        if (ok && parsed_width > 0) { return parsed_width * distance_unit; }
    }

    return fallback_width;
}

float beadDisplayWidth(Distance bead_width) {
    return static_cast<float>(bead_width()) * Constants::OpenGL::kObjectToView;
}

const QString kCylindricalAxisXComment            = "AXIS_X=";
const QString kCylindricalAxisYComment            = "AXIS_Y=";
const QString kWorldApproachTravelComment         = "WORLD APPROACH TRAVEL";
constexpr char kArcSpecialtiesCpOptionalParameter = 'C';

bool commentFieldValue(const QString& comment, const QString& field, double& value) {
    const int field_start = comment.indexOf(field, 0, Qt::CaseInsensitive);
    if (field_start < 0) { return false; }

    const int value_start = field_start + field.size();
    int value_end         = value_start;
    while (value_end < comment.size() && !comment[value_end].isSpace()) { ++value_end; }

    bool ok = false;
    value   = comment.mid(value_start, value_end - value_start).toDouble(&ok);
    return ok;
}

bool cylindricalAxisFromComment(const QString& comment, Distance distance_unit, float x_offset, float y_offset,
                                Point& center) {
    double axis_x = 0.0;
    double axis_y = 0.0;
    if (!commentFieldValue(comment, kCylindricalAxisXComment, axis_x) ||
        !commentFieldValue(comment, kCylindricalAxisYComment, axis_y)) {
        return false;
    }

    center.x((axis_x * distance_unit() + x_offset) * Constants::OpenGL::kObjectToView);
    center.y((axis_y * distance_unit() + y_offset) * Constants::OpenGL::kObjectToView);
    center.z(0.0);
    return true;
}

bool isCylindricalPrintComment(const QString& comment) {
    return (comment.contains(Constants::RegionTypeStrings::kRadial, Qt::CaseInsensitive) ||
            comment.contains(Constants::RegionTypeStrings::kHelical, Qt::CaseInsensitive)) &&
           !comment.contains(Constants::RegionTypeStrings::kTravel, Qt::CaseInsensitive);
}
}  // namespace

GCodeLoader::GCodeLoader(QString filename, bool alterFile)
    : m_filename(filename), m_adjust_file(alterFile), m_should_cancel(false) {
    m_sb = GSM->getGlobal();

    m_prestart        = QStringMatcher(Constants::PathModifierStrings::kPrestart.toUpper());
    m_initial_startup = QStringMatcher(Constants::PathModifierStrings::kInitialStartup.toUpper());
    m_slowdown        = QStringMatcher(Constants::PathModifierStrings::kSlowDown.toUpper());
    m_forward_tipwipe = QStringMatcher(Constants::PathModifierStrings::kForwardTipWipe.toUpper());
    m_reverse_tipwipe = QStringMatcher(Constants::PathModifierStrings::kReverseTipWipe.toUpper());
    m_angled_tipwipe  = QStringMatcher(Constants::PathModifierStrings::kAngledTipWipe.toUpper());
    m_coasting        = QStringMatcher(Constants::PathModifierStrings::kCoasting.toUpper());
    m_spirallift      = QStringMatcher(Constants::PathModifierStrings::kSpiralLift.toUpper());
    m_rampingup       = QStringMatcher(Constants::PathModifierStrings::kRampingUp.toUpper());
    m_rampingdown     = QStringMatcher(Constants::PathModifierStrings::kRampingDown.toUpper());
    m_leadin          = QStringMatcher(Constants::PathModifierStrings::kLeadIn.toUpper());

    m_modifier_colors.push_back(
        PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kPrestart));
    m_modifier_colors.push_back(
        PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kInitialStartup));
    m_modifier_colors.push_back(
        PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kSlowDown));
    m_modifier_colors.push_back(
        PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kTipWipeForward));
    m_modifier_colors.push_back(
        PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kTipWipeReverse));
    m_modifier_colors.push_back(
        PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kTipWipeAngled));
    m_modifier_colors.push_back(
        PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kCoasting));
    m_modifier_colors.push_back(
        PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kSpiralLift));
    m_modifier_colors.push_back(
        PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kRampingUp));
    m_modifier_colors.push_back(
        PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kRampingDown));
    m_modifier_colors.push_back(PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kLeadIn));

    m_perimeter    = QStringMatcher(Constants::RegionTypeStrings::kPerimeter.toUpper());
    m_radial       = QStringMatcher(Constants::RegionTypeStrings::kRadial.toUpper());
    m_helical      = QStringMatcher(Constants::RegionTypeStrings::kHelical.toUpper());
    m_inset        = QStringMatcher(Constants::RegionTypeStrings::kInset.toUpper());
    m_infill       = QStringMatcher(Constants::RegionTypeStrings::kInfill.toUpper());
    m_skin         = QStringMatcher(Constants::RegionTypeStrings::kSkin.toUpper());
    m_skeleton     = QStringMatcher(Constants::RegionTypeStrings::kSkeleton.toUpper());
    m_support      = QStringMatcher(Constants::RegionTypeStrings::kSupport.toUpper());
    m_support_roof = QStringMatcher(Constants::RegionTypeStrings::kSupportRoof.toUpper());
    m_travel       = QStringMatcher(Constants::RegionTypeStrings::kTravel.toUpper());
    m_raft         = QStringMatcher(Constants::RegionTypeStrings::kRaft.toUpper());
    m_brim         = QStringMatcher(Constants::RegionTypeStrings::kBrim.toUpper());
    m_skirt        = QStringMatcher(Constants::RegionTypeStrings::kSkirt.toUpper());
    m_laserscan    = QStringMatcher(Constants::RegionTypeStrings::kLaserScan.toUpper());
    m_thermalscan  = QStringMatcher(Constants::RegionTypeStrings::kThermalScan.toUpper());

    m_color_space_conversion = 1.0 / 255.0;
    m_layer_pattern          = QRegularExpression("W*(\\d+)W*");
}

QString GCodeLoader::additionalExportComments() {
    QString openingDelim = m_selected_meta.m_comment_starting_delimiter;
    QString closingDelim = m_selected_meta.m_comment_ending_delimiter;

    QString partMinTranslation;
    QVector3D translationMin;
    int index = 0;
    for (QSharedPointer<Part> part : CSM->parts()) {
        auto transformation = part->rootMesh()->transformation();
        QVector3D translation, scale;
        QQuaternion rotation;
        std::tie(translation, rotation, scale) = MathUtils::decomposeTransformMatrix(transformation);
        if (++index == 1 || translation.x() < translationMin.x() || translation.y() < translationMin.y())
            translationMin = translation;
    }
    if (index > 0) {
        partMinTranslation = openingDelim % "Part Translation: X" %
                             QString::number(translationMin.x() / 1000000, 'f', 4) % ", Y" %
                             QString::number(translationMin.y() / 1000000, 'f', 4) % ", Z" %
                             QString::number(translationMin.z() / 1000000, 'f', 4) % " m" % closingDelim % "\n";
    }

    QString travelTypes  = openingDelim % "Travel Types:";
    QString travelColors = openingDelim % "Travel Colors:";
    for (const auto& color : PreferencesManager::getInstance()->getVisualizationHexColors()) {
        travelTypes  = travelTypes % " " % QString::fromStdString(color.first);
        travelColors = travelColors % " " % QString::fromStdString(color.second).right(6);
    }
    travelTypes  = travelTypes % closingDelim % "\n";
    travelColors = travelColors % closingDelim % "\n";

    if (m_selected_meta == GcodeMetaList::TormachMeta)  // Tormach can't handle parsing the travel types and colors
    {
        return partMinTranslation;
    }

    return partMinTranslation % travelTypes % travelColors;
}

void GCodeLoader::run() {
    if (!m_filename.isEmpty()) {
        // Try-catch is necessary to prevent a crash when the GCode refresh button is clicked after an erroneous
        // modification
        try {
            // read in entire file and separate into lines
            QString text;
            QFile inputFile(m_filename);
            if (inputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&inputFile);
                text             = in.readAll();
                m_original_lines = text.split("\n");
                m_lines          = text.toUpper().split("\n");
                inputFile.close();
            }
            else {
                qDebug() << "Error: " << m_filename << " is not readable!";
                return;
            }

            Time min_time(0), max_time(0), total_time(0), total_adjusted_time(0), adjusted_min_time(0),
                adjusted_max_time(0), temp_time(0);
            QString weightInfo = "No statistics calculated";
            // parse header looking for syntax
            setParser(m_original_lines, m_lines);
            connect(m_parser.get(), &CommonParser::statusUpdate, this, &GCodeLoader::forwardDialogUpdate);
            connect(m_parser.get(), &CommonParser::forwardInfoToMainWindow, this,
                    &GCodeLoader::forwardInfoToMainWindow);

            m_parser->parseHeader();
            QHash<QString, double> visualizationSettings = m_parser->parseFooter();
            QList<QList<GcodeCommand>> m_motion_commands = m_parser->parseLines();

            if (m_parser->getWasModified()) {
                text    = m_original_lines.join("\n");
                m_lines = text.toUpper().split("\n");
            }

            QList<Time> layer_times          = m_parser->getLayerTimes();
            QList<Time> adjusted_layer_times = m_parser->getAdjustedLayerTimes();
            QList<double> layer_FR_modifiers = m_parser->getLayerFeedRateModifiers();
            QList<Volume> layer_volumes      = m_parser->getLayerVolumes();

            Volume total_volume;
            min_time          = std::numeric_limits<int>::max();
            max_time          = std::numeric_limits<int>::min();
            adjusted_min_time = std::numeric_limits<int>::max();
            adjusted_max_time = std::numeric_limits<int>::min();

            for (int i = 0; i < layer_times.size(); ++i) {
                Time& current = layer_times[i];

                min_time = qMin(current, min_time);
                max_time = qMax(current, max_time);

                temp_time         = adjusted_layer_times[i];
                adjusted_min_time = qMin(temp_time, adjusted_min_time);
                adjusted_max_time = qMax(temp_time, adjusted_max_time);

                // add current layer to total printing and adjusted time
                total_time += current;
                total_adjusted_time += temp_time;
                total_volume += layer_volumes[i];
            }

            PrintMaterial m_material =
                static_cast<PrintMaterial>((int)visualizationSettings[MS::Density::kMaterialType]);

            Density materialDensity =
                ((m_material == PrintMaterial::kOther) ? (visualizationSettings[MS::Density::kDensity])
                                                       : toDensityValue(m_material));

            Mass total_mass = total_volume * materialDensity;

            // forward to layer_times_window
            emit forwardInfoToLayerTimeWindow(
                layer_times, adjusted_layer_times, layer_FR_modifiers,
                ForceMinimumLayerTime::kSlow_Feedrate ==
                    static_cast<ForceMinimumLayerTime>(m_sb->setting<int>(MS::Cooling::kForceMinLayerTimeMethod)));

            weightInfo = QString::number((total_mass / m_selected_meta.m_mass_unit)()) % " " %
                         m_selected_meta.m_mass_unit.toString();

            // forward to build_log_export
            emit forwardInfoToBuildExportWindow(m_filename, m_selected_meta);

            QString keyInfo = "GCode file: " % m_filename % "\n" % "Total Time Estimate: " %
                              MathUtils::formattedTimeSpan(total_time()) % "\n";

            if (m_adjust_file && total_adjusted_time > 0 && m_sb->setting<int>(MS::Cooling::kForceMinLayerTime)) {
                keyInfo =
                    keyInfo % "Total Adjusted Time: " % MathUtils::formattedTimeSpan(total_adjusted_time()) % "\n";
            }

            double volumeValue = total_volume() / pow<3>(PreferencesManager::getInstance()->getDistanceUnit())();
            double distanceValue =
                (m_parser->getTotalDistance() / PreferencesManager::getInstance()->getDistanceUnit())();
            double printingDistanceValue =
                (m_parser->getPrintingDistance() / PreferencesManager::getInstance()->getDistanceUnit())();
            double travelDistanceValue =
                (m_parser->getTravelDistance() / PreferencesManager::getInstance()->getDistanceUnit())();
            const bool has_adjusted_feedrates =
                std::any_of(layer_FR_modifiers.cbegin(), layer_FR_modifiers.cend(),
                            [](double modifier) { return modifier > 0 && modifier != 1.0; });
            const Time travel_time_estimate =
                has_adjusted_feedrates ? m_parser->getAdjustedTravelTime() : m_parser->getTravelTime();
            double massValue = (total_mass / PreferencesManager::getInstance()->getMassUnit())();
            keyInfo = keyInfo % "Volume: " % QString::number(volumeValue) % " " %
                      PreferencesManager::getInstance()->getDistanceUnit().toString() % "³\n" % "Printing Distance: " %
                      QString::number(printingDistanceValue) % " " %
                      PreferencesManager::getInstance()->getDistanceUnit().toString() % "\n" % "Travel Distance: " %
                      QString::number(travelDistanceValue) % " " %
                      PreferencesManager::getInstance()->getDistanceUnit().toString() % "\n" %
                      "Total Travel Time Estimate: " % MathUtils::formattedTimeSpan(travel_time_estimate()) % "\n" %
                      "Total Distance: " % QString::number(distanceValue) % " " %
                      PreferencesManager::getInstance()->getDistanceUnit().toString() % "\n" % "Approximate Weight (" %
                      toString(m_material) % "): " % QString::number(massValue) % " " %
                      PreferencesManager::getInstance()->getMassUnit().toString() % "\n";

            QTime qt(0, 0);
            qt      = qt.addMSecs(CSM->getSliceTimeElapsed());
            keyInfo = keyInfo % "Total Slice Time (excluding gcode writing/parsing): " % qt.toString("hh:mm:ss.zzz");

            emit forwardInfoToMainWindow(keyInfo);

            m_x_offset                             = visualizationSettings[PRS::Dimensions::kXOffset];
            m_y_offset                             = visualizationSettings[PRS::Dimensions::kYOffset];
            const Distance& z_offset               = visualizationSettings[PRS::Dimensions::kZOffset];
            const Distance& z_min                  = GSM->getGlobal()->setting<Distance>(PRS::Dimensions::kZMin);
            m_z_offset                             = (z_min - z_offset)() * Constants::OpenGL::kObjectToView;
            m_start_pos                            = QVector3D(m_x_offset * Constants::OpenGL::kObjectToView,
                                                               m_y_offset * Constants::OpenGL::kObjectToView, 0.0f);
            m_origin                               = QVector3D(m_x_offset, m_y_offset, 0.0f);
            m_table_offset                         = 0.0f;
            m_prev_table_offset                    = 0.0f;
            m_has_arc_specialties_cylindrical_axis = false;
            m_arc_specialties_cylindrical_axis_matches_current_path = false;
            m_has_previous_arc_specialties_cp                       = false;

            // reserve more memory than the hash will need to guarantee no reallocation
            QHash<QString, QTextCharFormat> fontColors;
            fontColors.reserve(m_lines.size());

            // Retrieve the total number of layers
            const int& total_layer = m_motion_commands.size();

            // Prepopulate the layer settings with the global settings
            QVector<QSharedPointer<SettingsBase>> layer_settings(total_layer, GSM->getGlobal());

            // Set layer specific settings. Currently only supports a single part.
            if (CSM->parts().size() == 1) {
                // Retrieve the settings ranges for the first part
                const QSharedPointer<Part>& part                   = CSM->parts().first();
                const QList<QSharedPointer<SettingsRange>>& ranges = part->getSettingsRanges().values();

                // Populate the layer settings for each range
                for (const QSharedPointer<SettingsRange>& range : ranges) {
                    QSharedPointer<SettingsBase> sb = QSharedPointer<SettingsBase>::create();
                    sb->populate(GSM->getGlobal());
                    sb->populate(range->getSb());

                    for (uint layer = range->low(); layer <= range->high(); layer++) { layer_settings[layer + 1] = sb; }
                }
            }

            // Create the layers
            QVector<QVector<QSharedPointer<SegmentBase>>> layers;

            qint64 total_commands = 0;
            for (const QList<GcodeCommand>& layer_commands : m_motion_commands) {
                total_commands += layer_commands.size();
            }

            qint64 commands_processed       = 0;
            int last_visualization_progress = -1;
            auto emitVisualizationProgress  = [this, &last_visualization_progress](int progress) {
                progress = qBound(0, progress, 99);
                if (progress != last_visualization_progress) {
                    emit updateDialog(StatusUpdateStepType::kVisualization, progress);
                    last_visualization_progress = progress;
                }
            };
            emitVisualizationProgress(0);

            // Generate the segments for each layer
            int current_layer = 0;
            for (const QList<GcodeCommand>& layer_commands : m_motion_commands) {
                // Set the current layer settings
                m_sb = layer_settings[current_layer];

                QVector<QSharedPointer<SegmentBase>> layer;

                for (const GcodeCommand& command : layer_commands) {
                    QColor line_color(
                        PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kUnknown));

                    if (fontColors.contains(command.getComment())) {
                        line_color = fontColors[command.getComment()].foreground().color();
                    }
                    else if (!command.getComment().isEmpty()) {
                        line_color = determineSegmentColor(command.getCommandID(), command.getComment());
                        QTextCharFormat format;
                        format.setForeground(line_color);
                        fontColors.insert(m_original_lines[command.getLineNumber()], format);
                    }

                    QVector<QSharedPointer<SegmentBase>> generated_segments;

                    if (m_selected_meta.hasTravels) {
                        generated_segments = generateVisualSegment(
                            command.getLineNumber() + 1, current_layer, line_color, command.getCommandID(),
                            command.getParameters(), command.getDepositionActive(), command.getExtruderSpeed(), true,
                            command.getComment(), command.getOptionalParameters());
                    }
                    else {
                        generated_segments = generateVisualSegment(
                            command.getLineNumber() + 1, current_layer, line_color, command.getCommandID(),
                            command.getParameters(), command.getDepositionActive(), command.getExtruderSpeed(), false,
                            command.getComment(), command.getOptionalParameters());
                    }
                    layer.append(generated_segments);

                    ++commands_processed;
                    if (total_commands > 0) {
                        emitVisualizationProgress(static_cast<int>(
                            (static_cast<double>(commands_processed) / static_cast<double>(total_commands)) * 100.0));
                    }

                    if (m_should_cancel) { return; }
                }
                layers.push_back(layer);
                ++current_layer;

                if (total_commands == 0) {
                    emitVisualizationProgress(static_cast<int>((double)current_layer / (double)total_layer * 100.0));
                }

                if (m_should_cancel) { return; }
            }

            // emit vector for visualization
            emit gcodeLoadedVisualization(layers);
            // very likely to have allocated too much memory, free extra
            fontColors.squeeze();
            // send text and font colors for display, and line numbers for easy editor navigation
            emit gcodeLoadedText(text, fontColors, m_parser->getLayerStartLines());

            QString openingDelim          = m_selected_meta.m_comment_starting_delimiter;
            QString closingDelim          = m_selected_meta.m_comment_ending_delimiter;
            QString additionalHeaderBlock = openingDelim % "Sliced on: " %
                                            QDateTime::currentDateTime().toString("MM/dd/yyyy") % closingDelim % "\n" %
                                            openingDelim % "Expected Weight: " % weightInfo % closingDelim % "\n";
            if (m_adjust_file && total_adjusted_time > 0 && m_sb->setting<int>(MS::Cooling::kForceMinLayerTime)) {
                additionalHeaderBlock +=
                    openingDelim % "Expected Build Time: " % MathUtils::formattedTimeSpan(total_adjusted_time()) %
                    closingDelim % "\n" % openingDelim % "Minimum Layer Time: " %
                    MathUtils::formattedTimeSpan(adjusted_min_time()) % closingDelim % "\n" % openingDelim %
                    "Maximum Layer Time: " % MathUtils::formattedTimeSpan(adjusted_max_time()) % closingDelim % "\n";
            }
            else {
                additionalHeaderBlock +=
                    openingDelim % "Expected Build Time: " % MathUtils::formattedTimeSpan(total_time()) % closingDelim %
                    "\n" % openingDelim % "Minimum Layer Time: " % MathUtils::formattedTimeSpan(min_time()) %
                    closingDelim % "\n" % openingDelim % "Maximum Layer Time: " %
                    MathUtils::formattedTimeSpan(max_time()) % closingDelim % "\n";
            }
            additionalHeaderBlock += openingDelim % "XYZ Translation Data: " % QString::number(m_origin.x()) % ", " %
                                     QString::number(m_origin.y()) % ", " % QString::number(m_z_offset) % closingDelim %
                                     "\n" % additionalExportComments();

            // if we are allowed to adjust file, add header block and write out file
            // also takes care of situation in which layer times were adjusted
            if (m_adjust_file) {
                QFile tempFile(m_filename % "temp");
                if (tempFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                    QTextStream out(&tempFile);
                    out << additionalHeaderBlock;
                    for (QString& line : m_original_lines) { out << line << "\n"; }
                    tempFile.close();
                    bool ret        = QFile::remove(m_filename);
                    QString tempStr = tempFile.fileName();

                    ret = QFile::rename(tempFile.fileName(), m_filename);
                }
            }
            emit updateDialog(StatusUpdateStepType::kVisualization, 100);
        } catch (ExceptionBase& exception) {
            QString message = "Error parsing GCode: " + QString(exception.what());
            emit error(message);
        }
    }
}

void GCodeLoader::cancelSlice() {
    m_should_cancel = true;
    if (m_parser.get() != nullptr) { m_parser->cancelSlice(); }
}

void GCodeLoader::forwardDialogUpdate(StatusUpdateStepType type, int percentComplete) {
    emit updateDialog(type, percentComplete);
}

// at this moment, parsing the header is simply to find the syntax
void GCodeLoader::setParser(QStringList& originalLines, QStringList& lines) {
    int m_current_line = 0;
    bool foundSyntax   = false;
    QStringMatcher syntaxIdentifier1("G-CODE SYNTAX");
    QStringMatcher syntaxIdentifier2("GCODE SYNTAX");
    while (m_current_line < m_lines.size()) {
        if (syntaxIdentifier1.indexIn(m_lines[m_current_line]) != -1 ||
            syntaxIdentifier2.indexIn(m_lines[m_current_line]) != -1) {
            if (m_lines[m_current_line].contains(toString(GcodeSyntax::kAML3D).toUpper())) {
                m_parser.reset(new CincinnatiParser(GcodeMetaList::AML3DMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::AML3DMeta;
            }
            else if (m_lines[m_current_line].contains(toString(GcodeSyntax::kBeam).toUpper())) {
                m_parser.reset(new BeamParser(GcodeMetaList::BeamMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::BeamMeta;
            }
            else if (m_lines[m_current_line].contains(toString(GcodeSyntax::kCincinnati).toUpper())) {
                m_parser.reset(
                    new CincinnatiParser(GcodeMetaList::CincinnatiMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::CincinnatiMeta;
            }
            else if (m_lines[m_current_line].contains(toString(GcodeSyntax::kDmgDmu).toUpper())) {
                m_parser.reset(new CommonParser(GcodeMetaList::DmgDmuAndBeamMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::DmgDmuAndBeamMeta;
            }
            else if (m_lines[m_current_line].contains(toString(GcodeSyntax::kGudel).toUpper())) {
                m_parser.reset(new CommonParser(GcodeMetaList::GudelMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::GudelMeta;
            }
            else if (m_lines[m_current_line].contains(toString(GcodeSyntax::kHaasInch).toUpper())) {
                m_parser.reset(new CommonParser(GcodeMetaList::HaasInchMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::HaasInchMeta;
            }
            else if (m_lines[m_current_line].contains(toString(GcodeSyntax::kHaasMetric).toUpper())) {
                m_parser.reset(new CommonParser(GcodeMetaList::HaasMetricMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::HaasMetricMeta;
            }
            else if (m_lines[m_current_line].contains(toString(GcodeSyntax::kHurco).toUpper())) {
                m_parser.reset(new CommonParser(GcodeMetaList::HurcoMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::HurcoMeta;
            }
            else if (m_lines[m_current_line].contains(toString(GcodeSyntax::kIngersoll).toUpper())) {
                m_parser.reset(new CommonParser(GcodeMetaList::IngersollMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::IngersollMeta;
            }
            else if (m_lines[m_current_line].contains(toString(GcodeSyntax::kKraussMaffei).toUpper())) {
                m_parser.reset(new MarlinParser(GcodeMetaList::KraussMaffeiMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::KraussMaffeiMeta;
            }
            else if (m_lines[m_current_line].contains(toString(GcodeSyntax::kJuggerBot).toUpper())) {
                m_parser.reset(new MarlinParser(GcodeMetaList::MarlinMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::MarlinMeta;
            }
            else if (m_lines[m_current_line].contains(toString(GcodeSyntax::kMarlin).toUpper())) {
                m_parser.reset(new MarlinParser(GcodeMetaList::MarlinMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::MarlinMeta;
            }
            else if (m_lines[m_current_line].contains(toString(GcodeSyntax::kMach4).toUpper())) {  // Mach4 uses Marlin
                m_parser.reset(new MarlinParser(GcodeMetaList::MarlinMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::MarlinMeta;
            }
            else if (m_lines[m_current_line].contains(
                         toString(GcodeSyntax::kRepRap).toUpper())) {  // RepRap uses Marlin
                m_parser.reset(new MarlinParser(GcodeMetaList::RepRapMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::RepRapMeta;
            }
            else if (m_lines[m_current_line].contains(toString(GcodeSyntax::kMazak).toUpper())) {
                m_parser.reset(new MazakParser(GcodeMetaList::MazakMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::MazakMeta;
            }
            else if (m_lines[m_current_line].contains(toString(GcodeSyntax::kMeld).toUpper())) {
                m_parser.reset(new CommonParser(GcodeMetaList::MeldMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::MeldMeta;
            }
            else if (m_lines[m_current_line].contains(toString(GcodeSyntax::kMeltio).toUpper())) {
                m_parser.reset(new CommonParser(GcodeMetaList::MeltioMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::MeltioMeta;
            }
            else if (m_lines[m_current_line].contains(toString(GcodeSyntax::kMVP).toUpper())) {
                m_parser.reset(new MVPParser(GcodeMetaList::MVPMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::MVPMeta;
            }
            else if (m_lines[m_current_line].contains(toString(GcodeSyntax::kOkuma).toUpper())) {
                m_parser.reset(new CommonParser(GcodeMetaList::HaasMetricMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::ORNLMeta;
            }
            else if (m_lines[m_current_line].contains(toString(GcodeSyntax::kORNLMetric).toUpper())) {
                m_parser.reset(new CommonParser(GcodeMetaList::ORNLMetricMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::ORNLMetricMeta;
            }
            else if (m_lines[m_current_line].contains(toString(GcodeSyntax::kORNL).toUpper())) {
                m_parser.reset(new CommonParser(GcodeMetaList::ORNLMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::ORNLMeta;
            }
            else if (m_lines[m_current_line].contains(toString(GcodeSyntax::kArcSpecialties).toUpper())) {
                m_parser.reset(
                    new ArcSpecialtiesParser(GcodeMetaList::ArcSpecialtiesMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::ArcSpecialtiesMeta;
            }
            else if (m_lines[m_current_line].contains(toString(GcodeSyntax::kRomiFanuc).toUpper())) {
                m_parser.reset(new CommonParser(GcodeMetaList::RomiFanucMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::RomiFanucMeta;
            }
            else if (m_lines[m_current_line].contains(toString(GcodeSyntax::kSandia).toUpper())) {
                m_parser.reset(new CommonParser(GcodeMetaList::SandiaMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::SandiaMeta;
            }
            else if (m_lines[m_current_line].contains(toString(GcodeSyntax::kSiemens).toUpper())) {
                m_parser.reset(new SiemensParser(GcodeMetaList::SiemensMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::SiemensMeta;
            }
            else if (m_lines[m_current_line].contains(toString(GcodeSyntax::kThermwood).toUpper())) {
                m_parser.reset(
                    new CincinnatiParser(GcodeMetaList::CincinnatiMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::CincinnatiMeta;
            }
            else if (m_lines[m_current_line].contains(toString(GcodeSyntax::kTormach).toUpper())) {
                m_parser.reset(new TormachParser(GcodeMetaList::TormachMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::TormachMeta;
            }
            else if (m_lines[m_current_line].contains(toString(GcodeSyntax::kWolf).toUpper())) {
                m_parser.reset(new CommonParser(GcodeMetaList::WolfMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::WolfMeta;
            }
            else if (m_lines[m_current_line].contains(toString(GcodeSyntax::kAeroBasic).toUpper())) {
                m_parser.reset(new AeroBasicParser(GcodeMetaList::AeroBasicMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::AeroBasicMeta;
            }
            else if (m_lines[m_current_line].contains(toString(GcodeSyntax::kAdamantine).toUpper())) {
                m_parser.reset(
                    new AdamantineParser(GcodeMetaList::AdamantineMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::AdamantineMeta;
            }
            else {
                qDebug() << "Warning: unknown syntax";
                m_parser.reset(new CommonParser(GcodeMetaList::MarlinMeta, m_adjust_file, originalLines, lines));
                m_selected_meta = GcodeMetaList::MarlinMeta;
            }
            foundSyntax = true;
        }
        ++m_current_line;
        if (foundSyntax) { break; }
    }

    if (!foundSyntax) {
        qDebug() << "No syntax definition found: attempting common";
        m_parser.reset(new CommonParser(GcodeMetaList::MarlinMeta, m_adjust_file, originalLines, lines));
    }
}

QColor GCodeLoader::determineFontColor(const QString& comment) {
    if (m_prestart.indexIn(comment) != -1) {
        return PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kPrestart);
    }
    if (m_initial_startup.indexIn(comment) != -1) {
        return PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kInitialStartup);
    }
    if (m_slowdown.indexIn(comment) != -1) {
        return PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kSlowDown);
    }
    if (m_forward_tipwipe.indexIn(comment) != -1) {
        return PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kTipWipeForward);
    }
    if (m_reverse_tipwipe.indexIn(comment) != -1) {
        return PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kTipWipeReverse);
    }
    if (m_angled_tipwipe.indexIn(comment) != -1) {
        return PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kTipWipeAngled);
    }
    if (m_coasting.indexIn(comment) != -1) {
        return PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kCoasting);
    }
    if (m_spirallift.indexIn(comment) != -1) {
        return PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kSpiralLift);
    }
    if (m_rampingup.indexIn(comment) != -1) {
        return PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kRampingUp);
    }
    if (m_rampingdown.indexIn(comment) != -1) {
        return PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kRampingDown);
    }
    if (m_leadin.indexIn(comment) != -1) {
        return PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kLeadIn);
    }
    if (m_travel.indexIn(comment) != -1) {
        return PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kTravel);
    }
    if (m_support.indexIn(comment) != -1) {
        return PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kSupport);
    }
    if (m_perimeter.indexIn(comment) != -1) {
        return PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kPerimeter);
    }
    if (m_radial.indexIn(comment) != -1) {
        return PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kRadial);
    }
    if (m_helical.indexIn(comment) != -1) {
        return PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kHelical);
    }
    if (m_inset.indexIn(comment) != -1) {
        return PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kInset);
    }
    if (m_infill.indexIn(comment) != -1) {
        return PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kInfill);
    }
    if (m_skin.indexIn(comment) != -1) {
        return PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kSkin);
    }
    if (m_skeleton.indexIn(comment) != -1) {
        return PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kSkeleton);
    }
    if (m_raft.indexIn(comment) != -1) {
        return PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kRaft);
    }
    if (m_brim.indexIn(comment) != -1) {
        return PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kBrim);
    }
    if (m_skirt.indexIn(comment) != -1) {
        return PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kSkirt);
    }
    if (m_laserscan.indexIn(comment) != -1) {
        return PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kLaserScan);
    }
    if (m_thermalscan.indexIn(comment) != -1) {
        return PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kThermalScan);
    }

    return PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kUnknown);
}

QColor GCodeLoader::determineSegmentColor(int command_id, const QString& comment) {
    QColor color = determineFontColor(comment);
    if (command_id != 2 && command_id != 3) { return color; }

    if (containsColorPriorityModifier(comment)) { return color; }

    if (m_perimeter.indexIn(comment) != -1) {
        return PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kPerimeterArc);
    }

    if (m_inset.indexIn(comment) != -1) {
        return PreferencesManager::getInstance()->getVisualizationColor(VisualizationColors::kInsetArc);
    }

    return color;
}

bool GCodeLoader::containsColorPriorityModifier(const QString& comment) const {
    return m_prestart.indexIn(comment) != -1 || m_initial_startup.indexIn(comment) != -1 ||
           m_slowdown.indexIn(comment) != -1 || m_forward_tipwipe.indexIn(comment) != -1 ||
           m_reverse_tipwipe.indexIn(comment) != -1 || m_angled_tipwipe.indexIn(comment) != -1 ||
           m_coasting.indexIn(comment) != -1 || m_spirallift.indexIn(comment) != -1 ||
           m_rampingup.indexIn(comment) != -1 || m_rampingdown.indexIn(comment) != -1 ||
           m_leadin.indexIn(comment) != -1 || comment.contains(Constants::PathModifierStrings::kPerimeterTipWipe);
}

SegmentDisplayType GCodeLoader::determineSegmentDisplayType(const QString& comment) {
    SegmentDisplayType type = SegmentDisplayType::kNone;

    if (m_travel.indexIn(comment) != -1) { type |= SegmentDisplayType::kTravel; }
    if (m_support.indexIn(comment) != -1) { type |= SegmentDisplayType::kSupport; }

    return type == SegmentDisplayType::kNone ? SegmentDisplayType::kLine : type;
}

void GCodeLoader::setSegmentDisplayInfo(QSharedPointer<SegmentBase>& segment, SegmentDisplayType type,
                                        const QColor& color, const QString& comment, const QVector3D& start_pos,
                                        const QVector3D& end_pos, const int& line_num, const int& layer_num) {
    // Set the display info of the segment
    float display_width  = 0.0f;
    float display_height = m_sb->setting<float>(PS::Layer::kLayerHeight) * Constants::OpenGL::kObjectToView;
    float display_length = start_pos.distanceToPoint(end_pos);
    float scale =
        m_modifier_colors.contains(color) ? 1.1f : 1.0f;  // Scale modifier segments by 1.1 for better visibility

    // Set the display width of the segment based on its region type
    if (comment.contains(Constants::RegionTypeStrings::kRadial) ||
        comment.contains(Constants::RegionTypeStrings::kHelical)) {
        display_width = m_sb->setting<float>(PS::Layer::kBeadWidth) * Constants::OpenGL::kObjectToView;
    }
    else if (comment.startsWith(QStringLiteral("AD-") % Constants::RegionTypeStrings::kPerimeter)) {
        display_width = beadDisplayWidth(beadWidthFromRegionComment(comment, Constants::RegionTypeStrings::kPerimeter,
                                                                    m_sb->setting<Distance>(PS::Perimeter::kBeadWidth),
                                                                    m_selected_meta.m_distance_unit));
    }
    else if (comment.contains(Constants::RegionTypeStrings::kPerimeter)) {
        display_width = m_sb->setting<float>(PS::Perimeter::kBeadWidth) * Constants::OpenGL::kObjectToView;
    }
    else if (comment.startsWith(QStringLiteral("AD-") % Constants::RegionTypeStrings::kInset)) {
        display_width = beadDisplayWidth(beadWidthFromRegionComment(comment, Constants::RegionTypeStrings::kInset,
                                                                    m_sb->setting<Distance>(PS::Inset::kBeadWidth),
                                                                    m_selected_meta.m_distance_unit));
    }
    else if (comment.contains("INSET")) {
        display_width = m_sb->setting<float>(PS::Inset::kBeadWidth) * Constants::OpenGL::kObjectToView;
    }
    else if (comment.contains("SKELETON")) {
        display_width = beadDisplayWidth(beadWidthFromRegionComment(comment, Constants::RegionTypeStrings::kSkeleton,
                                                                    m_sb->setting<Distance>(PS::Skeleton::kBeadWidth),
                                                                    m_selected_meta.m_distance_unit));
    }
    else if (comment.contains("SKIN")) {
        display_width = m_sb->setting<float>(PS::Skin::kBeadWidth) * Constants::OpenGL::kObjectToView;
    }
    else if (comment.contains("INFILL")) {
        display_width = m_sb->setting<float>(PS::Infill::kBeadWidth) * Constants::OpenGL::kObjectToView;
    }
    else {  // Default to layer bead width
        display_width = m_sb->setting<float>(PS::Layer::kBeadWidth) * Constants::OpenGL::kObjectToView;
    }

    // Set the display info of the segment
    segment->setDisplayInfo(display_width * scale, display_length, display_height * scale, type, color, line_num,
                            layer_num);
}

void GCodeLoader::setSegmentMetaInfo(QSharedPointer<SegmentBase>& segment, const QString& comment,
                                     QVector3D& info_end_pos, const bool& deposition_active, const bool& info_speed_set,
                                     const double& extruder_speed) {
    // Set the type info of the segment
    segment->m_segment_info_meta.type = comment;

    // Set the start and end position info of the segment
    segment->m_segment_info_meta.start = m_info_start_pos;
    segment->m_segment_info_meta.end   = info_end_pos;

    // If deposition is active, retain the current speed metadata for the segment.
    if (!deposition_active && !info_speed_set) { segment->m_segment_info_meta.speed = ""; }
    else { segment->m_segment_info_meta.speed = m_info_speed; }

    // If deposition is active, set the material feed speed metadata.
    if (deposition_active) {
        if (m_info_extruder_speed.isEmpty()) {
            segment->m_segment_info_meta.extruderSpeed = QString().asprintf("%0.4f", extruder_speed) % " rpm";
        }
        else { segment->m_segment_info_meta.extruderSpeed = m_info_extruder_speed; }
    }
    else { segment->m_segment_info_meta.extruderSpeed = ""; }

    // Set the length info of the segment

    float length =
        m_info_start_pos.distanceToPoint(info_end_pos) / PreferencesManager::getInstance()->getDistanceUnit()();
    segment->m_segment_info_meta.length =
        QString().asprintf("%0.2f", length) % " " % PreferencesManager::getInstance()->getDistanceUnitText();
}

QVector<QSharedPointer<SegmentBase>> GCodeLoader::generateVisualSegment(
    int line_num, int layer_num, const QColor& color, int command_id, const QMap<char, double>& parameters,
    bool deposition_active, double extruder_speed, bool include_non_deposition_moves, QString comment,
    const QMap<char, double>& optional_parameters) {
    // Parameters for drawing and placing each segment in the world correctly
    QVector3D end_pos             = m_start_pos;
    QVector3D info_end_pos        = m_info_start_pos;
    bool info_speed_set           = false;
    const bool is_arc_specialties = m_selected_meta.m_syntax_id == GcodeSyntax::kArcSpecialties;
    const bool has_current_cp     = optional_parameters.contains(kArcSpecialtiesCpOptionalParameter);
    const double current_cp = has_current_cp ? optional_parameters.value(kArcSpecialtiesCpOptionalParameter) : 0.0;

    if (parameters.contains('F')) {
        info_speed_set = true;
        m_info_speed   = QString().asprintf("%0.4f", (Velocity(parameters['F']) /
                                                      PreferencesManager::getInstance()->getVelocityUnit())()) %
                         " " % PreferencesManager::getInstance()->getVelocityUnitText();
    }
    if (parameters.contains('S')) {
        m_info_extruder_speed = QString().asprintf("%0.4f", (AngularVelocity(parameters['S']) /
                                                             m_selected_meta.m_angular_velocity_unit)()) %
                                " rpm";
    }
    if (parameters.contains('W')) {
        m_prev_table_offset = m_table_offset;
        m_table_offset      = parameters['W'] * Constants::OpenGL::kObjectToView;

        // we don't draw segments for commands that are just table shifts, so only update end_pos if XY changes too
        if (parameters.contains('X') || parameters.contains('Y')) {
            end_pos.setZ(m_start_pos.z() + m_prev_table_offset - m_table_offset);
            m_prev_table_offset =
                parameters['W'] *
                Constants::OpenGL::kObjectToView;  // accounted for the table offset, so no need for prev
        }
    }

    //! \note optional_parameters hold start locations for gcommand.  Syntaxes that use this do not have travels.
    // Always draw segments if X and Y are specified (which should be all segments except table-shifts between layers)
    if (parameters.contains('X')) {
        info_end_pos.setX(parameters['X'] + m_x_offset);
        end_pos.setX(((parameters['X'] + m_x_offset) * Constants::OpenGL::kObjectToView));

        if (optional_parameters.contains('X')) {
            m_start_pos.setX(((optional_parameters['X']) * Constants::OpenGL::kObjectToView));
        }
    }
    if (parameters.contains('Y')) {
        info_end_pos.setY(parameters['Y'] + m_y_offset);
        end_pos.setY(((parameters['Y'] + m_y_offset) * Constants::OpenGL::kObjectToView));

        if (optional_parameters.contains('Y')) {
            m_start_pos.setY(((optional_parameters['Y']) * Constants::OpenGL::kObjectToView));
        }
    }

    // Z is special! Must account for the table movement and the fact that z=0
    // is not on the floor of the printer
    if (parameters.contains('Z')) {
        info_end_pos.setZ(parameters['Z']);
        end_pos.setZ((parameters['Z'] * Constants::OpenGL::kObjectToView) + m_z_offset - m_table_offset);

        if (optional_parameters.contains('Z')) {
            m_start_pos.setZ(((optional_parameters['Z']) * Constants::OpenGL::kObjectToView));
        }

        // if the z is specified, it accounts for any table shift that might have happened in this or any previous
        // command
        m_prev_table_offset = m_table_offset;
    }
    // Z wasn't specified, but it might need updated because of a table shift in a previous command
    // (if the table shift was in this command, we would've accounted for it already)
    else if (!qFuzzyCompare(m_prev_table_offset, m_table_offset) &&
             (parameters.contains('X') || parameters.contains('Y'))) {
        info_end_pos.setZ(m_info_start_pos.z() + m_prev_table_offset - m_table_offset);
        end_pos.setZ(m_start_pos.z() + m_prev_table_offset - m_table_offset);

        // we've accounted for the change in w, don't want to do it again until the table moves (w_offset changes) again
        m_prev_table_offset = m_table_offset;
    }

    const bool is_world_approach_travel =
        is_arc_specialties && comment.compare(kWorldApproachTravelComment, Qt::CaseInsensitive) == 0;
    if (is_world_approach_travel && parameters.contains('X') && parameters.contains('Y')) {
        m_arc_specialties_cylindrical_axis                      = Point(end_pos.x(), end_pos.y(), 0.0);
        m_has_arc_specialties_cylindrical_axis                  = true;
        m_arc_specialties_cylindrical_axis_matches_current_path = true;
    }

    QVector<QSharedPointer<SegmentBase>> generated_segments;
    const bool is_cylindrical_print = isCylindricalPrintComment(comment);
    if (is_arc_specialties && has_current_cp && !is_cylindrical_print && !is_world_approach_travel) {
        m_arc_specialties_cylindrical_axis_matches_current_path = false;
    }

    if (deposition_active || include_non_deposition_moves) {
        QSharedPointer<SegmentBase> segment;

        // Builds and draws segments according to their type (Line, Arc, Spline)
        if (command_id == 2 || command_id == 3) {  // G2 clockwise arc & G3 counter-clockwise arc
            // Parse extra params
            Point center;

            if (parameters.contains('R') && !parameters.contains('I') && !parameters.contains('J')) {
                const double radius                = parameters['R'] * Constants::OpenGL::kObjectToView;
                const double abs_radius            = qAbs(radius);
                const double dx                    = end_pos.x() - m_start_pos.x();
                const double dy                    = end_pos.y() - m_start_pos.y();
                const double chord_length          = std::hypot(dx, dy);
                const double half_chord            = chord_length / 2.0;
                const double center_offset_squared = (abs_radius * abs_radius) - (half_chord * half_chord);

                if (chord_length > std::numeric_limits<double>::epsilon() && center_offset_squared >= 0.0) {
                    const double center_offset = qSqrt(center_offset_squared);
                    const double mid_x         = (m_start_pos.x() + end_pos.x()) / 2.0;
                    const double mid_y         = (m_start_pos.y() + end_pos.y()) / 2.0;
                    const double left_normal_x = -dy / chord_length;
                    const double left_normal_y = dx / chord_length;
                    const bool use_left_center = (command_id == 3) == (radius >= 0.0);
                    const double direction     = use_left_center ? 1.0 : -1.0;

                    center.x(mid_x + (direction * left_normal_x * center_offset));
                    center.y(mid_y + (direction * left_normal_y * center_offset));
                    center.z(m_start_pos.z());
                    segment = QSharedPointer<ArcSegment>::create(m_start_pos, end_pos, center, (command_id == 3));
                }
            }
            else if (parameters.contains('I') && parameters.contains('J')) {
                // Determine center from I, J
                center.x(m_start_pos.x() + ((parameters['I']) * Constants::OpenGL::kObjectToView));
                center.y(m_start_pos.y() + ((parameters['J']) * Constants::OpenGL::kObjectToView));

                if (parameters.contains('K')) {
                    center.z(m_start_pos.z() + ((parameters['K']) * Constants::OpenGL::kObjectToView));
                }
                else { center.z(m_start_pos.z()); }

                segment = QSharedPointer<ArcSegment>::create(m_start_pos, end_pos, center, (command_id == 3));
            }
        }
        else if (command_id == 5) {  // G5 splines
            Point control_a;
            Point control_b;

            if (parameters.contains('I') && parameters.contains('J') && parameters.contains('P') &&
                parameters.contains('Q')) {
                control_a.x(m_start_pos.x() + ((parameters['I']) * Constants::OpenGL::kObjectToView));
                control_a.y(m_start_pos.y() + ((parameters['J']) * Constants::OpenGL::kObjectToView));
                control_b.x(end_pos.x() + ((parameters['P']) * Constants::OpenGL::kObjectToView));
                control_b.y(end_pos.y() + ((parameters['Q']) * Constants::OpenGL::kObjectToView));

                segment = QSharedPointer<BezierSegment>::create(m_start_pos, control_a, control_b, end_pos);
            }
        }
        else {  // G0, G1, or anything else is drawn as a line
            segment = QSharedPointer<LineSegment>::create(m_start_pos, end_pos);
        }

        if (segment.isNull()) { segment = QSharedPointer<LineSegment>::create(m_start_pos, end_pos); }

        segment->setDepositionActive(deposition_active);

        // Set the segment's display info
        setSegmentDisplayInfo(segment, determineSegmentDisplayType(comment), color, comment, m_start_pos, end_pos,
                              line_num, layer_num);

        if (is_cylindrical_print) {
            Point cylindrical_axis;
            bool has_cylindrical_axis = false;
            if (cylindricalAxisFromComment(comment, m_selected_meta.m_distance_unit, m_x_offset, m_y_offset,
                                           cylindrical_axis)) {
                has_cylindrical_axis = true;
            }
            else if (ArcSegment* arc_segment = dynamic_cast<ArcSegment*>(segment.data())) {
                cylindrical_axis     = arc_segment->center();
                has_cylindrical_axis = true;
            }
            else if (is_arc_specialties && m_has_previous_arc_specialties_cp && has_current_cp) {
                const std::optional<Point> reference_axis =
                    m_has_arc_specialties_cylindrical_axis && m_arc_specialties_cylindrical_axis_matches_current_path
                        ? std::optional<Point>(m_arc_specialties_cylindrical_axis)
                        : std::nullopt;
                const bool reverse_cp_delta =
                    comment.contains(Constants::RegionTypeStrings::kHelical, Qt::CaseInsensitive) &&
                    static_cast<HelicalPathHandedness>(m_sb->setting<int>(PS::Slicing::kHelicalPathHandedness)) ==
                        HelicalPathHandedness::kLeftHanded;
                has_cylindrical_axis = ArcSpecialtiesAxisInference::cylindricalAxisFromCpDelta(
                    m_start_pos, end_pos, m_previous_arc_specialties_cp, current_cp, reverse_cp_delta, reference_axis,
                    cylindrical_axis);
            }
            if (!has_cylindrical_axis && is_arc_specialties && m_has_arc_specialties_cylindrical_axis &&
                m_arc_specialties_cylindrical_axis_matches_current_path) {
                cylindrical_axis     = m_arc_specialties_cylindrical_axis;
                has_cylindrical_axis = true;
            }

            if (has_cylindrical_axis) {
                segment->setCylindricalBeadCenter(cylindrical_axis);
                if (is_arc_specialties) {
                    m_arc_specialties_cylindrical_axis                      = cylindrical_axis;
                    m_has_arc_specialties_cylindrical_axis                  = true;
                    m_arc_specialties_cylindrical_axis_matches_current_path = true;
                }
            }
        }

        // Set the segment's meta info
        setSegmentMetaInfo(segment, comment, info_end_pos, deposition_active, info_speed_set, extruder_speed);

        // Add the segment to the list of generated segments
        generated_segments.append(segment);
    }
    // Update our start position for the next command
    m_start_pos      = end_pos;
    m_info_start_pos = info_end_pos;
    if (is_arc_specialties && has_current_cp) {
        m_previous_arc_specialties_cp     = current_cp;
        m_has_previous_arc_specialties_cp = true;
    }

    return generated_segments;
}
}  // namespace ORNL
