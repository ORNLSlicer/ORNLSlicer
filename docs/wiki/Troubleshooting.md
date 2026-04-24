## SSH Keys

Setting up SSH keys allows you to authenticate with GitHub securely. **For more details, see** [GitHub's official SSH documentation](https://docs.github.com/en/authentication/connecting-to-github-with-ssh).

- **Generate** a new `SSH key`:
  ```
  ssh-keygen -t rsa
  ```
  - **Press** `Enter` to accept the default save location.
  - **Press** `Enter` to select no passphrase. Press `Enter` again to confirm no passphrase.
- **Copy** your public `SSH key`:
  ```
  cat ~/.ssh/id_rsa.pub
  ```
  - **Select** and copy the entire output.
- **Add** the `SSH key` to your GitHub account:
  - **Log in** to GitHub.
  - **Click** your avatar and select `Settings`.
  - **Navigate** to `SSH and GPG keys` in the left-hand menu.
  - **Select** `New SSH key`.
  - **Paste** the copied key into the `Key` field.
  - **Set** a `Title` (e.g., `"WSL SSH key"`).
  - **Click** `Add SSH key`.


## Connection Issues

If you are encountering an authentication or permission issue when you attempt to clone `ORNLSlicer`, it is most likely due to your network's firewall blocking port 22, which is used for SSH communication. To circumvent this, you need to redirect your SSH client to connect to GitHub on port 443.

- Navigate to your SSH configuration file located at `/home/<username>/.ssh/config`. Create this configuration file if it does not exist and insert the following:
  ```
  Host github.com
      Hostname ssh.github.com
      Port 443
  ```
  > Make sure your new file does not have an extension. You can double check this by going to `View > File name extensions` in Windows explorer.
- Add a key to allow SSH access. See the instructions [here](#ssh-keys).
