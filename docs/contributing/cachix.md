# Cachix Workflow

ORNLSlicer uses the public `ornl-slicer` Cachix binary cache to reuse Nix build
outputs between developer machines and CI runners. The cache reduces build time;
it does not replace source review or normal validation.

The [flake](../../flake.nix) configures the cache as an extra substituter and
declares its public signing key. Nix can download matching outputs when commands
are run with `--accept-flake-config`. Reading the public cache does not require
authentication. Publishing outputs and managing pins require a per-cache write
token.

Keep signature verification enabled. The repository uses this key:

```text
ornl-slicer.cachix.org-1:VIvBeaBy4B47CzxtF7VxZWsK0wXJt4Dm7VNuAb65Nzg=
```

Do not work around a trust-configuration problem with `--no-check-sigs`.

## Package and Cache Boundaries

Cachix substitutes exact Nix store paths. A cache hit therefore requires the
same package inputs, dependency revisions, build options, and target system.

The Linux and Windows application packages use a filtered source containing:

- `CMakeLists.txt` and `cmake/`
- `include/`, `src/`, and `tests/`
- `resources/`, including generated settings configuration
- `templates/`
- `version.json`
- `docs/ornlslicer-user-guide.pdf`, which is installed with the application

Changes to `.github/`, `openwiki/`, or non-installed documentation do not
change the application source path. Changes to the listed inputs do. Nix
expressions and locked dependencies remain derivation inputs independently of
the source filter.

The first-class `windowsInstaller` package depends on the versioned Windows
application, NSIS, `scripts/installer.nsi`, `LICENSE.md`, and the Windows icon.
Its output name is deterministic. CI adds the GitHub run number only to the
copied artifact name after the package has been built, so the installer store
path remains cacheable.

The supported cacheable outputs are:

| Output | Flake command or attribute |
| --- | --- |
| Linux application | `nix build .#ornl.ornlslicer` |
| Windows application | `nix build .#windows.ornl.ornlslicer` |
| Windows installer | `nix build .#windowsInstaller` |
| Linux AppImage | `nix bundle --bundler .#appimage .#ornl.ornlslicer` |
| Linux development shell | `nix build .#devShells.x86_64-linux.ornlslicerDev` |

## CI Behavior

The [Nix CI workflow](../../.github/workflows/ci.yml) passes
`--accept-flake-config` to checks, evaluations, builds, and bundling so each job
can consume the signed public cache. Its built-in GitHub token is read-only and
checkout credentials are not retained in the Linux and Windows build jobs. A
separate release-only publication job receives `contents: write` to attach
downloads after all builds and checks pass; it receives no Cachix token.

Cache write credentials are available in two trusted cases:

- A push to `develop` starts the Cachix daemon before the Linux and Windows
  builds. Newly built outputs are published automatically. The Linux job also
  realizes `devShells.x86_64-linux.ornlslicerDev`, deliberately populating the
  supported development environment. The macOS check does not publish a
  development shell.
- A published GitHub release builds the release tag, synchronously pushes the
  four release outputs, and pins them as described below.

Other branch pushes and fork pull requests remain read-only cache consumers.
The workflow does not provide `CACHIX_AUTH_TOKEN` to those jobs, and it does not
run untrusted pull request code with the token. Preserve those conditions when
adding triggers or reorganizing jobs.

Fork pull requests run the regular version validation, Linux and macOS flake
checks, Linux AppImage build and smoke test, and Windows package and installer
builds. These runs also upload build artifacts. The `pull_request` trigger tests
GitHub's merge commit, so it checks the proposed changes together with the target
branch. Same-repository PRs continue to use their existing push builds; their
additional PR-triggered Nix CI jobs are skipped to avoid duplicate builds.

GitHub may require a maintainer to approve a fork workflow run, depending on the
repository's **Settings > Actions > General** approval policy. Review the PR's
changes, including workflow changes, then select **Approve workflows to run**
on the PR if approval is requested. See GitHub's
[approval instructions](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/approve-runs-from-forks).

Adding a trigger does not retroactively create runs for already-open PRs. After
the workflow change reaches the target branch, close and reopen an existing fork
PR, or ask its contributor to push a new commit. Either action generates a new
PR event. Resolve merge conflicts first if GitHub cannot create the merge commit.
Check that **Nix CI** appears on the PR and approve the run if requested; rerunning
an existing **Version validation** run does not start Nix CI.

Consequently:

- A new dependency introduced by a branch may have a slow first build.
- Re-running that branch on a fresh GitHub-hosted runner may repeat the
  build if no authorized developer has populated the cache.
- The first successful `develop` build publishes its new outputs for later
  builds.
- Build-relevant changes produce new ORNLSlicer store paths even when unchanged
  dependencies are cached. Repository-only workflow or documentation changes
  retain the application paths.

All external workflow actions are pinned to full commit SHAs. The
[Dependabot configuration](../../.github/dependabot.yml) checks GitHub Actions
dependencies weekly and opens updates against `develop`.

## Release Retention

Cachix may garbage-collect old unpinned paths when cache storage is under
pressure. Each published release therefore creates four version-specific pins:

| Pin | Protected output |
| --- | --- |
| `ornlslicer-vX.Y.Z-linux` | Linux application package |
| `ornlslicer-vX.Y.Z-appimage` | Linux AppImage bundle |
| `ornlslicer-vX.Y.Z-windows` | Windows portable package |
| `ornlslicer-vX.Y.Z-windows-installer` | Windows installer package |

The workflow requires the release tag to equal `v` followed by the package
version before it pushes or pins an output. Each pin uses `--keep-forever`.
The retention policy is therefore to keep every published release indefinitely;
cache administrators should remove a release pin only as a deliberate release
retirement action.

After publishing or rerunning a release:

1. Open the release-triggered `Nix CI` run and confirm both `Pin ... Release
   Outputs` steps succeeded.
2. Open the `ornl-slicer` cache's
   [Pins dashboard](https://app.cachix.org/cache/ornl-slicer/pins).
3. Confirm all four names exist for the tag and reference the store paths from
   that workflow run.
4. Treat a missing pin as a failed release-maintenance task even if the GitHub
   release artifacts uploaded successfully.

The release-only publication job attaches the AppImage, Windows installer, and
portable ZIP to the GitHub release after all builds, checks, and pin steps pass.
See [Versioning and release preparation](versioning.md#github-workflow) for
branch promotion, download verification, and failed-run recovery.

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

On `x86_64-linux`, the installer is a separate package output:

```bash
cachix watch-exec ornl-slicer -- \
  nix build -L --no-link .#windowsInstaller --accept-flake-config
```

To populate the supported development shell explicitly:

```bash
cachix watch-exec ornl-slicer -- \
  nix build -L --no-link \
    .#devShells.x86_64-linux.ornlslicerDev \
    --accept-flake-config
```

To build and publish the AppImage bundle:

```bash
cachix watch-exec ornl-slicer -- \
  nix bundle -L \
    --accept-flake-config \
    --bundler .#appimage \
    .#ornl.ornlslicer \
    -o ornlslicer.appimage
```

This is the preferred workflow when the required outputs are not already in the
local Nix store. Only publish reviewed or otherwise trusted source revisions.

## Publish an Existing Result

`watch-exec` observes newly created store paths. If the requested output already
exists locally, build evaluation may be a no-op and there will be nothing for it
to observe. Push the existing package and its runtime closure instead:

```bash
nix build --no-link --print-out-paths \
  --accept-flake-config \
  .#ornl.ornlslicer \
  .#windows.ornl.ornlslicer \
  .#windowsInstaller \
  | cachix push ornl-slicer
```

This is sufficient to accelerate an exact rebuild of the same source revision:
Nix can substitute the completed package instead of rebuilding it.

`nix bundle` creates an output link rather than supporting
`--print-out-paths`. Push an existing AppImage store path with:

```bash
cachix push ornl-slicer "$(readlink -f ornlslicer.appimage)"
```

## Publish Build-Time Dependencies

Pushing the package output includes its runtime closure, but not every tool used
to build it. To warm build-time dependencies for later source revisions, publish
the realized outputs in the package's derivation closure:

```bash
drv_path="$(nix path-info --accept-flake-config \
  --derivation .#ornl.ornlslicer)"

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

`--accept-flake-config` accepts the repository's requested settings, but does
not grant an unprivileged user permission to change the daemon's cache trust.
These warnings are separate from TLS certificate errors: an ignored cache can
fall back to other sources, while a certificate error can block downloads.
Resolve [macOS certificate errors](development-setup.md#macos-nix-downloads-fail-with-a-certificate-error)
before troubleshooting cache access.

An administrator can allow the project cache when requested by the flake. Edit
the existing `/etc/nix/nix.conf` with `sudoedit`, preserving other settings and
merging with any existing entries for these keys:

```ini
extra-trusted-substituters = https://ornl-slicer.cachix.org
extra-trusted-public-keys = ornl-slicer.cachix.org-1:VIvBeaBy4B47CzxtF7VxZWsK0wXJt4Dm7VNuAb65Nzg=
```

On macOS, reload the daemon configuration with:

```bash
sudo launchctl kickstart -k system/org.nixos.nix-daemon
```

On systemd Linux, use `sudo systemctl restart nix-daemon.service`. Retry
`nix develop .#ornlslicerDev -L --accept-flake-config` after the restart. The
`trusted-public-keys` client warning may still appear because the flake also
requests that restricted setting; with the key configured in the daemon, it
already trusts the project signatures. See the
[Nix cache trust settings](https://nix.dev/manual/nix/2.35/command-ref/conf-file.html#conf-trusted-substituters).
This setup permits the specific cache without adding the developer to
`trusted-users`. Keep signature verification enabled.

Alternatively, if you already have the Cachix CLI, `cachix use ornl-slicer`
can configure cache access; follow its instructions for system-level changes.
See the [Cachix installation guide](https://docs.cachix.org/installation) if the
CLI is not installed.

### `watch-exec` uploads nothing

The output probably existed before `watch-exec` started. Use the existing-result
workflow above, or verify the exact result directly:

```bash
out_path="$(nix path-info --accept-flake-config .#ornl.ornlslicer)"
cachix doctor --cache ornl-slicer "$out_path"
```

### A release pin step fails

First confirm that the release tag is exactly `v` followed by the version in
`version.json`. Then run `cachix doctor --cache ornl-slicer` and confirm that
`CACHIX_AUTH_TOKEN` is a per-cache token with write access. A token that can
push paths is also required to create pins.

Rerun the failed release workflow after correcting the tag or credential. Do
not create a similarly named pin from an unrelated branch merely to make the
workflow green. Verify the four resulting pins in the Pins dashboard before
considering release retention complete.

### An expected cache hit rebuilds locally

Confirm the command includes `--accept-flake-config`, then inspect the exact
output path:

```bash
nix path-info --accept-flake-config .#ornl.ornlslicer
```

If the path differs from the cached revision, compare build-relevant source,
`flake.lock`, package expressions, compiler settings, and target system. Cachix
cannot substitute a different store path even when most inputs are unchanged.

For additional Cachix behavior and command variants, see the official
[pushing guide](https://docs.cachix.org/pushing) and
[pin documentation](https://docs.cachix.org/pins),
[garbage-collection documentation](https://docs.cachix.org/garbage-collection),
and [security documentation](https://docs.cachix.org/security).
