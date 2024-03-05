bash脚本中调用python返回的结果
```bash
# 常用快捷键
CTRL+A              # 移动到行首，同 <Home>
CTRL+B              # 向后移动，同 <Left>
CTRL+C              # 结束当前命令
CTRL+D              # 删除光标前的字符，同 <Delete> ，或者没有内容时，退出会话
CTRL+E              # 移动到行末，同 <End>
CTRL+F              # 向前移动，同 <Right>
CTRL+K              # 删除光标位置到行末的内容
CTRL+R              # 历史命令反向搜索，使用 CTRL+G 退出搜索
CTRL+T              # 交换前后两个字符
CTRL+U              # 删除字符到行首
CTRL+V              # 输入字符字面量，先按 CTRL+V 再按任意键
CTRL+W              # 删除光标左边的一个单词

ALT+b               # 向后（左边）移动一个单词
ALT+d               # 删除光标后（右边）一个单词
ALT+f               # 向前（右边）移动一个单词
ALT+t               # 交换单词
```

```bash
#!/usr/bin/bash
pc1()
{
	ip_expr="$1"
	if [[ $# -eq 0 || $1 = '-' ]]; then
		read -r ip_expr
	fi

	python -c 'print('"$ip_expr"')'
}

pc2()
{
	python -c 'import sys; print(eval(sys.argv[1]))' "$1" ;
}

pc2()
{
	python -c '\
		import sys;\
		print(eval(sys.argv[1]))\
		' "$1" ;
}

echo '1+2*3' | pc1
pc2 '1+2*3'
```


- `getopts`
	- ```bash
	  #!/bin/bash
	  
	  # Echo usage if something isn't right.
	  usage() { 
	      echo "Usage: $0 [-p <80|443>] [-h <string>] [-f]" 1>&2; exit 1; 
	  }
	  
	  while getopts ":p:h:f" o; do
	      case "${o}" in
	          p)
	              PORT=${OPTARG}
	              [[ $PORT != "80" && $PORT != "443" ]] && usage
	              ;;
	          h)
	              HOST=${OPTARG}
	              ;;
	          f)  
	              FORCE=1
	              ;;
	          :)  
	              echo "ERROR: Option -$OPTARG requires an argument"
	              usage
	              ;;
	          \?)
	              echo "ERROR: Invalid option -$OPTARG"
	              usage
	              ;;
	      esac
	  done
	  shift $((OPTIND-1))
	  
	  # Check required switches exist
	  if [ -z "${PORT}" ] || [ -z "${HOST}" ]; then
	      usage
	  fi
	  
	  echo "p = ${PORT}"
	  echo "h = ${HOST}"
	  ```
- `functions.bash`
	- ```bash
	  check_root () {
	    if [[ $EUID -ne 0 ]]; then
	      echo "This script must be run as root" 
	      exit 1
	    fi
	  }
	  
	  
	  ```
- ```bash
  使用统一的方式执行命令
  #!/bin/bash
  
  startup() {
          case "$1" in
                  *)
                          "$@"
                          ;;
          esac
  }
  
  startup ls -l /tmp
  
  在bash的提示显示git branch，可以放在~/.bashrc文件里
  # git prompt
  function color_my_prompt {
      local __user_and_host="\[\033[01;32m\]\u@\h"
      local __cur_location="\[\033[01;34m\]\w"
      local __git_branch_color="\[\033[31m\]"
      #local __git_branch="\`ruby -e \"print (%x{git branch 2> /dev/null}.grep(/^\*/).first || '').gsub(/^\* (.+)$/, '(\1) ')\"\`"
      local __git_branch='`git branch 2> /dev/null | grep -e ^* | sed -E  s/^\\\\\*\ \(.+\)$/\(\\\\\1\)\ /`'
      local __prompt_tail="\[\033[35m\]$"
      local __last_color="\[\033[00m\]"
      export PS1="$__user_and_host $__cur_location $__git_branch_color$__git_branch$__prompt_tail$__last_color "
  }
  color_my_prompt
  ```
- bash下实现spin功能
	- ```bash
	  # bash实现spin功能
	  #！/usr/bin/bash
	  SPIN='-\|/'
	  function spin {
	          i=0
	          while kill -0 $1 2> /dev/null
	          do
	                  i=$(( (i+1)%4 ))
	                  printf "\b${SPIN:$i:1}"
	                  sleep .1
	          done
	          printf "\bDONE\n"
	  }
	  
	  echo 1
	  sleep 20 &
	  spin $!
	  
	  echo 2
	  sleep 20 &
	  spin $!
	  
	  echo 3
	  sleep 20 &
	  spin $!
	  
	  使用场景：
	  Untaring build/macfie-powerpc.tgz ---> DONE
	  Untaring build/nms-ccap-en.tgz ---> DONE
	  .......
	  
	  
	  # ($!) Expands to the process ID of the job most recently placed into the background, 
	  whether executed as an asynchronous command or using the bg builtin
	  ```
- ```bash
  # while true
  SPIN='-\|/'
  
  i=0
  #while true
  while :
  do
          i=$(( (i+1)%4 ))
          printf "\b${SPIN:$i:1}"
          sleep .1
  done
  
   # turn on extended globbing
  shopt -s extglob 
  $ ls
  a.jpg  b.gif  c.png  d.pdf ee.pdf
  $ ls ?(*.jpg|*.gif)
  a.jpg  b.gif
  
  $ ls !(*.jpg|*.gif)  # not a jpg or a gif
  c.png d.pdf ee.pdf
  
  # 分别根据路径获取目录和文件名
  dirname /path/to/file.txt
  basename /path/to/file.txt
  
  #Bash自动产生的变量 Bash automatically assigns variables that provide information about the current user 
  # about the current user
  UID, EUID, GROUPS
  # aout the current host
  HOSTTYPE, OSTYPE, MACHTYPE, and HOSTNAME
  # about the instance of Bash that is running
  BASH, BASH_VERSION, and BASH_VERSINFO
  
  # 检查当前用户是否为root
  check_root () {
    if [[ $EUID -ne 0 ]]; then
      echo "This script must be run as root" 
      exit 1
    fi
  }
  
  # 重复执行命令多次
  function repeat(){
    for ((i=0;i<$1;i++)); do
      eval ${*:2}
    done
  }
  # usage: repeat 5 echo "1 2 3"
  
  # 获取当前执行脚本的路径
  MY_PATH=$(dirname $(readlink -f ${BASH_SOURCE[0]}))
  PYTHON_PATH=${MY_PATH}/../cathcart-utils/python
  export PYTHONPATH=${PYTHON_PATH}:${PYTHONPATH}
  
  #遍历bash脚本的参数
  i=1;
  j=$;
  while [ $i -le $j ] 
  do
      echo "Username - $i: $1";
      i=$((i + 1));
      shift 1;
  done
  ```
- **BASH_SOURCE**
	- ``` bash
	  BASH_SOURCE
	   An  array  variable  whose  members  are  the source filenames where the corresponding shell 
	   function names in the FUNCNAME array variable are defined.  The shell function ${FUNCNAME[$i]}
	   is defined in the file ${BASH_SOURCE[$i]} and called from ${BASH_SOURCE[$i+1]}.
	  
	  MY_PATH=$(dirname $(readlink -f ${BASH_SOURCE[0]}))
	  PYTHON_PATH=${MY_PATH}/../cathcart-utils/python
	  export PYTHONPATH=${PYTHON_PATH}:${PYTHONPATH}
	  
	  ```
- [[bash string]]
- Bash环境下执行脚本的不同方式
	- ```bash
	  1 source filename (. filename)
	  在当前Bash环境下执行指定脚本，而不会创建新的进程，脚本可以修改当前环境的变量和函数等信息，脚本中
	  的修改会在当前Bash环境中生效
	  意味着，如果在脚本A.sh中通过. B.sh调用脚本B，则脚本A传入的参数对于脚本B同样使用
	  A.sh:
	  #!/usr/bin/bash
	  . ./t1.sh
	  
	  B.sh:
	  usage()
	  {
	      echo "Usage: source yocto-init-build-env OPTIONS"
	      echo "Options:"
	      echo "-u, --update               Update repo and submodules"
	      echo "-d, --directory            <build directory>"
	      echo "-l, --layer                <layer path containing a conf template>"
	      echo "-o, --download             <download directory>"
	      echo "-s, --sstate               <shared state directory>"
	      echo "-m, --manifest             <set source revisions manifest>"
	      echo "-h, --help                 Display this help"
	  }
	  
	  UPDATE=false
	  BDIR=""
	  while [ 0 -ne $# ]; do
	      case "$1" in
	      -u)             UPDATE=true; shift 1;;
	      --update)       UPDATE=true; shift 1;;
	      -d)             BDIR="$2"; shift 2;;
	      --directory)    BDIR="$2"; shift 2;;
	      -l)             LAYER="$2"; shift 2;;
	      --layer)        LAYER="$2"; shift 2;;
	      -o)             DL_DIR="$2"; shift 2;;
	      --download)     DL_DIR="$2"; shift 2;;
	      -s)             SSTATE_DIR="$2"; shift 2;;
	      --sstate)       SSTATE_DIR="$2"; shift 2;;
	      -m)             MANIFEST="$2"; shift 2;;
	      --manifest)     MANIFEST="$2"; shift 2;;
	      -h)             usage; return;;
	      --help)         usage; return;;
	      *)              echo "Unrecognized Argument: $1"; usage; return;;
	      esac
	  done
	  
	  2 sh filename
	  启动一个新的子进程来执行脚本，脚本无法修改父shell环境中的变量
	  
	  3 ./filename
	  ./用来指定当前目录下的可执行文件，./filename 表示执行位于当前目录下的指定脚本文件，执行脚本时会启动
	  一个新的子进程来运行该脚本
	  ```
- Unix Shell I/O重定向
  collapsed:: true
	- 重定向是由shell首先完成的，在找到命令之前
		- shell会创建一个新的空文件或者截断（清空）一个现有的文件(^^Shell 并不关心你在命令行的什么地方进行文件的重定向^^)
		- 在做完重定向，并从命令行中删除语法后，shell会找到并执行命令（如果有的话）行中的语法，shell会找到并执行命令（如果有的话）
		- ```bash
		      echo hi there mom >file                   # echo has three arguments
		      echo hi there >file mom                   # echo has three arguments
		      echo hi >file there mom                   # echo has three arguments
		      echo >file hi there mom                   # echo has three arguments
		      >file echo hi there mom                   # echo has three arguments
		  ```
- Philosophy
  collapsed:: true
	- shell 脚本必须清晰可读的布局
		- 易于阅读，修改和维护
	- 避免使用不必要的命令
	  collapsed:: true
		- `grep "mystring" /tmp/myfile` 要优于 `cat /tmp/myfile | grep "mystring"` 因为创建和销毁进程会消耗操作系统的资源，特别是在多重循环等场景下。
- bash算术运算
  collapsed:: true
	- 执行算术运算的方法
		- ```bash
		  #算术扩展
		  $((expression))
		  (())用来求值
		  $用来保存结果
		    例如:
		      echo $((2+3))
		  
		  #使用awk进行算术运算
		  awk 'BEGIN { x = 2; y = 3; print "x + y = "(x+y) }'
		  
		  #使用bc命令
		  echo "2+3" | bc
		  
		  #使用dc命令
		  echo "2 3 + p" | dc
		  
		  #使用declare命令
		  declare -i x=2 y=3 z=x+y
		  echo $x + $y = $z
		  
		  #使用expr命令
		  expr 2 + 3
		  
		  #使用factor命令
		  factor 100
		  100: 2 2 5 5
		  
		  #使用let命令
		  let x=2+3 | echo $x
		  
		  #使用test命令
		  test 2 -gt 3; echo $?
		  
		  [ 2 -gt 3 ]; echo $?
		  
		  
		  echo $((x=2, y=3, x+y))
		  ((x=2, y=3, a=x+y, b=x*y, c=x**y)); echo $a, $b, $c
		  number=1
		  echo $((++number))
		  echo $((number++))
		  
		  #浮点数运算
		  awk 'BEGIN { x = 2.3; y = 3.2; print "x * y = "(x * y) }'
		  echo "2.3 * 3.2" | bc -l
		  perl -e 'print 2.3*3.2'
		  printf %.<precision>f "$((10**<multiplier> * <fraction>))e-<multiplier>"
		  printf %.2f "$((10**3 * 1/3))e-3"
		  
		  #百分比计算和四舍五入
		  printf %.2f "$((10**4 * part/total))e-4"%
		  printf %.2f%% "$((10**4 * 40/71))e-4"%
		  
		  awk 'BEGIN { printf "%.2f%%", (part/total*100) }'
		  awk 'BEGIN { printf "%.2f%%", (40/71*100) }'
		  
		  
		  #bash递归
		  factorial () {
		      if (($1 > 1))
		      then
		          echo $(( $( factorial $(($1 - 1)) ) * $1 ))
		      else
		          echo 1
		          return
		      fi
		  }
		  
		  #更高精度的计算
		  echo 'define factorial(x) {if (x>1){return x*factorial(x-1)};return 1}
		   factorial(<number>)' | bc
		   echo 'define factorial(x) {if (x>1){return x*factorial(x-1)};return 1} factorial(50)' | bc
		  
		  #创建bash计算函数
		  calculate() { printf "%s\n" "$@" | bc -l; }
		  calculate 1 + 2
		  3
		  
		  calculate 10/2
		  5
		  
		  #使用不同算术基数
		  base#number
		  echo $((2#1010+2#1010))
		  echo $((010+010))
		  echo $((0xA+0xA))
		  
		  echo $((2#10+2#10))
		  
		  ```
	- [Bash Math Operations (Bash Arithmetic) Explained](https://phoenixnap.com/kb/bash-math)
	- [Perform arithmetic operations](https://bash.cyberciti.biz/guide/Perform_arithmetic_operations)
- shell启动文件
  collapsed:: true
	- login shell
		- `/etc/profile`
		  collapsed:: true
			- 全局配置脚本，适合于所有用户
				- `/etc/profile.d/*.sh`
					- 在`/etc/profile`中会判断该目录下是否包含shell脚本，如果包含则运行
		- `~/.bash_profile`
		  collapsed:: true
			- 特定用户的启动文件，可以用来扩展或者覆写全局配置脚本的配置
		- `~/.bash_login`
		  collapsed:: true
			- 如果`~/.bash_profile`不存在，则会读取该脚本
		- `~/.profile`
		  collapsed:: true
			- 如果`~/.bash_profile` 和 `~/.bash_login`都不存在，则会读取该脚本
	- non-login shell
		- `/etc/bash.bashrc`
		  collapsed:: true
			- 全局配置脚本，适合于所有用户
		- `~/.bashrc`
		  collapsed:: true
			- 特定用户的启动文件，可以用来扩展或者覆写全局配置脚本的配置
		- In addition to reading the startup files above, non-login shells also inherit the environment from their parent process, usually a login shell
		- 除了读取以上配置文件，non-login shell也继承父进程的环境变量，通常是一个login shell
- 流程控制
  collapsed:: true
	- ```bash
	  # if语句
	  if commands; then
	      commands
	  elif commands; then
	      commands
	  else
	      commands]
	  fi
	  
	  # if示例
	  number=0
	  
	  read -p "Enter a number > " number
	  
	  echo "Number is $number"
	  if [ $((number % 2)) -eq 0 ]; then
	      echo "Number is even"
	  else
	      echo "Number is odd"
	  fi 
	  
	  
	  # case语句
	  case word in
	      patterns ) commands ;;
	  esac
	  
	  # case示例
	  read -p "Type a digit or a letter > " character
	  case $character in
	                                  # Check for letters
	      [[:lower:]] | [[:upper:]] ) echo "You typed the letter $character"
	                                  ;;
	  
	                                  # Check for digits
	      [0-9] )                     echo "You typed the digit $character"
	                                  ;;
	  
	                                  # Check for anything else
	      * )                         echo "You did not type a letter or a digit"
	  esac
	  
	  # 循环语句
	  # while语句（条件为真执行）
	  number=0
	  while [ "$number" -lt 10 ]; do
	      echo "Number = $number"
	      number=$((number + 1))
	  done
	  # while示例
	  interactive=
	  filename=~/sysinfo_page.html
	  
	  while [ "$1" != "" ]; do
	      case $1 in
	          -f | --file )           shift
	                                  filename="$1"
	                                  ;;
	          -i | --interactive )    interactive=1
	                                  ;;
	          -h | --help )           usage
	                                  exit
	                                  ;;
	          * )                     usage
	                                  exit 1
	      esac
	      shift
	  done
	  
	  # until语句（条件为假退出）
	  number=0
	  until [ "$number" -ge 10 ]; do
	      echo "Number = $number"
	      number=$((number + 1))
	  done
	  # until示例
	  selection=
	  until [ "$selection" = "0" ]; do
	      echo "
	      PROGRAM MENU
	      1 - Display free disk space
	      2 - Display free memory
	  
	      0 - exit program
	  	"
	      echo -n "Enter selection: "
	      read selection
	      echo ""
	      case $selection in
	          1 ) df ;;
	          2 ) free ;;
	          0 ) exit ;;
	          * ) echo "Please enter 1, 2, or 0"
	      esac
	  done
	  # for语句
	  for variable in words; do
	      commands
	  done
	  # for示例
	  system_info() {
	      # Find any release files in /etc
	  
	      if ls /etc/*release 1>/dev/null 2>&1; then
	          echo "<h2>System release info</h2>"
	          echo "<pre>"
	          for i in /etc/*release; do
	  
	              # Since we can't be sure of the
	              # length of the file, only
	              # display the first line.
	  
	              head -n 1 "$i"
	          done
	          uname -orp
	          echo "</pre>"
	      fi
	  
	  }   # end of system_info
	  ```
- 检查命令的退出状态
  collapsed:: true
	- ```bash
	  # Check the exit status
	  cd "$some_directory"
	  if [ "$?" = "0" ]; then
	    rm *
	  else
	    echo "Cannot change directory!" 1>&2
	    exit 1
	  fi
	  
	  # A better way
	  if cd "$some_directory"; then
	    rm ./*
	  else
	    echo "Could not change directory! Aborting." 1>&2
	    exit 1
	  fi
	  ```
- here script
  collapsed:: true
	- 也称作here document，是IO重定向的一种形式，它提供了一种方法用来包含文本内容作为另一个命令的标准输入。
	- ```bash
	  command << token
	  content to be used as command's standard input
	  token
	  ```
	- ```bash
	  cat << _EOF_
	  <html>
	  <head>
	      <title>
	      The title of your page
	      </title>
	  </head>
	  
	  <body>
	      Your page content goes here.
	  </body>
	  </html>
	  _EOF_
	  ```
	- ```bash
	  cat <<- _EOF_
	      <html>
	      <head>
	          <title>
	          My System Information
	          </title>
	      </head>
	  
	      <body>
	      <h1>My System Information</h1>
	      </body>
	      </html>
	  _EOF_
	  ```
	- 从"<<" 变为 "<< -"用来指示bash忽略here script开头的制表符(不忽略空格)
- [[eval]]命令
- [[xargs]]
- [[watch]]
- [[readlink]]
- 输入重定向
  collapsed:: true
	- ```bash
	  # wc knows it is reading from file, so file name is listed
	  $ wc -l file.txt
	  3 file.txt
	  
	  # wc only knows it is reading from standard input, so file name is not listed
	  $ wc -l < file.txt
	  3
	  ```
- 丢弃命令输出
  collapsed:: true
	- ```bash
	  command > /dev/null
	  command > /dev/null 2>&1
	  ```
- 命令分组
  collapsed:: true
	- ```bash
	  {
	  	echo "I found all these PNGs:"
	  	find . -name "*.png"
	  	echo "Within this bunch of files:"
	  	ls
	  } > PNGs.txt
	  
	  # 大括号前后有空格，命令最后有分号
	  { echo "123"; echo "456"; } > txt2
	  ```
- 自定义函数
  collapsed:: true
	- ```bash
	  function myadd()
	  {
	  	echo $(($1 + $2))
	  }
	  
	  echo "1 + 2 = $(myadd 1 2)"
	  ```
- 用户输入
  collapsed:: true
	- ```bash
	  read name
	  echo "Hello $name"
	  
	  read -p "please input your name: " name
	  echo $name
	  ```
- 文件读写
  collapsed:: true
	- ```bash
	  while read -r line
	  do
	  	echo "$line"
	  done <file.txt
	  ```
- here-string
  collapsed:: true
	- ```bash
	  read first second <<< "hello world"
	  echo $second $first
	  ```
- 进程替换
  collapsed:: true
	- 进程替换使用系统内部文件 `/dev/fd/<n>` 发送括号内进程的结果到另一个进程
	- 使用 echo 命令可以查看进程替换对应的文件名
		- ```bash
		  $ echo >(true)
		  /dev/fd/63
		  
		  $ echo <(true)
		  /dev/fd/63
		  
		  $ echo >(true) <(true)
		  /dev/fd/63 /dev/fd/62
		  
		  $ echo "qpy" > >(read; echo "hello, $REPLY")
		  hello, qpy
		  ```
	- 输入
		- `>(cmmand_list)`
			- ```bash
			  echo david > >(read user; echo $user)
			  ```
	- 输出
		- `<(command_list)`
			- ```bash
			  # user 为空，因为管道后的命令是在子 shell 中执行的，子 shell 执行完毕后回到当前 shell， user不可见
			  echo david | read user
			  echo $user
			  
			  # user 结果为 david
			  read user < <(echo david)
			  echo $user
			  
			  paste -d, <(seq 1 3) <(seq 2 4)
			  
			  # 将 标准输出和标准错误输出分别重定向
			  some_command > >(/bin/cmd_for_stdout) 2> >(/bin/cmd_for_stderr)
			  
			  # 进程替换支持嵌套
			  sort <(egrep -v '^#' <(paste -d: <(cut -d: -f5 /etc/passwd)  <(cut -d: -f1 /etc/passwd) ) )
			  ```
- 构建数组
  collapsed:: true
	- ```bash
	  files=(*.txt)
	  for file in ${files[@]}
	  do
	  	echo $file
	  done
	  ```
	- ```bash
	  files=()
	  while read -r -d ''
	  do
	  	files+=("$REPLY")
	  done < <(find . -name '*.txt' -print0)
	  
	  echo ${files[@]}
	  ```
	- ```bash
	  dec2bin=({0..1}{0..1}{0..1}{0..1}{0..1}{0..1}{0..1}{0..1})
	  echo ${dec2bin[25]}
	  
	  month=("Jan" "Feb" "Mar" "Apr" "May" "Jun" "Jul" "Aug" "Sep" "Oct" "Nov" "Dec")
	  echo ${month[3]}
	  ```
- 参数处理
	- ```bash
	  # ./xxx  -u john -a 23 -f 'John Smith'
	  if [ $# -lt 1 ]
	  then
	  	echo "Usage: $0 xxx"
	  	exit
	  fi
	  
	  while getopts u:a:f: flag
	  do
	  	case "${flag}" in
	  		u) usename=${OPTARG};;
	  		a) age=${OPTARG};;
	  		f) fullname=${OPTARG};;
	  	esac
	  done
	  
	  echo "Username: $usename"
	  echo "Age: $age"
	  echo "Full Name: $fullname"
	  ```
	- ```sh
	  ./xxx john matt bill
	  i=1
	  for user in "$@"
	  do
	  	echo "Username - $i: $user";
	  	i=$((i + 1))
	  done
	  ```
	- shift命令
		- shift 命令左移未知参数，同时修改 $# 和 $@
		- ```bash
		  # ./xxx 1 2 3 4
		  sum=0
		  until [ $# -eq 0 ]
		  do
		  	sum=$((sum + $1))
		  	shift
		  done
		  
		  echo "sum: $sum"
		  ```
- 常用命令
	- split
	- cat
	- [[realpath]]
	  background-color:: red
	  collapsed:: true
		- 获取文件的绝对路径
	- [[pgrep]]
	  collapsed:: true
		- 基于名称或者其他条件查找进程
	- [[exec]]
	  collapsed:: true
		- 不创建subshell，而是用命令替换当前shell执行
	- [[stat]]
	  collapsed:: true
		- 获取文件状态信息
	- [[env]]
	  collapsed:: true
		- 打印环境变量或者在不修改当前环境的情况下在新环境下运行应用
	- [[lsof]]
	  collapsed:: true
		- 列举打开的文件
	- [[fuser]]
	  collapsed:: true
		- > identify processes using files or sockets
	- [[timeout]]
	  collapsed:: true
		- > run a command with a time limit
	- [[column]]
	  collapsed:: true
		- > columnate lists
	- tldr
	  collapsed:: true
		- Too Long; Didn't Read
		  collapsed:: true
			- ```bash
			  tldr grep
			  ```
	- man
	  collapsed:: true
		- 获取命令或函数参考手册
		  collapsed:: true
			- ```bash
			  man dash
			  man builtin
			  man set
			  ```
	- [[ln]]
	- [[kill]]
	- [[cmp]]
- shell脚本格式检查
  collapsed:: true
	- [shellcheck online](https://www.shellcheck.net/)
- Bash带颜色输出
  collapsed:: true
	- ```bash
	  red='\e[0;31m'
	  blue='\e[0;34m'
	  green='\e[0;32m'
	  endColor='\e[0m'
	  
	  printf "%bhello world%b\n" ${red} ${endColor}
	  ```
- 通过`set`命令改变shell选项
  collapsed:: true
	- [内置的set命令](https://www.gnu.org/software/bash/manual/html_node/The-Set-Builtin.html)
	- 用法示例
		- ```bash
		  1. 在shell脚本里可以打开和关闭debug选项
		  set -x			# activate debugging from here
		  ......			>>> 中间是debug使能的命令
		  set +x			# stop debugging from here
		  
		  2. bash -x test.sh
		  
		  3.在脚本的开头可以指定 #!/bin/bash -xv
		  ```
- Tips
  collapsed:: true
	- if条件判断
		- ```bash
		  [ is POSIX
		  [[ is a Bash extension inspired from Korn shell
		  
		  Recommendation: always use []
		  
		  If you use [[ ]] you:
		  lose portability
		  force the reader to learn the intricacies of another bash extension. 
		  [ is just a regular command with a weird name, no special semantics are involved.
		  
		  优先使用double bracket进行条件判断，即 if [[ $var = "xxx" ]]，因为在[[]]里面，单词分割
		  和路径名扩展不会应用到单词上
		  
		  如果使用[]进行条件判断，必须把变量用双引号引用起来，即 if [[ "$var" != "xxx" ]]，否则如果$var
		  为null的话，判断条件变为 [ != "xxx" ]，此时会有"[: !=: unary operator expected"的语法错误
		  ```
	- 使用type命令查看bash函数，别名等定义
	  collapsed:: true
		- > type [-aftpP] name [name ...]
		  With no options, indicate how each name would be interpreted if used as a command name.  If the -t option is used, type prints a string which is  one  of  alias,  keyword,  function, builtin, or file if name is an alias, shell reserved word, function, builtin, or disk file, respectively.
		- ```bash
		  假设~/.bashrc里有如下函数定义
		  function foo()
		  {
		  	echo "foo function"
		  }
		  
		  则type命令有如下返回结果
		  morrism@localhost /tmp/x $ type foo
		  foo is a function
		  foo ()
		  {
		      echo "foo"
		  }
		  morrism@localhost /tmp/x $ type -t foo
		  function
		  ```
	- bash函数返回值
		- > `echo` generates *output*. A command substitution like`$(...)` captures that output, but if you run a command without it, that output will go to the terminal.
		  `return` sets *exit status*. This is what's used to determine which branch is taken when running `if your_function; then ...`, or to populate `$?`.
		- `echo` 用来产生输出内容，之后可以用 `$(...)` 用来捕获函数输出内容, 如果函数单独执行的话，输出内容将显示在终端上
		- `return` 用来设置退出状态，它用来决定当运行`if your_function; then ...`条件判断时执行哪一个分支，或者用来设置`$?`
		- `$?`的取值范围为`0 ~ 255`，如果return的取值大于255，则会转换为`uint8`类型进行函数退出状态的设置
- 参考文档
	- [How to Declare and Access Associative Array in Bash](https://phoenixnap.com/kb/bash-associative-array)
	- [Bash Scripting – Associative Array Explained With Examples](https://ostechnix.com/bash-associative-array/)
	- [Take control of your data with associative arrays in Bash](https://opensource.com/article/20/6/associative-arrays-bash)
	- [Bash shift builtin command](https://www.computerhope.com/unix/bash/shift.htm)
	- [A Complete Guide On How To Use Bash Arrays](https://www.shell-tips.com/bash/arrays/#gsc.tab=0)
	- [**How to Use Command Line Arguments in a Bash Script**](https://www.baeldung.com/linux/use-command-line-arguments-in-bash-script)
	- [pure bash bible](https://github.com/dylanaraps/pure-bash-bible?tab=readme-ov-file)
	- [Bash getopts builtin command](https://www.computerhope.com/unix/bash/getopts.htm)
	- [Parsing bash script options with getopts](https://sookocheff.com/post/bash/parsing-bash-script-arguments-with-shopts/)
	- [Parse Command Line Arguments in Bash](https://www.baeldung.com/linux/bash-parse-command-line-arguments)
	- [Double Quotes](https://www.gnu.org/software/bash/manual/html_node/Double-Quotes.html)
		- #+BEGIN_QUOTE
		  Enclosing characters in double quotes (‘"’) **preserves the literal value** of all characters within the quotes, **with the exception** of ‘$’, ‘`’, ‘\’, and, when history expansion is enabled, ‘!’. 
		  #+END_QUOTE
	- [Single Quotes](https://www.gnu.org/software/bash/manual/html_node/Single-Quotes.html)
	- #+BEGIN_QUOTE
	  Enclosing characters in single quotes (‘'’) **preserves the literal value of each character** within the quotes.
	  #+END_QUOTE
	- [bash(1)](https://manpages.org/bash)
		- BASHPID
	- [前后台进程、孤儿进程和 daemon 类进程的父子关系](https://www.cnblogs.com/f-ck-need-u/p/17718649.html)
	- [GNU Bash Reference Manual](https://www.linuxtopia.org/online_books/bash_reference_guide/index.html)
	- [Advanced Bash-Scripting Guide](https://www.linuxtopia.org/online_books/advanced_bash_scripting_guide/index.html) #online
	- [Bash Reference Manual](https://www.gnu.org/software/bash/manual/bash.html#)
		- [The set Command in Linux](https://www.baeldung.com/linux/set-command)
		- [Pattern Matching](https://www.gnu.org/software/bash/manual/bash.html#Pattern-Matching)
		- [Pattern Matching In Bash](https://www.linuxjournal.com/content/pattern-matching-bash)
		- [Special Parameters](https://www.gnu.org/software/bash/manual/bash.html#Shell-Parameters)
- [阮一峰 Bash 脚本教程](https://www.bookstack.cn/books/bash-tutorial)
- [Unix Shell I/O重定向](https://m24y.com/index.php/2022/04/03/unix-shell-i-o%e9%87%8d%e5%ae%9a%e5%90%91/)
- [Unix Shell I/O重定向](http://teaching.idallen.com/cst8207/12w/notes/270_redirection.txt)
- [Bash scripting cheatsheet](https://devhints.io/bash)
- [Advanced Bash-Scripting Guide](https://tldp.org/LDP/abs/html/)
- [Shell Style Guide](https://google.github.io/styleguide/shellguide.html)
- [syntax brackets](https://ss64.com/bash/syntax-brackets.html)
- [Shell Scripting Primer](https://developer.apple.com/library/archive/documentation/OpenSource/Conceptual/ShellScripting/Introduction/Introduction.html#//apple_ref/doc/uid/TP40004268)
- [bash style guide](https://github.com/bahamas10/bash-style-guide) #github
- [coding standards](https://linuxcommand.org/lc3_adv_standards.php)
- [Shell 编程范例](https://www.cntofu.com/book/44/index.html)
- [man sh](https://linux.die.net/man/1/sh)
- [编写健壮的 Shell 脚本](https://morven.life/posts/how-to-write-robust-shell-script/)
- [Bash Reference Manual](https://www.gnu.org/software/bash/manual/html_node/index.html#SEC_Contents)
- [ANSI Escape Sequences  颜色输出](https://gist.github.com/fnky/458719343aabd01cfb17a3a4f7296797)
- [Everything you never wanted to know about ANSI escape codes](https://notes.burke.libbey.me/ansi-escape-codes/)
- [ANSI escape code generator](https://ansi.gabebanks.net/)
- [explain shell](https://explainshell.com/) #online
- [Find the Script’s Filename Within the Same Script in Bash](https://www.baeldung.com/linux/find-bash-script-filename)
- [Changing the Default Shell in Linux](https://www.baeldung.com/linux/change-default-shell)
- [Include Files in a Bash Shell Script With source Command](https://www.baeldung.com/linux/source-include-files)
- [Bash Source Command](https://linuxize.com/post/bash-source-command/)
- [BASH TIPS & TRICKS](https://tecadmin.net/category/bash-tips-tricks/)
- [tldr](https://tldr.inbrowser.app/) #online
- [bash-utility](https://github.com/labbots/bash-utility) #online #github
- [Google Style Guides](https://google.github.io/styleguide/) #online
- [pure-sh-bible](https://github.com/dylanaraps/pure-sh-bible)
- [Understanding 2>&1 in Bash: A Beginner’s Guide](https://tecadmin.net/io-redirection-operator-in-bash/)