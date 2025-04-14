{
  src, version,

  lib, stdenv,

  cmake, pkg-config, ninja, wrapQtAppsHook, deployQtWinPluginsHook, audit,

  qtbase, qtcharts, qt5compat, assimp, boost182, cgal_5, eigen, nlohmann_json, gmp, mpfr,
  hdf5, vtk-qt, kuba-zip, clipper, psimpl, sockets
}:

stdenv.mkDerivation rec {
  pname = "slicer2";
  inherit version;
  inherit src;

  buildInputs = [
    qtbase
    qtcharts
    qt5compat
    assimp
    boost182
    cgal_5
    eigen
    nlohmann_json
    hdf5
    vtk-qt
    kuba-zip
    clipper
    psimpl
    sockets
    gmp
    mpfr
  ] ++ lib.optionals stdenv.hostPlatform.isMinGW [
    # Note: intentionally NOT native. Hook refers to target plugins internally.
    deployQtWinPluginsHook
  ];

  cmakeFlags = [
    "-DSLICER2_AUTO_GENERATE_MASTER_CONFIG=OFF"
  ];

  nativeBuildInputs = [
    cmake
    pkg-config
    ninja
  ] ++ lib.optionals stdenv.isLinux [
    wrapQtAppsHook
  ];

  dontWrapQtApps = stdenv.hostPlatform.isMinGW;

  qtWrapperArgs = lib.optionals stdenv.hostPlatform.isLinux (
    lib.mapAttrsToList (
      name: value: "--suffix ${name} : ${value}"
    ) audit.env
  );

  meta = {
    description = "An advanced object slicer for toolpathing by ORNL";
    homepage = "https://github.com/ORNLSlicer/Slicer-2/";
    license = lib.licenses.gpl3;
    maintainers = with lib.maintainers; [
      cadkin
    ];
  };
}
