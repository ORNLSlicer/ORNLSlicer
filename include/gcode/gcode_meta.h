#pragma once

#include "utilities/enums.h"

namespace ORNL {
//! \brief A plain old data structure for GCode meta information.
struct GcodeMeta {
    GcodeSyntax m_syntax_id;
    QString m_comment_starting_delimiter;
    QString m_comment_ending_delimiter;
    Distance m_distance_unit;
    Time m_time_unit;
    Angle m_angle_unit;
    Mass m_mass_unit;
    Velocity m_velocity_unit;
    Acceleration m_acceleration_unit;
    AngularVelocity m_angular_velocity_unit;
    QString m_file_suffix;
    bool hasTravels = true;
    QString m_layer_count_delimiter = "LAYER COUNT";
    QString m_layer_delimiter = "BEGINNING LAYER";

    bool operator==(const GcodeMeta& rhs) const = default;
};

//! \brief A namespace just to make the meta structs pretty to access and prevent
//! possible name clashing
namespace GcodeMetaList {

static GcodeMeta BeamMeta = {GcodeSyntax::kBeam, QString("("), QString(")"), in,    s, degree, lbm,
                             in / minute,        in / s / s,   rev / minute, ".mpf"};
static GcodeMeta CincinnatiMeta = {GcodeSyntax::kCincinnati,
                                   QString("("),
                                   QString(")"),
                                   in,
                                   s,
                                   degree,
                                   lbm,
                                   in / minute,
                                   in / s / s,
                                   rev / minute,
                                   ".nc"};
static GcodeMeta MarlinMeta = {GcodeSyntax::kMarlin,
                               QString(";"), // starting_delim
                               QString(),    // ending_delim
                               mm,           // distance
                               ms,           // time
                               degree,       // angle
                               g,            // mass
                               mm / minute,  // velocity
                               mm / s / s,   // acceleration
                               rev / minute, // angular velocity
                               ".gcode"};
static GcodeMeta SiemensMeta = {GcodeSyntax::kSiemens,
                                QString(";"), // starting_delim
                                QString(),    // ending_delim
                                in,           // distance
                                s,            // time
                                degree,       // angle
                                lbm,          // mass
                                in / minute,  // velocity
                                in / s / s,   // acceleration
                                rev / minute, // angular velocity
                                ".mpf"};
static GcodeMeta WolfMeta = {GcodeSyntax::kWolf,
                             QString(";"), // starting_delim
                             QString(),    // ending_delim
                             mm,           // distance
                             s,            // time
                             degree,       // angle
                             g,            // mass
                             mm / minute,  // velocity
                             mm / s / s,   // acceleration
                             rev / minute, // angular velocity
                             ".gcode"};
static GcodeMeta GKNMeta = {GcodeSyntax::kGKN,
                            QString(";"), // starting_delim
                            QString(),    // ending_delim
                            mm,           // distance
                            s,            // time
                            degree,       // angle
                            g,            // mass
                            m / s,        // velocity
                            m / s / s,    // acceleration
                            rev / minute, // angular velocity
                            ".gcode"};
static GcodeMeta HaasInchMeta = {GcodeSyntax::kHaasInch,
                                 QString("("), // starting_delim
                                 QString(")"), // ending_delim
                                 in,           // distance
                                 s,            // time
                                 degree,       // angle
                                 lbm,          // mass
                                 in / minute,  // velocity
                                 in / s / s,   // acceleration
                                 rev / minute, // angular velocity
                                 ".nc"};
static GcodeMeta HaasMetricMeta = {GcodeSyntax::kHaasMetric,
                                   QString("("), // starting_delim
                                   QString(")"), // ending_delim
                                   mm,           // distance
                                   s,            // time
                                   degree,       // angle
                                   g,            // mass
                                   mm / minute,  // velocity
                                   mm / s / s,   // acceleration
                                   rev / minute, // angular velocity
                                   ".nc"};
static GcodeMeta RomiFanucMeta = {GcodeSyntax::kRomiFanuc,
                                  QString("("), // starting_delim
                                  QString(")"), // ending_delim
                                  mm,           // distance
                                  ms,           // time
                                  degree,       // angle
                                  g,            // mass
                                  mm / minute,  // velocity
                                  mm / s / s,   // acceleration
                                  rev / minute, // angular velocity
                                  ".txt"};
static GcodeMeta DmgDmuAndBeamMeta = {GcodeSyntax::kDmgDmu,
                                      QString(";"), // starting_delim
                                      QString(),    // ending_delim
                                      mm,           // distance
                                      s,            // time
                                      degree,       // angle
                                      g,            // mass
                                      mm / minute,  // velocity
                                      mm / s / s,   // acceleration
                                      rev / minute, // angular velocity
                                      ".mpf"};
static GcodeMeta GudelMeta = {GcodeSyntax::kGudel,
                              QString(";"), // starting_delim
                              QString(),    // ending_delim
                              mm,           // distance
                              s,            // time
                              degree,       // angle
                              g,            // mass
                              mm / minute,  // velocity
                              mm / s / s,   // acceleration
                              rev / minute, // angular velocity
                              ".mpf"};
static GcodeMeta IngersollMeta = {GcodeSyntax::kIngersoll,
                                  QString(";"), // starting_delim
                                  QString(),    // ending_delim
                                  mm,           // distance
                                  s,            // time
                                  degree,       // angle
                                  g,            // mass
                                  mm / minute,  // velocity
                                  mm / s / s,   // acceleration
                                  rev / minute, // angular velocity
                                  ".gcode"};
static GcodeMeta MVPMeta = {GcodeSyntax::kMVP,
                            QString(";"), // starting_delim
                            QString(),    // ending_delim
                            mm,           // distance
                            s,            // time
                            degree,       // angle
                            g,            // mass
                            mm / minute,  // velocity
                            mm / s / s,   // acceleration
                            rev / minute, // angular velocity
                            ".mpf"};
static GcodeMeta MazakMeta = {GcodeSyntax::kMazak,
                              QString("("), // starting_delim
                              QString(")"), // ending_delim
                              mm,           // distance
                              s,            // time
                              degree,       // angle
                              g,            // mass
                              mm / minute,  // velocity
                              mm / s / s,   // acceleration
                              rev / minute, // angular velocity
                              ".eia"};
static GcodeMeta MeldMeta = {GcodeSyntax::kMeld,
                             QString("("), // starting_delim
                             QString(")"), // ending_delim
                             in,           // distance
                             ms,           // time
                             degree,       // angle
                             lbm,          // mass
                             in / minute,  // velocity
                             in / s / s,   // acceleration
                             rev / minute, // angular velocity
                             ".nc"};
static GcodeMeta HurcoMeta = {GcodeSyntax::kHurco,
                              QString("("), // starting_delim
                              QString(")"), // ending_delim
                              mm,           // distance
                              ms,           // time
                              degree,       // angle
                              g,            // mass
                              mm / minute,  // velocity
                              mm / s / s,   // acceleration
                              rev / minute, // angular velocity
                              ".nc"};
static GcodeMeta SkyBaamMeta = {GcodeSyntax::kSkyBaam, QString("("), QString(")"), in,   s, degree, lbm,
                                in / minute,           in / s / s,   rev / minute, ".nc"};

static GcodeMeta RPBFMeta = {GcodeSyntax::kRPBF, QString("//"), QString("//"), tensOfMicrons, s, degree, g, mm / s,
                             mm / s / s,         rev / minute,  ".cli",        false};

static GcodeMeta RepRapMeta = {
    GcodeSyntax::kRepRap,
    QString(";"), // starting_delim
    QString(),    // ending_delim
    mm,           // distance
    ms,           // time
    degree,       // angle
    g,            // mass
    mm / minute,  // velocity
    mm / s / s,   // acceleration
    rev / minute, // angular velocity
    ".gcode"      // suffix
};

static GcodeMeta AeroBasicMeta = {GcodeSyntax::kAeroBasic,
                                  QString("'"), // starting_delim
                                  QString(),    // ending_delim
                                  mm,           // distance
                                  s,            // time
                                  degree,       // angle
                                  g,            // mass
                                  mm / s,       // velocity
                                  mm / s / s,   // acceleration
                                  rev / s,      // angular velocity
                                  ".gcode"};

static GcodeMeta ORNLMeta = {GcodeSyntax::kORNL,
                             QString("("), // starting_delim
                             QString(")"), // ending_delim
                             in,
                             s,
                             degree,
                             lbm,
                             in / minute,
                             in / s / s,
                             rev / minute,
                             ".gcode"};
static GcodeMeta ORNLMetricMeta = {GcodeSyntax::kORNLMetric,
                                   QString("("), // starting_delim
                                   QString(")"), // ending_delim
                                   mm,
                                   s,
                                   degree,
                                   g,
                                   mm / minute,
                                   mm / s / s,
                                   rev / minute,
                                   ".gcode"};
static GcodeMeta TormachMeta = {GcodeSyntax::kTormach,
                                QString(";"), // starting_delim
                                QString(""),  // ending_delim
                                mm,
                                s,
                                degree,
                                g,
                                mm / minute,
                                mm / s / s,
                                rev / minute,
                                ".nc"};
static GcodeMeta AML3DMeta = {GcodeSyntax::kAML3D,
                              QString("("), // starting_delim
                              QString(")"), // ending_delim
                              mm,
                              s,
                              degree,
                              g,
                              mm / s,
                              mm / s / s,
                              rev / minute,
                              ".gcode"};
static GcodeMeta KraussMaffeiMeta = {GcodeSyntax::kKraussMaffei,
                                     QString(";"), // starting_delim
                                     QString(),    // ending_delim
                                     mm,           // distance
                                     ms,           // time
                                     degree,       // angle
                                     g,            // mass
                                     mm / minute,  // velocity
                                     mm / s / s,   // acceleration
                                     rev / minute, // angular velocity
                                     ".gcode"};
static GcodeMeta SandiaMeta = {GcodeSyntax::kSandia,
                               QString(";"), // starting_delim
                               QString(),    // ending_delim
                               mm,           // distance
                               ms,           // time
                               degree,       // angle
                               g,            // mass
                               m / s,        // velocity
                               mm / s / s,   // acceleration
                               rev / minute, // angular velocity
                               ".gcode"};
static GcodeMeta MeltioMeta = {GcodeSyntax::kMarlin,
                               QString("("), // starting_delim
                               QString(")"), // ending_delim
                               mm,           // distance
                               ms,           // time
                               degree,       // angle
                               g,            // mass
                               mm / minute,  // velocity
                               mm / s / s,   // acceleration
                               rev / minute, // angular velocity
                               ".nc"};

static GcodeMeta AdamantineMeta = {
    GcodeSyntax::kAdamantine, QString("("), QString(")"), m, s, degree, lbm, m / s, m / s / s, rev / s, ".txt"};
static QHash<int, GcodeMeta> createMapping() {
    QHash<int, GcodeMeta> result;
    result.insert((int)GcodeSyntax::k5AxisMarlin, MarlinMeta);
    result.insert((int)GcodeSyntax::kAML3D, AML3DMeta);
    result.insert((int)GcodeSyntax::kBeam, DmgDmuAndBeamMeta);
    result.insert((int)GcodeSyntax::kCincinnati, CincinnatiMeta);
    result.insert((int)GcodeSyntax::kCommon, MarlinMeta);
    result.insert((int)GcodeSyntax::kDmgDmu, DmgDmuAndBeamMeta);
    result.insert((int)GcodeSyntax::kGKN, GKNMeta);
    result.insert((int)GcodeSyntax::kGudel, GudelMeta);
    result.insert((int)GcodeSyntax::kHaasInch, HaasInchMeta);
    result.insert((int)GcodeSyntax::kHaasMetric, HaasMetricMeta);
    result.insert((int)GcodeSyntax::kHaasMetricNoComments, HaasInchMeta);
    result.insert((int)GcodeSyntax::kHurco, HurcoMeta);
    result.insert((int)GcodeSyntax::kIngersoll, IngersollMeta);
    result.insert((int)GcodeSyntax::kKraussMaffei, KraussMaffeiMeta);
    result.insert((int)GcodeSyntax::kMarlin, MarlinMeta);
    result.insert((int)GcodeSyntax::kMarlinPellet, MarlinMeta);
    result.insert((int)GcodeSyntax::kMazak, MazakMeta);
    result.insert((int)GcodeSyntax::kMeld, MeldMeta);
    result.insert((int)GcodeSyntax::kMeltio, MeltioMeta);
    result.insert((int)GcodeSyntax::kMVP, MVPMeta);
    result.insert((int)GcodeSyntax::kRomiFanuc, RomiFanucMeta);
    result.insert((int)GcodeSyntax::kRPBF, RPBFMeta);
    result.insert((int)GcodeSyntax::kSandia, SandiaMeta);
    result.insert((int)GcodeSyntax::kSiemens, SiemensMeta);
    result.insert((int)GcodeSyntax::kSkyBaam, SkyBaamMeta);
    result.insert((int)GcodeSyntax::kThermwood, CincinnatiMeta);
    result.insert((int)GcodeSyntax::kRepRap, RepRapMeta);
    result.insert((int)GcodeSyntax::kMach4, MarlinMeta);
    result.insert((int)GcodeSyntax::kAeroBasic, AeroBasicMeta);
    result.insert((int)GcodeSyntax::kORNL, ORNLMeta);
    result.insert((int)GcodeSyntax::kTormach, TormachMeta);
    result.insert((int)GcodeSyntax::kAdamantine, AdamantineMeta);
    result.insert((int)GcodeSyntax::kORNLMetric, ORNLMetricMeta);
    return result;
}

static QHash<int, GcodeMeta> SyntaxToMetaHash = createMapping();
} // namespace GcodeMetaList
} // namespace ORNL
