{
  fetchurl,
  runCommand,
  writeTextFile,
}:

let
  proot = fetchurl {
    url = "https://github.com/proot-me/proot/releases/download/v5.4.1/proot";
    hash = "sha256-GfRCg/XA5zCRxgGV9fzU9MEWVQXkRBDUNOKrG2d8Ggk=";
  };

  appRun = writeTextFile {
    name = "ornlslicer-appimage-apprun";
    executable = true;
    text = ''
      #!/bin/sh
      set -eu

      if [ -z "''${APPDIR:-}" ]; then
        echo "ORNLSlicer AppImage launcher: APPDIR is not set" >&2
        exit 127
      fi

      exec "$APPDIR/proot" \
        -b "$APPDIR/nix:/nix" \
        "$APPDIR/entrypoint" "$@"
    '';
  };
in
runCommand "ornlslicer-appimage-apprun" { } ''
  mkdir "$out"
  install -m 0555 ${appRun} "$out/AppRun"
  install -m 0555 ${proot} "$out/proot"
''
