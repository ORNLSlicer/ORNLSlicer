#pragma once

#include <cstdint>
#include <stdexcept>

#include <QCoreApplication>
#include <QMessageBox>
#include <qcolor.h>
#include <qtcoreexports.h>

#include "constants.h"
#include "exceptions/exceptions.h"
#include "units/unit.h"
#include "utilities/qt_json_conversion.h"

using json = fifojson;

namespace ORNL {
/*!
 * \enum RealTimeSlicingMode
 * \brief what mode real time slicing is operating in
 */
enum RealTimeSlicingMode { kClosedLoop = 0, kOpenLoop = 1 };

/*!
 * \enum RealTimeSlicingOutput
 * \brief how the gcode should be written when using real time slicing
 */
enum RealTimeSlicingOutput { kFile = 0, kNetwork = 1 };

/*!
 * \enum MeshType
 * \brief the various types of meshes supported by ORNLSlicer
 */
enum MeshType { kBuild, kClipping, kSettings, kSupport };

/*!
 * \enum MeshGeneratorType
 * \brief types of meshes that can be generated
 */
enum MeshGeneratorType {
    kNone = 0,
    kDefaultSettingRegion = 1,
    kOpenTopBox = 2,
    kRectangularBox = 3,
    kTriangularPyramid = 4,
    kCylinder = 5,
    kCone = 6
};

/*!
 * \enum LinkType
 * \brief Selects the type of link between threads
 */
enum class LinkType : uint8_t {
    kPreviousLayerExclusionInset,
    kZipperingInset,
    kPreviousLayerExclusionPerimeter,
    kZipperingPerimeter
};

/*! \enum BuildVolumeType
 * \brief Determines the type of build volume to create.
 */
enum class BuildVolumeType : uint8_t { kRectangular = 0, kCylindrical = 1, kToroidal = 2 };

/*!
 * \enum SlicerType
 * \brief Selects the type of slice to perform. Pass this to Session Manager to decide.
 */
enum class SlicerType : uint8_t {
    kPolymerSlice = 0,
    kMetalSlice = 1,
    kRPBFSlice = 2,
    kRealTimePolymer = 3,
    kRealTimeRPBF = 4,
    kImageSlice = 5
};

//! \brief Function for going from json to SlicerType
void to_json(json& j, const SlicerType& i);

//! \brief Function for going from SlicerType to json
void from_json(const json& j, SlicerType& i);

/*!
 * \enum AffectedArea
 * \brief The AffectedArea enum
 */
enum class AffectedArea : int // was uint8_t
{
    kNone = 0,
    kPerimeter = 1 << 0,
    kInset = 1 << 1,
    kInfill = 1 << 2,
    kTopSkin = 1 << 3,
    kBottomSkin = 1 << 4,
    kSkin = 1 << 5,
    kSupport = 1 << 6,
    kRaft = 1 << 7,
    kBrim = 1 << 8,
    kSkirt = 1 << 9,
    kLaserScan = 1 << 10,
    kThermalScan = 1 << 11
};

/*!
 * \enum ThemeName
 * \brief The ThemeName enum
 */
enum class ThemeName {
    kLightMode,
    kDarkMode,
};

inline QString toString(ThemeName theme) {
    switch (theme) {
        case ThemeName::kLightMode:
            return Constants::UI::Themes::kSystemMode;
        case ThemeName::kDarkMode:
            return Constants::UI::Themes::kDarkMode;
    }
}

inline ThemeName themeFromString(const QString& theme) {
    return theme == Constants::UI::Themes::kDarkMode ? ThemeName::kDarkMode : ThemeName::kLightMode;
}

/*!
 * \enum GcodeSyntax
 * \brief The GcodeSyntax enum
 */
enum class GcodeSyntax : uint8_t {
    kBeam,
    kCincinnati,
    kCommon,
    kDmgDmu,
    kGKN,
    kGudel,
    kHaasInch,
    kHaasMetric,
    kHaasMetricNoComments,
    kHurco,
    kIngersoll,
    kMarlin,
    kMarlinPellet,
    kMazak,
    kMVP,
    kRomiFanuc,
    kRPBF,
    kSiemens,
    kSkyBaam,
    kThermwood,
    kWolf,
    kRepRap,
    kMach4,
    kAeroBasic,
    kMeld,
    kORNL,
    kOkuma,
    kTormach,
    kAML3D,
    kKraussMaffei,
    kSandia,
    k5AxisMarlin,
    kMeltio,
    kAdamantine,
    kORNLMetric
};

inline QString toString(GcodeSyntax syntax) {
    switch (syntax) {
        case GcodeSyntax::k5AxisMarlin:
            return PRS::SyntaxString::k5AxisMarlin;
        case GcodeSyntax::kAML3D:
            return PRS::SyntaxString::kAML3D;
        case GcodeSyntax::kBeam:
            return PRS::SyntaxString::kBeam;
        case GcodeSyntax::kCincinnati:
            return PRS::SyntaxString::kCincinnati;
        case GcodeSyntax::kDmgDmu:
            return PRS::SyntaxString::kDmgDmu;
        case GcodeSyntax::kGKN:
            return PRS::SyntaxString::kGKN;
        case GcodeSyntax::kGudel:
            return PRS::SyntaxString::kGudel;
        case GcodeSyntax::kHaasInch:
            return PRS::SyntaxString::kHaasInch;
        case GcodeSyntax::kHaasMetric:
            return PRS::SyntaxString::kHaasMetric;
        case GcodeSyntax::kHaasMetricNoComments:
            return PRS::SyntaxString::kHaasMetricNoComments;
        case GcodeSyntax::kHurco:
            return PRS::SyntaxString::kHurco;
        case GcodeSyntax::kIngersoll:
            return PRS::SyntaxString::kIngersoll;
        case GcodeSyntax::kKraussMaffei:
            return PRS::SyntaxString::kKraussMaffei;
        case GcodeSyntax::kMarlin:
            return PRS::SyntaxString::kMarlin;
        case GcodeSyntax::kMarlinPellet:
            return PRS::SyntaxString::kMarlinPellet;
        case GcodeSyntax::kMazak:
            return PRS::SyntaxString::kMazak;
        case GcodeSyntax::kMeld:
            return PRS::SyntaxString::kMeld;
        case GcodeSyntax::kMeltio:
            return PRS::SyntaxString::kMeltio;
        case GcodeSyntax::kMVP:
            return PRS::SyntaxString::kMVP;
        case GcodeSyntax::kOkuma:
            return PRS::SyntaxString::kOkuma;
        case GcodeSyntax::kORNL:
            return PRS::SyntaxString::kORNL;
        case GcodeSyntax::kRomiFanuc:
            return PRS::SyntaxString::kRomiFanuc;
        case GcodeSyntax::kRPBF:
            return PRS::SyntaxString::kRPBF;
        case GcodeSyntax::kSandia:
            return PRS::SyntaxString::kSandia;
        case GcodeSyntax::kSiemens:
            return PRS::SyntaxString::kSiemens;
        case GcodeSyntax::kSkyBaam:
            return PRS::SyntaxString::kSkyBaam;
        case GcodeSyntax::kThermwood:
            return PRS::SyntaxString::kThermwood;
        case GcodeSyntax::kTormach:
            return PRS::SyntaxString::kTormach;
        case GcodeSyntax::kWolf:
            return PRS::SyntaxString::kWolf;
        case GcodeSyntax::kRepRap:
            return PRS::SyntaxString::kRepRap;
        case GcodeSyntax::kMach4:
            return PRS::SyntaxString::kMach4;
        case GcodeSyntax::kAeroBasic:
            return PRS::SyntaxString::kAeroBasic;
        case GcodeSyntax::kAdamantine:
            return PRS::SyntaxString::kAdamantine;
        case GcodeSyntax::kORNLMetric:
            return PRS::SyntaxString::kORNLMetric;
        default:
            return PRS::SyntaxString::kCommon;
    }
}
/*!
 * \enum InfillPatterns
 * \brief The InfillPatterns enum
 */
enum class InfillPatterns : uint8_t {
    kLines = 0,
    kGrid = 1,
    kConcentric = 2,
    kInsideOutConcentric = 3,
    kTriangles = 4,
    kHexagonsAndTriangles = 5,
    kHoneycomb = 6,
    kRadialHatch = 7
};

//! \brief Function for going from json to InfillPatterns
void to_json(json& j, const InfillPatterns& i);

//! \brief Function for going from InfillPatterns to json
void from_json(const json& j, InfillPatterns& i);

inline QString toString(InfillPatterns infill_type) {
    switch (infill_type) {
        case InfillPatterns::kLines:
            return Constants::InfillPatternTypeStrings::kLines;
        case InfillPatterns::kGrid:
            return Constants::InfillPatternTypeStrings::kGrid;
        case InfillPatterns::kConcentric:
            return Constants::InfillPatternTypeStrings::kConcentric;
        case InfillPatterns::kInsideOutConcentric:
            return Constants::InfillPatternTypeStrings::kInsideOutConcentric;
        case InfillPatterns::kTriangles:
            return Constants::InfillPatternTypeStrings::kTriangles;
        case InfillPatterns::kHexagonsAndTriangles:
            return Constants::InfillPatternTypeStrings::kHexagonsAndTriangles;
        case InfillPatterns::kHoneycomb:
            return Constants::InfillPatternTypeStrings::kHoneycomb;
        case InfillPatterns::kRadialHatch:
            return Constants::InfillPatternTypeStrings::kRadialHatch;
    }
}

/*!
 * \enum RegionType
 * \brief Types of regions, used to lookup when dealing with abstract region.
 */
enum class RegionType : int {
    kUnknown = 0,
    kPerimeter = 1 << 0,
    kInset = 1 << 1,
    kInfill = 1 << 2,
    kSkin = 1 << 3,
    kSkirt,
    kBrim,
    kRaft,
    kSupport,
    kSupportRoof,
    kLaserScan,
    kThermalScan,
    kSkeleton,
    kAnchor
};

enum class SkeletonInput : int { kSegments, kPoints };

//! \enum SkeletonFilter
//! \brief Types of filters for adaptive skeleton bead widths
enum class SkeletonFilter : int { kClamp, kPrune };

//! \brief Function for going from json to SkinInfillPatterns
void to_json(json& j, const SkeletonInput& i);

//! \brief Function for going from SkinInfillPatterns to json
void from_json(const json& j, SkeletonInput& i);

/*!
 * \enum StepType
 * \brief Types of steps, used to lookup when dealing with abstract steps.
 */
enum class StepType : uint8_t { kAll = 0, kLayer = 1, kRaft = 2, kScan = 4 };

inline StepType operator|(StepType a, StepType b) {
    return static_cast<StepType>(static_cast<int>(a) | static_cast<int>(b));
}

inline constexpr RegionType operator|(const RegionType& lhs, const RegionType& rhs) {
    return static_cast<RegionType>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

inline RegionType& operator|=(RegionType& lhs, const RegionType& rhs) { return lhs = lhs | rhs; }

inline constexpr RegionType operator&(const RegionType& lhs, const RegionType& rhs) {
    return static_cast<RegionType>(static_cast<int>(lhs) & static_cast<int>(rhs));
}

inline RegionType& operator&=(RegionType& lhs, const RegionType& rhs) { return lhs = lhs & rhs; }

inline RegionType fromString(QString type) {
    if (type == Constants::RegionTypeStrings::kUnknown) {
        return RegionType::kUnknown;
    }
    else if (type == Constants::RegionTypeStrings::kPerimeter) {
        return RegionType::kPerimeter;
    }
    else if (type == Constants::RegionTypeStrings::kInset) {
        return RegionType::kInset;
    }
    else if (type == Constants::RegionTypeStrings::kSkin) {
        return RegionType::kSkin;
    }
    else if (type == Constants::RegionTypeStrings::kInfill) {
        return RegionType::kInfill;
    }
    else if (type == Constants::RegionTypeStrings::kSupport) {
        return RegionType::kSupport;
    }
    else if (type == Constants::RegionTypeStrings::kSupportRoof) {
        return RegionType::kSupportRoof;
    }
    else if (type == Constants::RegionTypeStrings::kRaft) {
        return RegionType::kRaft;
    }
    else if (type == Constants::RegionTypeStrings::kBrim) {
        return RegionType::kBrim;
    }
    else if (type == Constants::RegionTypeStrings::kSkirt) {
        return RegionType::kSkirt;
    }
    else if (type == Constants::RegionTypeStrings::kLaserScan) {
        return RegionType::kLaserScan;
    }
    else if (type == Constants::RegionTypeStrings::kThermalScan) {
        return RegionType::kThermalScan;
    }
    else if (type == Constants::RegionTypeStrings::kSkeleton) {
        return RegionType::kSkeleton;
    }
    throw UnknownRegionTypeException("Cannot convert this string to RegionType");
}

inline QString toString(RegionType region_type) {
    switch (region_type) {
        case RegionType::kUnknown:
            return Constants::RegionTypeStrings::kUnknown;
        case RegionType::kPerimeter:
            return Constants::RegionTypeStrings::kPerimeter;
        case RegionType::kInset:
            return Constants::RegionTypeStrings::kInset;
        case RegionType::kSkin:
            return Constants::RegionTypeStrings::kSkin;
        case RegionType::kInfill:
            return Constants::RegionTypeStrings::kInfill;
        case RegionType::kSupport:
            return Constants::RegionTypeStrings::kSupport;
        case RegionType::kSupportRoof:
            return Constants::RegionTypeStrings::kSupportRoof;
        case RegionType::kRaft:
            return Constants::RegionTypeStrings::kRaft;
        case RegionType::kBrim:
            return Constants::RegionTypeStrings::kBrim;
        case RegionType::kSkirt:
            return Constants::RegionTypeStrings::kSkirt;
        case RegionType::kLaserScan:
            return Constants::RegionTypeStrings::kLaserScan;
        case RegionType::kThermalScan:
            return Constants::RegionTypeStrings::kThermalScan;
        case RegionType::kSkeleton:
            return Constants::RegionTypeStrings::kSkeleton;
        case RegionType::kAnchor:
            return Constants::RegionTypeStrings::kAnchor;
    }
    return QString();
}

/*!
 * \enum PathModifiers
 * \brief The PathModifiers enum
 */
enum class PathModifiers : uint16_t {
    kNone = 0,
    kReverseTipWipe = 1 << 0,
    kForwardTipWipe = 1 << 1,
    kPerimeterTipWipe = 1 << 2, // Rename to not include perimeter
    kAngledTipWipe = 1 << 3,
    kInitialStartup = 1 << 4,
    kSlowDown = 1 << 5,
    kCoasting = 1 << 6,
    kPrestart = 1 << 7,
    kSpiralLift = 1 << 8,
    kRampingUp = 1 << 9,
    kRampingDown = 1 << 10,
    kLeadIn = 1 << 11,
    kFlyingStart = 1 << 12
};

enum class TipWipeDirection { kOptimal = 0, kForward = 1, kReverse = 2, kAngled = 3 };

inline constexpr PathModifiers operator|(const PathModifiers& lhs, const PathModifiers& rhs) {
    return static_cast<PathModifiers>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

inline PathModifiers& operator|=(PathModifiers& lhs, const PathModifiers& rhs) { return lhs = lhs | rhs; }

inline constexpr PathModifiers operator&(const PathModifiers& lhs, const PathModifiers& rhs) {
    return static_cast<PathModifiers>(static_cast<int>(lhs) & static_cast<int>(rhs));
}

inline PathModifiers& operator&=(PathModifiers& lhs, const PathModifiers& rhs) { return lhs = lhs & rhs; }

inline QString toString(PathModifiers modifier_type) {
    switch (modifier_type) {
        case PathModifiers::kNone:
            return "None";
        case PathModifiers::kReverseTipWipe:
            return Constants::PathModifierStrings::kReverseTipWipe;
        case PathModifiers::kForwardTipWipe:
            return Constants::PathModifierStrings::kForwardTipWipe;
        case PathModifiers::kAngledTipWipe:
            return Constants::PathModifierStrings::kAngledTipWipe;
        case PathModifiers::kInitialStartup:
            return Constants::PathModifierStrings::kInitialStartup;
        case PathModifiers::kSlowDown:
            return Constants::PathModifierStrings::kSlowDown;
        case PathModifiers::kCoasting:
            return Constants::PathModifierStrings::kCoasting;
        case PathModifiers::kPrestart:
            return Constants::PathModifierStrings::kPrestart;
        case PathModifiers::kSpiralLift:
            return Constants::PathModifierStrings::kSpiralLift;
        case PathModifiers::kRampingUp:
            return Constants::PathModifierStrings::kRampingUp;
        case PathModifiers::kRampingDown:
            return Constants::PathModifierStrings::kRampingDown;
        case PathModifiers::kLeadIn:
            return Constants::PathModifierStrings::kLeadIn;
        case PathModifiers::kFlyingStart:
            return Constants::PathModifierStrings::kFlyingStart;
        case PathModifiers::kPerimeterTipWipe:
            return Constants::PathModifierStrings::kPerimeterTipWipe;
    }
    return QString();
}

/*!
 * \enum  IslandOrderOptimization
 * \brief The Path/ Island OrderOptimization enum
 */
enum class IslandOrderOptimization : uint8_t {
    kNextClosest = 0,
    kNextFarthest = 1,
    kShortestDistanceApprox = 2,
    kShortestDistanceBrute = 3,
    kLeastRecentlyVisited = 4,
    kRandom = 5,
    kCustomPoint = 6
};

enum class PathOrderOptimization : uint8_t {
    kNextClosest = 0,
    kNextFarthest = 1,
    kRandom = 2,
    kOutsideIn = 3,
    kInsideOut = 4,
    kCustomPoint = 5
};

enum class PointOrderOptimization : uint8_t {
    kNextClosest = 0,
    kNextFarthest = 1,
    kRandom = 2,
    kConsecutive = 3,
    kCustomPoint = 4
};

//! \brief Function for going from json to OrderOptimization
void to_json(json& j, const IslandOrderOptimization& i);

//! \brief Function for going from OrderOptimization to json
void from_json(const json& j, IslandOrderOptimization& i);

//! \brief Function for going from json to OrderOptimization
void to_json(json& j, const PathOrderOptimization& i);

//! \brief Function for going from OrderOptimization to json
void from_json(const json& j, PathOrderOptimization& i);

enum class Axis : uint8_t { kX, kY, kZ };

enum class IslandType : uint8_t {
    kAll,
    kBrim,
    kPolymer,
    kRaft,
    kLaserScan,
    kThermalScan,
    kSkirt,
    kSupport,
    kPowderSector,
    kWireFeed,
    kAnchor
};

enum class MachineType : uint8_t { kPellet, kFilament, kWire_Arc, kLaser_Wire, kConcrete, kThermoset };

enum class PrintMaterial : uint8_t {
    kABS20CF = 0,
    kABS = 1,
    kPPS = 2,
    kPPS50CF = 3,
    kPPSU = 4,
    kPPSU25CF = 5,
    kPESU = 6,
    kPESU25CF = 7,
    kPLA = 8,
    kConcrete = 9,
    kOther = 10
};

// ToDo: QMap might be preferred
inline QString toString(PrintMaterial material) {
    switch (material) {
        case PrintMaterial::kABS20CF:
            return "ABS 20% CF";
        case PrintMaterial::kABS:
            return "ABS";
        case PrintMaterial::kPPS:
            return "PPS";
        case PrintMaterial::kPPS50CF:
            return "PPS 50% CF";
        case PrintMaterial::kPPSU:
            return "PPSU";
        case PrintMaterial::kPPSU25CF:
            return "PPSU 25% CF";
        case PrintMaterial::kPESU:
            return "PESU";
        case PrintMaterial::kPESU25CF:
            return "PESU 25% CF";
        case PrintMaterial::kPLA:
            return "PLA";
        case PrintMaterial::kConcrete:
            return "Concrete";
        case PrintMaterial::kOther:
            return "Other";
        default:
            return "Other";
    }
}

// ToDo: QMap? Also let FlowrateCalc class use this code
inline Density toDensityValue(PrintMaterial material) {
    Density unit = lbm / (inch * inch * inch);
    switch (material) {
        case PrintMaterial::kABS20CF:
            return 0.041185 * unit;
        case PrintMaterial::kABS:
            return 0.03865 * unit;
        case PrintMaterial::kPPS:
            return 0.04877 * unit;
        case PrintMaterial::kPPS50CF:
            return 0.0552 * unit;
        case PrintMaterial::kPPSU:
            return 0.0466 * unit;
        case PrintMaterial::kPPSU25CF:
            return 0.0499 * unit;
        case PrintMaterial::kPESU:
            return 0.0494 * unit;
        case PrintMaterial::kPESU25CF:
            return 0.0532 * unit;
        case PrintMaterial::kPLA:
            return 0.0452 * unit;
        case PrintMaterial::kConcrete:
            return 0.0941 * unit;
        case PrintMaterial::kOther:
            return 0;
        default:
            return 0;
    }
}

enum class LayerChange : uint8_t { kZ_only = 0, kW_only = 1, kBoth_Z_and_W = 2 };

enum class SeamSelection : uint8_t { kRandom, kOptimized, kRotating };

enum class PrintDirection : uint8_t { kReverse_off, kReverse_All_Layers, kReverse_Alternating_Layers };

enum class ForceMinimumLayerTime : uint8_t { kUse_Purge_Dwells, kSlow_Feedrate };

enum class PreferenceChoice : uint8_t { kAsk = 0, kPerformAutomatically = 1, kSkipAutomatically = 2 };

enum class VisualizationColors {
    kBrim = 0,
    kCoasting,
    kInfill,
    kInitialStartup,
    kInset,
    kLaserScan,
    kLeadIn,
    kFlyingStart,
    kPerimeter,
    kPrestart,
    kRaft,
    kRampingDown,
    kRampingUp,
    kSkeleton,
    kSkin,
    kSkirt,
    kSlowDown,
    kSpiralLift,
    kSupport,
    kSupportRoof,
    kThermalScan,
    kTipWipeAngled,
    kTipWipeForward,
    kTipWipeReverse,
    kTravel,
    kUnknown,

    Length
};

inline QString VisualizationColorsName(VisualizationColors color) {
    switch (color) {
        case VisualizationColors::kBrim:
            return "Brim";
        case VisualizationColors::kCoasting:
            return "Coasting";
        case VisualizationColors::kInfill:
            return "Infill";
        case VisualizationColors::kInitialStartup:
            return "InitialStartup";
        case VisualizationColors::kInset:
            return "Inset";
        case VisualizationColors::kLaserScan:
            return "LaserScan";
        case VisualizationColors::kLeadIn:
            return "LeadIn";
        case VisualizationColors::kFlyingStart:
            return "FlyingStart";
        case VisualizationColors::kPerimeter:
            return "Perimeter";
        case VisualizationColors::kPrestart:
            return "Prestart";
        case VisualizationColors::kRaft:
            return "Raft";
        case VisualizationColors::kRampingDown:
            return "RampingDown";
        case VisualizationColors::kRampingUp:
            return "RampingUp";
        case VisualizationColors::kSkeleton:
            return "Skeleton";
        case VisualizationColors::kSkin:
            return "Skin";
        case VisualizationColors::kSkirt:
            return "Skirt";
        case VisualizationColors::kSlowDown:
            return "SlowDown";
        case VisualizationColors::kSpiralLift:
            return "SpiralLift";
        case VisualizationColors::kSupport:
            return "Support";
        case VisualizationColors::kSupportRoof:
            return "SupportRoof";
        case VisualizationColors::kThermalScan:
            return "ThermalScan";
        case VisualizationColors::kTipWipeAngled:
            return "TipWipeAngled";
        case VisualizationColors::kTipWipeForward:
            return "TipWipeForward";
        case VisualizationColors::kTipWipeReverse:
            return "TipWipeReverse";
        case VisualizationColors::kTravel:
            return "Travel";

        case VisualizationColors::kUnknown:
        case VisualizationColors::Length:
            return "Unknown";

        default:
            QMessageBox::critical(
                Q_NULLPTR, QCoreApplication::applicationName(),
                "Unimplemented corosponding visualization colors string.\n"
                "With a new enum entry for color, a corrosponding name (in VisualizationColorsName) and\n"
                "default color value (in VisualizationColorsDefaults) needs to be created.",
                QMessageBox::Cancel);
            throw std::invalid_argument("Unimplemented corosponding visualization colors string");
    }
}

inline constexpr const QColor VisualizationColorsDefaults(VisualizationColors color) {
    switch (color) {
        case VisualizationColors::kBrim:
            return QColor(200, 113, 55, 255);
        case VisualizationColors::kCoasting:
            return QColor(211, 95, 141, 255);
        case VisualizationColors::kInfill:
            return QColor(0, 255, 0, 255);
        case VisualizationColors::kInitialStartup:
            return QColor(135, 222, 205, 255);
        case VisualizationColors::kInset:
            return QColor(0, 204, 255, 255);
        case VisualizationColors::kLaserScan:
            return QColor(90, 255, 90, 255);
        case VisualizationColors::kLeadIn:
            return QColor(255, 153, 51, 255);
        case VisualizationColors::kFlyingStart:
            return QColor(120, 150, 250);
        case VisualizationColors::kPerimeter:
            return QColor(0, 0, 255, 255);
        case VisualizationColors::kPrestart:
            return QColor(204, 0, 255, 255);
        case VisualizationColors::kRaft:
            return QColor(102, 102, 102, 255);
        case VisualizationColors::kRampingDown:
            return QColor(22, 99, 137, 255);
        case VisualizationColors::kRampingUp:
            return QColor(99, 22, 137, 255);
        case VisualizationColors::kSkeleton:
            return QColor(160, 44, 44, 255);
        case VisualizationColors::kSkin:
            return QColor(0, 128, 0, 255);
        case VisualizationColors::kSkirt:
            return QColor(211, 188, 95, 255);
        case VisualizationColors::kSlowDown:
            return QColor(44, 160, 137, 255);
        case VisualizationColors::kSpiralLift:
            return QColor(113, 55, 200, 255);
        case VisualizationColors::kSupport:
            return QColor(255, 102, 0, 255);
        case VisualizationColors::kSupportRoof:
            return QColor(255, 179, 128, 255);
        case VisualizationColors::kThermalScan:
            return QColor(240, 130, 130, 255);
        case VisualizationColors::kTipWipeAngled:
            return QColor(179, 128, 255, 255);
        case VisualizationColors::kTipWipeForward:
            return QColor(179, 128, 255, 255);
        case VisualizationColors::kTipWipeReverse:
            return QColor(179, 128, 255, 255);
        case VisualizationColors::kTravel:
            return QColor(233, 175, 198, 255);

        case VisualizationColors::kUnknown:
        case VisualizationColors::Length:
            return QColor(0, 0, 0, 255);

        default:
            QMessageBox::critical(
                Q_NULLPTR, QCoreApplication::applicationName(),
                "Unimplemented corosponding visualization default color.\n"
                "With a new enum entry for color, a corrosponding name (in VisualizationColorsName) and\n"
                "default color value (in VisualizationColorsDefaults) needs to be created.",
                QMessageBox::Cancel);
            throw std::invalid_argument("Unimplemented corosponding visualization default color");
    }
}

enum class SegmentDisplayType : uint8_t {
    kNone = 0x00,
    kLine = 1 << 0,
    kTravel = 1 << 1,
    kSupport = 1 << 2,
    kAll = 0xff
};

inline constexpr SegmentDisplayType operator|(const SegmentDisplayType& lhs, const SegmentDisplayType& rhs) {
    return static_cast<SegmentDisplayType>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

inline constexpr SegmentDisplayType operator&(const SegmentDisplayType& lhs, const SegmentDisplayType& rhs) {
    return static_cast<SegmentDisplayType>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
}

inline constexpr SegmentDisplayType operator~(const SegmentDisplayType& lhs) {
    return static_cast<SegmentDisplayType>(~static_cast<uint8_t>(lhs));
}

inline SegmentDisplayType& operator|=(SegmentDisplayType& lhs, const SegmentDisplayType& rhs) {
    return lhs = lhs | rhs;
}

inline SegmentDisplayType& operator&=(SegmentDisplayType& lhs, const SegmentDisplayType& rhs) {
    return lhs = lhs & rhs;
}

enum class TravelLiftType : uint8_t { kBoth = 0, kLiftUpOnly = 1, kLiftLowerOnly = 2, kNoLift = 3 };

inline constexpr TravelLiftType operator|(const TravelLiftType& lhs, const TravelLiftType& rhs) {
    return static_cast<TravelLiftType>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

inline TravelLiftType& operator|=(TravelLiftType& lhs, const TravelLiftType& rhs) { return lhs = lhs | rhs; }

inline constexpr TravelLiftType operator&(const TravelLiftType& lhs, const TravelLiftType& rhs) {
    return static_cast<TravelLiftType>(static_cast<int>(lhs) & static_cast<int>(rhs));
}

inline TravelLiftType& operator&=(TravelLiftType& lhs, const TravelLiftType& rhs) { return lhs = lhs & rhs; }

enum class StatusUpdateStepType : uint8_t {
    kPreProcess = 0,
    kCompute = 1,
    kPostProcess = 2,
    kGcodeGeneraton = 3,
    kGcodeParsing = 4,
    kVisualization = 5,
    kRealTimeLayerCompleted = 6,
};

inline QString toString(StatusUpdateStepType statusType) {
    switch (statusType) {
        case StatusUpdateStepType::kPreProcess:
            return "Pre-Process:";
        case StatusUpdateStepType::kCompute:
            return "Compute:";
        case StatusUpdateStepType::kPostProcess:
            return "Post-Process:";
        case StatusUpdateStepType::kGcodeGeneraton:
            return "G-Code Generation:";
        case StatusUpdateStepType::kGcodeParsing:
            return "G-Code Parsing:";
        case StatusUpdateStepType::kVisualization:
            return "Visualization:";
        case StatusUpdateStepType::kRealTimeLayerCompleted:
            return "Layers completed:";
    }
}

enum class QuaternionOrder { kXYZ = 0, kZYX = 1 };

enum class RotationUnit { kPitchRollYaw = 0, kXYZ = 1 };

enum class LayerOrdering : uint8_t { kByHeight = 0, kByLayerNumber = 1, kByPart = 2 };

enum class NozzleAssignmentMethod : uint8_t { kXLocation = 0, kYLocation = 1, kArea = 2 };

enum class TormachMode : uint8_t { kMode21 = 0, kMode40 = 1, kMode102 = 2, kMode274 = 3, kMode509 = 4 };

enum class PolygonPartition : uint8_t { kConvex = 0, kMonoX = 1, kMonoY = 2 };

} // namespace ORNL
