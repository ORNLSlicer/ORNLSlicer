To conform to the enforced security protocols on an ORNL computer, `WSL` must be installed using the following instructions. These instructions are specific to the Windows 10 operating system. If using Windows 11 or newer, some details may have changed.

1. **Open** the `Privilege Elevation Tool - Self Service` application and generate a request with the following information:
    - **Reason for Privilege Elevation**: `WSL install`
    - **Category of Privilege Elevation**: `Install/Uninstall Software`
    > If you do not have access to the tool, you can request it via `UCAMS` or ask a team member to assist you.
2. **Save** the generated credentials for use in the next step.
3. **Logout** of your user account and login as the admin user using the credentials you just generated.
4. **Open** a Powershell and install `WSL`:
    - **Update** `WSL`:
      ```
      wsl --update
      ```
      > If a help message appears, you can ignore this step and `WSL` will be updated when installed.
    - **Install** `WSL`:
      ```
      wsl --install -d Debian
      ```
5. **Restart** your computer and login under your regular user account.
6. **Open** a PowerShell and install `Ubuntu`:
    - **Run**:
      ```
      wsl --install -d Ubuntu-24.04
      ```
    - When prompted to create a user account:
        - Use your 3-char id for the username.
        - Use any password you would like.
        - Save these credentials.
      > When complete, you will be in an Ubuntu bash shell.
7. **Configure** Ubuntu's DNS servers to use the internal resolution:
    - **Run**:
      ```
      sudo bash -c 'echo -e "[network]\ngenerateResolvConf = false" >> /etc/wsl.conf'
      ```
      > This disables the DNS pass-through from Windows to WSL.
    - When prompted for your password, use the password you just set for your Ubuntu account.
    - **Open** another Powershell and retrieve your host DNS server:
      ```
      Get-DnsClientServerAddress -AddressFamily IPv4
      ```
      > You will likely see two or more DNS servers, you can use the first address available.
    - **Run**:
       ```
       sudo bash -c 'echo -e "DNS=[insert_dns_here]" >> /etc/systemd/resolved.conf'
       ```
      > Remember to replace `[insert_dns_here]` with the upstream server found in the previous step. This enables the DNS resolver from systemd and uses the upstream DNS servers directly.
8. **Restart** `WSL` and set `Ubuntu` as the default.
    - **Exit** the Ubuntu bash shell:
      ```
      exit
      ```
      > This should put you back in the Powershell environment.
    - **Shutdown** `WSL`: 
      ```
      wsl --shutdown
      ```
    - **Set** Ubuntu 24.04 as the default:
      ```
      wsl --set-default Ubuntu-24.04
      ```
    - **Restart** `WSL`:
      ```
      wsl
      ```
    - Once back in the Ubuntu bash shell, test the DNS configuration:
      ```
      ping -c 10 ornl.gov
      ```
      > If configured correctly, you should see a response from `ornl.gov`. If nothing happens, there is likely an issue with your DNS configuration.
9. **Update package lists**:
     ```bash
     sudo apt update
     ```

WSL should now be installed!