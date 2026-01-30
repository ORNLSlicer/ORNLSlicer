{
  src, version,

  lib, stdenv,

  cmake, pkg-config, ninja,

  qt6, assimp, boost, cgal_5, eigen, nlohmann_json, gmp, mpfr,
  hdf5, vtkWithQt6, kubazip, ornl-clipper, psimpl, ornl-sockets
}:

let
  # Upstream drv's find_package() cmake config is broken without this
  kubazip' = kubazip.overrideAttrs {
    outputs = [ "out" ];
  };
in stdenv.mkDerivation rec {
  pname = "slicer2";
  inherit version;
  inherit src;

  buildInputs = [
    qt6.qtbase
    qt6.qtcharts
    qt6.qt5compat
    assimp
    boost
    cgal_5
    eigen
    nlohmann_json
    hdf5
    vtkWithQt6
    ornl-clipper
    kubazip'
    psimpl
    ornl-sockets
    gmp
    mpfr
  ];

  cmakeFlags = [
    "-DSLICER2_AUTO_GENERATE_MASTER_CONFIG=OFF"
  ];

  nativeBuildInputs = [
    cmake
    pkg-config
    ninja
    qt6.wrapQtAppsHook
  ];

  meta = {
    description = "An advanced object slicer for toolpathing by ORNL";
    homepage = "https://github.com/ORNLSlicer/Slicer-2/";
    license = lib.licenses.gpl3;
    maintainers = with lib.maintainers; [
      cadkin
    ];
  };
}
