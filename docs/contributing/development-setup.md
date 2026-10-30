# Developer Setup

Linux is the default development environment. Windows contributors use
[WSL2](#windows-wsl2); macOS contributors can use the
[native Nix environment](#macos). After the platform prerequisites, all three
paths use the same clone, configure, build, and test steps below.

The project uses C++23, CMake 3.25 or newer, Clang, Ninja Multi-Config, and Qt 6.
The Nix development shell supplies the compiler, build tools, Qt, geometry
libraries, Python, and formatting tools. You do not need to install those
libraries separately. An editor is optional; [VS Code](#optional-editor-setup)
is configured in the repository.

## Platform prerequisites

### Linux (default)

On Ubuntu 24.04, install the tools needed to fetch the repository and Nix:

```bash
sudo apt update
sudo apt install git git-lfs curl ca-certificates
```

On other distributions, install the equivalent packages using the system
package manager. Use a desktop session with OpenGL support to run the GUI.

Continue with [Install Nix](#install-nix).

### Windows (WSL2)

Development runs inside Linux under WSL2. The shared CMake commands build a
Linux application; native Windows packages use the cross-build commands below.

1. In an administrator PowerShell, install Ubuntu:

   ```powershell
   wsl --install -d Ubuntu-24.04
   ```

2. Restart if prompted, launch Ubuntu, and create your Linux user account.
3. Check the distribution uses WSL2:

   ```powershell
   wsl --list --verbose
   ```

   If necessary, convert it with `wsl --set-version Ubuntu-24.04 2`.
4. In Ubuntu, follow the [Linux prerequisites](#linux-default), then the shared
   Nix and build instructions. Keep the checkout in the Linux filesystem
   (for example `~/develop/ORNLSlicer`), rather than under `/mnt/c`.

For current Windows requirements and installation troubleshooting, see
[Microsoft's WSL installation guide](https://learn.microsoft.com/en-us/windows/wsl/install).
Running the GUI requires working WSL GUI support (WSLg); update WSL with
`wsl --update` from PowerShell if needed. On ORNL-managed machines, consult IT
and the [historical ORNL WSL2 procedure](ornl-wsl2.md) for local provisioning
context.

To produce native Windows artifacts from an `x86_64-linux` host, including
an x86-64 WSL2 environment, run these commands from the repository root:

```bash
nix build -L .#windows.ornl.ornlslicer --accept-flake-config -o ornlslicer-windows
nix build -L .#windowsInstaller --accept-flake-config -o ornlslicer-installer
```

The first output contains the Windows application; the second contains the
installer executable. The installer output is defined only for `x86_64-linux`.
These are the cross-build outputs used by CI; this guide does not provide a
native MSVC development environment.

### macOS

Install Apple's Command Line Tools if they are not already present:

```bash
xcode-select --install
```

Install Git LFS using the [Git LFS installation instructions](https://git-lfs.com/)
(or `brew install git-lfs` if you already use Homebrew). Git and curl are
available with the platform tools. Continue with the shared Nix installation
and build steps in a native terminal for your Mac's architecture.

The flake exposes development shells for Apple Silicon and Intel macOS.
Current CI runs flake checks on macOS, but does not compile or launch the macOS
application. Treat the native build as less validated than Linux and include
`uname -m`, `nix --version`, and the complete error output when reporting a
platform-specific failure. The AppImage and Windows installer workflows require
Linux.

## Install Nix

If Nix is already installed, check `nix --version` and continue to the flake
configuration below. Otherwise, use the
[official Nix installation guide](https://nix.dev/manual/nix/latest/installation/installing-binary.html).
For Linux with systemd and macOS, the multi-user installation is:

```bash
bash <(curl -L https://nixos.org/nix/install) --daemon
```

Open a new terminal after installation so `nix` is on `PATH`. On WSL, run the
installer inside Ubuntu; if systemd is unavailable, follow the installer's
platform guidance before continuing.

Enable the `nix-command` and `flakes` experimental features in your user Nix
configuration (`~/.config/nix/nix.conf` by default). Create the directory/file
if needed and add this line, merging with any existing feature list:

```ini
experimental-features = nix-command flakes
```

See the [Nix configuration reference](https://nix.dev/manual/nix/latest/command-ref/conf-file.html)
for configuration locations. The repository declares the `ornl-slicer` binary
cache and signing key in `flake.nix`; `--accept-flake-config` accepts those
settings. If an existing multi-user installation rejects the cache as
untrusted, follow [Cachix troubleshooting](cachix.md#nix-ignores-the-substituter-as-untrusted).
Cache write access is optional and is not needed to develop.

## Clone and restore assets

Run these commands in your Linux, WSL Ubuntu, or macOS terminal:

```bash
git lfs install
mkdir -p ~/develop
cd ~/develop
git clone https://github.com/ORNLSlicer/ORNLSlicer.git
cd ORNLSlicer
git lfs pull
```

HTTPS cloning does not require SSH keys. If you prefer SSH, use GitHub's
[SSH setup instructions](https://docs.github.com/en/authentication/connecting-to-github-with-ssh)
and see [connection troubleshooting](#github-connection-issues).

Run `git lfs pull` in each new checkout or worktree before configuration.
If the objects are already cached, `git lfs checkout` restores them without
downloading. Text pointers in place of images produce missing toolbar icons
and arrows; configuration/build checks reject unresolved bundled resources.
After restoring assets, configure and rebuild to regenerate the Qt resources.

## Configure, build, and run

From the repository root, enter the development shell and configure this
checkout before the first build:

```bash
nix develop .#ornlslicerDev -L --accept-flake-config
cmake --preset generic-llvm-ninja
cmake --build build/generic-llvm-ninja --config Debug --target ornlslicer -j 8
./build/generic-llvm-ninja/Debug/ornlslicer
```

Keep running build and test commands inside this shell. The initial shell
entry can take time while Nix downloads or builds dependencies. Adjust `8` to
fit your machine's CPU and memory. Replace `Debug` with `Release` for an
optimized build; both configurations share the same build tree. Reconfigure
after CMake/dependency changes and after restoring missing LFS resources.

## Run tests and formatting

Build **all** targets before running the full CTest suite; building only
`ornlslicer` does not build the test executables:

```bash
cmake --build build/generic-llvm-ninja --config Debug -j 8
ctest --test-dir build/generic-llvm-ninja -C Debug --output-on-failure
```

For focused iteration, build and run just the relevant test, for example:

```bash
cmake --build build/generic-llvm-ninja --config Debug --target ornlslicer_settings_manager_tests -j 8
ctest --test-dir build/generic-llvm-ninja -C Debug -R '^settings_manager_tests$' --output-on-failure
```

CMake test target names use the `ornlslicer_` prefix; CTest names do not.
Install the formatting hook once per checkout from the development shell:

```bash
pre-commit install
pre-commit run
```

The second command checks staged files. See [Formatting](formatting.md) and
the [contributor workflow](../../CONTRIBUTING.md) before opening a PR.
For settings changes, see [Creating and Updating Settings](settings.md).

## Optional editor setup

For VS Code, install the workspace recommendations in `.vscode/extensions.json`
(CMake Tools, clangd, LLDB DAP, EditorConfig, Nix, direnv, and Clang-Format).
On Windows, install VS Code on the host and its WSL extension, then open the
Linux checkout through **WSL: Connect to WSL**.

Launch the editor from the Nix shell with `code .` so its build tools inherit
the environment. Select the `generic-llvm-ninja` configure preset, the Debug
build preset, and `ornlslicer` as the build and launch target. For LLDB DAP,
find the debugger inside the shell:

```bash
which lldb-dap
```

Set that path in the extension's **Lldb-dap: Executable Path** setting. The
repository launch configuration uses the CMake launch target.

For automatic environment loading, install `direnv` with `nix-direnv` integration,
add the appropriate shell hook, and run `direnv allow` in this trusted checkout.
The repository `.envrc` uses `use flake`. See the
[nix-direnv setup instructions](https://github.com/nix-community/nix-direnv#installation)
for your shell. Explicit `nix develop` works without direnv.

## Troubleshooting

### Missing tools or build files

If `clang`, Qt, or Ninja is missing, enter the development shell again:

```bash
nix develop .#ornlslicerDev -L --accept-flake-config
```
 If the build directory does not exist or CMake
reports a cache from another checkout, configure this checkout with
`cmake --preset generic-llvm-ninja` before building. Do not reuse a CMake build
tree from a different worktree.

### Missing images or unresolved LFS pointers

Run `git lfs pull` (or `git lfs checkout` for cached objects), then configure
and rebuild. See [Clone and restore assets](#clone-and-restore-assets).

### GUI cannot connect to a display

Run from a desktop session with OpenGL support. Under WSL, check WSLg and
update WSL from PowerShell. A headless build/test session does not establish
that the interactive GUI works.

### GitHub connection issues

For SSH authentication, follow GitHub's
[SSH troubleshooting guide](https://docs.github.com/en/authentication/troubleshooting-ssh).
If your network blocks port 22, GitHub supports
[SSH over port 443](https://docs.github.com/en/authentication/troubleshooting-ssh/using-ssh-over-the-https-port).
Add the following to `~/.ssh/config` if using that route:

```sshconfig
Host github.com
    Hostname ssh.github.com
    Port 443
```

Alternatively, use the HTTPS clone URL above. If setup still fails,
[open an issue](https://github.com/ORNLSlicer/ORNLSlicer/issues) with your
platform, command, and complete error output.
