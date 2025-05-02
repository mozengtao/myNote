# what does useradd do
	  When we run the ‘useradd‘ command in the Linux terminal, it performs the following major things:
	  
	  1. It edits /etc/passwd, /etc/shadow, /etc/group, and /etc/gshadow files for the newly created user accounts.
	  2. Creates and populates a home directory for the new user.
	  3. Sets permissions and ownerships to the home directory.
```bash
  1. How to Add a New User in Linux
  useradd tecmint
  passwd tecmint
  # Once a new user is created, its entry is automatically added to the ‘/etc/passwd‘ file.
  tecmint:x:1000:1000:tecmint:/home/tecmint:/bin/bash
  Username:Password:User ID (UID):Group ID (GID):User Info:Home Directory:Shell
  
  Username: User login name used to login into the system. It should be between 1 to 32 characters 
  long.
  Password: User password (or x character) stored in /etc/shadow file in an encrypted format.
  User ID (UID): Every user must have a User ID (UID) User Identification Number. By default, UID 
  0 is reserved for the root user and UIDs ranging from 1-99 are reserved for other predefined 
  accounts. Further UIDs ranging from 100-999 are reserved for system accounts and groups.
  Group ID (GID): The primary Group ID (GID) Group Identification Number stored in the /etc/group 
  file.
  User Info: This field is optional and allows you to define extra information about the user. For 
  example, the user’s full name. This field is filled by the ‘finger’ command.
  Home Directory: The absolute location of the user’s home directory.
  Shell: The absolute location of a user’s shell i.e. /bin/bash.
  
  2. Create a User with a Different Home Directory
  useradd -d /data/projects anusha
  
  3. Create a User with a Specific User ID
  useradd -u 1002 navin
  
  4. Create a User with a Specific Group ID
  useradd -u 1005 -g tecmint tarunika
  # verify the user’s GID
  id -gn tarunika
  
  5. Add a User to Multiple Groups
  # create new groups
  groupadd admins
  groupadd webadmin
  groupadd developers
  # add existing user to groups
  usermod -a -G admins,webadmin,developers tecmint
  # add new user to groups
  useradd -G admins,webadmin,developers paddy
  # check user groups
  id tecmint
  
  6. Add a User without Home Directory
  useradd -M shilpi
  
  7. Create a User with an Account Expiry Date
  useradd -e 2021-08-27 aparna
  
  8. Create a User with Password Expiry Date
  useradd -e 2014-04-27 -f 45 mansi
  
  9. Add a User with Custom Comments
  useradd -c "Manis Khurana" mansi
  
  10. Create a User Login Shell in Linux
  useradd -s /sbin/nologin tecmint
  
  11. Add a User with a Specific Home Directory, Default Shell, and Custom Comment
  useradd -m -d /var/www/ravi -s /bin/bash -c "TecMint Owner" -U ravi
  
  12. Add a User with Home Directory, Custom Shell, Custom Comment, and UID/GID
  useradd -m -d /var/www/tarunika -s /bin/zsh -c "TecMint Technical Writer" -u 1000 -g 100 tarunika
  
  13. Add a User with Home Directory, No Shell, Custom Comment, and User ID
  useradd -m -d /var/www/avishek -s /usr/sbin/nologin -c "TecMint Sr. Technical Writer" -u 1019 avishek
  
  14. Add a User with Home Directory, Shell, Custom Skell/Comment, and User ID
  useradd -m -d /var/www/navin -k /etc/custom.skell -s /bin/tcsh -c "No Active Member of TecMint" -u 1027 navin
  
  15. Add a User without Home Directory, No Shell, No Group, and Custom Comment
  useradd -M -N -r -s /bin/false -c "Disabled TecMint Member" clayton
  ```

[The Complete Guide to “useradd” Command in Linux – 15 Practical Examples](https://www.tecmint.com/add-users-in-linux/)  