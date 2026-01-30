{ self, inputs, ... }:

{
  perSystem = { pkgs, system, ... }: {
    _module.args.pkgs = inputs.ornlpkgs.legacyPackages.${system};

    legacyPackages = pkgs;
  };
}
