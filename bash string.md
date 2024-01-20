- **字符串长度**
	- ==${#string}==
	  ```sh
	  distro="Ubuntu"
	  echo ${#distro}        -->    6
	  ```
-
- **连接字符串**
	- `${str1}${str2}`
	  ```sh
	  str1="hand"
	  str2="book"
	  
	  str3=$str1$str2        -->    "handbook"
	  ```
-
- **子字符串索引**
	- `expr index "${sub_str}" "${str}"`
	  ```sh
	  str="Bash is Cool"
	  word="Cool"
	  
	  expr index "$str" "$word"    -->    9
	  ```
-
- **子字符串提取**
	- `${string:position}, ${string:position:length}`
	  ```sh
	  foss="Fedora is a free operating system"
	  
	  echo ${foss:0:6}        -->    "Fedora"
	  echo ${foss:12}         -->    "free operating system"
	  ```
-
- **子字符串替换**
	- ```sh
	  str="aa bb bb aa"
	  ```
	- 只替换首次匹配
		- `${string/pattern/replacement}`
		  ```sh
		  echo ${str/aa/cc}	--->	cc bb bb aa
		  ```
-
	- 替换所有匹配
		- `${string//pattern/replacement}`
		  ```sh
		  echo ${str//aa/cc}	--->	cc bb bb cc
		  ```
- 替换从左开始的匹配
  collapsed:: true
	- `${string/#pattern/replacement}`
	  ```sh
	  echo ${str/#aa/cc}	--->	cc bb bb aa
	  ```
- 替换从右开始的匹配
  collapsed:: true
	- `${string/%pattern/replacement}`
	  ```sh
	  echo ${str/%aa/cc}	--->	aa bb bb cc
	  ```
- **子字符串删除**
  id:: 63686f62-392a-43f9-ac9e-5ac31496a636
	- ```sh
	  str="aa bb bb aa"
	  ```
-
	- 删除首次匹配
		- ```sh
		  echo ${str/aa}	--->	bb bb aa
		  ```
-
	- 删除所有匹配
		- ```sh
		  echo ${str//aa}	--->	bb bb
		  ```
-
- **字符串大小写转换**
	- ```sh
	  legend="john nash"
	  actor="JULIA ROBERTS"
	  
	  echo ${legend^^}            -->    "JOHN NASH"
	  echo ${actor,,}            -->    "julia roberts"
	  echo ${legend^}            -->    "John nash"
	  echo ${actor,}             -->    "jULIA ROBERTS"
	  echo ${legend^^[jn]}        -->    "JohN Nash"
	  ```
-
- **子字符串匹配删除**
	- 最短匹配
		- ```sh
		  filename="bash.string.txt"
		  ```
		- 从前开始匹配
			- ```sh
			  echo ${filename#*.}    -->    "string.txt"
			  ```
		- 从后开始匹配
			- ```sh
			  echo ${filename%.*}    -->    "bash.string"
			  ```
-
	- 最长匹配
		- 从前开始匹配
			- ```sh
			  echo ${filename##*.}    -->    "txt"
			  ```
		- 从后开始匹配
			- ```sh
			  echo ${filename%%.*}    -->    "bash"
			  ```
- 正则匹配
	- ```bash
	  #!/bin/bash
	  file="repair-report-12.5.pdf"
	  pattern='([0-9]*\.[0-9]*\.pdf)'
	  
	  if  [[ $file =~ $pattern ]]; then
	      echo ${BASH_REMATCH[1]}
	  else
	      echo "No version found"
	  fi
	  ```
	- [Matching regex in bash](https://thedukh.com/2022/10/matching-regex-in-bash/)