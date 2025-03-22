
[Bash Function & How to Use It](https://phoenixnap.com/kb/bash-function)  

## Bash Commands Similar to Linux Syscalls
```bash
## ​**1. Process Management**
| Linux Syscall | Bash Equivalent       | Purpose & Example                                        |
|---------------|-----------------------|----------------------------------------------------------|
| `fork()`      | `&` operator          | Create subshell/background process:<br>`sleep 10 &`      |
| `execve()`    | `exec`                | Replace shell process:<br>`exec /bin/zsh`                |
| `waitpid()`   | `wait`                | Wait for child processes:<br>`wait $pid`                 |
| `clone()`     | `( )` subshell        | Isolated execution context:<br>`(cd /tmp && ls)`         |
| `exit()`      | `exit`                | Terminate script:<br>`exit 1`                            |


## ​**2. File Operations**
| Linux Syscall | Bash Equivalent          | Example                                               |
|---------------|--------------------------|-------------------------------------------------------|
| `open()`      | `exec` + file descriptors| Persistent FD handling:<br>`exec 3> file.txt`         |
| `read()`      | `read`                   | Read from FD:<br>`read -u 3 data`                     |
| `write()`     | `echo` + redirection     | Write to FD:<br>`echo "text" >&3`                     |
| `close()`     | `exec` + `&-`            | Close FD:<br>`exec 3>&-`                              |
| `stat()`      | `test`                   | File checks:<br>`if [ -f file.txt ]; then ...`        |

## ​**3. Signal Handling**
| Linux Syscall  | Bash Equivalent | Example                                                       |
|----------------|-----------------|---------------------------------------------------------------|
| `kill()`       | `kill`          | Send signals:<br>`kill -HUP $pid`                             |
| `sigaction()`  | `trap`          | Signal handlers:<br>`trap 'cleanup' SIGINT`                   |
## ​**4. Memory Management**
| Linux Syscall | Bash Feature    | Example                                                                |
|---------------|-----------------|------------------------------------------------------------------------|
| `mmap()`      | `/dev/shm`      | Shared memory:<br>`dd if=/dev/zero of=/dev/shm/mem bs=1M count=100`    |
| `brk()`       | `ulimit`        | Memory limits:<br>`ulimit -v 500000` (500MB virtual memory limit)      |

## ​**5. Network Operations**
| Linux Syscall | Bash Tool       | Example                                                                |
|---------------|-----------------|------------------------------------------------------------------------|
| `socket()`    | `nc` (netcat)   | TCP communication:<br>`nc -l 8080`                                     |
| `connect()`   | `curl`/`wget`   | HTTP requests:<br>`curl -s http://api.example.com/data.json`           |
| `bind()`      | `ss`/`netstat`  | Socket monitoring:<br>`ss -tulpn \| grep :443`                         |

## ​**6. Advanced System Interaction**
| Linux Syscall       | Bash Feature    | Example                                                          |
|---------------------|-----------------|------------------------------------------------------------------|
| `ioctl()`           | `stty`          | Terminal control:<br>`stty -echo` (disable input echoing)        |
| `getpid()`          | `$$` variable   | Process ID:<br>`echo "Script PID: $$"`                           |
| `gettimeofday()`    | `date`          | Timestamps:<br>`date +%s.%N` (nanosecond precision)              |


## Common Patterns
# File Locking
	exec 200>/tmp/lockfile
	flock -n 200 || exit 1
	# Critical section here
	flock -u 200

# Signal-Driven Scripts
	trap 'echo "Exiting cleanly"; rm tempfile' EXIT SIGINT

# Debugging Tools
	strace -e trace=open,read bash -c 'cat file.txt'

## When to Avoid Bash
For syscall-intensive tasks, prefer:
	​C (direct syscall access via <unistd.h>)
	​Python (os, fcntl, mmap modules)
	​Rust (memory-safe systems programming)
```

## exec
```bash
##exec 命令主要用途
	1. replacing the current shell process

	exec command [arguments]

	Behavior:
		1. Replaces the ​current shell process with the specified command.
		2. The original shell process is terminated, and the new command takes over its PID.
		3. After the command exits, the shell session ends (if used interactively) or the script terminates.

		# Script exits after `nginx` finishes (no new process created)
		#!/bin/bash
		setup_environment
		exec nginx -g "daemon off;"

	Use Cases:
		1. Minimize Resource Usage: Avoid creating a subshell when running a final command in a script.
		2. Switch Shells: Replace the current shell with another (e.g., exec zsh).
		3. Run Commands with Environment Changes: Apply new environment variables permanently.

	2. manipulating file descriptors

	exec [n]<file   # Open file for reading on descriptor `n`
	exec [n]>file   # Open file for writing on descriptor `n`
	exec [n]>&-     # Close descriptor `n`

	# Redirect All Output
		exec >script.log 2>&1  # All subsequent output goes to script.log

	# Persistent File Handles
	exec 3>output.txt
	echo "Hello" >&3
	exec 3>&-  # Close descriptor 3	

	# Replace `;` with `+` to pass multiple files to a single process
	find . -name "*.log" -exec rm {} +

	# Overlay Environment Variables
	exec env VAR=value /path/to/program

	# Log Entire Script Execution
	#!/bin/bash
	exec > >(tee script.log) 2>&1  # Tee output to file and terminal
	echo "Debug info..."

	# Read Multiple Files Simultaneously
	exec 3<file1 4<file2
	read -u 3 line1
	read -u 4 line2
	exec 3<&- 4<&-

	# Replace shell process
	exec zsh

	# Replace `;` with `+` to pass multiple files to a single process
	find . -name "*.log" -exec rm {} +
```

## howto generate 512-byte disk image file
```bash
( echo "eb fe 11 22 33 44" | xxd -r -p; \
  cat /dev/zero | head -c 504; \
  echo "55 aa" | xxd -r -p \
) > minimal.img

# Structure of the Code
(
  [Part 1: Hex bytes]  					# Initial Hex Bytes
  [Part 2: 504 null bytes]  			# Null Padding
  [Part 3: Final 2-byte signature]		# Boot Signature
) > minimal.img
```

## subshell
```bash
# 在 Bash 中 subshell 是一个由 parent shell 创建的独立的 child process，用于在隔离的环境下执行 shell 命令

## What a Subshell Does
1. 创建隔离的运行环境 (Creates an isolated execution environment)
	Commands inside (...) run in a ​child process, separate from the parent shell
	Variables, aliases, and shell options modified inside the subshell do not affect the parent shell.
2. 合并输出 (​Combines output)
	All output (stdout) from commands inside the subshell is ​merged into a single stream

## subshell 的主要特征
#1. Sequential Execution: subshell 中的命令按顺序执行
#2. Single Redirection: subshell 的所有输出统一重定向到特定的文件
	( echo "eb fe 11 22 33 44" | xxd -r -p; \
	cat /dev/zero | head -c 504; \
	echo "55 aa" | xxd -r -p \
	) > minimal.img

	等价于

	# Equivalent without a subshell (less efficient):
	echo "eb fe 11 22 33 44" | xxd -r -p > minimal.img
	cat /dev/zero | head -c 504 >> minimal.img
	echo "55 aa" | xxd -r -p >> minimal.img

#3. Process Isolation: subshell 中的变量或者shell state改变不会影响到 parent shell
	(
	MY_VAR="changed"
	echo "Subshell: $MY_VAR"  # Output: Subshell: changed
	)
	echo "Parent: $MY_VAR"      # Output: Parent: [empty/unmodified]
#4. Parallel Processing: 并行处理
	(sleep 1 & sleep 2 & wait)  # Runs sleeps in parallel
	(long_command1) & (long_command2) & wait

## subshell 的主要应用场景
#1. Grouping Commands for Redirection
# Send all output to a log file
	(
	echo "Starting task..."
	complex_command
	echo "Finished."
	) > log.txt
#2. Temporary Environment Changes
	(cd /tmp && ls)  # Changing directory only affects the subshell
#3. Background Execution
	(
	command1
	command2
	) &
#4. Capturing Output
	contents=$(ls /tmp | grep 'log')

	result=$(
	echo "Hello"
	echo "World"
	)
	echo "$result"  # Output: Hello\nWorld
#5. Pipeline stages:
	generate_data | (process_data; cleanup)
	cat file.txt | ( while read line; do ...; done )
#6. Avoid Traps/Signals
	(trap 'echo Ignored' INT; sleep 10)  # Parent shell's INT trap is unaffected.

#7. Process substitution (<(cmd)) implicitly uses subshells
	diff <(echo "Hello") <(echo "World")

## Subshell ( ... ) vs. Code Block { ... }
# subshell
	#1. Runs in a separate process.
	#2. Inherits variables and state from the parent but cannot modify them.
	#3. Suitable for isolated tasks or combining output.

# code block
	#1. Runs in the ​current shell process (Shared environment).
	#2. Modifications to variables or state affect the parent shell (No isolation).
	#3. No process overhead (faster).
	#4. Cannot be backgrounded directly: The entire block runs in sequence
	#5. Syntax requirements: Commands must end with semicolons or newlines, and there must be spaces around the braces

	{
	echo "Hello"
	echo "World"
	} > output.txt

## Advanced Scenarios
#1 Chaining Multiple Subshells
(ls /tmp; (cd /var && ls)) > combined_output.txt

#2 Combining with Functions
run_in_subshell() {
    (echo "In subshell: $1"; process_data "$1")
}

process_data() {
    echo "Processing $1"
}

run_in_subshell "example"

#3 Complex Redirection with Code Blocks
{ 
    echo "Starting log"; 
    command1 2>&1; 
    command2 2>&1; 
    echo "Ending log"; 
} | tee logfile.txt

#4 Conditional Execution
	{ command1 && command2; } || echo "Failed"
```

## 判断命令是否安装
```bash
if ! command -v COMMAND &> /dev/null; then
    echo "Error: COMMAND is not installed. Please install it first."
    exit 1
fi
```

## 等待所有后台作业完成
```bash
# 启动两个后台作业
echo "Starting job 1..."
sleep 3 &  # 后台作业1
pid1=$!

echo "Starting job 2..."
sleep 6 &   # 后台作业2
pid2=$!

# 等待所有后台作业完成
wait < <(jobs -p)	# 命令 jobs -p 用于返回所有后台进程的 PID，wait 命令用于等待当前shell下所指定的后台进程结束（如果不指定参数，则代表所有的后台进程）
# wait $pid1 $pid2
# wait %1 %2
```

## 解析配置文件
```bash
## 使用 source 直接加载 符合 Bash 语法的配置文件
config.sh:
DB_HOST="localhost"
DB_PORT=3306
DEBUG=true

main.sh:
load_config() {
    local file="$1"
    if [ ! -f "$file" ]; then
        echo "Error: config file '$file' does not exist" >&2
        return 1
    fi
    source "$file"
}

load_config "config.sh"
echo "Host: $DB_HOST, Port: $DB_PORT, Debug: $DEBUG"

## 解析 JSON 文件
config.json:
{
    "database": {
        "host": "localhost",
        "port": 3306
    },
    "server": {
        "debug": true
    }
}

main.sh:
DB_HOST=""
DB_PORT=""
DEBUG=""

parse_json() {
    local file="$1"
    if ! command -v jq >/dev/null 2>&1; then
        echo "Error: command jq not found" >&2
        return 1
    fi
    if [ ! -f "$file" ]; then
        echo "Error: config file '$file' does not exist" >&2
        return 1
    fi

    DB_HOST=$(jq -r '.database.host' "$file")
    DB_PORT=$(jq -r '.database.port' "$file")
    DEBUG=$(jq -r '.server.debug' "$file")
    # export DB_HOST DB_PORT DEBUG
}

parse_json "config.json"
echo "Host: $DB_HOST, Port: $DB_PORT, Debug: $DEBUG"

## 直接解析键值对文件
config.env:
DB_HOST=localhost
DB_PORT=3306
DEBUG=true

main.sh:
parse_config() {
    local file="$1"
    if [ ! -f "$file" ]; then
        echo "Error: config file '$file' does not exist" >&2
        return 1
    fi

    while IFS='=' read -r key value; do
        # 跳过空行和注释
        [[ -z "$key" || "$key" =~ ^# ]] && continue
        # 去除首尾空白
        key=$(echo "$key" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
        value=$(echo "$value" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
		# value=$(echo "$value" | sed 's/^"\(.*\)"$/\1/;s/^'\''\(.*\)'\''$/\1/')	# 对于带双引号或者单引号的值，去掉引号
        export "$key=$value"	# 导出变量
		# eval "$key=$value"
    done < "$file"
}

parse_config "config.env"
echo "Host: $DB_HOST, Port: $DB_PORT, Debug: $DEBUG"
```

## 位置参数
```bash
$*：将所有位置参数合并为一个字符串，默认以 IFS（内部字段分隔符，通常是空格）连接
$@：表示所有位置参数的列表，保持每个参数的独立性

使用 $* 的场景：需要将所有参数作为一个整体字符串传递时
log() {
    echo "日志: $*" >> log.txt
}
log "操作成功" "用户: alice"	# log.txt 内容：日志: 操作成功 用户: alice

使用 $@ 的场景：需要逐个处理参数，或将参数传递给其他命令时
# 1
wrapper() {
    ls "$@"
}
wrapper -l dir1 "dir 2"		   # 等价于 ls -l dir1 "dir 2"，保持参数独立
# 2
call_cmd() {
    some_cmd "$@"
}
```

## 返回值 和 退出状态
```bash
add_nums() {
    if [ "$#" -lt 2 ]; then
        echo "Need at least 2 args"
        return 1
    fi
    
    local sum=0
    for num in "$@"; do
        sum=$(( $sum + $num ))
    done
    
    echo $sum		# 使用 echo 捕获返回值
    return 0		# 使用 return 返回状态码
}

# 2
add_nums() {
    if [ "$#" -lt 2 ]; then
        echo "Need at least 2 args"
        return 1
    fi
    
    local sum=0
    while [ "$#" -gt 0 ]; do
        sum=$(( $sum + $1 ))
        shift
    done
    
    echo $sum
    return 0
}

result=$(add_nums 2 3)
echo "Result: $result"
```

## 最佳实践
```bash
## 计算文件行数
# 参数: $1 - 文件路径
# 返回: 行数（通过 echo 输出），状态码 0 表示成功
count_lines() {
    local file="$1"
    local count

    if [ ! -f "$file" ]; then
        echo "错误: 文件 '$file' 不存在" >&2
        return 1
    fi

    count=$(wc -l < "$file")
    echo "$count"
    return 0
}

# 主逻辑
main() {
    local result
    result=$(count_lines "test.txt") || {
        echo "处理失败，退出"
        exit 1
    }
    echo "文件行数: $result"
}

main "$@"
```

## BATS(Bash Automated Testing System) 开元测试框架
```bash
# sudo apt-get install bats

# deploy.sh
deploy() {
        echo "deploy $1"
}

# test_deploy.bats
#!/usr/bin/env bats

load 'deploy.sh'

@test "test deploy" {
        run deploy "server1"
        [ "$status" -eq 0 ]
        [ "$output" = "deploy server1" ]
}
```

## 检查进程是否存在
```bash
# kill -0 pid 会向进程发送一个信号 0。这个信号不会被进程识别或处理，它只是用来测试进程是否存在
# 如果进程存在，kill -0 pid 的退出状态（exit status）为 0；如果进程不存在或当前用户没有权限发送信号给该进程，退出状态为非零值
if kill -0 $PID 2>/dev/null; then
    echo "进程 $PID 正在运行"
else
    echo "进程 $PID 已停止或不存在"
fi
```

## 进程替换
```bash
# <(...) 是 进程替换（process substitution） 的语法, 命令 ... 的输出被视为一个临时文件，<(...) 生成一个文件名（通常是 /dev/fd/<fd> 的形式）
# CMD < <(...) 该命令的作用是将 <(...) 生成的文件作为输入传递给前一个 <, 即前一个 < 是输入重定向的符号

##  应用：等待所有后台作业完成

# 启动两个后台作业
echo "Starting job 1..."
sleep 3 &  # 后台作业1
pid1=$!

echo "Starting job 2..."
sleep 6 &   # 后台作业2
pid2=$!

# 等待所有后台作业完成
wait < <(jobs -p)	# 命令 jobs -p 用于返回所有后台进程的 PID，wait 命令用于等待当前shell下所指定的后台进程结束（如果不指定参数，则代表所有的后台进程）
# wait $pid1 $pid2
# wait %1 %2

echo "All background jobs are done!"
```

## debug tips
```bash
#1 Use set -e for Error Handling
set -e  # Stop script on error
set -u  # Treat unset variables as errors

#2 Use "$@" for Handling Arguments, Use "$@" to reference all script arguments correctly, preserving spaces and special characters
for arg in "$@"; do
    echo "$arg"
done

#3 Check Command Success with $?
cp file1.txt file2.txt
if [ $? -eq 0 ]; then
    echo "Copy successful"
else
    echo "Copy failed"
fi

#4 Use Functions to Reuse Code
greet() {
    echo "Hello, $1!"
}
greet "Alice"  # Outputs: Hello, Alice!

#5 Use [[ for Safer Conditionals: Prefer [[ over [ for conditionals. It handles complex conditions better, especially with strings and regex
if [[ "$name" == "Alice" ]]; then
    echo "Hello, Alice!"
fi

#6 Use trap to Handle Signals: Use trap to clean up resources when the script is interrupted (e.g., on Ctrl + C).
trap 'echo "Script interrupted"; exit' INT

#7 Use Loops Efficiently: Use for, while, and until loops to process data efficiently
for file in *.txt; do
    echo "Processing $file"
done

#8 Use Parameter Expansion for Defaults: Provide default values for variables using ${VAR:-default}
name=${1:-"World"}  # If no argument is provided, default to "World"
echo "Hello, $name"

#9 Debug with set -x or bash -x: Use set -x within a script or run it with bash -x to print each command as it’s executed (great for debugging)
set -x  # Enable debugging
# Your code
set +x  # Disable debugging

#10 Use declare for Typed Variables: Use declare to enforce types on variables (like integers or read-only variables)
declare -i counter=10  # Integer variable
declare -r readonly_var="Cannot change me"  # Read-only variable

#11 use local keyword inside of function
my_function() {
  local count=5
  for ((i=1; i<=count; i++)); do
    echo "Iteration $i"
  done
}
```

## 解析命令行参数
```bash
#1 直接解析命令行参数
echo "script name: $0"
echo "1st param: $1"
echo "2st param: $2"
echo "all params: $@"
echo "param num: $#"

#2 使用 getopts 解析短选项选项和参数
# a:表示 -a 选项必须带有一个参数，c 表示 -c 选项不需要参数
# : 在开头表示静默错误处理模式，可以在 \? 分支中自定义错误处理，否则如果遇到无效选项，getopts 会自动输出错误信息到标准错误流
while getopts ":a:b:c" opt; do
  case $opt in
    a)
      echo "option -a value: $OPTARG"
      ;;
    b)
      echo "option -b value: $OPTARG"
      ;;
    c)
      echo "option -c enabled"
      ;;
    \?)
      echo "invalid option: -$OPTARG"
      ;;
    :)
      echo "option -$OPTARG needs a param"
      ;;
  esac
done

#3 自定义方式处理长选项
usage() {
	echo "usage: $0 [-a value1] [-b value2] [-h]"
}

while [[ "$1" != "" ]]; do
    case $1 in
        -a | --arg1 )
            shift
            ARG1=$1
            ;;
        -b | --arg2 )
            shift
            ARG2=$1
            ;;
        -h | --help )
            usage
            exit 0
            ;;
        * )
            echo "invalid option: $1"
            exit 1
    esac
    shift
done

echo "ARG1: $ARG1"
echo "ARG2: $ARG2"
```

```bash
# locale - get locale-specific information
morrism@PC24036:~/testdir$ locale -a
C
C.utf8
en_US.utf8
POSIX
zh_CN.utf8

morrism@PC24036:~/testdir$ echo $LANG
en_US.utf8

# 1 (preferred)
function_name() {
# Function code here
}

# 2 (not recommended)
function function_name() {
# Function code here
}

# 函数参数
Argument						Role
$0								Reserves the function's name when defined in the terminal. When defined in a bash script, $0 returns the script's name and location.
$1, $2, etc.					Corresponds to the argument's position after the function name.
$#								Holds the count of positional arguments passed to the function.
$@ and $*						Hold the positional arguments list and function the same when used this way.
"$@"							Expands the list to separate strings. For example "$1", "$2", etc.
"$*"							Expands the list into a single string, separating parameters with a space. For example "$1 $2" etc.
```


[GNU Coreutils](https://www.gnu.org/software/coreutils/manual/html_node/index.html)

[Bash Shebang](https://linuxize.com/post/bash-shebang/)
[What is Shebang in Bash? | Your Script Interpreter Guide](https://ioflood.com/blog/shebang-bash/)
[How does the #! shebang work?](https://stackoverflow.com/questions/3009192/how-does-the-shebang-work)
[The #! magic, details about the shebang/hash-bang mechanism on various Unix flavours](https://www.in-ulm.de/~mascheck/various/shebang/)
```bash
This sequence of characters (#!) is called shebang and is used to tell the operating system which interpreter to use to parse the rest of the file.

Shebang Interpreter Directive :
	#!interpreter [arguments]

	1.The directive must be the first line in the script.
	2.The directive must start with shebang #!
	3.White space after the shebang characters is optional.
	4.Interpreter is the full path to a binary file (ex: /bin/sh, /bin/bash).
	5.Interpreter arguments are optional.

Two ways to use the Shebang directive and set the interpreter:
1.Using the absolute path to the bash binary:
	#!/bin/bash
2.Using the env utility:
	#!/usr/bin/env bash
	it will search for the bash executable in the user’s $PATH environmental variable. If there are more than one paths to bash, the first one will be used by the script
```


bash下的特殊变量
```bash
morrism@localhost ~ $ echo $BASHPID
589311	# 当前bash的进程ID，在某些场景下可能与 $$ 的结果不同
morrism@localhost ~ $ echo $$
589311	# 当前bash的进程ID
morrism@localhost ~ $ echo $PPID
112411	# 当前bash的进程的父进程ID
```

options
```bash
-c string If  the -c option is present, then commands are read from string.  If there are arguments after
          the string, they are assigned to the positional parameters, starting with $0.

#1 bash xxx
读取 xxx 文件的内容，并将 xxx 的内容作为 bash 脚本命令执行

# bash -c xxx
将 xxx 字符串作为 bash 命令进行执行，等价于 start a bash session and execute xxx command，如果后面跟位置参数则位置参数作为 xxx 的参数
bash -c 'echo "$0" "$1"' foo bar	# foo bar
bash -c 'echo "$@"' bash foo bar	# foo bar


-l        Make bash act as if it had been invoked as a login shell (see INVOCATION below).
```

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

## bash脚本中调用python返回的结果
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

```bash
# getopts
usage() { 
	echo "Usage: $0 [-p <80|443>] [-h <string>] [-f]" 1>&2
	exit 1
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

# check user is root or not
check_root () {
if [[ $EUID -ne 0 ]]; then
	echo "This script must be run as root" 
	exit 1
fi
}

# 使用统一的方式执行命令
startup() {
		case "$1" in
				*)
					"$@"
					;;
		esac
}

startup ls -l /tmp
  
# git prompt (可以放在~/.bashrc文件里)
color_my_prompt {
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

# bash实现spin功能
SPIN='-\|/'
spin {
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

# ($!) Expands to the process ID of the job most recently placed into the background,  whether executed as an asynchronous command or using the bg builtin


# while true
SPIN='-\|/'

i=0
# while true
while :
do
		i=$(( (i+1)%4 ))
		printf "\b${SPIN:$i:1}"
		sleep .1
done

  
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
repeat() {
	for ((i=0;i<$1;i++)); do
		eval ${*:2}
	done
}
# usage: repeat 5 echo "1 2 3"

# 获取当前执行脚本的路径
MY_PATH=$(dirname $(readlink -f ${BASH_SOURCE[0]}))
PYTHON_PATH=${MY_PATH}/../cathcart-utils/python
export PYTHONPATH=${PYTHON_PATH}:${PYTHONPATH}

```
``` bash
	  BASH_SOURCE
	   An  array  variable  whose  members  are  the source filenames where the corresponding shell 
	   function names in the FUNCNAME array variable are defined.  The shell function ${FUNCNAME[$i]}
	   is defined in the file ${BASH_SOURCE[$i]} and called from ${BASH_SOURCE[$i+1]}.
	  
	  MY_PATH=$(dirname $(readlink -f ${BASH_SOURCE[0]}))
	  PYTHON_PATH=${MY_PATH}/../cathcart-utils/python
	  export PYTHONPATH=${PYTHON_PATH}:${PYTHONPATH}

		arr=()	Create an empty array
		arr=(1 2 3)	Initialize array
		${arr[2]}	Retrieve third element
		${arr[@]}	Retrieve all elements
		${!arr[@]}	Retrieve array indices
		${#arr[@]}	Calculate array size
		arr[0]=3	Overwrite 1st element
		arr+=(4)	Append value(s)
		str=$(ls)	Save ls output as a string
		arr=( $(ls) )	Save ls output as an array of files
		${arr[@]:s:n}	Retrieve n elements starting at index s
```
[An introduction to Bash arrays](https://opensource.com/article/18/5/you-dont-know-bash-intro-bash-arrays)
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
	  while [ $# -gt 0 ]; do
	      case "$1" in
			-u | --update)
				UPDATE=true
				shift 1
				;;
			-d | --directory)
				BDIR="$2"
				shift 2
				;;
			-l | --layer) 
				LAYER="$2"
				shift 2
				;;
			-o | --download) 
				DL_DIR="$2"
				shift 2
				;;
			-s | --sstate)
				SSTATE_DIR="$2"
				shift 2
				;;
			-m | --manifest) 
				MANIFEST="$2"
				shift 2
				;;
			-h | --help)  
				usage;
				return
				;;
			*)   
				echo "Unrecognized Argument: $1"
				usage; 
				return
				;;
	      esac
	  done
	  
	  2 sh filename
	  启动一个新的子进程来执行脚本，脚本无法修改父shell环境中的变量
	  
	  3 ./filename
	  ./用来指定当前目录下的可执行文件，./filename 表示执行位于当前目录下的指定脚本文件，执行脚本时会启动
	  一个新的子进程来运行该脚本
	  ```
- Unix Shell I/O重定向
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
	- shell 脚本必须清晰可读的布局
		- 易于阅读，修改和维护
	- 避免使用不必要的命令
		- `grep "mystring" /tmp/myfile` 要优于 `cat /tmp/myfile | grep "mystring"` 因为创建和销毁进程会消耗操作系统的资源，特别是在多重循环等场景下。
- bash算术运算
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
	- login shell
		- `/etc/profile`
			- 全局配置脚本，适合于所有用户
				- `/etc/profile.d/*.sh`
					- 在`/etc/profile`中会判断该目录下是否包含shell脚本，如果包含则运行
		- `~/.bash_profile`
			- 特定用户的启动文件，可以用来扩展或者覆写全局配置脚本的配置
		- `~/.bash_login`
			- 如果`~/.bash_profile`不存在，则会读取该脚本
		- `~/.profile`
			- 如果`~/.bash_profile` 和 `~/.bash_login`都不存在，则会读取该脚本
	- non-login shell
		- `/etc/bash.bashrc`
			- 全局配置脚本，适合于所有用户
		- `~/.bashrc`
			- 特定用户的启动文件，可以用来扩展或者覆写全局配置脚本的配置
		- In addition to reading the startup files above, non-login shells also inherit the environment from their parent process, usually a login shell
		- 除了读取以上配置文件，non-login shell也继承父进程的环境变量，通常是一个login shell
- 流程控制
```bash
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
	      patterns )
		  	commands
			;;
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
	          -f | --file )
			  		shift
					filename="$1"
					;;
	          -i | --interactive )
			  		interactive=1
					;;
	          -h | --help )
			  		usage
					exit
					;;
	          * )
			  		usage
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
检查命令的退出状态
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
	- ```bash
	  # wc knows it is reading from file, so file name is listed
	  $ wc -l file.txt
	  3 file.txt
	  
	  # wc only knows it is reading from standard input, so file name is not listed
	  $ wc -l < file.txt
	  3
	  ```
- 丢弃命令输出
	- ```bash
	  command > /dev/null
	  command > /dev/null 2>&1
	  ```
- 命令分组
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
	- ```bash
	  myadd() {
	  	echo $(($1 + $2))
	  }
	  
	  echo "1 + 2 = $(myadd 1 2)"
	  ```
- 用户输入
	- ```bash
	  read name
	  echo "Hello $name"
	  
	  read -p "please input your name: " name
	  echo $name
	  ```
- 文件读写
	- ```bash
	  while read -r line
	  do
	  	echo "$line"
	  done <file.txt
	  ```
- here-string
	- ```bash
	  read first second <<< "hello world"
	  echo $second $first
	  ```
- 进程替换
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

- shell脚本格式检查
	- [shellcheck online](https://www.shellcheck.net/)
- Bash带颜色输出
	- ```bash
	  red='\e[0;31m'
	  blue='\e[0;34m'
	  green='\e[0;32m'
	  endColor='\e[0m'
	  
	  printf "%bhello world%b\n" ${red} ${endColor}
	  ```
- 通过`set`命令改变shell选项
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

[Bash Reference Manual](https://www.gnu.org/software/bash/manual/html_node/index.html#SEC_Contents)  
[How to Declare and Access Associative Array in Bash](https://phoenixnap.com/kb/bash-associative-array)  
[Bash Scripting – Associative Array Explained With Examples](https://ostechnix.com/bash-associative-array/)  
[Take control of your data with associative arrays in Bash](https://opensource.com/article/20/6/associative-arrays-bash)  
[Bash shift builtin command](https://www.computerhope.com/unix/bash/shift.htm)  
[A Complete Guide On How To Use Bash Arrays](https://www.shell-tips.com/bash/arrays/#gsc.tab=0)  
[**How to Use Command Line Arguments in a Bash Script**](https://www.baeldung.com/linux/use-command-line-arguments-in-bash-script)  
[pure bash bible](https://github.com/dylanaraps/pure-bash-bible?tab=readme-ov-file)  
[Bash getopts builtin command](https://www.computerhope.com/unix/bash/getopts.htm)  
[Parsing bash script options with getopts](https://sookocheff.com/post/bash/parsing-bash-script-arguments-with-shopts/)  
[Parse Command Line Arguments in Bash](https://www.baeldung.com/linux/bash-parse-command-line-arguments)  
[Double Quotes](https://www.gnu.org/software/bash/manual/html_node/Double-Quotes.html)  
[Single Quotes](https://www.gnu.org/software/bash/manual/html_node/Single-Quotes.html)  
[bash(1)](https://manpages.org/bash)  
[前后台进程、孤儿进程和 daemon 类进程的父子关系](https://www.cnblogs.com/f-ck-need-u/p/17718649.html)  
[GNU Bash Reference Manual](https://www.linuxtopia.org/online_books/bash_reference_guide/index.html)  
[Advanced Bash-Scripting Guide](https://www.linuxtopia.org/online_books/advanced_bash_scripting_guide/index.html) #online  
[Bash Reference Manual](https://www.gnu.org/software/bash/manual/bash.html#)  
[The set Command in Linux](https://www.baeldung.com/linux/set-command)  
[Pattern Matching](https://www.gnu.org/software/bash/manual/bash.html#Pattern-Matching)  
[Pattern Matching In Bash](https://www.linuxjournal.com/content/pattern-matching-bash)  
[Special Parameters](https://www.gnu.org/software/bash/manual/bash.html#Shell-Parameters)  
[阮一峰 Bash 脚本教程](https://www.bookstack.cn/books/bash-tutorial)  
[Unix Shell I/O重定向](https://m24y.com/index.php/2022/04/03/unix-shell-i-o%e9%87%8d%e5%ae%9a%e5%90%91/)  
[Unix Shell I/O重定向](http://teaching.idallen.com/cst8207/12w/notes/270_redirection.txt)  
[Bash scripting cheatsheet](https://devhints.io/bash)  
[Advanced Bash-Scripting Guide](https://tldp.org/LDP/abs/html/)  
[Shell Style Guide](https://google.github.io/styleguide/shellguide.html)  
[syntax brackets](https://ss64.com/bash/syntax-brackets.html)  
[Shell Scripting Primer](https://developer.apple.com/library/archive/documentation/OpenSource/Conceptual/ShellScripting/Introduction/Introduction.html#//apple_ref/doc/uid/TP40004268)  
[bash style guide](https://github.com/bahamas10/bash-style-guide) #github  
[coding standards](https://linuxcommand.org/lc3_adv_standards.php)  
[man sh](https://linux.die.net/man/1/sh)  
[编写健壮的 Shell 脚本](https://morven.life/posts/how-to-write-robust-shell-script/)  
[ANSI Escape Sequences  颜色输出](https://gist.github.com/fnky/458719343aabd01cfb17a3a4f7296797)  
[Everything you never wanted to know about ANSI escape codes](https://notes.burke.libbey.me/ansi-escape-codes/)  
[ANSI escape code generator](https://ansi.gabebanks.net/)  
[explain shell](https://explainshell.com/) #online  
[Find the Script’s Filename Within the Same Script in Bash](https://www.baeldung.com/linux/find-bash-script-filename)  
[Changing the Default Shell in Linux](https://www.baeldung.com/linux/change-default-shell)  
[Include Files in a Bash Shell Script With source Command](https://www.baeldung.com/linux/source-include-files)  
[Bash Source Command](https://linuxize.com/post/bash-source-command/)  
[BASH TIPS & TRICKS](https://tecadmin.net/category/bash-tips-tricks/)  
[bash-utility](https://github.com/labbots/bash-utility) #online #github  
[Google Style Guides](https://google.github.io/styleguide/) #online  
[pure-sh-bible](https://github.com/dylanaraps/pure-sh-bible)  
[Understanding 2>&1 in Bash: A Beginner’s Guide](https://tecadmin.net/io-redirection-operator-in-bash/)  