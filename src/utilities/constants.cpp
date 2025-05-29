#include "utilities/constants.h"

#include "units/unit.h"

#include <limits>

namespace ORNL {
//================================================================================
// Units
//================================================================================
const QString Constants::Units::kInch = "in";
const QString Constants::Units::kInchPerSec = "in/sec";
const QString Constants::Units::kInchPerMin = "in/min";
const QString Constants::Units::kInchPerSec2 = "in/sec²";
const QString Constants::Units::kInchPerSec3 = "in/sec³";

const QString Constants::Units::kFeet = "ft";
const QString Constants::Units::kFeetPerSec = "ft/sec";
const QString Constants::Units::kFeetPerSec2 = "ft/sec²";
const QString Constants::Units::kFeetPerSec3 = "ft/sec³";

const QString Constants::Units::kMm = "mm";
const QString Constants::Units::kMmPerSec = "mm/sec";
const QString Constants::Units::kMmPerMin = "mm/min";
const QString Constants::Units::kMmPerSec2 = "mm/sec²";
const QString Constants::Units::kMmPerSec3 = "mm/sec³";

const QString Constants::Units::kCm = "cm";
const QString Constants::Units::kCmPerSec = "cm/sec";
const QString Constants::Units::kCmPerSec2 = "cm/sec²";
const QString Constants::Units::kCmPerSec3 = "cm/sec³";

const QString Constants::Units::kM = "m";
const QString Constants::Units::kMPerSec = "m/sec";
const QString Constants::Units::kMPerSec2 = "m/sec²";
const QString Constants::Units::kMPerSec3 = "m/sec³";

const QString Constants::Units::kMicron = "μm";
const QString Constants::Units::kTensOfMicrons = "μm * 10¹";
const QString Constants::Units::kMicronPerSec = "μm/sec";
const QString Constants::Units::kMicronPerSec2 = "μm/sec²";
const QString Constants::Units::kMicronPerSec3 = "μm/sec³";

const QString Constants::Units::kDegree = "deg";
const QString Constants::Units::kRadian = "rad";
const QString Constants::Units::kRevolution = "rev";

const QString Constants::Units::kSecond = "sec";
const QString Constants::Units::kMillisecond = "ms";
const QString Constants::Units::kMinute = "min";

const QString Constants::Units::kKg = "kg";
const QString Constants::Units::kG = "g";
const QString Constants::Units::kGPerCm3 = "g/cm³";
const QString Constants::Units::kMg = "mg";
const QString Constants::Units::kLb = "lbm";
const QString Constants::Units::kLbPerInch3 = "lbm/in³";

const QString Constants::Units::kCelsius = "°C";
const QString Constants::Units::kFahrenheit = "°F";
const QString Constants::Units::kKelvin = "°K";

const QString Constants::Units::kmuV = "µV";
const QString Constants::Units::kmV = "mV";
const QString Constants::Units::kV = "V";

const QString Constants::Units::kPitchRollYaw = "Pitch/Roll/Yaw";
const QString Constants::Units::kXYZ = "X/Y/Z";

const QStringList Constants::Units::kDistanceUnits = {Constants::Units::kInch, Constants::Units::kFeet,
                                                      Constants::Units::kMm,   Constants::Units::kCm,
                                                      Constants::Units::kM,    Constants::Units::kMicron};

const QStringList Constants::Units::kVelocityUnits = {Constants::Units::kInchPerSec, Constants::Units::kInchPerMin,
                                                      Constants::Units::kFeetPerSec, Constants::Units::kMmPerSec,
                                                      Constants::Units::kMmPerMin,   Constants::Units::kCmPerSec,
                                                      Constants::Units::kMPerSec,    Constants::Units::kMicronPerSec};

const QStringList Constants::Units::kAccelerationUnits = {
    Constants::Units::kInchPerSec2, Constants::Units::kFeetPerSec2, Constants::Units::kMmPerSec2,
    Constants::Units::kCmPerSec2,   Constants::Units::kMPerSec2,    Constants::Units::kMicronPerSec2};

const QStringList Constants::Units::kDensityUnits = {Constants::Units::kLbPerInch3, Constants::Units::kGPerCm3};

const QStringList Constants::Units::kTemperatureUnits = {Constants::Units::kCelsius, Constants::Units::kFahrenheit,
                                                         Constants::Units::kKelvin};

const QStringList Constants::Units::kMassUnits = {Constants::Units::kKg, Constants::Units::kG, Constants::Units::kMg,
                                                  Constants::Units::kLb};

const QStringList Constants::Units::kJerkUnits = {Constants::Units::kInchPerSec3, Constants::Units::kFeetPerSec3,
                                                  Constants::Units::kMmPerSec3, Constants::Units::kCmPerSec3,
                                                  Constants::Units::kMicronPerSec3};

const QStringList Constants::Units::kAngleUnits = {Constants::Units::kDegree, Constants::Units::kRadian,
                                                   Constants::Units::kRevolution};

const QStringList Constants::Units::kTimeUnits = {Constants::Units::kSecond, Constants::Units::kMillisecond,
                                                  Constants::Units::kMinute};

const QStringList Constants::Units::kVoltageUnits = {Constants::Units::kmuV, Constants::Units::kmuV,
                                                     Constants::Units::kmV, Constants::Units::kV};

const QStringList Constants::Units::kRotationUnits = {Constants::Units::kPitchRollYaw, Constants::Units::kXYZ};

//================================================================================
// Region Type Strings
//================================================================================
const QString Constants::RegionTypeStrings::kUnknown = "unknown";
const QString Constants::RegionTypeStrings::kPerimeter = "PERIMETER";
const QString Constants::RegionTypeStrings::kInset = "INSET";
const QString Constants::RegionTypeStrings::kInfill = "INFILL";
const QString Constants::RegionTypeStrings::kTopSkin = "TOP_SKIN";
const QString Constants::RegionTypeStrings::kBottomSkin = "BOTTOM_SKIN";
const QString Constants::RegionTypeStrings::kSkin = "SKIN";
const QString Constants::RegionTypeStrings::kSupport = "SUPPORT";
const QString Constants::RegionTypeStrings::kSupportRoof = "SUPPORT_ROOF";
const QString Constants::RegionTypeStrings::kTravel = "TRAVEL";
const QString Constants::RegionTypeStrings::kRaft = "RAFT";
const QString Constants::RegionTypeStrings::kBrim = "BRIM";
const QString Constants::RegionTypeStrings::kSkirt = "SKIRT";
const QString Constants::RegionTypeStrings::kLaserScan = "LASER SCANNER";
const QString Constants::RegionTypeStrings::kThermalScan = "IR CAMERA";
const QString Constants::RegionTypeStrings::kSkeleton = "SKELETON";
const QString Constants::RegionTypeStrings::kAnchor = "ANCHOR";

//================================================================================
// Legacy Region Type Strings
//================================================================================
const QString Constants::LegacyRegionTypeStrings::kThing = "";

//================================================================================
// Infill Pattern Strings
//================================================================================
const QString Constants::InfillPatternTypeStrings::kLines = "Lines";
const QString Constants::InfillPatternTypeStrings::kGrid = "Grid";
const QString Constants::InfillPatternTypeStrings::kConcentric = "Concentric";
const QString Constants::InfillPatternTypeStrings::kInsideOutConcentric = "Inside Out Concentric";
const QString Constants::InfillPatternTypeStrings::kTriangles = "Triangles";
const QString Constants::InfillPatternTypeStrings::kHexagonsAndTriangles = "Hexagons and Triangles";
const QString Constants::InfillPatternTypeStrings::kHoneycomb = "Honeycomb";
const QString Constants::InfillPatternTypeStrings::kRadialHatch = "Radial Hatch";

const QStringList Constants::InfillPatternTypeStrings::kInfillTypes = {
    Constants::InfillPatternTypeStrings::kLines,      Constants::InfillPatternTypeStrings::kGrid,
    Constants::InfillPatternTypeStrings::kConcentric, Constants::InfillPatternTypeStrings::kInsideOutConcentric,
    Constants::InfillPatternTypeStrings::kTriangles,  Constants::InfillPatternTypeStrings::kHexagonsAndTriangles,
    Constants::InfillPatternTypeStrings::kHoneycomb,  Constants::InfillPatternTypeStrings::kRadialHatch};

//================================================================================
// Machine Syntax Strings
//================================================================================
QString Constants::PrinterSettings::SyntaxString::kAML3D = "AML3D";
QString Constants::PrinterSettings::SyntaxString::k5AxisMarlin = "5AxisMarlin";
QString Constants::PrinterSettings::SyntaxString::kBeam = "Beam";
QString Constants::PrinterSettings::SyntaxString::kCincinnati = "Cincinnati";
QString Constants::PrinterSettings::SyntaxString::kCincinnatiLegacy = "Cincinnati-BERTHA";
QString Constants::PrinterSettings::SyntaxString::kCommon = "Common";
QString Constants::PrinterSettings::SyntaxString::kDmgDmu = "DMG DMU";
QString Constants::PrinterSettings::SyntaxString::kGKN = "GKN";
QString Constants::PrinterSettings::SyntaxString::kGudel = "Gudel";
QString Constants::PrinterSettings::SyntaxString::kHaasInch = "Haas-Inch";
QString Constants::PrinterSettings::SyntaxString::kHaasMetric = "Haas-Metric";
QString Constants::PrinterSettings::SyntaxString::kHaasMetricNoComments = "Haas-Metric-No-Comments";
QString Constants::PrinterSettings::SyntaxString::kHurco = "Hurco";
QString Constants::PrinterSettings::SyntaxString::kIngersoll = "Ingersoll";
QString Constants::PrinterSettings::SyntaxString::kKraussMaffei = "KraussMaffei";
QString Constants::PrinterSettings::SyntaxString::kMarlin = "Marlin";
QString Constants::PrinterSettings::SyntaxString::kMarlinPellet = "Marlin-Pellet";
QString Constants::PrinterSettings::SyntaxString::kMazak = "Mazak";
QString Constants::PrinterSettings::SyntaxString::kMeld = "Meld";
QString Constants::PrinterSettings::SyntaxString::kMeltio = "Meltio";
QString Constants::PrinterSettings::SyntaxString::kMVP = "MVP";
QString Constants::PrinterSettings::SyntaxString::kOkuma = "Okuma";
QString Constants::PrinterSettings::SyntaxString::kORNL = "ORNL";
QString Constants::PrinterSettings::SyntaxString::kRomiFanuc = "ROMI Fanuc";
QString Constants::PrinterSettings::SyntaxString::kRPBF = "RPBF";
QString Constants::PrinterSettings::SyntaxString::kSandia = "Sandia";
QString Constants::PrinterSettings::SyntaxString::kSiemens = "Siemens";
QString Constants::PrinterSettings::SyntaxString::kSkyBaam = "SkyBAAM";
QString Constants::PrinterSettings::SyntaxString::kThermwood = "Thermwood";
QString Constants::PrinterSettings::SyntaxString::kTormach = "Tormach";
QString Constants::PrinterSettings::SyntaxString::kWolf = "Wolf";
QString Constants::PrinterSettings::SyntaxString::kRepRap = "RepRap";
QString Constants::PrinterSettings::SyntaxString::kMach4 = "Mach4";
QString Constants::PrinterSettings::SyntaxString::kAeroBasic = "AeroBasic";
QString Constants::PrinterSettings::SyntaxString::kAdamantine = "Adamantine";

//================================================================================
// Optimizations
//================================================================================
const QString Constants::OrderOptimizationTypeStrings::kShortestTime = "shortest_time";
const QString Constants::OrderOptimizationTypeStrings::kShortestDistance = "shortest_distance";
const QString Constants::OrderOptimizationTypeStrings::kLargestDistance = "largest_distance";
const QString Constants::OrderOptimizationTypeStrings::kLeastRecentlyVisited = "least_recently_visited";
const QString Constants::OrderOptimizationTypeStrings::kNextClosest = "next_closest";
const QString Constants::OrderOptimizationTypeStrings::kApproximateShortest = "approximate_shortest";
const QString Constants::OrderOptimizationTypeStrings::kShortestDistance_DP = "shortest_distance_dp";
const QString Constants::OrderOptimizationTypeStrings::kRandom = "random";
const QString Constants::OrderOptimizationTypeStrings::kConsecutive = "consecutive";

const QStringList Constants::OrderOptimizationTypeStrings::kOrderOptimizationTypes = {
    Constants::OrderOptimizationTypeStrings::kShortestTime,
    Constants::OrderOptimizationTypeStrings::kShortestDistance,
    Constants::OrderOptimizationTypeStrings::kLargestDistance,
    Constants::OrderOptimizationTypeStrings::kLeastRecentlyVisited,
    Constants::OrderOptimizationTypeStrings::kNextClosest,
    Constants::OrderOptimizationTypeStrings::kApproximateShortest,
    Constants::OrderOptimizationTypeStrings::kShortestDistance_DP,
    Constants::OrderOptimizationTypeStrings::kRandom,
    Constants::OrderOptimizationTypeStrings::kConsecutive};

//================================================================================
// Path Modifer Strings
//================================================================================
const QString Constants::PathModifierStrings::kPrestart = "PRESTART";
const QString Constants::PathModifierStrings::kInitialStartup = "INITIAL STARTUP";
const QString Constants::PathModifierStrings::kSlowDown = "SLOW DOWN";
const QString Constants::PathModifierStrings::kForwardTipWipe = "FORWARD TIP WIPE";
const QString Constants::PathModifierStrings::kReverseTipWipe = "REVERSE TIP WIPE";
const QString Constants::PathModifierStrings::kAngledTipWipe = "ANGLED TIP WIPE";
const QString Constants::PathModifierStrings::kCoasting = "COASTING";
const QString Constants::PathModifierStrings::kSpiralLift = "SPIRAL LIFT";
const QString Constants::PathModifierStrings::kRampingUp = "RAMPING UP";
const QString Constants::PathModifierStrings::kRampingDown = "RAMPING DOWN";
const QString Constants::PathModifierStrings::kLeadIn = "LEAD IN";
const QString Constants::PathModifierStrings::kFlyingStart = "FLYING START";
const QString Constants::PathModifierStrings::kPerimeterTipWipe = "PERIMETER TIP WIPE";

//================================================================================
// Printer Settings
//================================================================================

// Machine Setup
const QString Constants::PrinterSettings::MachineSetup::kSyntax = "syntax";
const QString Constants::PrinterSettings::MachineSetup::kMachineType = "machine_type";
const QString Constants::PrinterSettings::MachineSetup::kForceG1 = "force_G1";
const QString Constants::PrinterSettings::MachineSetup::kSupportG3 = "supports_G2_3";
const QString Constants::PrinterSettings::MachineSetup::kAxisA = "axis_a";
const QString Constants::PrinterSettings::MachineSetup::kAxisB = "axis_b";
const QString Constants::PrinterSettings::MachineSetup::kAxisC = "axis_c";
const QString Constants::PrinterSettings::MachineSetup::kToolCoordinate = "tool_coordinate";
const QString Constants::PrinterSettings::MachineSetup::kBaseCoordinate = "base_coordinate";
const QString Constants::PrinterSettings::MachineSetup::kSupportsE1 = "supports_E1";
const QString Constants::PrinterSettings::MachineSetup::kSupportsE2 = "supports_E2";

// Dimensions
const QString Constants::PrinterSettings::Dimensions::kBuildVolumeType = "build_volume_type";
const QString Constants::PrinterSettings::Dimensions::kXMin = "minimum_x";
const QString Constants::PrinterSettings::Dimensions::kXMax = "maximum_x";
const QString Constants::PrinterSettings::Dimensions::kYMin = "minimum_y";
const QString Constants::PrinterSettings::Dimensions::kYMax = "maximum_y";
const QString Constants::PrinterSettings::Dimensions::kZMin = "minimum_z";
const QString Constants::PrinterSettings::Dimensions::kZMax = "maximum_z";
const QString Constants::PrinterSettings::Dimensions::kUseVariableForZ = "variable_for_z";
const QString Constants::PrinterSettings::Dimensions::kInnerRadius = "inner_radius";
const QString Constants::PrinterSettings::Dimensions::kOuterRadius = "outer_radius";
const QString Constants::PrinterSettings::Dimensions::kXOffset = "x_offset";
const QString Constants::PrinterSettings::Dimensions::kYOffset = "y_offset";
const QString Constants::PrinterSettings::Dimensions::kZOffset = "z_offset";
const QString Constants::PrinterSettings::Dimensions::kEnableW = "enable_w_axis";
const QString Constants::PrinterSettings::Dimensions::kWMin = "minimum_w";
const QString Constants::PrinterSettings::Dimensions::kWMax = "maximum_w";
const QString Constants::PrinterSettings::Dimensions::kInitialW = "initial_w";
const QString Constants::PrinterSettings::Dimensions::kLayerChangeAxis = "layer_change";
const QString Constants::PrinterSettings::Dimensions::kEnableDoffing = "doffing";
const QString Constants::PrinterSettings::Dimensions::kDoffingHeight = "doffing_location";
const QString Constants::PrinterSettings::Dimensions::kPurgeX = "purge_x";
const QString Constants::PrinterSettings::Dimensions::kPurgeY = "purge_y";
const QString Constants::PrinterSettings::Dimensions::kPurgeZ = "purge_z";
const QString Constants::PrinterSettings::Dimensions::kEnableGridX = "enable_grid_x";
const QString Constants::PrinterSettings::Dimensions::kGridXDistance = "grid_x_distance";
const QString Constants::PrinterSettings::Dimensions::kGridXOffset = "grid_x_offset";
const QString Constants::PrinterSettings::Dimensions::kEnableGridY = "enable_grid_y";
const QString Constants::PrinterSettings::Dimensions::kGridYDistance = "grid_y_distance";
const QString Constants::PrinterSettings::Dimensions::kGridYOffset = "grid_y_offset";

// Auxiliary
const QString Constants::PrinterSettings::Auxiliary::kEnableTamper = "enable_tamper";
const QString Constants::PrinterSettings::Auxiliary::kTamperVoltage = "tamper_voltage";
const QString Constants::PrinterSettings::Auxiliary::kGKNLaserPower = "gkn_laser_power";
const QString Constants::PrinterSettings::Auxiliary::kGKNWireSpeed = "gkn_wire_speed";

// Machine Speed
const QString Constants::PrinterSettings::MachineSpeed::kMinXYSpeed = "min_xy_speed";
const QString Constants::PrinterSettings::MachineSpeed::kMaxXYSpeed = "max_xy_speed";
const QString Constants::PrinterSettings::MachineSpeed::kMaxExtruderSpeed = "max_extruder_speed";
const QString Constants::PrinterSettings::MachineSpeed::kWTableSpeed = "w_table_speed";
const QString Constants::PrinterSettings::MachineSpeed::kZSpeed = "z_speed";
const QString Constants::PrinterSettings::MachineSpeed::kGKNPrintSpeed = "gkn_print_speed";
const QString Constants::PrinterSettings::MachineSpeed::kGearRatio = "extruder_gear_ratio";

// Acceleration
const QString Constants::PrinterSettings::Acceleration::kEnableDynamic = "enable_dynamic_acceleration";
const QString Constants::PrinterSettings::Acceleration::kDefault = "default_acceleration";
const QString Constants::PrinterSettings::Acceleration::kPerimeter = "perimeter_acceleration";
const QString Constants::PrinterSettings::Acceleration::kInset = "inset_acceleration";
const QString Constants::PrinterSettings::Acceleration::kSkin = "skin_acceleration";
const QString Constants::PrinterSettings::Acceleration::kInfill = "infill_acceleration";
const QString Constants::PrinterSettings::Acceleration::kSkeleton = "skeleton_acceleration";
const QString Constants::PrinterSettings::Acceleration::kSupport = "support_acceleration";

// G-Code
const QString Constants::PrinterSettings::GCode::kEnableStartupCode = "enable_default_startup_code";
const QString Constants::PrinterSettings::GCode::kEnableMaterialLoad = "enable_material_load";
const QString Constants::PrinterSettings::GCode::kEnableWaitForUser = "enable_wait_for_user";
const QString Constants::PrinterSettings::GCode::kEnableBoundingBox = "enable_bounding_box";
const QString Constants::PrinterSettings::GCode::kStartCode = "start_code";
const QString Constants::PrinterSettings::GCode::kLayerCodeChange = "layer_change_code";
const QString Constants::PrinterSettings::GCode::kEndCode = "end_code";

//================================================================================
// Material Settings
//================================================================================

// Material Categories
const QString Constants::MaterialSettings::Density::kMaterialType = "printing_material";
const QString Constants::MaterialSettings::Density::kDensity = "other_density";

// Startup
const QString Constants::MaterialSettings::Startup::kPerimeterEnable = "perimeter_start-up";
const QString Constants::MaterialSettings::Startup::kPerimeterDistance = "perimeter_start-up_distance";
const QString Constants::MaterialSettings::Startup::kPerimeterSpeed = "perimeter_start-up_speed";
const QString Constants::MaterialSettings::Startup::kPerimeterExtruderSpeed = "perimeter_start-up_extruder_speed";
const QString Constants::MaterialSettings::Startup::kPerimeterRampUpEnable = "perimeter_start-up_ramp-up";
const QString Constants::MaterialSettings::Startup::kPerimeterSteps = "perimeter_start-up_steps";
const QString Constants::MaterialSettings::Startup::kInsetEnable = "inset_start-up";
const QString Constants::MaterialSettings::Startup::kInsetDistance = "inset_start-up_distance";
const QString Constants::MaterialSettings::Startup::kInsetSpeed = "inset_start-up_speed";
const QString Constants::MaterialSettings::Startup::kInsetExtruderSpeed = "inset_start-up_extruder_speed";
const QString Constants::MaterialSettings::Startup::kInsetRampUpEnable = "inset_start-up_ramp-up";
const QString Constants::MaterialSettings::Startup::kInsetSteps = "inset_start-up_steps";
const QString Constants::MaterialSettings::Startup::kSkinEnable = "skin_start-up";
const QString Constants::MaterialSettings::Startup::kSkinDistance = "skin_start-up_distance";
const QString Constants::MaterialSettings::Startup::kSkinSpeed = "skin_start-up_speed";
const QString Constants::MaterialSettings::Startup::kSkinExtruderSpeed = "skin_start-up_extruder_speed";
const QString Constants::MaterialSettings::Startup::kSkinRampUpEnable = "skin_start-up_ramp-up";
const QString Constants::MaterialSettings::Startup::kSkinSteps = "skin_start-up_steps";
const QString Constants::MaterialSettings::Startup::kInfillEnable = "infill_start-up";
const QString Constants::MaterialSettings::Startup::kInfillDistance = "infill_start-up_distance";
const QString Constants::MaterialSettings::Startup::kInfillSpeed = "infill_start-up_speed";
const QString Constants::MaterialSettings::Startup::kInfillExtruderSpeed = "infill_start-up_extruder_speed";
const QString Constants::MaterialSettings::Startup::kInfillRampUpEnable = "infill_start-up_ramp-up";
const QString Constants::MaterialSettings::Startup::kInfillSteps = "infill_start-up_steps";
const QString Constants::MaterialSettings::Startup::kSkeletonEnable = "skeleton_start-up";
const QString Constants::MaterialSettings::Startup::kSkeletonDistance = "skeleton_start-up_distance";
const QString Constants::MaterialSettings::Startup::kSkeletonSpeed = "skeleton_start-up_speed";
const QString Constants::MaterialSettings::Startup::kSkeletonExtruderSpeed = "skeleton_start-up_extruder_speed";
const QString Constants::MaterialSettings::Startup::kSkeletonRampUpEnable = "skeleton_start-up_ramp-up";
const QString Constants::MaterialSettings::Startup::kSkeletonSteps = "skeleton_start-up_steps";
const QString Constants::MaterialSettings::Startup::kStartUpAreaModifier = "start-up_area_modifier";

// Slowdown
const QString Constants::MaterialSettings::Slowdown::kPerimeterEnable = "perimeter_slow_down";
const QString Constants::MaterialSettings::Slowdown::kPerimeterDistance = "perimeter_slow_down_distance";
const QString Constants::MaterialSettings::Slowdown::kPerimeterLiftDistance = "perimeter_slow_down_lift_distance";
const QString Constants::MaterialSettings::Slowdown::kPerimeterSpeed = "perimeter_slow_down_speed";
const QString Constants::MaterialSettings::Slowdown::kPerimeterExtruderSpeed = "perimeter_slow_down_extruder_speed";
const QString Constants::MaterialSettings::Slowdown::kPerimeterCutoffDistance =
    "perimeter_slow_down_extruder_off_distance";
const QString Constants::MaterialSettings::Slowdown::kInsetEnable = "inset_slow_down";
const QString Constants::MaterialSettings::Slowdown::kInsetDistance = "inset_slow_down_distance";
const QString Constants::MaterialSettings::Slowdown::kInsetLiftDistance = "inset_slow_down_lift_distance";
const QString Constants::MaterialSettings::Slowdown::kInsetSpeed = "inset_slow_down_speed";
const QString Constants::MaterialSettings::Slowdown::kInsetExtruderSpeed = "inset_slow_down_extruder_speed";
const QString Constants::MaterialSettings::Slowdown::kInsetCutoffDistance = "inset_slow_down_extruder_off_distance";
const QString Constants::MaterialSettings::Slowdown::kSkinEnable = "skin_slow_down";
const QString Constants::MaterialSettings::Slowdown::kSkinDistance = "skin_slow_down_distance";
const QString Constants::MaterialSettings::Slowdown::kSkinLiftDistance = "skin_slow_down_lift_distance";
const QString Constants::MaterialSettings::Slowdown::kSkinSpeed = "skin_slow_down_speed";
const QString Constants::MaterialSettings::Slowdown::kSkinExtruderSpeed = "skin_slow_down_extruder_speed";
const QString Constants::MaterialSettings::Slowdown::kSkinCutoffDistance = "skin_slow_down_extruder_off_distance";
const QString Constants::MaterialSettings::Slowdown::kInfillEnable = "infill_slow_down";
const QString Constants::MaterialSettings::Slowdown::kInfillDistance = "infill_slow_down_distance";
const QString Constants::MaterialSettings::Slowdown::kInfillLiftDistance = "infill_slow_down_lift_distance";
const QString Constants::MaterialSettings::Slowdown::kInfillSpeed = "infill_slow_down_speed";
const QString Constants::MaterialSettings::Slowdown::kInfillExtruderSpeed = "infill_slow_down_extruder_speed";
const QString Constants::MaterialSettings::Slowdown::kInfillCutoffDistance = "infill_slow_down_extruder_off_distance";
const QString Constants::MaterialSettings::Slowdown::kSkeletonEnable = "skeleton_slow_down";
const QString Constants::MaterialSettings::Slowdown::kSkeletonDistance = "skeleton_slow_down_distance";
const QString Constants::MaterialSettings::Slowdown::kSkeletonLiftDistance = "skeleton_slow_down_lift_distance";
const QString Constants::MaterialSettings::Slowdown::kSkeletonSpeed = "skeleton_slow_down_speed";
const QString Constants::MaterialSettings::Slowdown::kSkeletonExtruderSpeed = "skeleton_slow_down_extruder_speed";
const QString Constants::MaterialSettings::Slowdown::kSkeletonCutoffDistance =
    "skeleton_slow_down_extruder_off_distance";
const QString Constants::MaterialSettings::Slowdown::kSlowDownAreaModifier = "slow_down_area_modifier";

// TipWipe
const QString Constants::MaterialSettings::TipWipe::kPerimeterEnable = "perimeter_wipe";
const QString Constants::MaterialSettings::TipWipe::kPerimeterDistance = "perimeter_wipe_distance";
const QString Constants::MaterialSettings::TipWipe::kPerimeterSpeed = "perimeter_wipe_speed";
const QString Constants::MaterialSettings::TipWipe::kPerimeterExtruderSpeed = "perimeter_wipe_extruder_speed";
const QString Constants::MaterialSettings::TipWipe::kPerimeterDirection = "perimeter_wipe_direction";
const QString Constants::MaterialSettings::TipWipe::kPerimeterAngle = "perimeter_wipe_angle";
const QString Constants::MaterialSettings::TipWipe::kPerimeterCutoffDistance = "perimeter_wipe_cutoff_distance";
const QString Constants::MaterialSettings::TipWipe::kPerimeterLiftHeight = "perimeter_wipe_lift_height";
const QString Constants::MaterialSettings::TipWipe::kInsetEnable = "inset_wipe";
const QString Constants::MaterialSettings::TipWipe::kInsetDistance = "inset_wipe_distance";
const QString Constants::MaterialSettings::TipWipe::kInsetSpeed = "inset_wipe_speed";
const QString Constants::MaterialSettings::TipWipe::kInsetExtruderSpeed = "inset_wipe_extruder_speed";
const QString Constants::MaterialSettings::TipWipe::kInsetDirection = "inset_wipe_direction";
const QString Constants::MaterialSettings::TipWipe::kInsetAngle = "inset_wipe_angle";
const QString Constants::MaterialSettings::TipWipe::kInsetCutoffDistance = "inset_wipe_cutoff_distance";
const QString Constants::MaterialSettings::TipWipe::kInsetLiftHeight = "inset_wipe_lift_height";
const QString Constants::MaterialSettings::TipWipe::kSkinEnable = "skin_wipe";
const QString Constants::MaterialSettings::TipWipe::kSkinDistance = "skin_wipe_distance";
const QString Constants::MaterialSettings::TipWipe::kSkinSpeed = "skin_wipe_speed";
const QString Constants::MaterialSettings::TipWipe::kSkinExtruderSpeed = "skin_wipe_extruder_speed";
const QString Constants::MaterialSettings::TipWipe::kSkinDirection = "skin_wipe_direction";
const QString Constants::MaterialSettings::TipWipe::kSkinAngle = "skin_wipe_angle";
const QString Constants::MaterialSettings::TipWipe::kSkinCutoffDistance = "skin_wipe_cutoff_distance";
const QString Constants::MaterialSettings::TipWipe::kSkinLiftHeight = "skin_wipe_lift_height";
const QString Constants::MaterialSettings::TipWipe::kInfillEnable = "infill_wipe";
const QString Constants::MaterialSettings::TipWipe::kInfillDistance = "infill_wipe_distance";
const QString Constants::MaterialSettings::TipWipe::kInfillSpeed = "infill_wipe_speed";
const QString Constants::MaterialSettings::TipWipe::kInfillExtruderSpeed = "infill_wipe_extruder_speed";
const QString Constants::MaterialSettings::TipWipe::kInfillDirection = "infill_wipe_direction";
const QString Constants::MaterialSettings::TipWipe::kInfillAngle = "infill_wipe_angle";
const QString Constants::MaterialSettings::TipWipe::kInfillCutoffDistance = "infill_wipe_cutoff_distance";
const QString Constants::MaterialSettings::TipWipe::kInfillLiftHeight = "infill_wipe_lift_height";
const QString Constants::MaterialSettings::TipWipe::kSkeletonEnable = "skeleton_wipe";
const QString Constants::MaterialSettings::TipWipe::kSkeletonDistance = "skeleton_wipe_distance";
const QString Constants::MaterialSettings::TipWipe::kSkeletonSpeed = "skeleton_wipe_speed";
const QString Constants::MaterialSettings::TipWipe::kSkeletonExtruderSpeed = "skeleton_wipe_extruder_speed";
const QString Constants::MaterialSettings::TipWipe::kSkeletonDirection = "skeleton_wipe_direction";
const QString Constants::MaterialSettings::TipWipe::kSkeletonAngle = "skeleton_wipe_angle";
const QString Constants::MaterialSettings::TipWipe::kSkeletonCutoffDistance = "skeleton_wipe_cutoff_distance";
const QString Constants::MaterialSettings::TipWipe::kSkeletonLiftHeight = "skeleton_wipe_lift_height";
const QString Constants::MaterialSettings::TipWipe::kLaserPowerMultiplier = "laser_power_multiplier";
const QString Constants::MaterialSettings::TipWipe::kWireFeedMultiplier = "wire_feed_multiplier";
const QString Constants::MaterialSettings::TipWipe::kTipWipeVoltage = "tip_wipe_voltage";

// Spiral Lift
const QString Constants::MaterialSettings::SpiralLift::kPerimeterEnable = "enable_spiral_perimeter";
const QString Constants::MaterialSettings::SpiralLift::kInsetEnable = "enable_spiral_inset";
const QString Constants::MaterialSettings::SpiralLift::kSkinEnable = "enable_spiral_skin";
const QString Constants::MaterialSettings::SpiralLift::kInfillEnable = "enable_spiral_infill";
const QString Constants::MaterialSettings::SpiralLift::kLayerEnable = "spiral_end_of_layer";
const QString Constants::MaterialSettings::SpiralLift::kLiftHeight = "spiral_lift_height";
const QString Constants::MaterialSettings::SpiralLift::kLiftRadius = "spiral_lift_radius";
const QString Constants::MaterialSettings::SpiralLift::kLiftSpeed = "spiral_lift_speed";
const QString Constants::MaterialSettings::SpiralLift::kLiftPoints = "spiral_lift_points";

// Purge
const QString Constants::MaterialSettings::Purge::kInitialDuration = "initial_purge_duration";
const QString Constants::MaterialSettings::Purge::kInitialScrewRPM = "initial_purge_dwell_screw_rpm";
const QString Constants::MaterialSettings::Purge::kInitialTipWipeDelay = "initial_purge_tip_wipe_delay";
const QString Constants::MaterialSettings::Purge::kEnablePurgeDwell = "purge_during_dwell";
const QString Constants::MaterialSettings::Purge::kPurgeDwellDuration = "purge_dwell_duration";
const QString Constants::MaterialSettings::Purge::kPurgeDwellRPM = "purge_dwell_screw_rpm";
const QString Constants::MaterialSettings::Purge::kPurgeDwellTipWipeDelay = "purge_tip_wipe_delay";
const QString Constants::MaterialSettings::Purge::kPurgeLength = "purge_length";
const QString Constants::MaterialSettings::Purge::kPurgeFeedrate = "purge_feedrate";

// Extruder
const QString Constants::MaterialSettings::Extruder::kInitialSpeed = "initial_extruder_speed";
const QString Constants::MaterialSettings::Extruder::kExtruderPrimeVolume = "extruder_prime_volume";
const QString Constants::MaterialSettings::Extruder::kExtruderPrimeSpeed = "extruder_prime_speed";
const QString Constants::MaterialSettings::Extruder::kOnDelayPerimeter = "extruder_on_delay_perimeter";
const QString Constants::MaterialSettings::Extruder::kOnDelayInset = "extruder_on_delay_inset";
const QString Constants::MaterialSettings::Extruder::kOnDelaySkin = "extruder_on_delay_skin";
const QString Constants::MaterialSettings::Extruder::kOnDelayInfill = "extruder_on_delay_infill";
const QString Constants::MaterialSettings::Extruder::kOnDelaySkeleton = "extruder_on_delay_skeleton";
const QString Constants::MaterialSettings::Extruder::kOffDelay = "extruder_off_delay";
const QString Constants::MaterialSettings::Extruder::kServoToTravelSpeed = "servo_extruder_to_travel_speed";
const QString Constants::MaterialSettings::Extruder::kEnableM3S = "enable_m3s";

// Filament
const QString Constants::MaterialSettings::Filament::kDiameter = "filament_diameter";
const QString Constants::MaterialSettings::Filament::kRelative = "filament_relative_distance";
const QString Constants::MaterialSettings::Filament::kDisableG92 = "disable_g92";
const QString Constants::MaterialSettings::Filament::kFilamentBAxis = "filament_b_axis";

// Retraction
const QString Constants::MaterialSettings::Retraction::kEnable = "retraction";
const QString Constants::MaterialSettings::Retraction::kMinTravel = "retract_min_travel_length";
const QString Constants::MaterialSettings::Retraction::kLength = "retraction_length";
const QString Constants::MaterialSettings::Retraction::kSpeed = "retraction_speed";
const QString Constants::MaterialSettings::Retraction::kOpenSpacesOnly = "retraction_open_spaces_only";
const QString Constants::MaterialSettings::Retraction::kLayerChange = "retraction_layer_change";
const QString Constants::MaterialSettings::Retraction::kPrimeSpeed = "filament_prime_speed";
const QString Constants::MaterialSettings::Retraction::kPrimeAdditionalLength = "filament_prime_length";

// Temperatures
const QString Constants::MaterialSettings::Temperatures::kBed = "bed_temperature";
const QString Constants::MaterialSettings::Temperatures::kTwoZones = "two_zone_extruders";
const QString Constants::MaterialSettings::Temperatures::kThreeZones = "three_zone_extruders";
const QString Constants::MaterialSettings::Temperatures::kFourZones = "four_zone_extruders";
const QString Constants::MaterialSettings::Temperatures::kFiveZones = "five_zone_extruders";
const QString Constants::MaterialSettings::Temperatures::kExtruder0 = "extruder0_temperature";
const QString Constants::MaterialSettings::Temperatures::kExtruder1 = "extruder1_temperature";
const QString Constants::MaterialSettings::Temperatures::kStandBy0 = "standby0_temperature";
const QString Constants::MaterialSettings::Temperatures::kStandBy1 = "standby1_temperature";
const QString Constants::MaterialSettings::Temperatures::kExtruder0Zone1 = "extruder0_zone1";
const QString Constants::MaterialSettings::Temperatures::kExtruder0Zone2 = "extruder0_zone2";
const QString Constants::MaterialSettings::Temperatures::kExtruder0Zone3 = "extruder0_zone3";
const QString Constants::MaterialSettings::Temperatures::kExtruder0Zone4 = "extruder0_zone4";
const QString Constants::MaterialSettings::Temperatures::kExtruder0Zone5 = "extruder0_zone5";
const QString Constants::MaterialSettings::Temperatures::kExtruder1Zone1 = "extruder1_zone1";
const QString Constants::MaterialSettings::Temperatures::kExtruder1Zone2 = "extruder1_zone2";
const QString Constants::MaterialSettings::Temperatures::kExtruder1Zone3 = "extruder1_zone3";
const QString Constants::MaterialSettings::Temperatures::kExtruder1Zone4 = "extruder1_zone4";
const QString Constants::MaterialSettings::Temperatures::kExtruder1Zone5 = "extruder1_zone5";

// Cooling
const QString Constants::MaterialSettings::Cooling::kEnable = "fan";
const QString Constants::MaterialSettings::Cooling::kMinSpeed = "fan_min_speed";
const QString Constants::MaterialSettings::Cooling::kMaxSpeed = "fan_max_speed";
const QString Constants::MaterialSettings::Cooling::kForceMinLayerTime = "force_minimum_layer_time";
const QString Constants::MaterialSettings::Cooling::kForceMinLayerTimeMethod = "minimum_layer_time_method";
const QString Constants::MaterialSettings::Cooling::kMinLayerTime = "minimum_layer_time";
const QString Constants::MaterialSettings::Cooling::kMaxLayerTime = "maximum_layer_time";
const QString Constants::MaterialSettings::Cooling::kExtruderScaleFactor = "extruder_scale_factor";
const QString Constants::MaterialSettings::Cooling::kPrePauseCode = "pre_pause_code";
const QString Constants::MaterialSettings::Cooling::kPostPauseCode = "post_pause_code";

// Platform Adhesion
const QString Constants::MaterialSettings::PlatformAdhesion::kRaftEnable = "raft";
const QString Constants::MaterialSettings::PlatformAdhesion::kRaftOffset = "raft_offset";
const QString Constants::MaterialSettings::PlatformAdhesion::kRaftLayers = "raft_layer_count";
const QString Constants::MaterialSettings::PlatformAdhesion::kRaftBeadWidth = "raft_bead_width";
const QString Constants::MaterialSettings::PlatformAdhesion::kBrimEnable = "brim";
const QString Constants::MaterialSettings::PlatformAdhesion::kBrimWidth = "brim_width";
const QString Constants::MaterialSettings::PlatformAdhesion::kBrimLayers = "brim_layer_count";
const QString Constants::MaterialSettings::PlatformAdhesion::kBrimBeadWidth = "brim_bead_width";
const QString Constants::MaterialSettings::PlatformAdhesion::kSkirtEnable = "skirt";
const QString Constants::MaterialSettings::PlatformAdhesion::kSkirtLoops = "skirt_loops";
const QString Constants::MaterialSettings::PlatformAdhesion::kSkirtDistanceFromObject = "skirt_distance_from_object";
const QString Constants::MaterialSettings::PlatformAdhesion::kSkirtLayers = "skirt_layer_count";
const QString Constants::MaterialSettings::PlatformAdhesion::kSkirtMinLength = "skirt_minimum_length";
const QString Constants::MaterialSettings::PlatformAdhesion::kSkirtBeadWidth = "skirt_bead_width";

// MultiMaterial
const QString Constants::MaterialSettings::MultiMaterial::kEnable = "enable_multi_material";
const QString Constants::MaterialSettings::MultiMaterial::kPerimterNum = "perimeter_material_num";
const QString Constants::MaterialSettings::MultiMaterial::kInsetNum = "inset_material_num";
const QString Constants::MaterialSettings::MultiMaterial::kSkinNum = "skin_material_num";
const QString Constants::MaterialSettings::MultiMaterial::kInfillNum = "infill_material_num";
const QString Constants::MaterialSettings::MultiMaterial::kTransitionDistance = "material_transition_distance";
const QString Constants::MaterialSettings::MultiMaterial::kEnableSecondDistance = "enable_second_transition_distance";
const QString Constants::MaterialSettings::MultiMaterial::kSecondDistance = "second_transition_distance";
const QString Constants::MaterialSettings::MultiMaterial::kUseM222 = "enable_m222";

//================================================================================
// Profile Settings
//================================================================================

// Layer
const QString Constants::ProfileSettings::Layer::kLayerHeight = "layer_height";
const QString Constants::ProfileSettings::Layer::kNozzleDiameter = "nozzle_diameter";
const QString Constants::ProfileSettings::Layer::kBeadWidth = "default_width";
const QString Constants::ProfileSettings::Layer::kSpeed = "default_speed";
const QString Constants::ProfileSettings::Layer::kExtruderSpeed = "default_extruder_speed";
const QString Constants::ProfileSettings::Layer::kMinExtrudeLength = "minimum_extrude_length";

// Perimeter
const QString Constants::ProfileSettings::Perimeter::kEnable = "perimeter";
const QString Constants::ProfileSettings::Perimeter::kCount = "perimeter_count";
const QString Constants::ProfileSettings::Perimeter::kBeadWidth = "perimeter_width";
const QString Constants::ProfileSettings::Perimeter::kSpeed = "perimeter_speed";
const QString Constants::ProfileSettings::Perimeter::kExtruderSpeed = "perimeter_extruder_speed";
const QString Constants::ProfileSettings::Perimeter::kExtrusionMultiplier = "perimeter_extrusion_multiplier";
const QString Constants::ProfileSettings::Perimeter::kMinPathLength = "perimeter_minimum_path_length";
const QString Constants::ProfileSettings::Perimeter::kPower = "perimeter_power";
const QString Constants::ProfileSettings::Perimeter::kFocus = "perimeter_focus";
const QString Constants::ProfileSettings::Perimeter::kSpotSize = "perimeter_spot_size";
const QString Constants::ProfileSettings::Perimeter::kEnableLeadIn = "perimeter_lead_in";
const QString Constants::ProfileSettings::Perimeter::kLeadInFirstLayerOnly = "perimeter_lead_in_first_layer";
const QString Constants::ProfileSettings::Perimeter::kEnableLeadInX = "perimeter_lead_in_x";
const QString Constants::ProfileSettings::Perimeter::kEnableLeadInY = "perimeter_lead_in_y";
const QString Constants::ProfileSettings::Perimeter::kEnableFlyingStart = "perimeter_flying_start";
const QString Constants::ProfileSettings::Perimeter::kFlyingStartDistance = "perimeter_flying_start_distance";
const QString Constants::ProfileSettings::Perimeter::kFlyingStartSpeed = "perimeter_flying_start_speed";
const QString Constants::ProfileSettings::Perimeter::kEnableShiftedBeads = "perimeter_shifted_beads";

// Inset
const QString Constants::ProfileSettings::Inset::kEnable = "inset";
const QString Constants::ProfileSettings::Inset::kCount = "inset_count";
const QString Constants::ProfileSettings::Inset::kBeadWidth = "inset_width";
const QString Constants::ProfileSettings::Inset::kSpeed = "inset_speed";
const QString Constants::ProfileSettings::Inset::kExtruderSpeed = "inset_extruder_speed";
const QString Constants::ProfileSettings::Inset::kExtrusionMultiplier = "inset_extrusion_multiplier";
const QString Constants::ProfileSettings::Inset::kMinPathLength = "inset_minimum_path_length";
const QString Constants::ProfileSettings::Inset::kOverlap = "inset_overlap_distance";

// Skeleton
const QString Constants::ProfileSettings::Skeleton::kEnable = "skeleton";
const QString Constants::ProfileSettings::Skeleton::kSkeletonInput = "skeleton_input";
const QString Constants::ProfileSettings::Skeleton::kSkeletonInputCleaningDistance = "skeleton_input_cleaning_distance";
const QString Constants::ProfileSettings::Skeleton::kSkeletonInputChamferingAngle = "skeleton_input_chamfering_angle";
const QString Constants::ProfileSettings::Skeleton::kSkeletonOutputCleaningDistance =
    "skeleton_output_cleaning_distance";
const QString Constants::ProfileSettings::Skeleton::kBeadWidth = "skeleton_width";
const QString Constants::ProfileSettings::Skeleton::kSpeed = "skeleton_speed";
const QString Constants::ProfileSettings::Skeleton::kExtruderSpeed = "skeleton_extruder_speed";
const QString Constants::ProfileSettings::Skeleton::kExtrusionMultiplier = "skeleton_extrusion_multiplier";
const QString Constants::ProfileSettings::Skeleton::kSkeletonAdapt = "skeleton_adapt";
const QString Constants::ProfileSettings::Skeleton::kSkeletonAdaptStepSize = "skeleton_adapt_step_size";
const QString Constants::ProfileSettings::Skeleton::kSkeletonAdaptMinWidth = "skeleton_adapt_min_width";
const QString Constants::ProfileSettings::Skeleton::kSkeletonAdaptMinWidthFilter = "skeleton_adapt_min_width_filter";
const QString Constants::ProfileSettings::Skeleton::kSkeletonAdaptMaxWidth = "skeleton_adapt_max_width";
const QString Constants::ProfileSettings::Skeleton::kSkeletonAdaptMaxWidthFilter = "skeleton_adapt_max_width_filter";
const QString Constants::ProfileSettings::Skeleton::kMinPathLength = "skeleton_minimum_path_length";
const QString Constants::ProfileSettings::Skeleton::kUseSkinMcode = "skeleton_skin_mcode";

// Skin
const QString Constants::ProfileSettings::Skin::kEnable = "skin";
const QString Constants::ProfileSettings::Skin::kTopCount = "skin_top_count";
const QString Constants::ProfileSettings::Skin::kBottomCount = "skin_bottom_count";
const QString Constants::ProfileSettings::Skin::kPattern = "skin_pattern";
const QString Constants::ProfileSettings::Skin::kAngle = "skin_angle";
const QString Constants::ProfileSettings::Skin::kAngleRotation = "skin_angle_rotation";
const QString Constants::ProfileSettings::Skin::kBeadWidth = "skin_width";
const QString Constants::ProfileSettings::Skin::kSpeed = "skin_speed";
const QString Constants::ProfileSettings::Skin::kExtruderSpeed = "skin_extruder_speed";
const QString Constants::ProfileSettings::Skin::kExtrusionMultiplier = "skin_extrusion_multiplier";
const QString Constants::ProfileSettings::Skin::kOverlap = "skin_exterior_overlap";
const QString Constants::ProfileSettings::Skin::kMinPathLength = "skin_minimum_path_length";
const QString Constants::ProfileSettings::Skin::kPrestart = "skin_prestart";
const QString Constants::ProfileSettings::Skin::kPrestartDistance = "skin_prestart_distance";
const QString Constants::ProfileSettings::Skin::kPrestartSpeed = "skin_prestart_speed";
const QString Constants::ProfileSettings::Skin::kPrestartExtruderSpeed = "skin_prestart_extruder_speed";
const QString Constants::ProfileSettings::Skin::kInfillEnable = "skin_gradual_infill";
const QString Constants::ProfileSettings::Skin::kInfillSteps = "skin_gradual_infill_steps";
const QString Constants::ProfileSettings::Skin::kInfillPattern = "skin_gradual_infill_pattern";
const QString Constants::ProfileSettings::Skin::kInfillAngle = "skin_gradual_infill_angle";
const QString Constants::ProfileSettings::Skin::kInfillRotation = "skin_gradual_infill_angle_rotation";

// Infill
const QString Constants::ProfileSettings::Infill::kEnable = "infill";
const QString Constants::ProfileSettings::Infill::kLineSpacing = "infill_line_spacing";
const QString Constants::ProfileSettings::Infill::kDensity = "infill_density";
const QString Constants::ProfileSettings::Infill::kManualLineSpacing = "infill_manual_spacing";
const QString Constants::ProfileSettings::Infill::kPattern = "infill_pattern";
const QString Constants::ProfileSettings::Infill::kPrintInfillEveryXLayers = "infill_every_x_layers";
const QString Constants::ProfileSettings::Infill::kBasedOnPrinter = "infill_based_on_printer";
const QString Constants::ProfileSettings::Infill::kAngle = "infill_angle";
const QString Constants::ProfileSettings::Infill::kAngleRotation = "infill_angle_rotation";
const QString Constants::ProfileSettings::Infill::kOverlap = "infill_overlap_distance";
const QString Constants::ProfileSettings::Infill::kBeadWidth = "infill_width";
const QString Constants::ProfileSettings::Infill::kSpeed = "infill_speed";
const QString Constants::ProfileSettings::Infill::kExtruderSpeed = "infill_extruder_speed";
const QString Constants::ProfileSettings::Infill::kExtrusionMultiplier = "infill_extrusion_multiplier";
const QString Constants::ProfileSettings::Infill::kCombineXLayers = "infill_combine_every_x_layers";
const QString Constants::ProfileSettings::Infill::kMinPathLength = "infill_minimum_path_length";
const QString Constants::ProfileSettings::Infill::kPrestart = "infill_prestart";
const QString Constants::ProfileSettings::Infill::kPrestartDistance = "infill_prestart_distance";
const QString Constants::ProfileSettings::Infill::kPrestartSpeed = "infill_prestart_speed";
const QString Constants::ProfileSettings::Infill::kPrestartExtruderSpeed = "infill_prestart_extruder_speed";
const QString Constants::ProfileSettings::Infill::kSectorCount = "infill_sector_count";
const QString Constants::ProfileSettings::Infill::kPower = "infill_power";
const QString Constants::ProfileSettings::Infill::kFocus = "infill_focus";
const QString Constants::ProfileSettings::Infill::kSpotSize = "infill_spot_size";
const QString Constants::ProfileSettings::Infill::kEnableAlternatingLines = "infill_alternating_lines";

// Support
const QString Constants::ProfileSettings::Support::kEnable = "support";
const QString Constants::ProfileSettings::Support::kPrintFirst = "support_print_first";
const QString Constants::ProfileSettings::Support::kTaper = "support_tapering";
const QString Constants::ProfileSettings::Support::kThresholdAngle = "support_threshold_angle";
const QString Constants::ProfileSettings::Support::kXYDistance = "support_xy_distance";
const QString Constants::ProfileSettings::Support::kLayerOffset = "support_layer_offset";
const QString Constants::ProfileSettings::Support::kMinInfillArea = "support_minimum_infill_area";
const QString Constants::ProfileSettings::Support::kMinArea = "support_minimum_area";
const QString Constants::ProfileSettings::Support::kPattern = "support_pattern";
const QString Constants::ProfileSettings::Support::kLineSpacing = "support_line_spacing";

// Travel
const QString Constants::ProfileSettings::Travel::kSpeed = "travel_speed";
const QString Constants::ProfileSettings::Travel::kMinLength = "minimum_travel_length";
const QString Constants::ProfileSettings::Travel::kMinTravelForLift = "minimum_travel_for_lift";
const QString Constants::ProfileSettings::Travel::kLiftHeight = "travel_lift_height";
const QString Constants::ProfileSettings::Travel::kEnableTravelPause = "enable_travel_pause";
const QString Constants::ProfileSettings::Travel::kEnableTravelCentroidMove = "travel_centroid_move";
const QString Constants::ProfileSettings::Travel::kTravelPauseDuration = "travel_pause_duration";

// G-Code
const QString Constants::ProfileSettings::GCode::kPerimeterStart = "perimeter_start_code";
const QString Constants::ProfileSettings::GCode::kPerimeterEnd = "perimeter_end_code";
const QString Constants::ProfileSettings::GCode::kInsetStart = "inset_start_code";
const QString Constants::ProfileSettings::GCode::kInsetEnd = "inset_end_code";
const QString Constants::ProfileSettings::GCode::kSkeletonStart = "skeleton_start_code";
const QString Constants::ProfileSettings::GCode::kSkeletonEnd = "skeleton_end_code";
const QString Constants::ProfileSettings::GCode::kSkinStart = "skin_start_code";
const QString Constants::ProfileSettings::GCode::kSkinEnd = "skin_end_code";
const QString Constants::ProfileSettings::GCode::kInfillStart = "infill_start_code";
const QString Constants::ProfileSettings::GCode::kInfillEnd = "infill_end_code";
const QString Constants::ProfileSettings::GCode::kSupportStart = "support_start_code";
const QString Constants::ProfileSettings::GCode::kSupportEnd = "support_end_code";

// Special Modes
const QString Constants::ProfileSettings::SpecialModes::kSmoothing = "smoothing";
const QString Constants::ProfileSettings::SpecialModes::kSmoothingTolerance = "smoothing_tolerance";
const QString Constants::ProfileSettings::SpecialModes::kEnableSpiralize = "enable_spiralize_mode";
const QString Constants::ProfileSettings::SpecialModes::kEnableFixModel = "enable_fix_model";
const QString Constants::ProfileSettings::SpecialModes::kEnableOversize = "oversize";
const QString Constants::ProfileSettings::SpecialModes::kOversizeDistance = "oversize_distance";
const QString Constants::ProfileSettings::SpecialModes::kEnableWidthHeight = "enable_width_height";

// Optimizations
const QString Constants::ProfileSettings::Optimizations::kEnableGPU = "enable_gpu_acceleration";
const QString Constants::ProfileSettings::Optimizations::kIslandOrder = "island_order_optimization";
const QString Constants::ProfileSettings::Optimizations::kPathOrder = "path_order_optimization";
const QString Constants::ProfileSettings::Optimizations::kCustomIslandXLocation = "custom_island_order_x_location";
const QString Constants::ProfileSettings::Optimizations::kCustomIslandYLocation = "custom_island_order_y_location";
const QString Constants::ProfileSettings::Optimizations::kCustomPathXLocation = "custom_path_order_x_location";
const QString Constants::ProfileSettings::Optimizations::kCustomPathYLocation = "custom_path_order_y_location";
const QString Constants::ProfileSettings::Optimizations::kPointOrder = "point_order_optimization";
const QString Constants::ProfileSettings::Optimizations::kLocalRandomnessEnable = "local_randomness_enable";
const QString Constants::ProfileSettings::Optimizations::kLocalRandomnessRadius = "local_randomness_radius";
const QString Constants::ProfileSettings::Optimizations::kMinDistanceEnabled = "enable_min_distance";
const QString Constants::ProfileSettings::Optimizations::kMinDistanceThreshold = "min_distance_threshold";
const QString Constants::ProfileSettings::Optimizations::kConsecutiveDistanceThreshold =
    "consecutive_path_distance_threshold";
const QString Constants::ProfileSettings::Optimizations::kCustomPointXLocation = "custom_point_order_x_location";
const QString Constants::ProfileSettings::Optimizations::kCustomPointYLocation = "custom_point_order_y_location";
const QString Constants::ProfileSettings::Optimizations::kEnableSecondCustomLocation =
    "enable_second_custom_point_location";
const QString Constants::ProfileSettings::Optimizations::kEnableSecondCustomLocationEveryTwo =
    "enable_second_point_every_two";
const QString Constants::ProfileSettings::Optimizations::kCustomPointSecondXLocation =
    "custom_second_point_order_x_location";
const QString Constants::ProfileSettings::Optimizations::kCustomPointSecondYLocation =
    "custom_second_point_order_y_location";

// Ordering
const QString Constants::ProfileSettings::Ordering::kRegionOrder = "region_order";
const QString Constants::ProfileSettings::Ordering::kPerimeterReverseDirection = "perimeter_reverse_direction";
const QString Constants::ProfileSettings::Ordering::kInsetReverseDirection = "inset_reverse_direction";

// Laser Scanner
const QString Constants::ProfileSettings::LaserScanner::kLaserScanner = "laser_scanner";
const QString Constants::ProfileSettings::LaserScanner::kSpeed = "laser_speed";
const QString Constants::ProfileSettings::LaserScanner::kLaserScannerHeightOffset = "laser_scanner_height_offset";
const QString Constants::ProfileSettings::LaserScanner::kLaserScannerXOffset = "laser_scanner_x_offset";
const QString Constants::ProfileSettings::LaserScanner::kLaserScannerYOffset = "laser_scanner_y_offset";
const QString Constants::ProfileSettings::LaserScanner::kLaserScannerHeight = "laser_scanner_height";
const QString Constants::ProfileSettings::LaserScanner::kLaserScannerWidth = "laser_scanner_width";
const QString Constants::ProfileSettings::LaserScanner::kLaserScannerStepDistance = "laser_scanner_step_distance";
const QString Constants::ProfileSettings::LaserScanner::kLaserScanLineResolution = "laser_scan_line_resolution";
const QString Constants::ProfileSettings::LaserScanner::kLaserScannerAxis = "laser_scanner_axis";
const QString Constants::ProfileSettings::LaserScanner::kInvertLaserScannerHead = "invert_laser_scanner_head";
const QString Constants::ProfileSettings::LaserScanner::kEnableBedScan = "enable_bed_scan";
const QString Constants::ProfileSettings::LaserScanner::kScanLayerSkip = "scan_layer_skip";
const QString Constants::ProfileSettings::LaserScanner::kEnableScannerBuffer = "enable_scanner_buffer";
const QString Constants::ProfileSettings::LaserScanner::kBufferDistance = "buffer_distance";
const QString Constants::ProfileSettings::LaserScanner::kTransmitHeightMap = "transmit_height_map";
const QString Constants::ProfileSettings::LaserScanner::kGlobalScan = "global_scan";
const QString Constants::ProfileSettings::LaserScanner::kOrientationAxis = "orientation_axis";
const QString Constants::ProfileSettings::LaserScanner::kOrientationAngle = "orientation_angle";
const QString Constants::ProfileSettings::LaserScanner::kEnableOrientationDefinition = "enable_orientation_definition";
const QString Constants::ProfileSettings::LaserScanner::kOrientationA = "orientation_a";
const QString Constants::ProfileSettings::LaserScanner::kOrientationB = "orientation_b";
const QString Constants::ProfileSettings::LaserScanner::kOrientationC = "orientation_c";

// Thermal Scanner
const QString Constants::ProfileSettings::ThermalScanner::kThermalScanner = "thermal_scanner";
const QString Constants::ProfileSettings::ThermalScanner::kThermalScannerTemperatureCutoff =
    "thermal_scanner_temperature_cutoff";
const QString Constants::ProfileSettings::ThermalScanner::kThermalScannerXOffset = "thermal_scanner_x_offset";
const QString Constants::ProfileSettings::ThermalScanner::kThermalScannerYOffset = "thermal_scanner_y_offset";
const QString Constants::ProfileSettings::ThermalScanner::kPyrometerMove = "pyrometer_move";

// Slicing Angle
const QString Constants::ProfileSettings::SlicingVector::kSlicingVectorX = "slicing_vector_x";
const QString Constants::ProfileSettings::SlicingVector::kSlicingVectorY = "slicing_vector_y";
const QString Constants::ProfileSettings::SlicingVector::kSlicingVectorZ = "slicing_vector_z";

//================================================================================
// Experimental Settings
//================================================================================

// Printer Config
const QString Constants::ExperimentalSettings::PrinterConfig::kSlicerType = "slicer_type";
const QString Constants::ExperimentalSettings::PrinterConfig::kLayerOrdering = "layer_ordering";
const QString Constants::ExperimentalSettings::PrinterConfig::kLayerGroupingTolerance = "layer_grouping_tolerance";

// Single Path
const QString Constants::ExperimentalSettings::SinglePath::kEnableSinglePath = "enable_single_path";
const QString Constants::ExperimentalSettings::SinglePath::kEnableBridgeExclusion = "enable_bridge_exclusion";
const QString Constants::ExperimentalSettings::SinglePath::kEnableZippering = "enable_zippering";
const QString Constants::ExperimentalSettings::SinglePath::kPrevLayerExclusionDistance =
    "previous_layer_exclusion_distance";
const QString Constants::ExperimentalSettings::SinglePath::kCornerExclusionDistance = "corner_exclusion_distance";
const QString Constants::ExperimentalSettings::SinglePath::kMaxBridgeLength = "max_bridge_length";
const QString Constants::ExperimentalSettings::SinglePath::kMinBridgeSeparation = "min_bridge_separation";

// RPBF Slicing
const QString Constants::ExperimentalSettings::RPBFSlicing::kSectorSize = "sector_size";
const QString Constants::ExperimentalSettings::RPBFSlicing::kSectorOffsettingEnable = "sector_offsetting_enable";
const QString Constants::ExperimentalSettings::RPBFSlicing::kSectorOverlap = "sector_overlap";
const QString Constants::ExperimentalSettings::RPBFSlicing::kSectorStaggerEnable = "sector_stagger_enable";
const QString Constants::ExperimentalSettings::RPBFSlicing::kSectorStaggerAngle = "sector_stagger_angle";
const QString Constants::ExperimentalSettings::RPBFSlicing::kClockingAngle = "clocking_angle";

// Multi-Nozzle
const QString Constants::ExperimentalSettings::MultiNozzle::kEnableMultiNozzleMultiMaterial = "enable_multi_nozzle";
const QString Constants::ExperimentalSettings::MultiNozzle::kNozzleCount = "nozzle_count";
const QString Constants::ExperimentalSettings::MultiNozzle::kNozzleOffsetX = "nozzle_offset_x";
const QString Constants::ExperimentalSettings::MultiNozzle::kNozzleOffsetY = "nozzle_offset_y";
const QString Constants::ExperimentalSettings::MultiNozzle::kNozzleOffsetZ = "nozzle_offset_z";
const QString Constants::ExperimentalSettings::MultiNozzle::kNozzleMaterial = "nozzle_material";
const QString Constants::ExperimentalSettings::MultiNozzle::kEnableDuplicatePathRemoval =
    "enable_duplicate_path_removal";
const QString Constants::ExperimentalSettings::MultiNozzle::kDuplicatePathSimilarity = "duplicate_path_similarity";
const QString Constants::ExperimentalSettings::MultiNozzle::kEnableIndependentNozzles = "enable_indpendent_nozzles";
const QString Constants::ExperimentalSettings::MultiNozzle::kNozzleAssignmentMethod = "independent_nozzle_optimizer";

// Visualization
const QString Constants::ExperimentalSettings::GcodeVisualization::kDisableVisualization = "disable_visualization";
const QString Constants::ExperimentalSettings::GcodeVisualization::kVisualizationSkip = "visualization_layer_skip";

// Ramping
const QString Constants::ExperimentalSettings::Ramping::kTrajectoryAngleEnabled = "trajectory_angle_slow_down";
const QString Constants::ExperimentalSettings::Ramping::kTrajectoryAngleThreshold =
    "trajectory_angle_threshold_slow_down";
const QString Constants::ExperimentalSettings::Ramping::kTrajectoryAngleRampDownDistance =
    "trajectory_angle_distance_slow_down";
const QString Constants::ExperimentalSettings::Ramping::kTrajectoryAngleRampUpDistance =
    "trajectory_angle_distance_speed_up";
const QString Constants::ExperimentalSettings::Ramping::kTrajectoryAngleSpeedSlowDown =
    "trajectory_angle_speed_slow_down";
const QString Constants::ExperimentalSettings::Ramping::kTrajectoryAngleExtruderSpeedSlowDown =
    "trajectory_angle_extruder_speed_slow_down";
const QString Constants::ExperimentalSettings::Ramping::kTrajectoryAngleSpeedUp = "trajectory_angle_speed_up";
const QString Constants::ExperimentalSettings::Ramping::kTrajectoryAngleExtruderSpeedUp =
    "trajectory_angle_extruder_speed_up";

// Wire Feed
const QString Constants::ExperimentalSettings::WireFeed::kWireFeedEnable = "wire_feed_enable";
const QString Constants::ExperimentalSettings::WireFeed::kSettingsRegionMeshSplit = "setting_region_mesh_split";
const QString Constants::ExperimentalSettings::WireFeed::kInitialTravelSpeed = "wire_feed_initial_travel";
const QString Constants::ExperimentalSettings::WireFeed::kAnchorEnable = "anchor_enable";
const QString Constants::ExperimentalSettings::WireFeed::kAnchorWidth = "anchor_width";
const QString Constants::ExperimentalSettings::WireFeed::kAnchorHeight = "anchor_height";
const QString Constants::ExperimentalSettings::WireFeed::kAnchorObjectDistanceLeft = "anchor_object_distance_left";
const QString Constants::ExperimentalSettings::WireFeed::kAnchorObjectDistanceRight = "anchor_object_distance_right";
const QString Constants::ExperimentalSettings::WireFeed::kWireCutoffDistance = "wire_feed_cutoff_distance";
const QString Constants::ExperimentalSettings::WireFeed::kWireStickoutDistance = "wire_feed_stickout_distance";
const QString Constants::ExperimentalSettings::WireFeed::kWirePrestartDistance = "wire_feed_prestart_distance";

// File Output
const QString Constants::ExperimentalSettings::FileOutput::kMeldCompanionOutput = "additional_meld_output";
const QString Constants::ExperimentalSettings::FileOutput::kMeldDiscrete = "meld_discrete_feed_commands";
const QString Constants::ExperimentalSettings::FileOutput::kTormachOutput = "tormach_file_output";
const QString Constants::ExperimentalSettings::FileOutput::kTormachMode = "tormach_mode";
const QString Constants::ExperimentalSettings::FileOutput::kAML3DOutput = "aml3d_file_output";
const QString Constants::ExperimentalSettings::FileOutput::kAML3DWeaveLength = "aml3d_weave_length";
const QString Constants::ExperimentalSettings::FileOutput::kAML3DWeaveWidth = "aml3d_weave_width";
const QString Constants::ExperimentalSettings::FileOutput::kSandiaOutput = "sandia_file_output";
const QString Constants::ExperimentalSettings::FileOutput::kSandiaMetalFile = "sandia_metal_file";
const QString Constants::ExperimentalSettings::FileOutput::kSandiaCVEL = "sandia_cvel";
const QString Constants::ExperimentalSettings::FileOutput::kMarlinOutput = "marlin_file_output";
const QString Constants::ExperimentalSettings::FileOutput::kMarlinTravels = "marlin_include_travels";
const QString Constants::ExperimentalSettings::FileOutput::kSimulationOutput = "simulation_file_output";

// Rotation Origin
const QString Constants::ExperimentalSettings::RotationOrigin::kXOffset = "rotation_origin_offset_x";
const QString Constants::ExperimentalSettings::RotationOrigin::kYOffset = "rotation_origin_offset_y";

// Image Resolution
const QString Constants::ExperimentalSettings::ImageResolution::kImageResolutionX = "image_resolution_x";
const QString Constants::ExperimentalSettings::ImageResolution::kImageResolutionY = "image_resolution_y";

// Cross-Section
const QString Constants::ExperimentalSettings::CrossSection::kLargestGap = "cross_section_largest_gap";
const QString Constants::ExperimentalSettings::CrossSection::kMaxStitch = "cross_section_max_stitch_distance";

//================================================================================
// Settings
//================================================================================
// Master
const std::string Constants::Settings::Master::kDisplay = "display";
const std::string Constants::Settings::Master::kType = "type";
const std::string Constants::Settings::Master::kToolTip = "tooltip";
const std::string Constants::Settings::Master::kMinor = "minor";
const std::string Constants::Settings::Master::kMajor = "major";
const std::string Constants::Settings::Master::kOptions = "options";
const std::string Constants::Settings::Master::kDepends = "depends";
const std::string Constants::Settings::Master::kDefault = "default";
const std::string Constants::Settings::Master::kDependencyGroup = "dependency_group";
const std::string Constants::Settings::Master::kLocal = "local";

// Session
const std::string Constants::Settings::Session::kParts = "parts";
const std::string Constants::Settings::Session::kName = "name";
const std::string Constants::Settings::Session::kTransform = "transform";
const std::string Constants::Settings::Session::kTransforms = "transforms";
const std::string Constants::Settings::Session::kMeshType = "mesh_type";
const std::string Constants::Settings::Session::kGenType = "gen_type";
const std::string Constants::Settings::Session::kOrgDims = "org_dim";
const std::string Constants::Settings::Session::kFile = "file";
const std::string Constants::Settings::Session::kDir = "#SESSION#";
const std::string Constants::Settings::Session::LocalFile::kName = "name";
const std::string Constants::Settings::Session::LocalFile::kSettings = "settings";
const std::string Constants::Settings::Session::LocalFile::kRanges = "ranges";
const std::string Constants::Settings::Session::Files::kSession = "session.s2c";
const std::string Constants::Settings::Session::Files::kGlobal = "global.s2c";
const std::string Constants::Settings::Session::Files::kLocal = "local.s2c";
const std::string Constants::Settings::Session::Files::kPref = "pref.s2c";
const std::string Constants::Settings::Session::Files::kModel = "model";
const std::string Constants::Settings::Session::Range::kLow = "low";
const std::string Constants::Settings::Session::Range::kHigh = "high";
const std::string Constants::Settings::Session::Range::kName = "name";
const std::string Constants::Settings::Session::Range::kSettings = "settings";

// Current Tabs
const QString Constants::Settings::SettingTab::kPrinter = "Printer";
const QString Constants::Settings::SettingTab::kMaterial = "Material";
const QString Constants::Settings::SettingTab::kProfile = "Profile";
const QString Constants::Settings::SettingTab::kExperimental = "Experimental";

//================================================================================
// Segment Settings(G-Code Output)
//================================================================================
const QString Constants::SegmentSettings::kHeight = "height";
const QString Constants::SegmentSettings::kWidth = "width";
const QString Constants::SegmentSettings::kSpeed = "speed";
const QString Constants::SegmentSettings::kAccel = "accel";
const QString Constants::SegmentSettings::kExtruderSpeed = "extruder_speed";
const QString Constants::SegmentSettings::kWaitTime = "wait_time";
const QString Constants::SegmentSettings::kRegionType = "region_type";
const QString Constants::SegmentSettings::kPathModifiers = "path_modifiers";
const QString Constants::SegmentSettings::kMaterialNumber = "material_number";
const QString Constants::SegmentSettings::kRotation = "rotation";
const QString Constants::SegmentSettings::kRecipe = "recipe_index";
const QString Constants::SegmentSettings::kTilt = "tilt";
const QString Constants::SegmentSettings::kCCW = "ccw";
const QString Constants::SegmentSettings::kESP = "esp";
const QString Constants::SegmentSettings::kExtruders = "extruders";
const QString Constants::SegmentSettings::kIsRegionStartSegment = "is_region_start_segment";
const QString Constants::SegmentSettings::kWireFeed = "wire_feed";
const QString Constants::SegmentSettings::kFinalWireCoast = "final_wire_coast";
const QString Constants::SegmentSettings::kFinalWireFeed = "final_wire_feed";

//================================================================================
// Limits
//================================================================================
// Maximums
const Distance Constants::Limits::Maximums::kMaxDistance = std::numeric_limits<float>::max();
const Velocity Constants::Limits::Maximums::kMaxSpeed = std::numeric_limits<float>::max();
const Acceleration Constants::Limits::Maximums::kMaxAccel = std::numeric_limits<float>::max();
const AngularVelocity Constants::Limits::Maximums::kMaxAngVel = std::numeric_limits<float>::max();
const Time Constants::Limits::Maximums::kMaxTime = std::numeric_limits<float>::max();
const Temperature Constants::Limits::Maximums::kMaxTemperature = std::numeric_limits<float>::max();
const Angle Constants::Limits::Maximums::kMaxAngle = 2 * pi;
const Area Constants::Limits::Maximums::kMaxArea = std::numeric_limits<float>::max();
const Voltage Constants::Limits::Maximums::kMaxVoltage = std::numeric_limits<float>::max();
const float Constants::Limits::Maximums::kMaxFloat = std::numeric_limits<float>::max();
const float Constants::Limits::Maximums::kInfFloat = std::numeric_limits<float>::infinity();

// Minimums
const Distance Constants::Limits::Minimums::kMinDistance = 0.0;
const Distance Constants::Limits::Minimums::kMinLocation = std::numeric_limits<float>::lowest();
const Velocity Constants::Limits::Minimums::kMinSpeed = std::numeric_limits<float>::lowest();
const Acceleration Constants::Limits::Minimums::kMinAccel = std::numeric_limits<float>::lowest();
const AngularVelocity Constants::Limits::Minimums::kMinAngVel = std::numeric_limits<float>::lowest();
const Time Constants::Limits::Minimums::kMinTime = std::numeric_limits<float>::lowest();
const Temperature Constants::Limits::Minimums::kMinTemperature = std::numeric_limits<float>::lowest();
const Angle Constants::Limits::Minimums::kMinAngle = -2 * pi;
const Area Constants::Limits::Minimums::kMinArea = std::numeric_limits<float>::lowest();
const float Constants::Limits::Minimums::kMinFloat = std::numeric_limits<float>::lowest();

//================================================================================
// Colors
//================================================================================
const QColor Constants::Colors::kYellow = QColor(255, 255, 0, 255);
const QColor Constants::Colors::kRed = QColor(255, 0, 0, 255);
const QColor Constants::Colors::kBlue = QColor(0, 0, 255, 255);
const QColor Constants::Colors::kLightBlue = QColor(0, 255, 255, 255);
const QColor Constants::Colors::kGreen = QColor(0, 255, 0, 255);
const QColor Constants::Colors::kPurple = QColor(127, 0, 255, 255);
const QColor Constants::Colors::kOrange = QColor(255, 128, 0, 255);
const QColor Constants::Colors::kWhite = QColor(255, 255, 255, 255);
const QColor Constants::Colors::kBlack = QColor(0, 0, 0, 255);
const QVector<QColor> Constants::Colors::kModelColors = {kBlue, kPurple, kRed, kOrange, kGreen};

//================================================================================
// UI
//================================================================================
// Shadow
const int Constants::UI::Common::DropShadow::kXOffset = 2;
const int Constants::UI::Common::DropShadow::kYOffset = 2;
const int Constants::UI::Common::DropShadow::kBlurRadius = 4;
const QColor Constants::UI::Common::DropShadow::kColor = QColor(10, 10, 10, 60);

// MainWindow
const QSize Constants::UI::MainWindow::kWindowSize = QSize(1280, 820);
const QSize Constants::UI::MainWindow::kViewWidgetSize = QSize(745, 750);
const QSize Constants::UI::MainWindow::kLayerbarMinSize = QSize(40, 0);
const int Constants::UI::MainWindow::kProgressBarWidth = 200;
const int Constants::UI::MainWindow::kStatusBarMaxHeight = 200;

// MainWindow: SideDock
const int Constants::UI::MainWindow::SideDock::kSettingsWidth = 500;
const int Constants::UI::MainWindow::SideDock::kGCodeWidth = 500;
const int Constants::UI::MainWindow::SideDock::kLayerTimesWidth = 500;
const int Constants::UI::MainWindow::SideDock::kExternalFileWidth = 500;

// MainWindow: Margins
const int Constants::UI::MainWindow::Margins::kMainLayoutSpacing = 6;
const int Constants::UI::MainWindow::Margins::kMainContainerSpacing = 0;
const int Constants::UI::MainWindow::Margins::kMainLayout = 11;
const int Constants::UI::MainWindow::Margins::kMainContainer = 0;

// Main Toolbar
const int Constants::UI::MainToolbar::kMaxWidth = 1250;
const int Constants::UI::MainToolbar::kStartOffset = 20;
const int Constants::UI::MainToolbar::kEndOffset = 10;
const int Constants::UI::MainToolbar::kVerticalOffset = 10;

// View Controls Toolbars
const int Constants::UI::ViewControlsToolbar::kHeight = 80;
const int Constants::UI::ViewControlsToolbar::kWidth = 290;
const int Constants::UI::ViewControlsToolbar::kBottomOffset = 10;
const int Constants::UI::ViewControlsToolbar::kRightOffset = 10;

// Part Toolbar
const int Constants::UI::PartToolbar::kWidth = 40;
const int Constants::UI::PartToolbar::kHeight = 550;
const int Constants::UI::PartToolbar::kLeftOffset = 10;
const int Constants::UI::PartToolbar::kMinTopOffset = 60;

// Part Toolbar: Input
const int Constants::UI::PartToolbar::Input::kBoxWidth = 700;
const int Constants::UI::PartToolbar::Input::kBoxHeight = 70;
const int Constants::UI::PartToolbar::Input::kExtraButtonWidth = 50;
const int Constants::UI::PartToolbar::Input::kPrecision = 4;
const int Constants::UI::PartToolbar::Input::kAnimationInTime = 400;
const int Constants::UI::PartToolbar::Input::kAnimationOutTime = 400;

// Part Control
const QSize Constants::UI::PartControl::kSize = QSize(275, 150);
const int Constants::UI::PartControl::kLeftOffset = 10;
const int Constants::UI::PartControl::kBottomOffset = 10;

// Themes
const QString Constants::UI::Themes::kSystemMode = "System (default)";
const QString Constants::UI::Themes::kDarkMode = "Dark Mode";

const QStringList Constants::UI::Themes::kThemes = {Constants::UI::Themes::kSystemMode,
                                                    Constants::UI::Themes::kDarkMode};

//================================================================================
// OpenGL
//================================================================================
const double Constants::OpenGL::kZoomDefault = -75;
const double Constants::OpenGL::kZoomMin = -125;
const double Constants::OpenGL::kZoomMax = -0.01;
const double Constants::OpenGL::kTrackball = 90.0f;

const double Constants::OpenGL::kFov = 60.0;
const double Constants::OpenGL::kNearPlane = 0.1;
const double Constants::OpenGL::kFarPlane = 1000.0;

const float Constants::OpenGL::kObjectToView = 0.00001f;
const float Constants::OpenGL::kViewToObject = 100000.0f;

// ShaderProgram 1 files
const char* Constants::OpenGL::Shader::kVertShaderFile = ":/shaders/vert";
const char* Constants::OpenGL::Shader::kFragShaderFile = ":/shaders/frag";

const char* Constants::OpenGL::Shader::kLightingColorName = "lightColor";
const char* Constants::OpenGL::Shader::kLightingPositionName = "lightPos";
const char* Constants::OpenGL::Shader::kCameraPositionName = "viewPos";
const char* Constants::OpenGL::Shader::kAmbientStrengthName = "ambientStrength";
const char* Constants::OpenGL::Shader::kUsingSolidWireframeModeName = "usingSolidWireframeMode";

const char* Constants::OpenGL::Shader::kPositionName = "position";
const char* Constants::OpenGL::Shader::kColorName = "color";
const char* Constants::OpenGL::Shader::kNormalName = "normal";
const char* Constants::OpenGL::Shader::kUVName = "uv";

const char* Constants::OpenGL::Shader::kProjectionName = "projection";
const char* Constants::OpenGL::Shader::kViewName = "view";
const char* Constants::OpenGL::Shader::kModelName = "model";
const char* Constants::OpenGL::Shader::kStackingAxisName = "stackingAxis";
const char* Constants::OpenGL::Shader::kOverhangAngleName = "overhangAngle";
const char* Constants::OpenGL::Shader::kOverhangModeName = "usingOverhangMode";
const char* Constants::OpenGL::Shader::kRenderingPartObjectName = "renderingPartObject";
//================================================================================
// Slicer 1 Keys - used for gcode processing (all caps)
//================================================================================
const QString Constants::GcodeFileVariables::kPrinterBaseOffset = "PRINTERBASEOFFSET";
const QString Constants::GcodeFileVariables::kExtrusionWidth = "EXTRUSIONWIDTH";
const QString Constants::GcodeFileVariables::kXOffset = "XOFFSET";
const QString Constants::GcodeFileVariables::kYOffset = "YOFFSET";
const QString Constants::GcodeFileVariables::kLiftSpeed = "LIFTSPEED";
const QString Constants::GcodeFileVariables::kTravelSpeedMin = "TRAVELSPEEDMIN";
const QString Constants::GcodeFileVariables::kTravelSpeed = "TRAVELSPEED";
const QString Constants::GcodeFileVariables::kWTableSpeed = "WTABLESPEED";
const QString Constants::GcodeFileVariables::kInitialLayerThickness = "INITIALLAYERTHICKNESS";
const QString Constants::GcodeFileVariables::kLayerThickness = "LAYERTHICKNESS";
const QString Constants::GcodeFileVariables::kFirstLayerDefaultWidth = "LAYER0EXTRUSIONWIDTH";
const QString Constants::GcodeFileVariables::kForceMinLayerTime = "FORCEMINIMUMLAYERTIME";
const QString Constants::GcodeFileVariables::kForceMinLayerTimeMethod = "MINIMUMLAYERTIMEMETHOD";
const QString Constants::GcodeFileVariables::kMinimalLayerTime = "MINIMALLAYERTIME";
const QString Constants::GcodeFileVariables::kMaximalLayerTime = "MAXIMALLAYERTIME";
const QString Constants::GcodeFileVariables::kPlasticType = "PLASTICTYPE";
const QString Constants::GcodeFileVariables::kManualDensity = "MANUALDENSITY";
// actualdensity?

// Slicer 1 and Slicer 2 keys that are necessary for gcode parsing
// Slicer 1 keys are converted to Slicer 2 counterparts
// Slicer 2 keys are inserted as is
const QHash<QString, QString> Constants::GcodeFileVariables::kNecessaryVariables = QHash<QString, QString>({
    {Constants::GcodeFileVariables::kPrinterBaseOffset, Constants::PrinterSettings::Dimensions::kZOffset},
    {Constants::GcodeFileVariables::kExtrusionWidth, Constants::ProfileSettings::Layer::kBeadWidth},
    {Constants::GcodeFileVariables::kXOffset, Constants::PrinterSettings::Dimensions::kXOffset},
    {Constants::GcodeFileVariables::kYOffset, Constants::PrinterSettings::Dimensions::kYOffset},
    {Constants::GcodeFileVariables::kInitialLayerThickness, Constants::ProfileSettings::Layer::kLayerHeight},
    {Constants::GcodeFileVariables::kLayerThickness, Constants::ProfileSettings::Layer::kLayerHeight},
    {Constants::GcodeFileVariables::kFirstLayerDefaultWidth, Constants::ProfileSettings::Layer::kBeadWidth},
    {Constants::GcodeFileVariables::kLiftSpeed, Constants::PrinterSettings::MachineSpeed::kZSpeed},
    {Constants::GcodeFileVariables::kTravelSpeedMin, Constants::PrinterSettings::MachineSpeed::kMinXYSpeed},
    {Constants::GcodeFileVariables::kTravelSpeed, Constants::PrinterSettings::MachineSpeed::kMaxXYSpeed},
    {Constants::GcodeFileVariables::kWTableSpeed, Constants::PrinterSettings::MachineSpeed::kWTableSpeed},
    {Constants::GcodeFileVariables::kForceMinLayerTime, Constants::MaterialSettings::Cooling::kForceMinLayerTime},
    {Constants::GcodeFileVariables::kForceMinLayerTimeMethod,
     Constants::MaterialSettings::Cooling::kForceMinLayerTimeMethod},
    {Constants::GcodeFileVariables::kMinimalLayerTime, Constants::MaterialSettings::Cooling::kMinLayerTime},
    {Constants::GcodeFileVariables::kMaximalLayerTime, Constants::MaterialSettings::Cooling::kMaxLayerTime},
    {Constants::GcodeFileVariables::kPlasticType, Constants::MaterialSettings::Density::kMaterialType},
    {Constants::GcodeFileVariables::kManualDensity, Constants::MaterialSettings::Density::kDensity},

    {Constants::PrinterSettings::Dimensions::kZOffset.toUpper(), Constants::PrinterSettings::Dimensions::kZOffset},
    {Constants::ProfileSettings::Layer::kBeadWidth.toUpper(), Constants::ProfileSettings::Layer::kBeadWidth},
    {Constants::PrinterSettings::Dimensions::kXMin.toUpper(), Constants::PrinterSettings::Dimensions::kXMin},
    {Constants::PrinterSettings::Dimensions::kXMax.toUpper(), Constants::PrinterSettings::Dimensions::kXMax},
    {Constants::PrinterSettings::Dimensions::kYMin.toUpper(), Constants::PrinterSettings::Dimensions::kYMin},
    {Constants::PrinterSettings::Dimensions::kYMax.toUpper(), Constants::PrinterSettings::Dimensions::kYMax},
    {Constants::PrinterSettings::Dimensions::kXOffset.toUpper(), Constants::PrinterSettings::Dimensions::kXOffset},
    {Constants::PrinterSettings::Dimensions::kYOffset.toUpper(), Constants::PrinterSettings::Dimensions::kYOffset},
    {Constants::PrinterSettings::Dimensions::kOuterRadius.toUpper(),
     Constants::PrinterSettings::Dimensions::kOuterRadius},
    {Constants::PrinterSettings::Dimensions::kBuildVolumeType.toUpper(),
     Constants::PrinterSettings::Dimensions::kBuildVolumeType},
    {Constants::ProfileSettings::Layer::kLayerHeight.toUpper(), Constants::ProfileSettings::Layer::kLayerHeight},
    {Constants::PrinterSettings::MachineSpeed::kZSpeed.toUpper(), Constants::PrinterSettings::MachineSpeed::kZSpeed},
    {Constants::PrinterSettings::MachineSpeed::kMinXYSpeed.toUpper(),
     Constants::PrinterSettings::MachineSpeed::kMinXYSpeed},
    {Constants::PrinterSettings::MachineSpeed::kMaxXYSpeed.toUpper(),
     Constants::PrinterSettings::MachineSpeed::kMaxXYSpeed},
    {Constants::PrinterSettings::MachineSpeed::kWTableSpeed.toUpper(),
     Constants::PrinterSettings::MachineSpeed::kWTableSpeed},
    {Constants::MaterialSettings::Cooling::kForceMinLayerTime.toUpper(),
     Constants::MaterialSettings::Cooling::kForceMinLayerTime},
    {Constants::MaterialSettings::Cooling::kMinLayerTime.toUpper(),
     Constants::MaterialSettings::Cooling::kMinLayerTime},
    {Constants::MaterialSettings::Cooling::kMaxLayerTime.toUpper(),
     Constants::MaterialSettings::Cooling::kMaxLayerTime},
    {Constants::MaterialSettings::Density::kMaterialType.toUpper(),
     Constants::MaterialSettings::Density::kMaterialType},
    {Constants::MaterialSettings::Density::kDensity.toUpper(), Constants::MaterialSettings::Density::kDensity},
    {Constants::ExperimentalSettings::MultiNozzle::kNozzleCount.toUpper(),
     Constants::ExperimentalSettings::MultiNozzle::kNozzleCount},
    {Constants::ExperimentalSettings::MultiNozzle::kNozzleOffsetX.toUpper(),
     Constants::ExperimentalSettings::MultiNozzle::kNozzleOffsetX},
    {Constants::ExperimentalSettings::MultiNozzle::kNozzleOffsetY.toUpper(),
     Constants::ExperimentalSettings::MultiNozzle::kNozzleOffsetY},
    {Constants::ExperimentalSettings::MultiNozzle::kNozzleOffsetZ.toUpper(),
     Constants::ExperimentalSettings::MultiNozzle::kNozzleOffsetZ},

});

// specific Slicer 1 keys must be converted to new base in Slicer 2, this lists those keys
const QHash<QString, QString> Constants::GcodeFileVariables::kRequiredConversion = QHash<QString, QString>(
    {{Constants::GcodeFileVariables::kLiftSpeed, Constants::PrinterSettings::MachineSpeed::kZSpeed},
     {Constants::GcodeFileVariables::kTravelSpeedMin, Constants::PrinterSettings::MachineSpeed::kMinXYSpeed},
     {Constants::GcodeFileVariables::kTravelSpeed, Constants::PrinterSettings::MachineSpeed::kMaxXYSpeed},
     {Constants::GcodeFileVariables::kWTableSpeed, Constants::PrinterSettings::MachineSpeed::kWTableSpeed}});

const std::string Constants::SettingFileStrings::kHeader = "header";
const std::string Constants::SettingFileStrings::kCreatedBy = "created_by";
const std::string Constants::SettingFileStrings::kCreatedOn = "created_on";
const std::string Constants::SettingFileStrings::kLastModified = "last_modified";
const std::string Constants::SettingFileStrings::kVersion = "version";
const std::string Constants::SettingFileStrings::kLock = "lock";
const std::string Constants::SettingFileStrings::kSettings = "settings";

const QString Constants::ConsoleOptionStrings::kInputProjectFile = "input_project_file";
const QString Constants::ConsoleOptionStrings::kInputStlFiles = "input_stl_files";
const QString Constants::ConsoleOptionStrings::kInputStlFilesDirectory = "input_stl_files_directory";
const QString Constants::ConsoleOptionStrings::kInputSupportStlFiles = "input_support_stl_files";
const QString Constants::ConsoleOptionStrings::kInputSupportStlFilesDirectory = "input_support_stl_files_directory";
const QString Constants::ConsoleOptionStrings::kInputStlCount = "input_stl_count";
const QString Constants::ConsoleOptionStrings::kInputSupportStlCount = "input_support_stl_count";
const QString Constants::ConsoleOptionStrings::kInputGlobalSettings = "input_global_settings";
const QString Constants::ConsoleOptionStrings::kInputLocalSettings = "input_local_settings";
const QString Constants::ConsoleOptionStrings::kInputSTLTransform = "input_stl_transform";
const QString Constants::ConsoleOptionStrings::kOutputLocation = "output_location";

const QString Constants::ConsoleOptionStrings::kShiftPartsOnLoad = "shift_parts_on_load";
const QString Constants::ConsoleOptionStrings::kAlignParts = "align_parts";
const QString Constants::ConsoleOptionStrings::kUseImplicitTransforms = "use_implicit_transforms";

const QString Constants::ConsoleOptionStrings::kOverwriteOutputFile = "overwrite_output_file";
const QString Constants::ConsoleOptionStrings::kIncludeAuxiliaryFiles = "include_auxiliary_files";
const QString Constants::ConsoleOptionStrings::kIncludeProjectFile = "include_project_file";
const QString Constants::ConsoleOptionStrings::kBundleOutput = "bundle_output";
const QString Constants::ConsoleOptionStrings::kHeaderSlicedBy = "header_sliced_by";
const QString Constants::ConsoleOptionStrings::kHeaderDescription = "header_description";

const QString Constants::ConsoleOptionStrings::kSliceBounds = "slice_bounds";
const QString Constants::ConsoleOptionStrings::kRealTimeMode = "real_time_mode";
const QString Constants::ConsoleOptionStrings::kRecoveryFilePath = "recovery_file_path";
const QString Constants::ConsoleOptionStrings::kOpenLoop = "open_loop";
const QString Constants::ConsoleOptionStrings::kRealTimeCommunicationMode = "real_time_communication_mode";
const QString Constants::ConsoleOptionStrings::kRealTimeNetworkAddress = "real_time_network_address";
const QString Constants::ConsoleOptionStrings::kRealTimeNetworkIP = "real_time_network_ip";
const QString Constants::ConsoleOptionStrings::kRealTimeNetworkPort = "real_time_network_port";
const QString Constants::ConsoleOptionStrings::kRealTimePrinter = "real_time_printer_name";
const QString Constants::ConsoleOptionStrings::kSingleSliceHeight = "single_slice_height";
const QString Constants::ConsoleOptionStrings::kSingleSliceLayerNumber = "single_slice_layer_number";

const QString Constants::ConsoleOptionStrings::kVersion = "version";

} // namespace ORNL
