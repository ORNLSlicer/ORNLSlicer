{ inputOverlays, ... }:

{
  overlays = [(
    finalPkgs: prevPkgs: {
      # NOP
    }
  )] ++ inputOverlays;

  config = {
    allowUnfree = true;
  };
}
