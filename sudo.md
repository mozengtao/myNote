- ```bash
  #1 Adding User to the Sudoers File  /etc/sudoers
  sudo cp /etc/sudoers{,.backup_$(date +%Y%m%d)}
  # When making changes to the sudoers file always use visudo. This command checks the file after 
  editing, and if there is a syntax error it will not save the changes. If you open the file with 
  a text editor, a syntax error will result in losing the sudo access.
  sudo visudo
  linuxize  ALL=(ALL) NOPASSWD:ALL
  linuxize ALL=(ALL) NOPASSWD:/bin/mkdir,/bin/mv
  
  
  #2 Using /etc/sudoers.d
  sudo nano /etc/sudoers.d/linuxize
  linuxize  ALL=(ALL) NOPASSWD:ALL
  
  
  # verify the sudo privileges for user account
  morrism@r660atc:~$ sudo -l
  Matching Defaults entries for morrism on r660atc:
      env_reset, mail_badpass, secure_path=/usr/local/sbin\:/usr/local/bin\:/usr/sbin\:/usr/bin\:/sbin\:/bin\:/snap/bin, use_pty
  
  User morrism may run the following commands on r660atc:
      (ALL) NOPASSWD: ALL
      (ALL : ALL) ALL
  
  ```
- [How to Disable the Sudo Command Password](https://gcore.com/learning/how-to-disable-password-for-sudo-command/)
- [How to set up passwordless sudo in Linux](https://www.simplified.guide/linux/enable-passwordless-sudo)
- [How to Run Sudo Command Without Password](https://linuxize.com/post/how-to-run-sudo-command-without-password/)