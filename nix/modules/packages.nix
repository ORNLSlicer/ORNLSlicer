{ self, lib, ... }:

let
  fetchVersion = version_file: let
    inherit (
      lib.pipe version_file [
        builtins.readFile builtins.fromJSON
      ]
    ) major minor patch suffix;

    version      = "${major}.${minor}.${patch}+${suffix}";
    revisionHash = self.shortRev or self.dirtyShortRev;
    fullVersion  = "${version}-${revisionHash}";
  in (
    fullVersion
  );
in {
  perSystem = { pkgs, self', ... }: let
    callPackage' = targetPkgs: extraArgs: targetPkgs.callPackage ../packages/slicer2/package.nix {
      version = fetchVersion ../../version.json;
      src = self;
    } // extraArgs;

  in {
    packages = {
      slicer2-gcc = callPackage' pkgs {};

      slicer2-llvm = callPackage' pkgs {
        stdenv = pkgs.llvmPackages.stdenv;
        openmp = pkgs.llvmPackages.openmp;
      };

      slicer2-win = callPackage' pkgs.pkgsCross.mingwW64 {
        stdenv = pkgs.pkgsCross.mingwW64.stdenv;
      };

      slicer2 = self'.packages.slicer2-llvm;

      default = self'.packages.slicer2;
    };
  };
}
