#ifndef CONSTANTS_H
#define CONSTANTS_H

//! \file constants.h

#include "units/unit.h"

#include <QColor>
#include <QHash>
#include <QVector3D>
#include <QVector>
#include <string>

namespace ORNL {
/*!
 * \class Constants
 * \brief Class that holds all static constants
 */
class Constants {
  public:
    /*!
     * \class Units
     * \brief Units strings used for preferences
     */
    class Units {
      public:
        static const QString kInch;
        static const QString kInchPerSec;
        static const QString kInchPerMin;
        static const QString kInchPerSec2;
        static const QString kInchPerSec3;

        static const QString kFeet;
        static const QString kFeetPerSec;
        static const QString kFeetPerSec2;
        static const QString kFeetPerSec3;

        static const QString kMm;
        static const QString kMmPerSec;
        static const QString kMmPerMin;
        static const QString kMmPerSec2;
        static const QString kMmPerSec3;

        static const QString kCm;
        static const QString kCmPerSec;
        static const QString kCmPerSec2;
        static const QString kCmPerSec3;

        static const QString kM;
        static const QString kMPerSec;
        static const QString kMPerSec2;
        static const QString kMPerSec3;

        static const QString kMicron;
        static const QString kTensOfMicrons;
        static const QString kMicronPerSec;
        static const QString kMicronPerSec2;
        static const QString kMicronPerSec3;

        static const QString kDegree;
        static const QString kRadian;
        static const QString kRevolution;

        static const QString kSecond;
        static const QString kMillisecond;
        static const QString kMinute;

        static const QString kKg;
        static const QString kG;
        static const QString kGPerCm3;
        static const QString kMg;
        static const QString kLb;
        static const QString kLbPerInch3;

        static const QString kCelsius;
        static const QString kFahrenheit;
        static const QString kKelvin;

        static const QString kmuV;
        static const QString kmV;
        static const QString kV;

        static const QString kPitchRollYaw;
        static const QString kXYZ;

        static const QStringList kDistanceUnits;
        static const QStringList kVelocityUnits;
        static const QStringList kAccelerationUnits;
        static const QStringList kDensityUnits;
        static const QStringList kTemperatureUnits;
        static const QStringList kMassUnits;
        static const QStringList kJerkUnits;
        static const QStringList kAngleUnits;
        static const QStringList kTimeUnits;
        static const QStringList kVoltageUnits;
        static const QStringList kRotationUnits;
    };

    class RegionTypeStrings {
      public:
        static const QString kUnknown;
        static const QString kPerimeter;
        static const QString kInset;
        static const QString kInfill;
        static const QString kTopSkin;
        static const QString kBottomSkin;
        static const QString kSkin;
        static const QString kSupport;
        static const QString kSupportRoof;
        static const QString kTravel;
        static const QString kRaft;
        static const QString kBrim;
        static const QString kSkirt;
        static const QString kLaserScan;
        static const QString kThermalScan;
        static const QString kSkeleton;
        static const QString kAnchor;
    };

    // Used?
    class LegacyRegionTypeStrings {
      public:
        static const QString kThing;
    };

    // Used?
    class InfillPatternTypeStrings {
      public:
        static const QString kLines;
        static const QString kGrid;
        static const QString kConcentric;
        static const QString kInsideOutConcentric;
        static const QString kTriangles;
        static const QString kHexagonsAndTriangles;
        static const QString kHoneycomb;
        static const QString kRadialHatch;

        static const QStringList kInfillTypes;
    };

    // Used?
    class OrderOptimizationTypeStrings {
      public:
        static const QString kShortestTime;
        static const QString kShortestDistance;
        static const QString kLargestDistance;
        static const QString kLeastRecentlyVisited;
        static const QString kNextClosest;
        static const QString kApproximateShortest;
        static const QString kShortestDistance_DP;
        static const QString kRandom;
        static const QString kConsecutive;

        static const QStringList kOrderOptimizationTypes;
    };

    class PathModifierStrings {
      public:
        static const QString kPrestart;
        static const QString kInitialStartup;
        static const QString kSlowDown;
        static const QString kForwardTipWipe;
        static const QString kReverseTipWipe;
        static const QString kAngledTipWipe;
        static const QString kCoasting;
        static const QString kSpiralLift;
        static const QString kRampingUp;
        static const QString kRampingDown;
        static const QString kLeadIn;
        static const QString kFlyingStart;
        static const QString kPerimeterTipWipe;
    };

    class PrinterSettings {
      public:
        class MachineSetup {
          public:
            static const QString kSyntax;
            static const QString kMachineType;
            static const QString kForceG1;
            static const QString kSupportG3;
            static const QString kAxisA;
            static const QString kAxisB;
            static const QString kAxisC;
            static const QString kToolCoordinate;
            static const QString kBaseCoordinate;
            static const QString kSupportsE1;
            static const QString kSupportsE2;
        };

        // Categories
        /*!
         * \class Dimensions
         *
         * \brief Keys for machine dimensions
         */
        class Dimensions {
          public:
            static const QString kBuildVolumeType;
            static const QString kXMin;
            static const QString kXMax;
            static const QString kYMin;
            static const QString kYMax;
            static const QString kZMin;
            static const QString kZMax;
            static const QString kInnerRadius;
            static const QString kOuterRadius;
            static const QString kXOffset;
            static const QString kYOffset;
            static const QString kZOffset;
            static const QString kUseVariableForZ;
            static const QString kEnableW;
            static const QString kWMin;
            static const QString kWMax;
            static const QString kInitialW;
            static const QString kLayerChangeAxis;
            static const QString kEnableDoffing;
            static const QString kDoffingHeight;
            static const QString kPurgeX;
            static const QString kPurgeY;
            static const QString kPurgeZ;
            static const QString kEnableGridX;
            static const QString kGridXDistance;
            static const QString kGridXOffset;
            static const QString kEnableGridY;
            static const QString kGridYDistance;
            static const QString kGridYOffset;
        };

        class Auxiliary {
          public:
            static const QString kEnableTamper;
            static const QString kTamperVoltage;
            static const QString kGKNLaserPower;
            static const QString kGKNWireSpeed;
        };

        class MachineSpeed {
          public:
            static const QString kMinXYSpeed;
            static const QString kMaxXYSpeed;
            static const QString kMaxExtruderSpeed;
            static const QString kWTableSpeed;
            static const QString kZSpeed;
            static const QString kGKNPrintSpeed;
            static const QString kGearRatio;
        };

        class Acceleration {
          public:
            static const QString kEnableDynamic;
            static const QString kDefault;
            static const QString kPerimeter;
            static const QString kInset;
            static const QString kSkin;
            static const QString kInfill;
            static const QString kSkeleton;
            static const QString kSupport;
        };

        class GCode {
          public:
            static const QString kEnableStartupCode;
            static const QString kEnableMaterialLoad;
            static const QString kEnableWaitForUser;
            static const QString kEnableBoundingBox;
            static const QString kStartCode;
            static const QString kLayerCodeChange;
            static const QString kEndCode;
            static const QString kRemoveComments;
        };

        //            /*!
        //             * \class SyntaxString
        //             *
        //             * \brief Keys for machine syntax
        //             */
        class SyntaxString {
          public:
            static QString kAML3D;
            static QString k5AxisMarlin;
            static QString kBeam;
            static QString kCincinnati;
            static QString kCincinnatiLegacy;
            static QString kCommon;
            static QString kDmgDmu;
            static QString kGKN;
            static QString kGudel;
            static QString kHaasInch;
            static QString kHaasMetric;
            static QString kHaasMetricNoComments;
            static QString kHurco;
            static QString kIngersoll;
            static QString kKraussMaffei;
            static QString kMarlin;
            static QString kMarlinPellet;
            static QString kMazak;
            static QString kMeld;
            static QString kMeltio;
            static QString kMVP;
            static QString kOkuma;
            static QString kORNL;
            static QString kRomiFanuc;
            static QString kRPBF;
            static QString kSandia;
            static QString kSiemens;
            static QString kSkyBaam;
            static QString kThermwood;
            static QString kTormach;
            static QString kWolf;
            static QString kRepRap;
            static QString kMach4;
            static QString kAeroBasic;
            static QString kAdamantine;
        };
    };

    /*!
     * \class MaterialSettings
     * \brief Keys for material settings
     */
    class MaterialSettings {
      public:
        class Density {
          public:
            static const QString kMaterialType;
            static const QString kDensity;
        };

        class Startup {
          public:
            static const QString kPerimeterEnable;
            static const QString kPerimeterDistance;
            static const QString kPerimeterSpeed;
            static const QString kPerimeterExtruderSpeed;
            static const QString kPerimeterRampUpEnable;
            static const QString kPerimeterSteps;

            static const QString kInsetEnable;
            static const QString kInsetDistance;
            static const QString kInsetSpeed;
            static const QString kInsetExtruderSpeed;
            static const QString kInsetRampUpEnable;
            static const QString kInsetSteps;

            static const QString kSkinEnable;
            static const QString kSkinDistance;
            static const QString kSkinSpeed;
            static const QString kSkinExtruderSpeed;
            static const QString kSkinRampUpEnable;
            static const QString kSkinSteps;

            static const QString kInfillEnable;
            static const QString kInfillDistance;
            static const QString kInfillSpeed;
            static const QString kInfillExtruderSpeed;
            static const QString kInfillRampUpEnable;
            static const QString kInfillSteps;

            static const QString kSkeletonEnable;
            static const QString kSkeletonDistance;
            static const QString kSkeletonSpeed;
            static const QString kSkeletonExtruderSpeed;
            static const QString kSkeletonRampUpEnable;
            static const QString kSkeletonSteps;

            static const QString kStartUpAreaModifier;
        };

        class Slowdown {
          public:
            static const QString kPerimeterEnable;
            static const QString kPerimeterDistance;
            static const QString kPerimeterLiftDistance;
            static const QString kPerimeterSpeed;
            static const QString kPerimeterExtruderSpeed;
            static const QString kPerimeterCutoffDistance;

            static const QString kInsetEnable;
            static const QString kInsetDistance;
            static const QString kInsetLiftDistance;
            static const QString kInsetSpeed;
            static const QString kInsetExtruderSpeed;
            static const QString kInsetCutoffDistance;

            static const QString kSkinEnable;
            static const QString kSkinDistance;
            static const QString kSkinLiftDistance;
            static const QString kSkinSpeed;
            static const QString kSkinExtruderSpeed;
            static const QString kSkinCutoffDistance;

            static const QString kInfillEnable;
            static const QString kInfillDistance;
            static const QString kInfillLiftDistance;
            static const QString kInfillSpeed;
            static const QString kInfillExtruderSpeed;
            static const QString kInfillCutoffDistance;

            static const QString kSkeletonEnable;
            static const QString kSkeletonDistance;
            static const QString kSkeletonLiftDistance;
            static const QString kSkeletonSpeed;
            static const QString kSkeletonExtruderSpeed;
            static const QString kSkeletonCutoffDistance;

            static const QString kSlowDownAreaModifier;
        };

        class TipWipe {
          public:
            static const QString kPerimeterEnable;
            static const QString kPerimeterDistance;
            static const QString kPerimeterSpeed;
            static const QString kPerimeterExtruderSpeed;
            static const QString kPerimeterDirection;
            static const QString kPerimeterAngle;
            static const QString kPerimeterCutoffDistance;
            static const QString kPerimeterLiftHeight;

            static const QString kInsetEnable;
            static const QString kInsetDistance;
            static const QString kInsetSpeed;
            static const QString kInsetExtruderSpeed;
            static const QString kInsetDirection;
            static const QString kInsetAngle;
            static const QString kInsetCutoffDistance;
            static const QString kInsetLiftHeight;

            static const QString kSkinEnable;
            static const QString kSkinDistance;
            static const QString kSkinSpeed;
            static const QString kSkinExtruderSpeed;
            static const QString kSkinDirection;
            static const QString kSkinAngle;
            static const QString kSkinCutoffDistance;
            static const QString kSkinLiftHeight;

            static const QString kInfillEnable;
            static const QString kInfillDistance;
            static const QString kInfillSpeed;
            static const QString kInfillExtruderSpeed;
            static const QString kInfillDirection;
            static const QString kInfillAngle;
            static const QString kInfillCutoffDistance;
            static const QString kInfillLiftHeight;

            static const QString kSkeletonEnable;
            static const QString kSkeletonDistance;
            static const QString kSkeletonSpeed;
            static const QString kSkeletonExtruderSpeed;
            static const QString kSkeletonDirection;
            static const QString kSkeletonAngle;
            static const QString kSkeletonCutoffDistance;
            static const QString kSkeletonLiftHeight;

            static const QString kLaserPowerMultiplier;
            static const QString kWireFeedMultiplier;
            static const QString kTipWipeVoltage;
        };

        class SpiralLift {
          public:
            static const QString kPerimeterEnable;
            static const QString kInsetEnable;
            static const QString kSkinEnable;
            static const QString kInfillEnable;
            static const QString kLayerEnable;
            static const QString kLiftHeight;
            static const QString kLiftRadius;
            static const QString kLiftSpeed;
            static const QString kLiftPoints;
        };

        class Purge {
          public:
            static const QString kInitialDuration;
            static const QString kInitialScrewRPM;
            static const QString kInitialTipWipeDelay;
            static const QString kEnablePurgeDwell;
            static const QString kPurgeDwellDuration;
            static const QString kPurgeDwellRPM;
            static const QString kPurgeDwellTipWipeDelay;
            static const QString kPurgeLength;
            static const QString kPurgeFeedrate;
        };

        class Extruder {
          public:
            static const QString kInitialSpeed;
            static const QString kExtruderPrimeVolume;
            static const QString kExtruderPrimeSpeed;
            static const QString kOnDelayPerimeter;
            static const QString kOnDelayInset;
            static const QString kOnDelaySkin;
            static const QString kOnDelayInfill;
            static const QString kOnDelaySkeleton;
            static const QString kOffDelay;
            static const QString kServoToTravelSpeed;
            static const QString kEnableM3S;
        };

        class Filament {
          public:
            static const QString kDiameter;
            static const QString kRelative;
            static const QString kDisableG92;
            static const QString kFilamentBAxis;
        };

        class Retraction {
          public:
            static const QString kEnable;
            static const QString kMinTravel;
            static const QString kLength;
            static const QString kSpeed;
            static const QString kOpenSpacesOnly;
            static const QString kLayerChange;
            static const QString kPrimeSpeed;
            static const QString kPrimeAdditionalLength;
        };

        class Temperatures {
          public:
            static const QString kBed;
            static const QString kTwoZones;
            static const QString kThreeZones;
            static const QString kFourZones;
            static const QString kFiveZones;
            static const QString kExtruder0;
            static const QString kExtruder1;
            static const QString kStandBy0;
            static const QString kStandBy1;
            static const QString kExtruder0Zone1;
            static const QString kExtruder0Zone2;
            static const QString kExtruder0Zone3;
            static const QString kExtruder0Zone4;
            static const QString kExtruder0Zone5;
            static const QString kExtruder1Zone1;
            static const QString kExtruder1Zone2;
            static const QString kExtruder1Zone3;
            static const QString kExtruder1Zone4;
            static const QString kExtruder1Zone5;
        };

        class Cooling {
          public:
            static const QString kEnable;
            static const QString kDisable;
            static const QString kDisableXLayers;
            static const QString kMinSpeed;
            static const QString kMaxSpeed;
            static const QString kForceMinLayerTime;
            static const QString kForceMinLayerTimeMethod;
            static const QString kMinLayerTime;
            static const QString kMaxLayerTime;
            static const QString kExtruderScaleFactor;
            static const QString kPrePauseCode;
            static const QString kPostPauseCode;
        };

        class PlatformAdhesion {
          public:
            static const QString kRaftEnable;
            static const QString kRaftOffset;
            static const QString kRaftLayers;
            static const QString kRaftBeadWidth;

            static const QString kBrimEnable;
            static const QString kBrimWidth;
            static const QString kBrimLayers;
            static const QString kBrimBeadWidth;

            static const QString kSkirtEnable;
            static const QString kSkirtLoops;
            static const QString kSkirtDistanceFromObject;
            static const QString kSkirtLayers;
            static const QString kSkirtMinLength;
            static const QString kSkirtBeadWidth;
        };

        class MultiMaterial {
          public:
            static const QString kEnable;
            static const QString kPerimterNum;
            static const QString kInsetNum;
            static const QString kSkinNum;
            static const QString kInfillNum;
            static const QString kTransitionDistance;
            static const QString kEnableSecondDistance;
            static const QString kSecondDistance;
            static const QString kUseM222;
        };
    };

    class ProfileSettings {
      public:
        /*!
         * \class Layer
         *
         * \brief Keys for layer settings
         */
        class Layer {
          public:
            static const QString kLayerHeight;
            static const QString kNozzleDiameter;
            static const QString kBeadWidth;
            static const QString kSpeed;
            static const QString kExtruderSpeed;
            static const QString kMinExtrudeLength;
        };

        class Perimeter {
          public:
            static const QString kEnable;
            static const QString kCount;
            static const QString kBeadWidth;
            static const QString kFirstLayerBeadWidth;
            static const QString kSpeed;
            static const QString kExtruderSpeed;
            static const QString kExtrusionMultiplier;
            static const QString kMinPathLength;
            static const QString kPower;
            static const QString kFocus;
            static const QString kSpotSize;
            static const QString kEnableLeadIn;
            static const QString kLeadInFirstLayerOnly;
            static const QString kEnableLeadInX;
            static const QString kEnableLeadInY;
            static const QString kEnableFlyingStart;
            static const QString kFlyingStartDistance;
            static const QString kFlyingStartSpeed;
            static const QString kEnableShiftedBeads;
        };

        class Inset {
          public:
            static const QString kEnable;
            static const QString kCount;
            static const QString kBeadWidth;
            static const QString kFirstLayerBeadWidth;
            static const QString kSpeed;
            static const QString kExtruderSpeed;
            static const QString kExtrusionMultiplier;
            static const QString kMinPathLength;
            static const QString kOverlap;
        };

        class Skeleton {
          public:
            static const QString kEnable;
            static const QString kSkeletonInput;
            static const QString kSkeletonInputCleaningDistance;
            static const QString kSkeletonInputChamferingAngle;
            static const QString kSkeletonOutputCleaningDistance;
            static const QString kBeadWidth;
            static const QString kSpeed;
            static const QString kExtruderSpeed;
            static const QString kExtrusionMultiplier;
            static const QString kSkeletonAdapt;
            static const QString kSkeletonAdaptStepSize;
            static const QString kSkeletonAdaptMinWidth;
            static const QString kSkeletonAdaptMinWidthFilter;
            static const QString kSkeletonAdaptMaxWidth;
            static const QString kSkeletonAdaptMaxWidthFilter;
            static const QString kMinPathLength;
            static const QString kUseSkinMcode;
        };

        class Skin {
          public:
            static const QString kEnable;
            static const QString kTopCount;
            static const QString kBottomCount;
            static const QString kPattern;
            static const QString kAngle;
            static const QString kAngleRotation;
            static const QString kBeadWidth;
            static const QString kSpeed;
            static const QString kExtruderSpeed;
            static const QString kExtrusionMultiplier;
            static const QString kOverlap;
            static const QString kMinPathLength;
            static const QString kPrestart;
            static const QString kPrestartDistance;
            static const QString kPrestartSpeed;
            static const QString kPrestartExtruderSpeed;
            static const QString kInfillEnable;
            static const QString kInfillSteps;
            static const QString kInfillPattern;
            static const QString kInfillAngle;
            static const QString kInfillRotation;
        };

        class Infill {
          public:
            static const QString kEnable;
            static const QString kLineSpacing;
            static const QString kDensity;
            static const QString kManualLineSpacing;
            static const QString kPattern;
            static const QString kPrintInfillEveryXLayers;
            static const QString kBasedOnPrinter;
            static const QString kAngle;
            static const QString kAngleRotation;
            static const QString kOverlap;
            static const QString kBeadWidth;
            static const QString kSpeed;
            static const QString kExtruderSpeed;
            static const QString kExtrusionMultiplier;
            static const QString kCombineXLayers;
            static const QString kMinPathLength;
            static const QString kPrestart;
            static const QString kPrestartDistance;
            static const QString kPrestartSpeed;
            static const QString kPrestartExtruderSpeed;
            static const QString kSectorCount;
            static const QString kPower;
            static const QString kFocus;
            static const QString kSpotSize;
            static const QString kEnableAlternatingLines;
        };

        class Support {
          public:
            static const QString kEnable;
            static const QString kPrintFirst;
            static const QString kTaper;
            static const QString kThresholdAngle;
            static const QString kXYDistance;
            static const QString kLayerOffset;
            static const QString kMinInfillArea;
            static const QString kMinArea;
            static const QString kPattern;
            static const QString kLineSpacing;
        };

        class Travel {
          public:
            static const QString kSpeed;
            static const QString kMinLength;
            static const QString kMinTravelForLift;
            static const QString kLiftHeight;
            static const QString kEnableTravelPause;
            static const QString kEnableTravelCentroidMove;
            static const QString kTravelPauseDuration;
        };

        class GCode {
          public:
            static const QString kPerimeterStart;
            static const QString kPerimeterEnd;
            static const QString kInsetStart;
            static const QString kInsetEnd;
            static const QString kSkeletonStart;
            static const QString kSkeletonEnd;
            static const QString kSkinStart;
            static const QString kSkinEnd;
            static const QString kInfillStart;
            static const QString kInfillEnd;
            static const QString kSupportStart;
            static const QString kSupportEnd;
        };

        class SpecialModes {
          public:
            static const QString kSmoothing;
            static const QString kSmoothingType;
            static const QString kSmoothingTolerance;
            static const QString kEnableSpiralize;
            static const QString kEnableFixModel;
            static const QString kEnableOversize;
            static const QString kOversizeDistance;
            static const QString kEnableWidthHeight;
        };

        class Optimizations {
          public:
            static const QString kEnableGPU;
            static const QString kIslandOrder;
            static const QString kPathOrder;
            static const QString kCustomIslandXLocation;
            static const QString kCustomIslandYLocation;
            static const QString kCustomPathXLocation;
            static const QString kCustomPathYLocation;
            static const QString kPointOrder;
            static const QString kLocalRandomnessEnable;
            static const QString kLocalRandomnessRadius;
            static const QString kMinDistanceEnabled;
            static const QString kMinDistanceThreshold;
            static const QString kConsecutiveDistanceThreshold;
            static const QString kCustomPointXLocation;
            static const QString kCustomPointYLocation;
            static const QString kEnableSecondCustomLocation;
            static const QString kEnableSecondCustomLocationEveryTwo;
            static const QString kCustomPointSecondXLocation;
            static const QString kCustomPointSecondYLocation;
        };

        class Ordering {
          public:
            static const QString kRegionOrder;
            static const QString kPerimeterReverseDirection;
            static const QString kInsetReverseDirection;
        };

        class LaserScanner {
          public:
            static const QString kLaserScanner;
            static const QString kSpeed;
            static const QString kLaserScannerHeightOffset;
            static const QString kLaserScannerXOffset;
            static const QString kLaserScannerYOffset;
            static const QString kLaserScannerHeight;
            static const QString kLaserScannerWidth;
            static const QString kLaserScannerStepDistance;
            static const QString kLaserScanLineResolution;
            static const QString kLaserScannerAxis;
            static const QString kInvertLaserScannerHead;
            static const QString kEnableBedScan;
            static const QString kScanLayerSkip;
            static const QString kEnableScannerBuffer;
            static const QString kBufferDistance;
            static const QString kTransmitHeightMap;
            static const QString kGlobalScan;
            static const QString kOrientationAxis;
            static const QString kOrientationAngle;
            static const QString kEnableOrientationDefinition;
            static const QString kOrientationA;
            static const QString kOrientationB;
            static const QString kOrientationC;
        };

        class ThermalScanner {
          public:
            static const QString kThermalScanner;
            static const QString kThermalScannerTemperatureCutoff;
            static const QString kThermalScannerXOffset;
            static const QString kThermalScannerYOffset;
            static const QString kPyrometerMove;
        };

        class SlicingVector {
          public:
            static const QString kSlicingVectorX;
            static const QString kSlicingVectorY;
            static const QString kSlicingVectorZ;
        };
    };

    class ExperimentalSettings {
      public:
        class PrinterConfig {
          public:
            static const QString kSlicerType;
            static const QString kLayerOrdering;
            static const QString kLayerGroupingTolerance;
        };

        class SinglePath {
          public:
            static const QString kEnableSinglePath;
            static const QString kEnableBridgeExclusion;
            static const QString kEnableZippering;
            static const QString kPrevLayerExclusionDistance;
            static const QString kCornerExclusionDistance;
            static const QString kMaxBridgeLength;
            static const QString kMinBridgeSeparation;
        };

        class RPBFSlicing {
          public:
            static const QString kSectorSize;
            static const QString kSectorOffsettingEnable;
            static const QString kSectorOverlap;
            static const QString kSectorStaggerEnable;
            static const QString kSectorStaggerAngle;
            static const QString kClockingAngle;
        };

        class MultiNozzle {
          public:
            static const QString kNozzleCount;
            static const QString kNozzleOffsetX;
            static const QString kNozzleOffsetY;
            static const QString kNozzleOffsetZ;
            static const QString kNozzleMaterial;
            static const QString kEnableMultiNozzleMultiMaterial;
            static const QString kEnableDuplicatePathRemoval;
            static const QString kDuplicatePathSimilarity;
            static const QString kEnableIndependentNozzles;
            static const QString kNozzleAssignmentMethod;
        };

        class GcodeVisualization {
          public:
            static const QString kDisableVisualization;
            static const QString kVisualizationSkip;
        };

        class Ramping {
          public:
            static const QString kTrajectoryAngleEnabled;
            static const QString kTrajectoryAngleThreshold;
            static const QString kTrajectoryAngleRampDownDistance;
            static const QString kTrajectoryAngleRampUpDistance;
            static const QString kTrajectoryAngleSpeedSlowDown;
            static const QString kTrajectoryAngleExtruderSpeedSlowDown;
            static const QString kTrajectoryAngleSpeedUp;
            static const QString kTrajectoryAngleExtruderSpeedUp;
        };

        class WireFeed {
          public:
            static const QString kWireFeedEnable;
            static const QString kSettingsRegionMeshSplit;
            static const QString kInitialTravelSpeed;
            static const QString kAnchorEnable;
            static const QString kAnchorWidth;
            static const QString kAnchorHeight;
            static const QString kAnchorObjectDistanceLeft;
            static const QString kAnchorObjectDistanceRight;
            static const QString kWireCutoffDistance;
            static const QString kWireStickoutDistance;
            static const QString kWirePrestartDistance;
        };

        class FileOutput {
          public:
            static const QString kMeldCompanionOutput;
            static const QString kMeldDiscrete;
            static const QString kTormachOutput;
            static const QString kTormachMode;
            static const QString kAML3DOutput;
            static const QString kAML3DWeaveLength;
            static const QString kAML3DWeaveWidth;
            static const QString kSandiaOutput;
            static const QString kSandiaMetalFile;
            static const QString kSandiaCVEL;
            static const QString kMarlinOutput;
            static const QString kMarlinTravels;
            static const QString kSimulationOutput;
        };

        class RotationOrigin {
          public:
            static const QString kXOffset;
            static const QString kYOffset;
        };

        class ImageResolution {
          public:
            static const QString kImageResolutionX;
            static const QString kImageResolutionY;
        };

        class CrossSection {
          public:
            static const QString kLargestGap;
            static const QString kMaxStitch;
        };
    };

    class Settings {
      public:
        // NOTE: All keys here are std::strings since they will primarily be used with nlohmann::json.
        class Master {
          public:
            static const std::string kDisplay;
            static const std::string kType;
            static const std::string kToolTip;
            static const std::string kMinor;
            static const std::string kMajor;
            static const std::string kOptions;
            static const std::string kDepends;
            static const std::string kDefault;
            static const std::string kDependencyGroup;
            static const std::string kLocal;
        };
        class Session {
          public:
            static const std::string kParts;
            static const std::string kName;
            static const std::string kTransform;
            static const std::string kTransforms;
            static const std::string kMeshType;
            static const std::string kGenType;
            static const std::string kOrgDims;
            static const std::string kFile;
            static const std::string kDir;
            class LocalFile {
              public:
                static const std::string kName;
                static const std::string kSettings;
                static const std::string kRanges;
            };
            class Range {
              public:
                static const std::string kLow;
                static const std::string kHigh;
                static const std::string kName;
                static const std::string kSettings;
            };
            class Files {
              public:
                static const std::string kSession;
                static const std::string kGlobal;
                static const std::string kLocal;
                static const std::string kPref;
                static const std::string kModel;
            };
        };

        class SettingTab {
          public:
            static const QString kPrinter;
            static const QString kMaterial;
            static const QString kProfile;
            static const QString kExperimental;
        };
    };

    //! \class SegmentSettings
    //! \brief The settings that segments use to define their output.
    class SegmentSettings {
      public:
        static const QString kHeight;
        static const QString kWidth;
        static const QString kSpeed;
        static const QString kAccel;
        static const QString kExtruderSpeed;
        static const QString kWaitTime;
        static const QString kRegionType;
        static const QString kPathModifiers;
        static const QString kMaterialNumber;
        static const QString kRotation;
        static const QString kRecipe;
        static const QString kTilt;
        static const QString kCCW;
        static const QString kESP;
        static const QString kExtruders;
        static const QString kIsRegionStartSegment;
        static const QString kWireFeed;
        static const QString kFinalWireCoast;
        static const QString kFinalWireFeed;
    };

    class Limits {
      public:
        /*!
         * \class Maximums
         * \brief Maximum values for various data types
         */
        class Maximums {
          public:
            static const Distance kMaxDistance;
            static const Velocity kMaxSpeed;
            static const Acceleration kMaxAccel;
            static const AngularVelocity kMaxAngVel;
            static const Time kMaxTime;
            static const Temperature kMaxTemperature;
            static const Angle kMaxAngle;
            static const Area kMaxArea;
            static const Voltage kMaxVoltage;
            static const float kMaxFloat;
            static const float kInfFloat;
        };

        /*!
         * \class Minimums
         * \brief Minimum values for various data types
         */
        class Minimums {
          public:
            static const Distance kMinDistance;
            static const Distance kMinLocation;
            static const Velocity kMinSpeed;
            static const Acceleration kMinAccel;
            static const AngularVelocity kMinAngVel;
            static const Time kMinTime;
            static const Temperature kMinTemperature;
            static const Angle kMinAngle;
            static const Area kMinArea;
            static const float kMinFloat;
        };
    };

    class Colors {
      public:
        static const QColor kYellow;
        static const QColor kRed;
        static const QColor kBlue;
        static const QColor kLightBlue;
        static const QColor kGreen;
        static const QColor kPurple;
        static const QColor kOrange;
        static const QColor kWhite;
        static const QColor kBlack;
        static const QVector<QColor> kModelColors;
    };
    /*!
     * \class UI
     * \brief Constants for UI widgets
     */
    class UI {
      public:
        class Common {
          public:
            class DropShadow {
              public:
                static const int kXOffset;
                static const int kYOffset;
                static const int kBlurRadius;
                static const QColor kColor;
            };
        };

        class MainWindow {
          public:
            static const QSize kWindowSize;
            static const QSize kViewWidgetSize;
            static const QSize kLayerbarMinSize;
            static const int kProgressBarWidth;
            static const int kStatusBarMaxHeight;

            class SideDock {
              public:
                static const int kSettingsWidth;
                static const int kGCodeWidth;
                static const int kLayerTimesWidth;
                static const int kExternalFileWidth;
            };

            class Margins {
              public:
                static const int kMainLayoutSpacing;
                static const int kMainContainerSpacing;
                static const int kMainLayout;
                static const int kMainContainer;
            };
        };

        class MainToolbar {
          public:
            static const int kMaxWidth;
            static const int kStartOffset;
            static const int kEndOffset;
            static const int kVerticalOffset;
        };

        class ViewControlsToolbar {
          public:
            static const int kHeight;
            static const int kWidth;
            static const int kRightOffset;
            static const int kBottomOffset;
        };

        class PartToolbar {
          public:
            static const int kHeight;
            static const int kWidth;
            static const int kLeftOffset;
            static const int kMinTopOffset;
            class Input {
              public:
                static const int kBoxWidth;
                static const int kBoxHeight;
                static const int kExtraButtonWidth;
                static const int kPrecision;
                static const int kAnimationInTime;
                static const int kAnimationOutTime;
            };
        };

        class PartControl {
          public:
            static const QSize kSize;
            static const int kLeftOffset;
            static const int kBottomOffset;
        };

        class Themes {
          public:
            static const QString kSystemMode;
            static const QString kDarkMode;
            static const QStringList kThemes;
        };
    };

    /*!
     * \class OpenGL
     * \brief The constants for the OpenGL widgets
     */
    class OpenGL {
      public:
        static const double kZoomDefault;
        static const double kZoomMax;
        static const double kZoomMin;
        static const double kTrackball;

        static const double kFov;
        static const double kNearPlane;
        static const double kFarPlane;

        static const float kObjectToView;
        static const float kViewToObject;

        class Shader {
          public:
            static const char* kVertShaderFile;
            static const char* kFragShaderFile;

            // Fragment shader - Uniform
            static const char* kLightingColorName;
            static const char* kLightingPositionName;
            static const char* kCameraPositionName;
            static const char* kAmbientStrengthName;
            static const char* kUsingSolidWireframeModeName;

            // Vertex Shader - Buffers
            static const char* kPositionName;
            static const char* kNormalName;
            static const char* kColorName;
            static const char* kUVName;

            // Vertex Shader - Uniform
            static const char* kModelName;
            static const char* kProjectionName;
            static const char* kViewName;
            static const char* kStackingAxisName;
            static const char* kOverhangAngleName;
            static const char* kOverhangModeName;
            static const char* kRenderingPartObjectName;
        };
    };

    class GcodeFileVariables {
      public:
        static const QString kPrinterBaseOffset;
        static const QString kExtrusionWidth;
        static const QString kXOffset;
        static const QString kYOffset;
        static const QString kLiftSpeed;
        static const QString kTravelSpeedMin;
        static const QString kTravelSpeed;
        static const QString kWTableSpeed;
        static const QString kInitialLayerThickness;
        static const QString kLayerThickness;
        static const QString kFirstLayerDefaultWidth;
        static const QString kForceMinLayerTime;
        static const QString kForceMinLayerTimeMethod;
        static const QString kMinimalLayerTime;
        static const QString kMaximalLayerTime;
        static const QString kPlasticType;
        static const QString kManualDensity;

        static const QHash<QString, QString> kNecessaryVariables;
        static const QHash<QString, QString> kRequiredConversion;
    };

    class SettingFileStrings {
      public:
        static const std::string kHeader;
        static const std::string kCreatedBy;
        static const std::string kCreatedOn;
        static const std::string kLastModified;
        static const std::string kVersion;
        static const std::string kLock;
        static const std::string kSettings;
    };

    class ConsoleOptionStrings {
      public:
        static const QString kInputProjectFile;
        static const QString kInputStlFiles;
        static const QString kInputStlFilesDirectory;
        static const QString kInputSupportStlFiles;
        static const QString kInputSupportStlFilesDirectory;
        static const QString kInputStlCount;
        static const QString kInputSupportStlCount;
        static const QString kInputGlobalSettings;
        static const QString kInputLocalSettings;
        static const QString kInputSTLTransform;
        static const QString kOutputLocation;

        static const QString kShiftPartsOnLoad;
        static const QString kAlignParts;
        static const QString kUseImplicitTransforms;

        static const QString kOverwriteOutputFile;
        static const QString kIncludeAuxiliaryFiles;
        static const QString kIncludeProjectFile;
        static const QString kBundleOutput;
        static const QString kHeaderSlicedBy;
        static const QString kHeaderDescription;

        static const QString kSliceBounds;
        static const QString kRealTimeMode;
        static const QString kRecoveryFilePath;
        static const QString kOpenLoop;
        static const QString kRealTimeCommunicationMode;
        static const QString kRealTimeNetworkAddress;
        static const QString kRealTimeNetworkIP;
        static const QString kRealTimeNetworkPort;
        static const QString kRealTimePrinter;
        static const QString kSingleSliceHeight;
        static const QString kSingleSliceLayerNumber;

        static const QString kVersion;
    };
};
} // namespace ORNL
#endif // CONSTANTS_H
