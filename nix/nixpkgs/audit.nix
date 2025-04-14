{ }:

builtins.toFile "fallback.yaml" (builtins.toJSON {
  log_level = "error";
  rules = [
    {
      cond = {
        rtld = "nix";
        lib  = "(.*)";
      };

      default.prepend = [
        { dir = "/usr/lib/x86_64-linux-gnu"; }
      ];
    }
  ];
})
