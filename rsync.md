- # Rsync Command Examples
	```bash
	  # sync remote directory to local
	  rsync -avz admin@192.168.109.166:/home/admin/myNote /home/morrism
	  # sync local directory to remote
	  rsync -avz /home/morrism/myNote admin@192.168.109.166:/home/admin/

	  # Copy a Single File Locally
	  rsync -v /home/test/Desktop/sample.txt /home/test/Desktop/rsync/
	  
	  # Copy Multiple Files Locally
	  rsync -v /home/test/Desktop/sample.txt /home/test/Desktop/sample2rs.txt /home/test/Desktop/rsync
	  
	  # Copy a Directory and All Subdirectories Locally (Copy Files and Directories Recursively)
	  rsync -av /home/test/Desktop/Linux /home/test/Desktop/rsync
	  
	  
	  # Copy a File or Directory from Local to Remote Machine
	  rsync -av /home/test/Desktop/Linux 192.168.56.100:/home/test/Desktop/rsync
	  rsync -a /home/test/Desktop/Linux jay@192.168.56.100:/home/test/Desktop/rsync
	  
	  # Copy Multiple Files or Directories from Local to Remote Machine
	  rsync -av /home/test/Desktop/Linux/ /home/test/Music 192.168.56.100:/home/test/Desktop/rsync
	  
	  # Specify rsync Protocol for Remote Transfers
	  rsync -e ssh /home/test/Desktop/sample.txt 192.168.56.100:/home/test/Desktop
	  
	  # Copy a File or Directory from a Remote to a Local Machine
	  rsync -av 192.168.56.100:/home/test/Desktop/DirM /home/test/Desktop
	  rsync -v 192.168.56.100:/home/test/Desktop/testfile.txt /home/test/Desktop
	  
	  # Copy Multiple Files or Directories from Local to Remote Machine
	  rsync -av 192.168.56.100:{/home/test/Desktop/DirM,/home/test/Desktop/Dir1} /home/test/rsync
	  
	  # Show rsync Progress During Data Transfer
	  rsync -av --progress /home/test/Desktop/Dir1 192.168.56.100:/home/test/Desktop/rsync
	  
	  # Delete a Nonexistent Source File or Directory from Destination
	  rsync -av --delete /home/test/Desktop/Dir1 192.168.56.100:/home/test/Desktop/rsync
	  
	  # Delete Source Files After Transfer
	  rsync -v --remove-source-files /home/test/backup/weekly.zip 192.168.56.100:/home/test/Desktop/rsync/
	  
	  # Rsync Dry Run
	  rsync -av --dry-run --delete /home/test/Desktop/Dir1 192.168.56.100:/home/test/Desktop/rsync
	  
	  # Set Maximum File Size for Transfer
	  rsync -av --max-size=500k /home/test/Desktop/Dir1 192.168.56.100:/home/test/Desktop/rsync/
	  
	  # Set Minimum File Size for Transfer
	  rsync -av --min-size=10k /home/test/Desktop/ 192.168.56.100:/home/test/Desktop/rsync/
	  
	  # Set rsync Bandwidth Limit
	  rsync -av --bwlimit=50 --progress /home/test/Desktop/Dir1 192.168.56.100:/home/test/Desktop/rsync/
	  
	  # Copy Specific File Type
	  rsync -v /home/test/Documents/*.txt /home/test/Desktop/rsync/
	  
	  # Copy Directory Structure but Skip Files
	  rsync -av -f"+ */" -f"- *"  /home/test/Desktop/Linux /home/test/Documents
	  
	  # Add Date Stamp to Directory Name
	  rsync -av /home/test/Desktop/Linux /home/test/Desktop/rsync$(date +%Y-%m-%d)
	  
	  # Do Not Copy Source File if the Same Destination File is Modified
	  rsync -avu /home/test/Desktop/Linux/ /home/test/Desktop/rsync
	  
	  # Show the Difference Between the Source and Destination Files
	  rsync -avi /home/test/Desktop/Linux/ /home/test/Desktop/rsync
	  ```
- # 参考
	- [man rsync](https://linux.die.net/man/1/rsync)
	- [Rsync Command Examples](https://phoenixnap.com/kb/rsync-command-linux-examples)