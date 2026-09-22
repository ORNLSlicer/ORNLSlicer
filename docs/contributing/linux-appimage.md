# Linux AppImage Packaging

ORNLSlicer is distributed for x86-64 Linux as a type-2 AppImage. The AppImage
contains the complete Nix store closure needed by the application; users do not
need Nix installed on the host.

## Runtime Design

The AppImage runtime mounts its SquashFS payload and invokes `AppRun`. ORNLSlicer's
`AppRun` uses the statically linked
[PRoot 5.4.1](https://github.com/proot-me/proot/releases/tag/v5.4.1) runtime to
make the payload's `nix` directory visible to the application at `/nix`. PRoot
performs this path translation with same-user `ptrace`; it does not create a
Linux user namespace or run the application with elevated privileges.

The application keeps the invoking user's UID, environment, working directory,
home directory, and normal access to the host filesystem. PRoot provides path
translation, not a security sandbox.

## Host Requirements

The release AppImage supports x86-64 Linux systems with:

- A working type-2 AppImage/FUSE mount path.
- `/bin/sh` for the `AppRun` launcher.
- Permission for a process to trace its own child processes with `ptrace`.

Ubuntu 24.04's default AppArmor restriction on unprivileged user namespaces is
compatible with this runtime. Do not launch ORNLSlicer with `sudo`, disable
`kernel.apparmor_restrict_unprivileged_userns`, or weaken AppArmor policy. Those
workarounds are unnecessary and can create root-owned user files.

## Build

Build the Linux application and create the same AppImage used by CI:

```sh
nix build -L .#legacyPackages.x86_64-linux.ornl.ornlslicer \
  --accept-flake-config
nix bundle -L \
  --bundler .#appimage \
  .#legacyPackages.x86_64-linux.ornl.ornlslicer \
  -o ornlslicer.appimage
```

The bundler accepts either a derivation or a flake app and retains the public
`nix bundle --bundler .#appimage` shorthand when run on x86-64 Linux.

## Ubuntu 24.04 Validation

Use a normal user account on a default, non-NixOS Ubuntu 24.04 installation.
Confirm the security setting and exercise the packaged command-line entrypoint:

```sh
test "$(id -u)" -ne 0
test "$(sysctl -n kernel.apparmor_restrict_unprivileged_userns)" = "1"
test ! -d /nix/store
chmod +x ./ornlslicer.appimage
timeout 30s ./ornlslicer.appimage --help
```

Launch `./ornlslicer.appimage` without `sudo` and verify all of the following:

1. The main window opens without a `cannot write uid_map` error.
2. Native file dialogs start in `/home/<user>`, not `/root`.
3. Importing and exporting application preferences succeeds.
4. Adding an additional settings location succeeds.
5. Files created by those workflows are owned by the invoking user; verify with
   `stat -c '%U:%G %n' <file>`.

Record the Ubuntu version, kernel version, AppImage filename, sysctl value, and
results of these checks in the release pull request.
