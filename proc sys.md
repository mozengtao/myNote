- `/proc/sys` 是linux下的一个目录，用来读取和修改内核相关的参数
  > _/proc/sys_ is a ==directory== (present since 1.3.57) contains a number of files and subdirectories corresponding to ==kernel variables==.  These variables can be read and in some cases
    modified using the /[[proc]] filesystem, and the [[sysctl]] system call.
- `/proc/sys` 目录结构
	- ```sh
	  /proc/sys
	  ├── abi       --- application binary information
	  ├── crypto    --- 
	  ├── debug     --- 
	  ├── dev       --- device-specific information
	  ├── fs        --- filesystem information
	  ├── kernel    --- kernel parameters
	  ├── net       --- networking stuff
	  ├── user      --- namespace information
	  └── vm        --- memory management tuning, buffer, and cache management
	  ```