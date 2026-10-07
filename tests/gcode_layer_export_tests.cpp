#include <QCoreApplication>
#include <iostream>

#include "gcode/gcode_layer_export.h"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    bool passed       = true;
    const auto expect = [&passed](bool condition, const char* message) {
        if (!condition) {
            std::cerr << message << '\n';
            passed = false;
        }
    };

    const QString gcode =
        "; machine header\n"
        ";BEGINNING LAYER: 1\n"
        "G1 X1\n"
        ";BEGINNING LAYER: 2\n"
        "G1 X2\n"
        "M30\n";
    const QVector<ORNL::GcodeLayer> layers = ORNL::splitGcodeIntoLayers(gcode, ";");

    expect(layers.size() == 2, "Expected one output for each G-code layer.");
    if (layers.size() == 2) {
        expect(layers[0].number == 1 && layers[1].number == 2,
               "Expected output layers to retain their source layer numbers.");
        expect(layers[0].text == "; machine header\n;BEGINNING LAYER: 1\nG1 X1\n",
               "Expected the original header and only the first layer in the first output.");
        expect(layers[1].text == "; machine header\n;BEGINNING LAYER: 2\nG1 X2\nM30\n",
               "Expected the original header and only the second layer in the second output.");
    }

    const QVector<ORNL::GcodeLayer> no_layers = ORNL::splitGcodeIntoLayers("; machine header\nG1 X1\n", ";");
    expect(no_layers.isEmpty(), "Expected G-code without layer markers to produce no layers.");

    return passed ? 0 : 1;
}
