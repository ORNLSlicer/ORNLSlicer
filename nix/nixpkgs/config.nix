{ ... }:

{
  overlays = [(
    finalPkgs: prevPkgs: {
      opencascade-occt = prevPkgs.opencascade-occt.overrideAttrs (old: {
        propagatedBuildInputs =
          if finalPkgs.stdenv.hostPlatform.isMinGW then [
            finalPkgs.freetype
            finalPkgs.fontconfig
          ] else old.propagatedBuildInputs;
      });
    }
  )];

  config = {
    allowUnfree = true;
    glibc.withLdFallbackPatch = true;
  };
}
