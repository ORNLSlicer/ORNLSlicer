#include "console/command_line_processor.h"

#include "QHostAddress"
#include "boost/preprocessor.hpp"

namespace ORNL {

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
                      "Run Slicer-2 using project file at <directory>.", "directory", ""});
    parser.addOption({Constants::ConsoleOptionStrings::kInputStlFiles,
                      "List of STLs to load for slicing. Parameter can be specified multiple times.", "file-list", ""});
    parser.addOption({Constants::ConsoleOptionStrings::kInputSupportStlFiles,
                      "List of support STLs to load for slicing. Parameter can be specified multiple times.",
                      "file-list", ""});
    parser.addOption({Constants::ConsoleOptionStrings::kInputStlFilesDirectory,
                      "Loads all STLs for slicing at <directory>. Equivalent to listing the files individually using "
                      "--input_stl_files.",
                      "directory", ""});
    parser.addOption({Constants::ConsoleOptionStrings::kInputSupportStlFilesDirectory,
                      "Loads all STLs as supports for slicing at <directory>. Equivalent to listing the support files "
                      "individual using --input_support_stl_files.",
                      "directory", ""});

    parser.addOption({Constants::ConsoleOptionStrings::kInputGlobalSettings,
                      "Valid settings file to extract global settings from.", "file", ""});
    // parser.addOption({Constants::ConsoleOptionStrings::kInputLocalSettings, "Comma separated pair: index,file. Where
    // index is 0-based id for STL reference and file is the settings file to apply.", "index,file pair", ""});
    parser.addOption(
        {Constants::ConsoleOptionStrings::kInputSTLTransform, "File containing stl transforms.", "file", ""});
    parser.addOption(
        {Constants::ConsoleOptionStrings::kOutputLocation, "File to write out Gcode to.", "directory", ""});

    // preference option
    parser.addOption({Constants::ConsoleOptionStrings::kShiftPartsOnLoad,
                      "Specifies whether or not to shift STLs after load. Default is true.", "bool", "true"});
    parser.addOption({Constants::ConsoleOptionStrings::kAlignParts,
                      "Specifies whether or not to align/center STLs after load.  Default is true.", "bool", "true"});
    parser.addOption(
        {Constants::ConsoleOptionStrings::kUseImplicitTransforms,
         "Specifies whether or not to use implicit transforms.  Default is false. Typically used in conjunction with "
         "center parts. Turn off center parts and turn this on to load parts in coordinate system as exported.",
         "bool", "false"});
    parser.addOption(
        {Constants::ConsoleOptionStrings::kAlwaysDropParts,
         "Specifies whether or not to drop the part to the build region. Default is false.", "bool", "false"});
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
    parser.addOption({Constants::ConsoleOptionStrings::kRealTimeMode,
                      "Keeps slicing instance running and generates Gcode for each "
                      "layer. Requires network communication. Default is false."});
    parser.addOption({Constants::ConsoleOptionStrings::kRecoveryFilePath,
                      "If using real time mode, slicing will continue from a recovery file position.",
                      "Recovery file path", ""});
    parser.addOption({Constants::ConsoleOptionStrings::kOpenLoop,
                      "If using real time mode, open loop will not wait for signals "
                      "before compute the next layer. Default is false."});
    parser.addOption({Constants::ConsoleOptionStrings::kRealTimeCommunicationMode,
                      "0 - file, 1 - network. Specifies whether to write Gcode to file or socket."
                      " In the case of file, Gcode will be written to the location specified by output_location option "
                      "and completion signal will be sent to "
                      " real_time_network_address. In the case of network, Gcode will be sent directly to "
                      "real_time_network_address. Default is 0.",
                      "0/1", ""});
    parser.addOption({Constants::ConsoleOptionStrings::kRealTimeNetworkAddress,
                      "Comma separated pair: IP Address,Port. Specifies connection information for real-time mode. "
                      "Default is localhost/12345.",
                      "IP Address,Port pair", ""});
    parser.addOption({Constants::ConsoleOptionStrings::kRealTimePrinter,
                      "The name of the printer to stream commands and gcode to over the network. This is set in Sensor "
                      "Control 2. Default is \"Default\"",
                      "Printer Name", ""});
    parser.addOption({Constants::ConsoleOptionStrings::kSingleSliceHeight,
                      "List of heights to slice in lieu of slicing the entire object. Mutually exclusive to "
                      "single_slice_layer_number. Parameter can be specified multiple times.",
                      "height-list", ""});
    parser.addOption({Constants::ConsoleOptionStrings::kSingleSliceLayerNumber,
                      "List of layer numbers to slice in lieu of slicing the entire object. Mutually exclusive to "
                      "single_slice_height. Parameter can be specified multiple times.",
                      "layer-list", ""});
    parser.addOption({Constants::ConsoleOptionStrings::kVersion, "Current Slicer-2 Version"});
}

bool CommandLineConverter::checkRequiredSettings(QCommandLineParser& parser, QSharedPointer<SettingsBase> options) {
    if (parser.isSet(Constants::ConsoleOptionStrings::kVersion)) {
        qInfo() << BOOST_PP_STRINGIZE(SLICER2_VERSION);
        return false;
    }

    // either both or neither stls/project file were specified.  Must have one or the other.
    if (parser.isSet(Constants::ConsoleOptionStrings::kInputStlFiles) +
            parser.isSet(Constants::ConsoleOptionStrings::kInputProjectFile) +
            parser.isSet(Constants::ConsoleOptionStrings::kInputStlFilesDirectory) !=
        1) {
        qInfo() << "Either stls, an stl directory, or a project file must be specified as input";
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
            validSTL = isValid(stlList[i], "stl");
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
        QStringList names = dir.entryList(QStringList() << "*.stl");
        if (names.size() == 0)
            return false;

        validSTLDirectory = true;
        for (int i = 0; i < names.size(); ++i)
            options->setSetting(Constants::ConsoleOptionStrings::kInputStlFiles + "_" + QString::number(i),
                                dir.absoluteFilePath(names[i]));

        options->setSetting(Constants::ConsoleOptionStrings::kInputStlCount, names.size());
    }

    if (!validProject && !validSTL && !validSTLDirectory) {
        qInfo() << "No valid stl file, directory, or project was specified";
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
            qInfo() << "No stls specified for transform application";
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

    // optional STLs
    if (parser.isSet(Constants::ConsoleOptionStrings::kInputSupportStlFiles) &&
        parser.isSet(Constants::ConsoleOptionStrings::kInputSupportStlFilesDirectory)) {
        qInfo() << "Either support stls or a directory can be specified, not both";
        return false;
    }

    if (parser.isSet(Constants::ConsoleOptionStrings::kInputSupportStlFiles)) {
        QStringList stlList = parser.values(Constants::ConsoleOptionStrings::kInputSupportStlFiles);
        for (int i = 0, end = stlList.size(); i < end; ++i) {
            validSTL = isValid(stlList[i], "stl");
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
        QStringList names = dir.entryList(QStringList() << "*.stl");
        if (names.size() == 0) {
            options->setSetting(Constants::ConsoleOptionStrings::kInputSupportStlCount, 0);
            qInfo() << "No support stls found in specified directory";
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

    if (parser.isSet(Constants::ConsoleOptionStrings::kAlwaysDropParts))
        options->setSetting<bool>(
            Constants::ConsoleOptionStrings::kAlwaysDropParts,
            QVariant(parser.value(Constants::ConsoleOptionStrings::kAlwaysDropParts)).toBool());
    else
        options->setSetting<bool>(Constants::ConsoleOptionStrings::kAlwaysDropParts, false);

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

    if (parser.isSet(Constants::ConsoleOptionStrings::kRealTimeMode))
        options->setSetting<bool>(Constants::ConsoleOptionStrings::kRealTimeMode, true);
    else
        options->setSetting<bool>(Constants::ConsoleOptionStrings::kRealTimeMode, false);

    if (parser.isSet(Constants::ConsoleOptionStrings::kRecoveryFilePath)) {
        QString value = parser.value(Constants::ConsoleOptionStrings::kRecoveryFilePath);
        if (!value.isEmpty())
            options->setSetting<QString>(Constants::ConsoleOptionStrings::kRecoveryFilePath, value);
        else {
            qInfo() << "Not a valid value for " << Constants::ConsoleOptionStrings::kRecoveryFilePath;
            return false;
        }
    }
    else
        options->setSetting<QString>(Constants::ConsoleOptionStrings::kRecoveryFilePath, "");

    if (parser.isSet(Constants::ConsoleOptionStrings::kOpenLoop))
        options->setSetting<bool>(Constants::ConsoleOptionStrings::kOpenLoop, true);
    else
        options->setSetting<bool>(Constants::ConsoleOptionStrings::kOpenLoop, false);

    if (parser.isSet(Constants::ConsoleOptionStrings::kRealTimeCommunicationMode)) {
        bool ok;
        int value = QVariant(parser.value(Constants::ConsoleOptionStrings::kRealTimeCommunicationMode)).toInt(&ok);
        if (ok)
            options->setSetting<int>(Constants::ConsoleOptionStrings::kRealTimeCommunicationMode, value);
        else {
            qInfo() << "Not a valid value for real_time_communication_mode";
            return false;
        }
    }
    else {
        if (parser.isSet(Constants::ConsoleOptionStrings::kRealTimeMode))
            options->setSetting<int>(Constants::ConsoleOptionStrings::kRealTimeCommunicationMode, 0);
    }

    if (parser.isSet(Constants::ConsoleOptionStrings::kRealTimeNetworkAddress)) {
        QStringList ipAndPort = parser.value(Constants::ConsoleOptionStrings::kRealTimeNetworkAddress).split(',');
        if (ipAndPort.size() != 2) {
            qInfo() << "IP Address/Port must be a comma-separated pair";
            return false;
        }

        QHostAddress address(ipAndPort[0]);
        if (QAbstractSocket::IPv4Protocol != address.protocol() &&
            QAbstractSocket::IPv6Protocol != address.protocol()) {
            qInfo() << "Not a valid IP Address";
            return false;
        }

        bool ok;
        int port = QVariant(ipAndPort[1]).toInt(&ok);
        if (port < 1 || port > 65535) {
            qInfo() << "Not a valid port";
            return false;
        }

        options->setSetting<QString>(Constants::ConsoleOptionStrings::kRealTimeNetworkIP, ipAndPort[0]);
        options->setSetting<int>(Constants::ConsoleOptionStrings::kRealTimeNetworkPort, port);
    }
    else {
        if (parser.isSet(Constants::ConsoleOptionStrings::kRealTimeMode)) {
            options->setSetting<QString>(Constants::ConsoleOptionStrings::kRealTimeNetworkIP, "localhost");
            options->setSetting<int>(Constants::ConsoleOptionStrings::kRealTimeNetworkPort, 12345);
        }
    }

    if (parser.isSet(Constants::ConsoleOptionStrings::kRealTimePrinter)) {
        QString name = parser.value(Constants::ConsoleOptionStrings::kRealTimePrinter);
        options->setSetting<QString>(Constants::ConsoleOptionStrings::kOutputLocation, name);
    }
    else {
        options->setSetting<QString>(Constants::ConsoleOptionStrings::kRealTimePrinter, "Default");
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
} // namespace ORNL
