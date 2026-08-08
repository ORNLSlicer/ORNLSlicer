#pragma once

#include <QTextCharFormat>
#include <QThread>
#include <qcolor.h>
#include <qcontainerfwd.h>
#include <qhash.h>
#include <qlist.h>
#include <qmap.h>
#include <qregularexpression.h>
#include <qscopedpointer.h>
#include <qsharedpointer.h>
#include <qstringmatcher.h>
#include <qtmetamacros.h>
#include <qvectornd.h>

#include "gcode/gcode_meta.h"
#include "gcode/parsers/common_parser.h"
#include "geometry/point.h"
#include "geometry/segment_base.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {
/*!
 * \class GCodeLoader
 * \brief Threaded class that loads gcode, invokes appropriate parser,
 * and generates visualization.
 */
class GCodeLoader : public QThread {
    Q_OBJECT
  public:
    //! \brief Constructor
    //! \param filename: Name of file to load
    //! \param conversion ratio to convert from model space to view space for
    //! visualization (currently a set constant)
    //! \param alterFile: boolean to determine whether to allow file alteration via
    //! minimum layer time enforcement defined in the file
    GCodeLoader(QString filename, bool alterFile);

    //! \brief Function that is run when start is called on this thread.
    void run() override;

  signals:

    //! \brief signal to view with info for visualization
    //! \param layers: vector of graphics layers for visualization.  Information is
    //! provided to OpenGL view
    void gcodeLoadedVisualization(QVector<QVector<QSharedPointer<SegmentBase>>> layers);

    //! \brief signal to UI with info for text and text font color
    //! \param text: text from file to display
    //! \param fontColors: Hash with formats to define colors for all lines of text. Allows lookup for each line
    //! into the hash for appropriate format.
    //! \param layerFirstLineNumbers: line numbers for BEGINNING LAYER for each layer to jump the cursor to
    //! appropriate line when spinbox moves up and down.
    void gcodeLoadedText(QString text, QHash<QString, QTextCharFormat> fontColors, QList<int> layerFirstLineNumbers);

    //! \brief Emits error signal
    //! \param msg: Qstring error message
    void error(QString msg);

    //! \brief signal to layer time window with info
    //! \param layertimes: List of layer times for display
    void forwardInfoToLayerTimeWindow(QList<Time> layer_times, QList<Time> adjusted_layer_times,
                                      QList<double> layer_FR_modifiers, bool adjusted_layer_time);

    //! \brief signal to export window with info
    //! \param filename: temp gcode filename to copy
    //! \param meta: Currently selected meta
    void forwardInfoToBuildExportWindow(QString filename, GcodeMeta meta);

    //! \brief signal to main window with info
    //! \param parsingInfo: Formatted QString with various pieces of info to display in
    //!  main_window's status window.  Info includes: time, volume, weight, and material info.
    void forwardInfoToMainWindow(QString parsingInfo);

    //! \brief update slice dialog with current status
    //! \param type: Current step process
    //! \param percentComplete: Current completion percentage
    void updateDialog(StatusUpdateStepType type, int percentComplete);

  public slots:

    //! \brief Set cancel flag as received from slice dialog
    void cancelSlice();

    //! \brief Forward update from parser to update slice dialog with current status
    //! \param type: Current step process
    //! \param percentComplete: Current completion percentage
    void forwardDialogUpdate(StatusUpdateStepType type, int percentComplete);

  private:
    //! \brief parse header looking for syntax info and base offset
    //! \param originalLines: Original lines from gcode file
    //! \param lines Uppercase: lines used for ease of parsing/comparison
    void setParser(QStringList& originalLines, QStringList& lines);

    //! \brief determine color based on comment keywords
    //! \param comment: Comment to parse
    //! \return color based on comment keywords
    QColor determineFontColor(const QString& comment);

    //! \brief determine visualization color based on motion command and comment keywords
    //! \param command_id: Parsed G-code motion command ID.
    //! \param comment: Comment to parse
    //! \return color based on command type and comment keywords
    QColor determineSegmentColor(int command_id, const QString& comment);

    //! \brief Checks whether a comment contains a path modifier with color precedence.
    //! \param comment: Comment to parse
    //! \return True when modifier coloring should not be overridden by region-specific arc colors.
    bool containsColorPriorityModifier(const QString& comment) const;

    //! \brief determine display type based on comment keywords
    //! \param comment: Comment to parse
    //! \return semantic display type used for visibility and selection behavior
    SegmentDisplayType determineSegmentDisplayType(const QString& comment);

    //! \brief Set the segment display information based on the region type defined in the comment.
    //! \param segment: Segment to set display info for.
    //! \param type: Segment visibility/display classification.
    //! \param color: Color to set for the segment.
    //! \param comment: Comment to parse for the segment type.
    //! \param start_pos: The start position of the segment.
    //! \param end_pos: The end position of the segment.
    //! \param line_num: The line number of the segment.
    //! \param layer_num: The layer number of the segment.
    void setSegmentDisplayInfo(QSharedPointer<SegmentBase>& segment, SegmentDisplayType type, const QColor& color,
                               const QString& comment, const QVector3D& start_pos, const QVector3D& end_pos,
                               const int& line_num, const int& layer_num);

    //! \brief Set the segment meta information. This is the information that is displayed when the user selects
    //! a segment in the UI.
    //! \param segment: Segment to set meta info for.
    //! \param comment: Comment to parse for the segment type.
    //! \param info_end_pos: The end position of the segment.
    //! \param deposition_active: Indicates if the command is actively depositing material.
    //! \param info_speed_set: Indicates if the speed is set.
    //! \param extruder_speed: The speed of the extruder.
    void setSegmentMetaInfo(QSharedPointer<SegmentBase>& segment, const QString& comment, QVector3D& info_end_pos,
                            const bool& deposition_active, const bool& info_speed_set, const double& extruder_speed);

    //! \brief generate additional export comments
    //! \return string
    QString additionalExportComments();

    //! \brief generates an open gl object for a given gcode command
    //! \param line_num: Line number that links visual segment to gcode for highlighting
    //! \param layer_num Layer: to inset segment into
    //! \param color: Color of segment based on comments from gcode
    //! \param command_id: the id of the gcode command for th segment
    //! \param parameters: Parameters of gcodecommand end (x, y, z, w, i, j, p, q)
    //! \param deposition_active: whether the command is actively depositing material
    //! \param extruder_speed: double value read from gcode
    //! \param include_non_deposition_moves: if true, generates visual segments when deposition is inactive
    //! \param optional_parameters: Parameters of gcodecommand for start (x, y, z, w)
    //! \return List of generated visual segments.
    QVector<QSharedPointer<SegmentBase>>
    generateVisualSegment(int line_num, int layer_num, const QColor& color, int command_id,
                          const QMap<char, double>& parameters, bool deposition_active, double extruder_speed,
                          bool include_non_deposition_moves, const QString comment,
                          const QMap<char, double>& optional_parameters = QMap<char, double>());

    //! \brief Filename.
    QString m_filename;

    //! \brief Gcode parser set to appropriate syntax once header is parsed
    QScopedPointer<CommonParser> m_parser;

    //! \brief Meta info determined from file
    GcodeMeta m_selected_meta;

    //! \brief Original lines in gcode file used as keys for text font hash
    //! m_lines contains all uppercase version to simplify text comparison
    QStringList m_lines, m_original_lines;

    //! \brief matchers for modifier identification for coloring
    QStringMatcher m_prestart, m_initial_startup, m_slowdown, m_forward_tipwipe, m_reverse_tipwipe, m_angled_tipwipe,
        m_coasting, m_spirallift, m_rampingup, m_rampingdown, m_leadin;

    //! \brief matchers for type identification for coloring
    QStringMatcher m_perimeter, m_radial, m_helical, m_inset, m_infill, m_skin, m_skeleton, m_support, m_support_roof,
        m_travel, m_raft, m_brim, m_skirt, m_laserscan, m_thermalscan;

    //! \brief colors for modifiers to adjust display size
    QVector<QColor> m_modifier_colors;

    //! \brief bool to indicate whether gcode should be adjusted for minimal layer time
    bool m_adjust_file;

    //! \brief Settings for visualization
    //! \brief conversion from internal units to OpenGL units
    float m_micron_to_view_conversion;

    //! \brief Current Gcode start position
    QVector3D m_start_pos;

    //! \brief Origin adjusted for offsets in settings, needed to undo adjustment in gcode
    QVector3D m_origin;

    //! \brief Offset for x, y, z
    float m_x_offset, m_y_offset, m_z_offset;

    //! \brief Offset for table - w
    float m_table_offset;

    //! \brief Previous offset for table - w, used to account for Z, W, or both axes
    float m_prev_table_offset;

    //! \brief conversion between color space of Qt to OpenGL
    float m_color_space_conversion;

    //! \brief flag to indicate if process should cancel
    bool m_should_cancel;

    //! \brief A regular expression to find the layer number in a comment
    QRegularExpression m_layer_pattern;

    //! \brief Current Gcode start position for info display
    QVector3D m_info_start_pos;

    //! \brief Current Gcode speed for info display
    QString m_info_speed;

    //! \brief Current Gcode extruder speed for info display
    QString m_info_extruder_speed;

    //! \brief Current settings for gcode loader
    QSharedPointer<SettingsBase> m_sb;
}; // class GCodeLoader
} // namespace ORNL
