#include "console/command_line_processor.h"

#include <vector>

#include <CGAL/property_map.h>
#include <qcommandlineparser.h>
#include <qcontainerfwd.h>
#include <qdir.h>
#include <qfiledevice.h>
#include <qfileinfo.h>
#include <qlogging.h>
#include <qsharedpointer.h>

#include "configs/settings_base.h"
#include "ornlslicer/build_info.h"
#include "units/unit.h"
#include "utilities/constants.h"

namespace ORNL {
namespace {
const QStringList kModelSuffixes = {"stl", "3mf", "obj", "amf", "step", "stp"};

QStringList modelNameFilters() {
    QStringList filters;
    for (const QString& suffix : kModelSuffixes) {
        filters << "*." + suffix;
        filters << "*." + suffix.toUpper();
    }
    return filters;
}
} // namespace

CommandLineConverter::CommandLineConverter() {
    m_master = QSharedPointer<SettingsBase>::create();
    QFile master_file(":/configs/master.conf");
    master_file.open(QIODevice::ReadOnly);

    QString master_data = master_file.readAll();
    m_master->json(json::parse(master_data.toStdString()));
}

void CommandLineConverter::setupCommandLineParser(QCommandLineParser& parser) {

    // parser in-built options
    parser.addHelpOption();

    // custom options needed for loading/slicing
    parser.addOption({Constants::ConsoleOptionStrings::kInputProjectFile,
                      "Run ORNLSlicer using project file at <directory>.", "directory", ""});
    parser.addOption({Constants::ConsoleOptionStrings::kInputStlFiles,
                      "List of model files to load for slicing. Parameter can be specified multiple times.",
                      "file-list", ""});
    parser.addOption({Constants::ConsoleOptionStrings::kInputSupportStlFiles,
                      "List of support model files to load for slicing. Parameter can be specified multiple times.",
                      "file-list", ""});
    parser.addOption({Constants::ConsoleOptionStrings::kInputStlFilesDirectory,
                      "Loads all supported model files for slicing at <directory>. Equivalent to listing the files "
                      "individually using "
                      "--input_stl_files.",
                      "directory", ""});
    parser.addOption({Constants::ConsoleOptionStrings::kInputSupportStlFilesDirectory,
                      "Loads all supported model files as supports for slicing at <directory>. Equivalent to listing "
                      "the support files individually using --input_support_stl_files.",
                      "directory", ""});

    parser.addOption({Constants::ConsoleOptionStrings::kInputGlobalSettings,
                      "Valid settings file to extract global settings from.", "file", ""});
    parser.addOption(
        {Constants::ConsoleOptionStrings::kInputSTLTransform, "File containing model transforms.", "file", ""});
    parser.addOption(
        {Constants::ConsoleOptionStrings::kOutputLocation, "File to write out Gcode to.", "directory", ""});

    // preference option
    parser.addOption({Constants::ConsoleOptionStrings::kShiftPartsOnLoad,
                      "Specifies whether or not to shift models after load. Default is true.", "bool", "true"});
    parser.addOption({Constants::ConsoleOptionStrings::kAlignParts,
                      "Specifies whether or not to align/center models after load.  Default is true.", "bool", "true"});
    parser.addOption(
        {Constants::ConsoleOptionStrings::kUseImplicitTransforms,
         "Specifies whether or not to use implicit transforms.  Default is false. Typically used in conjunction with "
         "center parts. Turn off center parts and turn this on to load parts in coordinate system as exported.",
         "bool", "false"});

    // control options for gcode export
    parser.addOption({Constants::ConsoleOptionStrings::kOverwriteOutputFile,
                      "Specifies whether output will overwrite existing files.  Default is true."});
    parser.addOption({Constants::ConsoleOptionStrings::kIncludeAuxiliaryFiles,
                      "Specifies whether to save auxiliary files with the Gcode output.  Default is true."});
    parser.addOption(
        {Constants::ConsoleOptionStrings::kIncludeProjectFile,
         "Specifies whether to save a copy of the project file with the Gcode output.  Default is false."});
    parser.addOption({Constants::ConsoleOptionStrings::kBundleOutput,
                      "Specifies creation of specific folder to bundle output.", "directory", ""});
    parser.addOption({Constants::ConsoleOptionStrings::kHeaderSlicedBy,
                      "Define Operator (Sliced by) header text. Default is empty.", "text", ""});
    parser.addOption({Constants::ConsoleOptionStrings::kHeaderDescription,
                      "Define description header text. Default is empty.", "text", ""});

    // options for maincontrol behavior
    parser.addOption({Constants::ConsoleOptionStrings::kSliceBounds,
                      "Comma separated pair: low,high. 0-based indicies specifying layers to slice (inclusive).",
                      "low,high pair", ""});
    parser.addOption({Constants::ConsoleOptionStrings::kSingleSliceHeight,
                      "List of heights to slice in lieu of slicing the entire object. Mutually exclusive to "
                      "single_slice_layer_number. Parameter can be specified multiple times.",
                      "height-list", ""});
    parser.addOption({Constants::ConsoleOptionStrings::kSingleSliceLayerNumber,
                      "List of layer numbers to slice in lieu of slicing the entire object. Mutually exclusive to "
                      "single_slice_height. Parameter can be specified multiple times.",
                      "layer-list", ""});
    parser.addOption({Constants::ConsoleOptionStrings::kVersion, "Current ORNLSlicer Version"});
}

bool CommandLineConverter::checkRequiredSettings(QCommandLineParser& parser, QSharedPointer<SettingsBase> options) {
    if (parser.isSet(Constants::ConsoleOptionStrings::kVersion)) {
        qInfo() << QString::fromUtf8(ORNL::BuildInfo::Version);
        return false;
    }

    // Either a model list, model directory, or project file must be specified.
    if (parser.isSet(Constants::ConsoleOptionStrings::kInputStlFiles) +
            parser.isSet(Constants::ConsoleOptionStrings::kInputProjectFile) +
            parser.isSet(Constants::ConsoleOptionStrings::kInputStlFilesDirectory) !=
        1) {
        qInfo() << "Either model files, a model directory, or a project file must be specified as input";
        return false;
    }

    bool validProject = false;
    if (parser.isSet(Constants::ConsoleOptionStrings::kInputProjectFile)) {
        validProject = isValid(parser.value(Constants::ConsoleOptionStrings::kInputProjectFile), "s2p");
        options->setSetting(Constants::ConsoleOptionStrings::kInputProjectFile,
                            parser.value(Constants::ConsoleOptionStrings::kInputProjectFile));
    }

    bool validSTL = false;
    if (parser.isSet(Constants::ConsoleOptionStrings::kInputStlFiles)) {
        QStringList stlList = parser.values(Constants::ConsoleOptionStrings::kInputStlFiles);
        for (int i = 0, end = stlList.size(); i < end; ++i) {
            validSTL = isValidModelFile(stlList[i]);
            if (!validSTL)
                return false;

            options->setSetting(Constants::ConsoleOptionStrings::kInputStlFiles + "_" + QString::number(i), stlList[i]);
        }
        options->setSetting(Constants::ConsoleOptionStrings::kInputStlCount, stlList.size());
    }
    else
        options->setSetting(Constants::ConsoleOptionStrings::kInputStlCount, 0);

    bool validSTLDirectory = false;
    if (parser.isSet(Constants::ConsoleOptionStrings::kInputStlFilesDirectory)) {
        QDir dir(parser.value(Constants::ConsoleOptionStrings::kInputStlFilesDirectory));
        QStringList names = dir.entryList(modelNameFilters());
        if (names.size() == 0)
            return false;

        validSTLDirectory = true;
        for (int i = 0; i < names.size(); ++i)
            options->setSetting(Constants::ConsoleOptionStrings::kInputStlFiles + "_" + QString::number(i),
                                dir.absoluteFilePath(names[i]));

        options->setSetting(Constants::ConsoleOptionStrings::kInputStlCount, names.size());
    }

    if (!validProject && !validSTL && !validSTLDirectory) {
        qInfo() << "No valid model file, directory, or project was specified";
        return false;
    }

    if (parser.isSet(Constants::ConsoleOptionStrings::kInputGlobalSettings) &&
        parser.isSet(Constants::ConsoleOptionStrings::kInputProjectFile)) {
        qInfo() << "Cannot specify global settings and project file";
        return false;
    }

    bool validSettings = true;
    if (parser.isSet(Constants::ConsoleOptionStrings::kInputGlobalSettings)) {
        validSettings = isValid(parser.value(Constants::ConsoleOptionStrings::kInputGlobalSettings), "s2c");
        if (validSettings)
            options->setSetting<QString>(Constants::ConsoleOptionStrings::kInputGlobalSettings,
                                         parser.value(Constants::ConsoleOptionStrings::kInputGlobalSettings));
        else
            return false;
    }

    if (parser.isSet(Constants::ConsoleOptionStrings::kInputSTLTransform)) {
        if (!parser.isSet(Constants::ConsoleOptionStrings::kInputStlFiles)) {
            qInfo() << "No models specified for transform application";
            return false;
        }
        options->setSetting<QString>(Constants::ConsoleOptionStrings::kInputSTLTransform,
                                     parser.value(Constants::ConsoleOptionStrings::kInputSTLTransform));
        // CSM->loadPartsJson()
    }

    if (parser.isSet(Constants::ConsoleOptionStrings::kOutputLocation)) {
        QDir().mkpath(parser.value(Constants::ConsoleOptionStrings::kOutputLocation));
        options->setSetting<QString>(Constants::ConsoleOptionStrings::kOutputLocation,
                                     parser.value(Constants::ConsoleOptionStrings::kOutputLocation));
    }
    else {
        qInfo() << "Output location must be specified";
        return false;
    }

    // optional support models
    if (parser.isSet(Constants::ConsoleOptionStrings::kInputSupportStlFiles) &&
        parser.isSet(Constants::ConsoleOptionStrings::kInputSupportStlFilesDirectory)) {
        qInfo() << "Either support models or a directory can be specified, not both";
        return false;
    }

    if (parser.isSet(Constants::ConsoleOptionStrings::kInputSupportStlFiles)) {
        QStringList stlList = parser.values(Constants::ConsoleOptionStrings::kInputSupportStlFiles);
        for (int i = 0, end = stlList.size(); i < end; ++i) {
            validSTL = isValidModelFile(stlList[i]);
            if (!validSTL)
                return false;

            options->setSetting(Constants::ConsoleOptionStrings::kInputSupportStlFiles + "_" + QString::number(i),
                                stlList[i]);
        }
        options->setSetting(Constants::ConsoleOptionStrings::kInputSupportStlCount, stlList.size());
    }
    else
        options->setSetting(Constants::ConsoleOptionStrings::kInputSupportStlCount, 0);

    if (parser.isSet(Constants::ConsoleOptionStrings::kInputSupportStlFilesDirectory)) {
        QDir dir(parser.value(Constants::ConsoleOptionStrings::kInputSupportStlFilesDirectory));
        QStringList names = dir.entryList(modelNameFilters());
        if (names.size() == 0) {
            options->setSetting(Constants::ConsoleOptionStrings::kInputSupportStlCount, 0);
            qInfo() << "No support model files found in specified directory";
        }
        else {
            for (int i = 0; i < names.size(); ++i)
                options->setSetting(Constants::ConsoleOptionStrings::kInputSupportStlFiles + "_" + QString::number(i),
                                    dir.absoluteFilePath(names[i]));

            options->setSetting(Constants::ConsoleOptionStrings::kInputSupportStlCount, names.size());
        }
    }

    return true;
}

bool CommandLineConverter::checkOptionalPartSettingsAndPreferences(QCommandLineParser& parser,
                                                                   QSharedPointer<SettingsBase> options) {
    if (parser.isSet(Constants::ConsoleOptionStrings::kShiftPartsOnLoad))
        options->setSetting<bool>(Constants::ConsoleOptionStrings::kShiftPartsOnLoad,
                                  QVariant(parser.value(Constants::ConsoleOptionStrings::kShiftPartsOnLoad)).toBool());
    else
        options->setSetting<bool>(Constants::ConsoleOptionStrings::kShiftPartsOnLoad, true);

    if (parser.isSet(Constants::ConsoleOptionStrings::kAlignParts))
        options->setSetting<bool>(Constants::ConsoleOptionStrings::kAlignParts,
                                  QVariant(parser.value(Constants::ConsoleOptionStrings::kAlignParts)).toBool());
    else
        options->setSetting<bool>(Constants::ConsoleOptionStrings::kAlignParts, true);

    if (parser.isSet(Constants::ConsoleOptionStrings::kUseImplicitTransforms))
        options->setSetting<bool>(
            Constants::ConsoleOptionStrings::kUseImplicitTransforms,
            QVariant(parser.value(Constants::ConsoleOptionStrings::kUseImplicitTransforms)).toBool());
    else
        options->setSetting<bool>(Constants::ConsoleOptionStrings::kUseImplicitTransforms, false);

    return true;
}

bool CommandLineConverter::checkOptionalExportOptions(QCommandLineParser& parser,
                                                      QSharedPointer<SettingsBase> options) {
    if (parser.isSet(Constants::ConsoleOptionStrings::kOverwriteOutputFile))
        options->setSetting<bool>(
            Constants::ConsoleOptionStrings::kOverwriteOutputFile,
            QVariant(parser.value(Constants::ConsoleOptionStrings::kOverwriteOutputFile)).toBool());
    else
        options->setSetting<bool>(Constants::ConsoleOptionStrings::kOverwriteOutputFile, true);

    if (parser.isSet(Constants::ConsoleOptionStrings::kIncludeAuxiliaryFiles))
        options->setSetting<bool>(
            Constants::ConsoleOptionStrings::kIncludeAuxiliaryFiles,
            QVariant(parser.value(Constants::ConsoleOptionStrings::kIncludeAuxiliaryFiles)).toBool());
    else
        options->setSetting<bool>(Constants::ConsoleOptionStrings::kIncludeAuxiliaryFiles, true);

    if (parser.isSet(Constants::ConsoleOptionStrings::kIncludeProjectFile))
        options->setSetting<bool>(
            Constants::ConsoleOptionStrings::kIncludeProjectFile,
            QVariant(parser.value(Constants::ConsoleOptionStrings::kIncludeProjectFile)).toBool());
    else
        options->setSetting<bool>(Constants::ConsoleOptionStrings::kIncludeProjectFile, false);

    if (parser.isSet(Constants::ConsoleOptionStrings::kBundleOutput))
        options->setSetting<QString>(Constants::ConsoleOptionStrings::kBundleOutput,
                                     Constants::ConsoleOptionStrings::kBundleOutput);

    if (parser.isSet(Constants::ConsoleOptionStrings::kHeaderSlicedBy))
        options->setSetting<QString>(Constants::ConsoleOptionStrings::kHeaderSlicedBy,
                                     Constants::ConsoleOptionStrings::kHeaderSlicedBy);

    if (parser.isSet(Constants::ConsoleOptionStrings::kHeaderDescription))
        options->setSetting<QString>(Constants::ConsoleOptionStrings::kHeaderDescription,
                                     Constants::ConsoleOptionStrings::kHeaderDescription);

    return true;
}

bool CommandLineConverter::checkAdvancedOptions(QCommandLineParser& parser, QSharedPointer<SettingsBase> options) {
    if (parser.isSet(Constants::ConsoleOptionStrings::kSliceBounds)) {
        options->setSetting<bool>(
            Constants::ConsoleOptionStrings::kOverwriteOutputFile,
            QVariant(parser.value(Constants::ConsoleOptionStrings::kOverwriteOutputFile)).toBool());
    }

    if (parser.isSet(Constants::ConsoleOptionStrings::kSingleSliceHeight) &&
        parser.isSet(Constants::ConsoleOptionStrings::kSingleSliceLayerNumber)) {
        qInfo() << "Either heights or layer numbers may be specified for single slice, not both";
        return false;
    }

    if (parser.isSet(Constants::ConsoleOptionStrings::kSingleSliceHeight)) {
        std::vector<double> heights;
        QStringList heightStrings = parser.values(Constants::ConsoleOptionStrings::kSingleSliceHeight);
        for (int i = 0, end = heightStrings.size(); i < end; ++i) {
            bool ok;
            double val = heightStrings[i].toDouble(&ok);
            if (ok)
                heights.push_back(val);
            else {
                qInfo() << "All heights must be valid doubles";
                return false;
            }
            options->setSetting(Constants::ConsoleOptionStrings::kSingleSliceHeight, heights);
        }
    }

    if (parser.isSet(Constants::ConsoleOptionStrings::kSingleSliceLayerNumber)) {
        std::vector<int> layers;
        QStringList layerStrings = parser.values(Constants::ConsoleOptionStrings::kSingleSliceLayerNumber);
        for (int i = 0, end = layerStrings.size(); i < end; ++i) {
            bool ok;
            double val = layerStrings[i].toInt(&ok);
            if (ok && val >= 0)
                layers.push_back(val);
            else {
                qInfo() << "All layers must be valid ints of at least 0";
                return false;
            }
            options->setSetting(Constants::ConsoleOptionStrings::kSingleSliceLayerNumber, layers);
        }
    }

    return true;
}

bool CommandLineConverter::convertOptions(QCommandLineParser& parser, QSharedPointer<SettingsBase> options) {
    if (!checkRequiredSettings(parser, options) || !checkOptionalPartSettingsAndPreferences(parser, options) ||
        !checkOptionalExportOptions(parser, options) || !checkAdvancedOptions(parser, options)) {
        return false;
    }

    return true;
}

bool CommandLineConverter::isValid(QString path, QString suffix) {
    QFileInfo info(path);
    if (!info.exists()) {
        qInfo() << "Input: " + path + " does not exist";
        return false;
    }
    if (info.completeSuffix().toLower() != suffix) {
        qInfo() << "Input: " + path + " does not have an " + suffix + " extension";
        return false;
    }
    return true;
}

bool CommandLineConverter::isValidModelFile(QString path) {
    QFileInfo info(path);
    if (!info.exists()) {
        qInfo() << "Input: " + path + " does not exist";
        return false;
    }

    const QString suffix = info.suffix().toLower();
    if (!kModelSuffixes.contains(suffix)) {
        qInfo() << "Input: " + path + " does not have a supported model extension";
        return false;
    }

    return true;
}
} // namespace ORNL
