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
  perSystem = { pkgs, self', ... }: {
    packages = {
      slicer2 = pkgs.callPackage ../packages/slicer2/package.nix {
        version = fetchVersion ../../version.json;
        src = self;
      };
      default = self'.packages.slicer2;
    };
  };
}
