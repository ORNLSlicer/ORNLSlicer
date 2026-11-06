#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "test_utils.h"
#include "threading/gcode_arc_specialties_saver.h"

namespace {
QString readFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QString::fromUtf8(file.readAll());
}
}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    Q_INIT_RESOURCE(configs);
    bool passed       = true;
    const auto expect = [&](bool condition, const char* message) {
        passed &= ORNL::Testing::expect(condition, message);
    };
    QTemporaryDir directory;
    expect(directory.isValid(), "Could not create export fixture directory.");
    if (!directory.isValid()) return 1;

    const QString output = directory.filePath("part.nc");
    QFile existing(output);
    expect(existing.open(QIODevice::WriteOnly), "Could not create existing output.");
    existing.write("previous program\n");
    existing.close();
    expect(QDir(directory.path()).mkdir("part_sch.nc"), "Could not block companion destination.");
    ORNL::GCodeArcSpecialtiesSaver failed(output, "#FILE NAME[ G80=\"\" ]\nnew program\n");
    QString error;
    int completions      = 0;
    const auto completed = [&](const QString& message) {
        error = message;
        ++completions;
    };
    QObject::connect(&failed, &ORNL::GCodeArcSpecialtiesSaver::completed, completed);
    failed.run();
    expect(completions == 1 && error.contains("part_sch.nc"), "Companion failure must report an error exactly once.");
    expect(readFile(output) == "previous program\n", "A failed companion export must preserve the previous program.");

    expect(QDir(directory.path()).rmdir("part_sch.nc"), "Could not unblock companion destination.");
    ORNL::GCodeArcSpecialtiesSaver successful(output, "#FILE NAME[ G80=\"old.nc\" ]\nnew program\n");
    QObject::connect(&successful, &ORNL::GCodeArcSpecialtiesSaver::completed, completed);
    successful.run();
    expect(completions == 2 && error.isEmpty(), "Successful export must report success exactly once.");
    expect(readFile(output) == "#FILE NAME[ G80=\"part_sch.nc\" ]\nnew program\n",
           "Successful export must replace existing output and update its companion reference.");
    expect(readFile(directory.filePath("part_sch.nc")).contains("$ENDSWITCH"),
           "Successful export must complete its companion.");

    const QString blocked_output = directory.filePath("blocked.nc");
    expect(QDir(directory.path()).mkdir("blocked.nc"), "Could not block main program destination.");
    ORNL::GCodeArcSpecialtiesSaver blocked(blocked_output, "#FILE NAME[ G80=\"\" ]\n");
    QObject::connect(&blocked, &ORNL::GCodeArcSpecialtiesSaver::completed, completed);
    blocked.run();
    expect(completions == 3 && error.contains("blocked.nc"), "Main program failure must report an export error.");

    const QString layer_base = directory.filePath("layers.with.dots.nc");
    const QVector<ORNL::GcodeLayer> layers {{12, "#FILE NAME[ G80=\"\" ]\nG01 X=1 FV.S.SPEED\n"},
                                            {13, "#FILE NAME[ G80=\"old.nc\" ]\nG01 X=2 FV.S.SPEED\n"}};
    ORNL::GCodeArcSpecialtiesSaver layer_saver(layer_base, {}, layers);
    expect(layer_saver.save().isEmpty(), "Standalone ARC layers must export successfully.");
    expect(!QFile::exists(layer_base), "Individual-layer export must not create a continuous program.");
    expect(readFile(directory.filePath("layers.with.dots_sch.nc")).contains("$ENDSWITCH"),
           "Standalone layers must share one complete companion.");
    for (const auto& layer : layers) {
        const QString path = directory.filePath("layers.with.dots_layer_" + QString::number(layer.number) + ".nc");
        QString expected   = layer.text;
        expected.replace(layer.number == 12 ? "G80=\"\"" : "G80=\"old.nc\"", "G80=\"layers.with.dots_sch.nc\"");
        expect(readFile(path) == expected, "Each standalone layer must reference its shared companion.");
    }
    expect(QDir(directory.path()).mkdir("failed_layers_sch.nc"), "Could not block layer companion destination.");
    ORNL::GCodeArcSpecialtiesSaver failed_layers(directory.filePath("failed_layers.nc"), {}, layers);
    expect(!failed_layers.save().isEmpty() && !QFile::exists(directory.filePath("failed_layers_layer_12.nc")),
           "A failed layer companion must report failure before saving any layer programs.");
    expect(QDir(directory.path()).mkdir("blocked_layers_layer_12.nc"), "Could not block layer output destination.");
    ORNL::GCodeArcSpecialtiesSaver blocked_layers(directory.filePath("blocked_layers.nc"), {}, layers);
    expect(!blocked_layers.save().isEmpty(), "Layer write failures must propagate through the ARC saver.");
    return passed ? 0 : 1;
}
