// CGAL must be parsed before OpenCASCADE defines its Handle macro.
// clang-format off
#include "threading/mesh_loader.h"
// clang-format on

#include <QCoreApplication>
#include <QTemporaryDir>
#include <iostream>

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRep_Builder.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Interface_Static.hxx>
#include <STEPControl_Writer.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>

#include "geometry/mesh/mesh_base.h"
#include "test_utils.h"

namespace {
bool writeStep(const TopoDS_Shape& shape, const QString& path) {
    Interface_Static::SetCVal("write.step.unit", "MM");
    STEPControl_Writer writer;
    if (writer.Transfer(shape, STEPControl_AsIs) != IFSelect_RetDone) return false;
    return writer.Write(path.toUtf8().constData()) == IFSelect_RetDone;
}

//! Two closed boxes that share a full face, as in an assembly of touching parts.
TopoDS_Shape touchingBoxes() {
    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    builder.Add(compound, BRepPrimAPI_MakeBox(gp_Pnt(0, 0, 0), gp_Pnt(10, 10, 10)).Shape());
    builder.Add(compound, BRepPrimAPI_MakeBox(gp_Pnt(10, 0, 0), gp_Pnt(20, 10, 10)).Shape());
    return compound;
}

//! A single solid with an internal void, i.e. one solid made of two shells.
TopoDS_Shape boxWithVoid() {
    TopoDS_Shape outer = BRepPrimAPI_MakeBox(gp_Pnt(0, 0, 0), gp_Pnt(30, 30, 30)).Shape();
    TopoDS_Shape inner = BRepPrimAPI_MakeBox(gp_Pnt(10, 10, 10), gp_Pnt(20, 20, 20)).Shape();
    return BRepAlgoAPI_Cut(outer, inner).Shape();
}

bool importsClosed(const TopoDS_Shape& shape, const QTemporaryDir& dir, const QString& name) {
    const QString path = dir.filePath(name);
    if (!ORNL::Testing::expect(writeStep(shape, path), "Failed to write STEP fixture " + name.toStdString()))
        return false;

    const auto meshes = ORNL::MeshLoader::LoadMeshes(path);
    if (!ORNL::Testing::expect(meshes.size() == 1, "Expected one mesh from " + name.toStdString())) return false;

    return ORNL::Testing::expect(meshes.front().mesh->isClosed(),
                                 "Expected " + name.toStdString() + " to import as a closed mesh.");
}
}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    if (!ORNL::Testing::expect(dir.isValid(), "Failed to create temporary directory.")) return 1;

    bool passed = true;
    passed &= importsClosed(touchingBoxes(), dir, "touching_boxes.step");
    passed &= importsClosed(boxWithVoid(), dir, "box_with_void.step");
    const auto box = BRepPrimAPI_MakeBox(gp_Pnt(0, 0, 0), gp_Pnt(10, 10, 10)).Shape();
    for (const QString& extension : {"STEP", "StEp", "stp", "STP", "StP"})
        passed &= importsClosed(box, dir, "box." + extension);

    if (!passed) return 1;
    std::cout << "STEP import tests passed\n";
    return 0;
}
