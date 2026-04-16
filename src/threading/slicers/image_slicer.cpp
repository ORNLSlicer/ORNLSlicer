#include "threading/slicers/image_slicer.h"

#include <algorithm>
#include <cmath>
#include <tuple>

#include <QtCore/QDir>
#include <QtCore/QSharedPointer>
#include <nlohmann/json_fwd.hpp>
#include <qcontainerfwd.h>
#include <qfiledevice.h>
#include <qfileinfo.h>
#include <qhashfunctions.h>
#include <qmatrix4x4.h>
#include <qnumeric.h>
#include <qstringliteral.h>
#include <qtmetamacros.h>
#include <qvectornd.h>
#include <vtkCellArray.h>
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkType.h>

#include "cross_section/cross_section.h"
#include "gcode/writers/writer_base.h"
#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/mesh_base.h"
#include "geometry/mesh/open_mesh.h"
#include "geometry/plane.h"
#include "geometry/point.h"
#include "geometry/polygon.h"
#include "geometry/polygon_list.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "part/part.h"
#include "slicing/slicing_utilities.h"
#include "threading/traditional_ast.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "utilities/qt_json_conversion.h"

#undef emit
#include <vtkImageData.h>
#include <vtkImageStencilToImage.h>
#include <vtkLine.h>
#include <vtkPNGWriter.h>
#include <vtkPolyData.h>
#include <vtkPolyDataToImageStencil.h>
#define emit

#include <atomic>
#include <functional>
#include <thread>
#include <vector>

namespace ORNL {

ImageSlicer::ImageSlicer(QString gcodeLocation) : TraditionalAST(gcodeLocation, true) {}

void ImageSlicer::preProcess(nlohmann::json opt_data) {
    QVector<QSharedPointer<Part>> parts = SlicingUtilities::GetPartsByType(CSM->parts(), MeshType::kBuild);
    QVector<QSharedPointer<Part>> moreParts = SlicingUtilities::GetPartsByType(CSM->parts(), MeshType::kSupport);
    parts += moreParts;

    Distance layerHeight = GSM->getGlobal()->setting<Distance>(PS::Layer::kLayerHeight);
    Point globalMax;

    QVector<SlicingPlaneWithLayer> slicing_planes;
    Plane lowestSlicingPlane(Point(0, 0, 0), QVector3D(0, 0, 1));

    QVector<MeshAndBounds> allMeshes;
    QVector<MeshAndBounds> supportMeshes;

    QMatrix4x4 center;
    QVector3D shift(QVector3D(GSM->getGlobal()->setting<double>(PRS::Dimensions::kXOffset),
                              GSM->getGlobal()->setting<double>(PRS::Dimensions::kYOffset), 0));
    center.translate(shift);

    for (QSharedPointer<Part> part : parts) {
        MeshAndBounds currentMesh;
        Plane slicing_plane;
        Point mesh_min, mesh_max;

        QSharedPointer<MeshBase> mesh;
        auto closed_mesh = dynamic_cast<ClosedMesh*>(part->rootMesh().get());
        if (closed_mesh != nullptr)
            mesh = QSharedPointer<ClosedMesh>::create(ClosedMesh(*closed_mesh));
        else
            mesh = QSharedPointer<OpenMesh>::create(OpenMesh(*dynamic_cast<OpenMesh*>(part->rootMesh().get())));

        QMatrix4x4 trans = mesh->transformation();
        trans.translate(QVector3D(GSM->getGlobal()->setting<double>(PRS::Dimensions::kXOffset),
                                  GSM->getGlobal()->setting<double>(PRS::Dimensions::kYOffset), 0));
        mesh->setTransformation(trans);

        currentMesh.m_mesh = mesh;

        std::tie(slicing_plane, mesh_min, mesh_max) = SlicingUtilities::GetDefaultSlicingAxis(GSM->getGlobal(), mesh);

        if (slicing_plane.point().z() < lowestSlicingPlane.point().z())
            lowestSlicingPlane = slicing_plane;

        currentMesh.m_min = mesh->min();
        currentMesh.m_max = mesh->max();

        QFileInfo fi(part->sourceFilePath());
        currentMesh.m_original_name = fi.completeBaseName();

        if (mesh_max.z() > globalMax.z())
            globalMax = mesh_max;

        if (mesh->type() == MeshType::kSupport) {
            currentMesh.m_id = 65535;
            supportMeshes.push_back(currentMesh);
        }
        else
            allMeshes.push_back(currentMesh);
    }

    // sort meshes to determine id: first by z, then y, then x
    std::sort(allMeshes.begin(), allMeshes.end(), [](const MeshAndBounds& a, const MeshAndBounds& b) {
        if (std::rint(a.m_min.z()) != std::rint(b.m_min.z())) {
            return std::rint(a.m_min.z()) < std::rint(b.m_min.z());
        }
        if (std::rint(a.m_min.y()) != std::rint(b.m_min.y())) {
            return std::rint(a.m_min.y()) < std::rint(b.m_min.y());
        }
        return std::rint(a.m_min.x()) < std::rint(b.m_min.x());
    });

    // Ids are now conveniently mesh location in vector
    for (int i = 0; i < allMeshes.size(); ++i)
        allMeshes[i].m_id = i + 1;

    // include support meshes, id was previously set to static value
    allMeshes += supportMeshes;

    // reserve enough planes for tallest object or selected planes
    int maxSlices = (globalMax.z() - lowestSlicingPlane.point().z()) / layerHeight();

    QVector<int> layers;
    QSharedPointer<SettingsBase> consoleSettings = GSM->getConsoleSettings();
    if (!consoleSettings->empty()) {
        if (consoleSettings->contains(Constants::ConsoleOptionStrings::kSingleSliceHeight)) {
            auto vec =
                consoleSettings->setting<std::vector<double>>(Constants::ConsoleOptionStrings::kSingleSliceHeight);
            QVector<double> heights = QVector<double>(vec.begin(), vec.end());
            for (double height : heights) {
                int layer = qRound(height / layerHeight()) - 1;
                if (layer < 0)
                    layer = 0;
                layers.push_back(layer);
            }
        }

        if (consoleSettings->contains(Constants::ConsoleOptionStrings::kSingleSliceLayerNumber)) {
            auto vec =
                consoleSettings->setting<std::vector<int>>(Constants::ConsoleOptionStrings::kSingleSliceLayerNumber);
            layers = QVector<int>(vec.begin(), vec.end());
        }
    }
    if (layers.size() > 0)
        slicing_planes.reserve(layers.size());
    else
        slicing_planes.reserve(maxSlices);

    for (int i = 0; i < maxSlices; ++i) {
        SlicingUtilities::ShiftSlicingPlane(GSM->getGlobal(), lowestSlicingPlane, layerHeight);
        Plane next_plane = lowestSlicingPlane;
        if (layers.size() > 0) {
            if (i == layers[0]) {
                slicing_planes.push_back(SlicingPlaneWithLayer(i, next_plane));
                layers.pop_front();
                if (layers.size() == 0)
                    break;
            }
        }
        else
            slicing_planes.push_back(SlicingPlaneWithLayer(i, next_plane));
    }

    emit statusUpdate(StatusUpdateStepType::kPreProcess, 100);

    double xResolution = GSM->getGlobal()->setting<double>(ES::ImageResolution::kImageResolutionX);
    double yResolution = GSM->getGlobal()->setting<double>(ES::ImageResolution::kImageResolutionY);
    int volumeXDim = std::ceil((GSM->getGlobal()->setting<double>(PRS::Dimensions::kXMax) -
                                GSM->getGlobal()->setting<double>(PRS::Dimensions::kXMin)) /
                               xResolution) +
                     1;

    int volumeYDim = std::ceil((GSM->getGlobal()->setting<double>(PRS::Dimensions::kYMax) -
                                GSM->getGlobal()->setting<double>(PRS::Dimensions::kYMin)) /
                               yResolution) +
                     1;

    parallelForDynamic(0, slicing_planes.size(), 100, [&](int i) {
        Plane currentPlane = slicing_planes[i].m_slicing_plane;
        Point shift_amount(0, 0, 0);
        QVector3D average_normal;

        QVector<PolygonListAndColor> currentCrossSections;
        currentCrossSections.reserve(allMeshes.size());

        for (int j = 0; j < allMeshes.size(); ++j) {
            if (allMeshes[j].m_mesh->max().z() >= currentPlane.point().z()) {
                PolygonList result = CrossSection::doCrossSection(allMeshes[j].m_mesh, currentPlane, shift_amount,
                                                                  average_normal, GSM->getGlobal());

                for (int k = result.size() - 1; k >= 0; --k) {
                    if (result[k].size() <= 2)
                        result.removeAt(k);
                }

                if (!result.empty())
                    currentCrossSections.push_back(PolygonListAndColor(result, allMeshes[j].m_id));
            }
        }

        createImageStencilVTK(currentCrossSections, slicing_planes[i].m_layer, xResolution, yResolution, volumeXDim,
                              volumeYDim);
    });

    // create companion json file
    fifojson originalName;
    int i = 0;
    for (MeshAndBounds mesh : allMeshes) {
        if (mesh.m_id != 65535)
            originalName[QString::number(mesh.m_id).toStdString()] = mesh.m_original_name;
        else {
            originalName[QString::number(mesh.m_id).toStdString()][QString::number(i).toStdString()] =
                mesh.m_original_name;
            ++i;
        }
    }

    QFile file(m_temp_gcode_dir.absoluteFilePath("idFileLinks.dat"));
    file.open(QIODevice::WriteOnly);
    file.write(originalName.dump().c_str());
    file.close();
}

void ImageSlicer::createImageStencilVTK(QVector<PolygonListAndColor> geometryAndColor, int layer, double xResolution,
                                        double yResolution, int gridWidth, int gridHeight) {
    vtkNew<vtkPolyData> blankData;

    vtkNew<vtkPolyDataToImageStencil> blankStenc;
    blankStenc->SetInputData(blankData);
    blankStenc->SetTolerance(0);
    blankStenc->SetOutputWholeExtent(0, gridWidth, 0, gridHeight, 0, 0);
    blankStenc->Update();

    vtkNew<vtkImageStencilToImage> blankImage;
    blankImage->SetInputConnection(blankStenc->GetOutputPort());
    blankImage->SetOutputScalarTypeToUnsignedShort();
    blankImage->SetOutsideValue(0);
    blankImage->Update();

    vtkImageData* fullImage = blankImage->GetOutput();

    for (PolygonListAndColor polyListAndColor : geometryAndColor) {
        vtkNew<vtkPoints> points;
        vtkNew<vtkCellArray> lines;

        Point minPt = Point(std::rint(polyListAndColor.m_cross_section.min().x() / xResolution),
                            std::rint(polyListAndColor.m_cross_section.min().y() / yResolution));
        Point maxPt = Point(std::rint(polyListAndColor.m_cross_section.max().x() / xResolution),
                            std::rint(polyListAndColor.m_cross_section.max().y() / yResolution));

        for (Polygon polygon : polyListAndColor.m_cross_section) {
            for (int i = 0; i < polygon.size(); ++i) {
                Point p1 = polygon[i];
                Point p2 = polygon[(i + 1) % polygon.size()];

                vtkIdType start =
                    points->InsertNextPoint(p1.x() / xResolution - minPt.x(), p1.y() / yResolution - minPt.y(), 0);
                vtkIdType end =
                    points->InsertNextPoint(p2.x() / xResolution - minPt.x(), p2.y() / yResolution - minPt.y(), 0);

                vtkNew<vtkLine> line;
                line->GetPointIds()->SetId(0, start);
                line->GetPointIds()->SetId(1, end);

                lines->InsertNextCell(line);
            }
        }

        vtkNew<vtkPolyData> polyData;
        polyData->SetPoints(points);
        polyData->SetLines(lines);

        vtkNew<vtkPolyDataToImageStencil> pol2stenc;
        pol2stenc->SetInputData(polyData);
        pol2stenc->SetTolerance(0);
        pol2stenc->SetOutputWholeExtent(0, maxPt.x() - minPt.x(), 0, maxPt.y() - minPt.y(), 0, 0);
        pol2stenc->Update();

        vtkNew<vtkImageStencilToImage> imageStencilToImage;
        imageStencilToImage->SetInputConnection(pol2stenc->GetOutputPort());
        imageStencilToImage->SetOutputScalarTypeToUnsignedShort();
        imageStencilToImage->SetInsideValue(polyListAndColor.m_color);
        imageStencilToImage->SetOutsideValue(0);
        imageStencilToImage->Update();

        for (int x = minPt.x(); x < maxPt.x(); ++x) {
            for (int y = minPt.y(); y < maxPt.y(); ++y) {
                double val =
                    imageStencilToImage->GetOutput()->GetScalarComponentAsDouble(x - minPt.x(), y - minPt.y(), 0, 0);
                if (val > 0)
                    fullImage->SetScalarComponentFromDouble(x, y, 0, 0, val);
            }
        }
    }

    vtkNew<vtkPNGWriter> imageWriter;
    imageWriter->SetFileName(
        (m_temp_gcode_dir.absoluteFilePath(QStringLiteral("%1").arg(layer, m_total_digits, 10, QLatin1Char('0'))) +
         ".png")
            .toStdString()
            .c_str());
    imageWriter->SetInputData(fullImage);
    imageWriter->Write();
}

void ImageSlicer::parallelForDynamic(int start, int end, int batchSize, const std::function<void(int)>& func) {
    const int total = end - start;
    std::atomic<int> index {start};

    int thread_count = std::jthread::hardware_concurrency();
    std::vector<std::jthread> threads;

    for (int t = 0; t < thread_count; ++t) {
        threads.emplace_back([&]() {
            while (true) {
                int i = index.fetch_add(batchSize);
                if (i >= end)
                    break;

                int i_end = std::min(i + batchSize, end);
                for (int j = i; j < i_end; ++j) {
                    func(j);
                }
            }
        });
    }
}

void ImageSlicer::postProcess(nlohmann::json opt_data) { emit statusUpdate(StatusUpdateStepType::kPostProcess, 100); }

void ImageSlicer::writeGCode() { emit statusUpdate(StatusUpdateStepType::kGcodeGeneraton, 100); }
} // namespace ORNL
