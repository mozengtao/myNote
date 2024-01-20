- 语法
	- ```bash
	  chmod WhoWhatWhich file | directory
	  
	  Who - represents identities: u,g,o,a (user, group, other, all)
	  What - represents actions: +, -, = (add, remove, set exact)
	  Which - represents access levels: r, w, x (read, write, execute)
	  
	  示例
	  	chmod ug+rw test.txt
	  ```
- 特殊权限
	- user + s (pecial)
		- SUID
		- A file with **SUID** always executes as the user who owns the file, regardless of the user passing the command. If the file owner doesn't have execute permissions, then use an uppercase **S** here（文件执行时，调用者暂时获得该文件拥有者的权限）
		- 应用示例
			- ```bash
			  morrism@localhost ~/repos/meta-gs (develop) $ ls -l /usr/bin/passwd
			  -rwsr-xr-x. 1 root root 33600 Apr  7  2020 /usr/bin/passwd
			  
			  普通用户可以修改自己的密码
			  ```
	- group + s (pecial)
		- SGID
		- If set on a file, it allows the file to be executed as the **group** that owns the file (similar to SUID) （文件执行时，暂时获得改文件所属组的权限）
		- If set on a directory, any files created in the directory will have their **group** ownership set to that of the directory owner （当用户对某一目录有写和执行权限时，该用户就可以在该目录下建立文件，如果该目录用SGID修饰，则该用户在这个目录下建立的文件都是属于这个目录所属的组）
	- other + t (sticky)
		- "sticky bit."
		- This permission does not affect individual files. However, at the directory level, it restricts file deletion. Only the **owner** (and **root**) of a file can remove the file within that directory. （它出现在其他用户权限的执行位上，只能用来修饰一个目录。当某一个目录拥有SBIT权限时，则任何一个能够在这个目录下建立文件的用户，该用户在这个目录下所建立的文件，只有该用户自己和root可以删除，其他用户均不可以）
		- 应用示例
			- ```bash
			  morrism@localhost /tmp/x $ ll -d /tmp
			  drwxrwxrwt. 13 root root 4096 Jul  4 09:52 /tmp
			  ```
- 参考文档
	- [Linux permissions: SUID, SGID, and sticky bit](https://www.redhat.com/sysadmin/suid-sgid-sticky-bit)
	- [What is SUID, SGID and Sticky bit ?](https://www.thegeekdiary.com/what-is-suid-sgid-and-sticky-bit/)
	- [How Do I Set Up Setuid, Setgid, and Sticky Bits on Linux?](https://www.liquidweb.com/kb/how-do-i-set-up-setuid-setgid-and-sticky-bits-on-linux/)
	- [euid, ruid, suid](https://book.hacktricks.xyz/linux-hardening/privilege-escalation/euid-ruid-suid)
	- [Understanding Unix UIDs](https://dtrugman.medium.com/unraveling-the-unix-uid-conundrum-fbd5b13004fd)
	- [SetUID Rabbit Hole](https://0xdf.gitlab.io/2022/05/31/setuid-rabbithole.html)