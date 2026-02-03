{ self, inputs, ... }:

let
  overlay = finalPkgs: prevPkgs: {
    python3 = finalPkgs.python311;
    python3Packages = finalPkgs.python311Packages;
  };
in {
  perSystem = { pkgs, system, ... }: {
    _module.args.pkgs = inputs.ornlpkgs.legacyPackages.${system}.appendOverlays [
      overlay
    ];

    legacyPackages.pkgs = pkgs;
  };
}
