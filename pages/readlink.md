- readlink - print resolved symbolic links or canonical file names
	- ```bash
	  -f, --canonicalize
	     canonicalize by following every symlink in every component of the given name recursively;
	     all but the last component must exist
	  ```
- 示例
	- ```bash
	  # 显示文件的标准路径
	  morrism@localhost /tmp/x $ ls txt1
	  txt1
	  morrism@localhost /tmp/x $ readlink -f txt1
	  /tmp/x/txt1
	  ```