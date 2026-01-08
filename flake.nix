{
  description = "ORNL Slicer-2 -  An advanced slicing application for Additive Manufacturing";

  inputs = {
    nixpkgs.url  = gitlab:mdf/nixpkgs/slicer2?host=code.ornl.gov;
    utils.url    = github:numtide/flake-utils;
    appimage = {
      url = github:ralismark/nix-appimage;
    };
  };

  outputs = inputs @ { self, utils, ... }: utils.lib.eachDefaultSystem (system: let
    config = rec {
      pkgs = import inputs.nixpkgs {
        inherit system;
        inherit (import ./nix/nixpkgs/config.nix {}) overlays config;
      };

      stdenv = llvm.stdenv;

      llvm = rec {
        packages = pkgs.llvmPackages_18;
        stdenv   = packages.stdenv;

        tooling = rec {
          lldb = packages.lldb;
          clang-tools = packages.clang-tools;
          clang-tools-libcxx = clang-tools.override {
              enableLibcxx = true;
          };
        };
      };
    };
  in with config; rec {
    inherit config;

    lib = rec {
      fetchVersion = version_file: let
        inherit (lib.pipe version_file [ builtins.readFile builtins.fromJSON ]) major minor patch suffix;
        suffixShort = builtins.substring 0 1 suffix;

        version      = "${major}.${minor}.${patch}+${suffix}";
        revisionHash = self.shortRev or self.dirtyShortRev;
        fullVersion  = "${version}-${revisionHash}";
      in fullVersion;

      mkPackages = { pkgs, stdenv ? pkgs.stdenv }: rec {
        nixpkgs = pkgs;

        ornl = rec {
          libraries = rec {
            sockets  = pkgs.qt6.callPackage ./nix/packages/sockets {};

            clipper  = pkgs.callPackage ./nix/packages/clipper  {};
            kuba-zip = pkgs.callPackage ./nix/packages/kuba-zip {};
            psimpl   = pkgs.callPackage ./nix/packages/psimpl   {};
          };

          slicer2 = pkgs.qt6.callPackage ./nix/slicer2 {
            src     = self;
            version = (lib.fetchVersion ./version.json);

            inherit (libraries) sockets kuba-zip clipper psimpl;
            inherit stdenv;
          };
        };
      };
    } // config.pkgs.lib;

    legacyPackages = {
      inherit (lib.mkPackages { inherit pkgs stdenv; } ) ornl nixpkgs;
      windows = (lib.mkPackages { pkgs = pkgs.pkgsCross.mingwW64; });
    };

    packages = rec {
      default = slicer2;
      slicer2 = legacyPackages.ornl.slicer2;
    };

    bundlers = rec {
      default = appimage;

      appimage = inputs.appimage.bundlers.${system}.default;
    };

    devShells = rec {
      default = s2Dev;

      # Main developer shell.
      s2Dev = pkgs.mkShell.override { inherit stdenv; } rec {
        name = "s2-dev";

        packages = [
          pkgs.git
          pkgs.jq

          pkgs.doxygen
          pkgs.graphviz

          llvm.tooling.lldb
          llvm.tooling.clang-tools

          (
            pkgs.python3.withPackages (py: [
              py.pandas
              py.odfpy
            ])
          )
        ] ++ lib.optionals stdenv.isLinux [
          pkgs.nsis
          pkgs.cntr
          pkgs.clazy
        ];

        inputsFrom = [
          legacyPackages.ornl.slicer2
        ];

        LD_FALLBACK_PATH = "/usr/lib/x86_64-linux-gnu";
      };
    };
  });

  nixConfig = {
    extra-substituters = [ "https://mdfbaam.cachix.org" ];
    extra-trusted-public-keys = [ "mdfbaam.cachix.org-1:WCQinXaMJP7Ny4sMlKdisNUyhcO2MHnPoobUef5aTmQ=" ];
  };
}
