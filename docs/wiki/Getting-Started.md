# Development Environment Setup (Windows via WSL2)

Setting up the development environment on **Windows** is streamlined using **WSL2** (Windows Subsystem for Linux), which enables Linux-based development and cross-compilation for native Windows executables. This approach improves automation and maintainability.

These instructions are tailored for **Windows + WSL2**, but much of the setup is applicable to **Linux** and **macOS** as well—simply skip WSL-specific steps and use your native shell environment.

> **Note:** At this time, we only support **Visual Studio Code (VSCode)** as the development environment.


## 1. Enable Windows Long Path Support

Windows limits file paths to 260 characters by default, which can break builds with deep directory structures. To enable long path support:

1. **Open** a **PowerShell** window as **Administrator**.
2. **Run** the following command:
   ```powershell
   New-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem" `
                   -Name "LongPathsEnabled" -Value 1 -PropertyType DWORD -Force
   ```
3. **Restart** your computer to apply the change.

📘 [More info](https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation?tabs=powershell)


## 2. Install WSL2 (Ubuntu 24.04)

**WSL2** provides a native Linux experience inside Windows. We recommend using **Ubuntu 24.04**.

- **For ORNL-managed machines**, follow the [ORNL-specific WSL2 guide](ORNL-WSL2-Installation).
- **For all others**:
  
  1. Open a **PowerShell** window.
  2. **Run**:
     ```powershell
     wsl --install -d Ubuntu-24.04
     ```
  3. When prompted, create a **username** and **password** (you'll use these frequently).
  4. After installation, you'll be dropped into a Linux shell.

     > From this point forward, **all commands should be run inside the Ubuntu shell**. To re-enter later, open PowerShell and run:
       ```powershell
       wsl
       ```

- **Update package lists**:
     ```bash
     sudo apt update
     ```


## 3. Install `direnv`

[`direnv`](https://direnv.net/) automatically loads environment variables based on the current directory.

```bash
sudo apt install direnv
```


## 4. Install Git LFS

[Git Large File Storage (LFS)](https://git-lfs.com/) manages large binary files in Git repositories.

```bash
sudo apt install git-lfs
git lfs install
```


## 5. Configure Git SSH Access

Authenticate with GitHub securely using SSH keys:

1. **Generate a new SSH key**:
   ```bash
   ssh-keygen -t rsa
   ```
   - Press `Enter` to accept default paths.
   - Press `Enter` twice to skip setting a passphrase.

2. **Display your public key**:
   ```bash
   cat ~/.ssh/id_rsa.pub
   ```
   - Copy the entire output.

3. **Add the key to GitHub**:
   - Log in to GitHub.
   - Go to **Settings > SSH and GPG Keys**.
   - Click **New SSH Key**.
   - Paste your key, give it a name (e.g., *WSL SSH*), and click **Add SSH key**.


## 6. Clone the `ORNLSlicer` Repository

1. **Create a working directory**:
   ```bash
   mkdir -p ~/develop
   cd ~/develop
   ```

2. **Clone the repository**:
   ```bash
   git clone git@github.com:ORNLSlicer/ORNLSlicer.git
   ```

   > If prompted, type `yes` to confirm the SSH connection. If nothing happens, click this link:

   🔧 [Troubleshooting SSH Connection Issues](Troubleshooting#connection-issues)


## 7. Install Nix

Nix is used to provide reproducible development environments.

1. **Run**:
   ```bash
   curl --proto '=https' --tlsv1.2 -sSf -L https://install.determinate.systems/nix | \
   sh -s -- install --extra-conf "trusted-users = $USER"
   ```
   > ⚠️**When prompted to install Determinate Nix, type** `no`.

   > You will then be prompted to install Nix, type `yes`.

   > To get started using Nix, open a new shell or run `. /nix/var/nix/profiles/default/etc/profile.d/nix-daemon.sh`

2. **Enter the development shell**:
   ```bash
   cd ~/develop/ORNLSlicer/
   nix develop -L
   ```

   > This may take time—downloads and builds will run automatically. Approve any prompts.


## 8. Set Up Visual Studio Code

1. **Install VSCode**:
   - Download from [code.visualstudio.com](https://code.visualstudio.com/download) (choose the *User Installer for Windows*).

2. **Launch VSCode**.
3. **Connect to WSL**:
   - Click the green icon in the bottom-left corner.
   - Choose **WSL: Connect to Ubuntu-24.04**.
4. **Open the project**:
   - Go to **File > Open Folder**.
   - Navigate to `/home/<your-username>/develop/ORNLSlicer`.

5. **Install recommended extensions**:
   - Click the **Extensions** icon (left toolbar).
   - Search for `@recommended`.
   - Click the **Install Workspace Recommended Extensions** icon.
   - ✅ Ensure the following are installed:
     - `Clang-Format`
     - `clangd`
     - `CMake Tools`
     - `direnv`
     - `EditorConfig for VS Code`
     - `github.copilot` (optional)
     - `LLDB DAP`
     - `Nix`
     - `WSL`


## 9. Build and Run ORNLSlicer

1. **Open a terminal** in VSCode: `Terminal > New Terminal`.

2. **Configure the LLDB Debugger**:
   - Run:
     ```bash
     nix develop --command which lldb-dap
     ```
   - Copy the output path.
   - In VSCode:
     - Go to the **LLDB DAP** extension settings.
     - Paste the copied path into `Lldb-dap: Executable Path`.

3. **Build and Launch**:
   - Click the `Build` icon in the bottom-left, then choose `Generic (llvm-ninja)`.
   - Open the command pallet (ctrl + shift + p) and search for direnv
     - Select the option for direnv: Reset and reload environment
     - When that is complete, select the blue restart button at the bottom right
   - In the **CMake** tab under **Project Outline**:
     - Right-click an executable (e.g., `ornlslicer`).
     - Select **Set as Build Target** and **Set as Launch/Debug Target**.
   - Go to **Run > Start Debugging**.


## 💡 Need Help?

If you encounter issues or unclear steps, please [create an issue](https://github.com/ORNLSlicer/ORNLSlicer/issues) describing the problem and steps to reproduce it.
