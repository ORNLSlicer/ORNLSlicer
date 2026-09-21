# Cachix Workflow

ORNLSlicer uses the public `ornl-slicer` Cachix binary cache to reuse Nix build
outputs between developer machines and CI runners. The cache reduces build time;
it does not replace source review or normal validation.

The flake configures the cache as an extra substituter, so Nix can download
matching outputs when commands are run with `--accept-flake-config`. Reading the
public cache does not require authentication. Publishing outputs requires a
per-cache write token.

## CI Behavior

The Nix CI workflow consumes cached outputs for checks and Linux and Windows
builds. It publishes newly built outputs only when the workflow is running on
the `develop` branch. Pull request and other branch builds intentionally do not
receive the `CACHIX_AUTH_TOKEN` secret and therefore do not populate the cache.

Consequently:

- A new dependency introduced by a pull request may have a slow first build.
- Re-running that pull request on a fresh GitHub-hosted runner may repeat the
  build if no authorized developer has populated the cache.
- The first successful `develop` build publishes its new outputs for later
  builds.
- Each source change still produces a new ORNLSlicer derivation, even when its
  unchanged dependencies are cached.

## Obtain Write Access

Use a per-cache token with write access to `ornl-slicer`. Do not use a personal
token, which grants broader access than this workflow needs.

1. Sign in to the [Cachix dashboard](https://app.cachix.org/).
2. Select the `ornl-slicer` cache.
3. Open the cache settings and create a write token.
4. Give the token a machine-specific description, such as
   `alice-development-workstation`.

If the cache or its settings are not visible, ask a cache administrator to add
your account or issue a per-cache token.

A write token authorizes its holder to publish binaries that other developers
and CI may trust. Create a separate token for each development machine, store it
as a secret, and revoke or rotate it if the machine or token is compromised.
Never include a token in a command, issue, pull request, chat message, or tracked
file.

## Configure the Cachix CLI

Capture the token without displaying it or recording it in shell history:

```bash
read -rsp "Cachix token: " ORNLSLICER_CACHIX_TOKEN
printf '\n'
printf '%s' "$ORNLSLICER_CACHIX_TOKEN" | cachix authtoken --stdin
unset ORNLSLICER_CACHIX_TOKEN
```

The CLI stores its configuration outside the repository, normally at
`~/.config/cachix/cachix.dhall`. Restrict access to the file and verify the
configuration:

```bash
chmod 600 ~/.config/cachix/cachix.dhall
cachix doctor --cache ornl-slicer
```

The default Cachix-managed signing configuration does not require developers to
create or distribute a signing key.

## Build and Publish New Outputs

Wrap a build with `cachix watch-exec` to upload each store path produced by that
build:

```bash
cachix watch-exec ornl-slicer -- \
  nix build -L --no-link .#ornl.ornlslicer --accept-flake-config
```

For the cross-compiled Windows package, use:

```bash
cachix watch-exec ornl-slicer -- \
  nix build -L --no-link .#windows.ornl.ornlslicer --accept-flake-config
```

This is the preferred workflow when the required outputs are not already in the
local Nix store. Only publish reviewed or otherwise trusted source revisions.

## Publish an Existing Result

`watch-exec` observes newly created store paths. If the requested output already
exists locally, build evaluation may be a no-op and there will be nothing for it
to observe. Push the existing package and its runtime closure instead:

```bash
nix build --no-link --print-out-paths \
  .#ornl.ornlslicer --accept-flake-config \
  | cachix push ornl-slicer
```

This is sufficient to accelerate an exact rebuild of the same source revision:
Nix can substitute the completed package instead of rebuilding it.

## Publish Build-Time Dependencies

Pushing the package output includes its runtime closure, but not every tool used
to build it. To warm build-time dependencies for later source revisions, publish
the realized outputs in the package's derivation closure:

```bash
drv_path="$(nix path-info --derivation .#ornl.ornlslicer)"

nix-store --query --requisites --include-outputs "$drv_path" \
  | grep -v '\.drv$' \
  | cachix push ornl-slicer
```

This closure can be substantially larger than the runtime closure. Prefer
`watch-exec` for normal development and use the broader upload when a new
build-time dependency would otherwise be repeatedly rebuilt by clean CI
runners.

## Troubleshooting

### Cachix reports an authorization failure

Run:

```bash
cachix doctor --cache ornl-slicer
```

Confirm that the configured token is a per-cache token with write access to
`ornl-slicer`. Regenerating a token and running `cachix authtoken --stdin` again
replaces the locally configured token.

### Nix ignores the substituter as untrusted

The repository's Nix installation instructions configure the developer as a
trusted Nix user. On an existing multi-user Nix installation, configure the
cache using:

```bash
cachix use ornl-slicer
```

Follow the command's instructions if system-level Nix configuration requires
administrator access. Do not disable signature verification.

### `watch-exec` uploads nothing

The output probably existed before `watch-exec` started. Use the existing-result
workflow above, or verify the exact result directly:

```bash
out_path="$(nix path-info .#ornl.ornlslicer)"
cachix doctor --cache ornl-slicer "$out_path"
```

For additional Cachix behavior and command variants, see the official
[pushing guide](https://docs.cachix.org/pushing) and
[security documentation](https://docs.cachix.org/security).
