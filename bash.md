# Bash 完整指南

## 目录

- [参考资源](#参考资源)
- [子文档链接](#子文档链接)
- [基础知识](#基础知识)
- [变量与参数](#变量与参数)
- [字符串操作](#字符串操作)
- [数组操作](#数组操作)
- [算术运算](#算术运算)
- [条件判断与控制流](#条件判断与控制流)
- [函数与脚本结构](#函数与脚本结构)
- [文件操作](#文件操作)
- [进程管理](#进程管理)
- [网络工具](#网络工具)
- [系统工具](#系统工具)
- [实用技巧](#实用技巧)
- [最佳实践](#最佳实践)
- [调试技巧](#调试技巧)

---

## 参考资源

### 官方文档
- [Bash Reference Manual](https://www.gnu.org/software/bash/manual/bash.html)
- [GNU Bash Reference Manual](https://www.linuxtopia.org/online_books/bash_reference_guide/index.html)
- [Bash Features](https://www.gnu.org/software/bash/manual/html_node/)
- [Shell Parameter Expansion](https://www.gnu.org/software/bash/manual/html_node/Shell-Parameter-Expansion.html)
- [Shell Expansions](https://www.gnu.org/software/bash/manual/html_node/Shell-Expansions.html)
- [Bash Variables](https://www.gnu.org/software/bash/manual/html_node/Bash-Variables.html)

### 教程与指南
- [Advanced Bash-Scripting Guide](https://hangar118.sdf.org/p/bash-scripting-guide/)
- [Advanced Bash-Scripting Guide](https://tldp.org/LDP/abs/html/)
- [阮一峰 Bash 脚本教程](https://www.bookstack.cn/books/bash-tutorial)
- [The Ultimate Bash Scripting Tutorial: From Beginner to Advanced](https://dev.to/mohammad1105/the-ultimate-bash-scripting-tutorial-from-beginner-to-advanced-3ipk)
- [Advanced Bash - More about the Command Line](https://groups.oist.jp/scs/advanced-bash)

### 实用工具与技巧
- [Notes](https://johannst.github.io/notes/intro.html#notes)
- [GNU bash shell tips](http://molk.ch/tips/gnu/bash/index.html)
- [5 Advanced Bash Scripting Techniques for Linux Users](https://tecadmin.net/advanced-bash-scripting-techniques/)
- [12 Advanced Bash Tricks for Writing High-Performance Scripts](https://medium.com/@obaff/12-advanced-bash-tricks-for-writing-high-performance-scripts-d904511be5be)
- [How to Use Regex in Bash Scripting](https://labex.io/tutorials/shell-how-to-use-regex-in-bash-scripting-392579)

### 风格指南
- [Google Shell Style Guide](https://google.github.io/styleguide/shellguide.html)
- [bash style guide](https://github.com/bahamas10/bash-style-guide)
- [coding standards](https://linuxcommand.org/lc3_adv_standards.php)

### 在线工具
- [explain shell](https://explainshell.com/)
- [Bash scripting cheatsheet](https://devhints.io/bash)
- [pure-bash-bible](https://github.com/dylanaraps/pure-bash-bible)
- [bash-utility](https://github.com/labbots/bash-utility)

---

## 子文档链接

- [start-stop-daemon 命令](./bash/start-stop-daemon.md)
- [Bash命令替换 - command substitution](./bash/command_substitution.md)
- [命令行参数解析 - command line arguments](./bash/parse_cmd_args.md)
- [Bash 并行执行任务的方法 - run parallel functions](./bash/run_parallel_functions.md)
- [匹配指定pattern及其后所有行 - pattern and rest lines](./bash/get_pattern_rest_lines.md)
- [bash 读取文件中的命令行 - read command line file](./bash/read_cmd_line_file.md)
- [read 命令](./bash/read.md)
- [后台运行进程 - run service at background](./bash/run_service_at_background.md)
- [Heredoc 用法](./bash/heredoc.md)
- [Bash Array](./bash/array.md)
- [ps 命令](./bash/ps.md)
- [grep 命令](./bash/grep.md)
- [string 操作](./bash/string.md)
- [htop 命令](./bash/htop.md)

---

## 基础知识

### Shebang 与脚本执行

```bash
# Shebang Interpreter Directive
#!interpreter [arguments]

# 使用绝对路径
#!/bin/bash

# 使用 env 工具（推荐）
#!/usr/bin/env bash
```

**特点说明：**
1. 指令必须是脚本的第一行
2. 必须以 `#!` 开头
3. `#!` 后可以有空白字符
4. 解释器必须是二进制文件的完整路径
5. 解释器参数是可选的

### 脚本执行方式

```bash
# 1. 直接执行脚本
bash file
chmod +x file && ./file

# 2. 从标准输入读取脚本
bash < file
bash -s < file
bash -s -- arg1 arg2 < file

# 3. 把文件内容作为命令行参数传递
bash -c "$(< file)"
bash -c "$(< file)" -- arg1 arg2
```

### 选项与参数

```bash
# -c: 从字符串读取命令
bash -c 'echo "$0" "$1"' foo bar  # foo bar

# -l: 作为登录shell启动
bash -l

# -x: 调试模式，打印每个执行的命令
bash -x script.sh

# -e: 遇到错误立即退出
set -e

# -u: 使用未设置的变量时报错
set -u
```

---

## 变量与参数

### 特殊变量

```bash
$0          # 脚本名称
$1, $2...   # 位置参数
$#          # 参数个数
$@          # 所有参数（保持独立性）
$*          # 所有参数（合并为字符串）
$$          # 当前进程ID
$!          # 最后一个后台进程ID
$?          # 最后一个命令的退出状态
$BASHPID    # 当前bash进程ID
$PPID       # 父进程ID
```

### 位置参数详解

```bash
# $* vs $@
log() {
    echo "日志: $*" >> log.txt  # 将所有参数作为整体字符串
}

wrapper() {
    ls "$@"  # 逐个传递参数，保持独立性
}

# 示例
log "操作成功" "用户: alice"     # 日志: 操作成功 用户: alice
wrapper -l dir1 "dir 2"         # 等价于 ls -l dir1 "dir 2"
```

### 参数展开

```bash
# 默认值
name=${1:-"World"}              # 如果$1为空，使用"World"
name=${1-"World"}               # 如果$1未设置，使用"World"

# 赋值默认值
name=${1:="World"}              # 如果$1为空，设置并使用"World"

# 错误处理
name=${1:?"参数不能为空"}        # 如果$1为空，输出错误并退出

# 长度
echo ${#name}                   # 变量长度

# 字符串截取
echo ${name:0:3}                # 从位置0开始，取3个字符
echo ${name:2}                  # 从位置2开始到末尾

# 模式匹配替换
echo ${name/old/new}            # 替换第一个匹配
echo ${name//old/new}           # 替换所有匹配
echo ${name#prefix}             # 删除最短前缀匹配
echo ${name##prefix}            # 删除最长前缀匹配
echo ${name%suffix}             # 删除最短后缀匹配
echo ${name%%suffix}            # 删除最长后缀匹配
```

### 环境变量与作用域

```bash
# 本地变量
local var="value"

# 环境变量
export VAR="value"

# 只读变量
readonly VAR="value"
declare -r VAR="value"

# 整数变量
declare -i counter=10

# 数组变量
declare -a array_name
declare -A assoc_array  # 关联数组
```

---

## 字符串操作

### 基本字符串操作

```bash
str="hello world"

# 长度
echo ${#str}                    # 11

# 截取
echo ${str:6:5}                 # "world"
echo ${str:6}                   # "world"

# 查找替换
echo ${str/world/bash}          # "hello bash"
echo ${str//o/0}                # "hell0 w0rld"

# 大小写转换
echo ${str^}                    # "Hello world" (首字母大写)
echo ${str^^}                   # "HELLO WORLD" (全部大写)
echo ${str,}                    # "hello world" (首字母小写)
echo ${str,,}                   # "hello world" (全部小写)

# 模式删除
filename="document.txt"
echo ${filename%.txt}           # "document"
echo ${filename%.*}             # "document"

path="/home/user/file.txt"
echo ${path##*/}                # "file.txt"
echo ${path%/*}                 # "/home/user"
```

### 字符串比较

```bash
str1="hello"
str2="world"

# 相等比较
[[ "$str1" == "$str2" ]]
[[ "$str1" = "$str2" ]]

# 不等比较
[[ "$str1" != "$str2" ]]

# 模式匹配
[[ "$str1" == h* ]]             # 以h开头
[[ "$str1" == *.txt ]]          # 以.txt结尾

# 正则匹配
[[ "$str1" =~ ^[a-z]+$ ]]       # 只包含小写字母
```

### 字符串检查

```bash
str="hello world"

# 检查是否为空
[[ -z "$str" ]]                 # 空字符串
[[ -n "$str" ]]                 # 非空字符串

# 检查结尾
[[ "$str" == *.txt ]]

# 正则表达式匹配
if [[ "hello123" =~ ^([a-z]+)([0-9]+)$ ]]; then
    echo "完整匹配: ${BASH_REMATCH[0]}"   # hello123
    echo "字母部分: ${BASH_REMATCH[1]}"   # hello
    echo "数字部分: ${BASH_REMATCH[2]}"   # 123
fi
```

---

## 数组操作

### 索引数组

```bash
# 创建数组
arr=()                          # 空数组
arr=(1 2 3)                     # 初始化
arr[0]=3                        # 设置元素

# 数组操作
echo ${arr[2]}                  # 第三个元素
echo ${arr[@]}                  # 所有元素
echo ${arr[*]}                  # 所有元素（合并）
echo ${!arr[@]}                 # 所有索引
echo ${#arr[@]}                 # 数组长度

# 添加元素
arr+=(4)                        # 追加
arr+=(5 6 7)                    # 追加多个

# 数组切片
echo ${arr[@]:2:3}              # 从索引2开始，取3个元素

# 遍历数组
for i in "${arr[@]}"; do
    echo "$i"
done

for i in "${!arr[@]}"; do
    echo "Index: $i, Value: ${arr[$i]}"
done
```

### 关联数组

```bash
# 声明关联数组
declare -A assoc

# 设置值
assoc[key1]="value1"
assoc[key2]="value2"

# 批量设置
declare -A config=(
    [host]="localhost"
    [port]="3306"
    [debug]="true"
)

# 访问
echo ${assoc[key1]}
echo ${config[host]}

# 遍历
for key in "${!assoc[@]}"; do
    echo "Key: $key, Value: ${assoc[$key]}"
done

# 检查键是否存在
[[ -v assoc[key1] ]] && echo "key1 exists"
```

### 实用数组示例

```bash
# 文件操作
files=(*.txt)
for file in "${files[@]}"; do
    echo "$file"
done

# 二进制转换表
dec2bin=({0..1}{0..1}{0..1}{0..1}{0..1}{0..1}{0..1}{0..1})
echo ${dec2bin[25]}

# 月份数组
month=("Jan" "Feb" "Mar" "Apr" "May" "Jun" 
       "Jul" "Aug" "Sep" "Oct" "Nov" "Dec")
echo ${month[3]}  # Apr
```

---

## 算术运算

### 算术运算方法

```bash
# 1. 使用 $(( ))
echo $((3 + 2))                 # 5
echo $((5 - 2))                 # 3
echo $((3 * 2))                 # 6
echo $((6 / 2))                 # 3
echo $((5 % 2))                 # 1

# 变量运算
num=10
echo $((num + 5))               # 15
((num++))                       # 自增
echo $((++num))                 # 先增后用
echo $((num++))                 # 先用后增

# 2. 使用 let
let result=3+2
echo $result                    # 5

# 3. 使用 expr
echo $(expr 3 + 2)              # 5
echo $(expr 3 \* 2)             # 6 (需要转义)

# 4. 浮点运算（使用 bc）
echo "3.5 + 2.1" | bc           # 5.6
echo "scale=2; 6.5 / 2.1" | bc  # 3.09

# 自定义计算函数
calculate() { 
    printf "%s\n" "$@" | bc -l 
}
calculate "1.5 * 2.3"
```

### 进制转换

```bash
# 不同进制
echo $((2#1010))                # 二进制转十进制: 10
echo $((8#17))                  # 八进制转十进制: 15
echo $((16#FF))                 # 十六进制转十进制: 255

# 进制计算
echo $((2#1010 + 2#1010))       # 20
echo $((0x10 + 0x10))           # 32
```

---

## 条件判断与控制流

### 条件测试

```bash
# 文件测试
[[ -f file ]]                   # 是否为文件
[[ -d dir ]]                    # 是否为目录
[[ -e path ]]                   # 是否存在
[[ -r file ]]                   # 是否可读
[[ -w file ]]                   # 是否可写
[[ -x file ]]                   # 是否可执行
[[ -s file ]]                   # 是否非空

# 字符串测试
[[ -z "$str" ]]                 # 空字符串
[[ -n "$str" ]]                 # 非空字符串
[[ "$str1" == "$str2" ]]        # 相等
[[ "$str1" != "$str2" ]]        # 不等
[[ "$str" =~ regex ]]           # 正则匹配

# 数值比较
[[ $num1 -eq $num2 ]]          # 等于
[[ $num1 -ne $num2 ]]          # 不等于
[[ $num1 -lt $num2 ]]          # 小于
[[ $num1 -le $num2 ]]          # 小于等于
[[ $num1 -gt $num2 ]]          # 大于
[[ $num1 -ge $num2 ]]          # 大于等于

# 逻辑运算
[[ condition1 && condition2 ]]  # 与
[[ condition1 || condition2 ]]  # 或
[[ ! condition ]]               # 非
```

### 控制结构

```bash
# if 语句
if [[ condition ]]; then
    commands
elif [[ condition2 ]]; then
    commands
else
    commands
fi

# case 语句
case $var in
    pattern1)
        commands
        ;;
    pattern2|pattern3)
        commands
        ;;
    *)
        commands
        ;;
esac

# for 循环
for i in {1..10}; do
    echo $i
done

for file in *.txt; do
    echo $file
done

for ((i=1; i<=10; i++)); do
    echo $i
done

# while 循环
while [[ condition ]]; do
    commands
done

# until 循环
until [[ condition ]]; do
    commands
done
```

### 循环控制

```bash
# break 和 continue
for i in {1..10}; do
    [[ $i -eq 5 ]] && continue  # 跳过5
    [[ $i -eq 8 ]] && break     # 在8处停止
    echo $i
done

# select 菜单
select option in "Option 1" "Option 2" "Quit"; do
    case $option in
        "Option 1")
            echo "You chose option 1"
            ;;
        "Option 2") 
            echo "You chose option 2"
            ;;
        "Quit")
            break
            ;;
    esac
done
```

---

## 函数与脚本结构

### 函数定义

```bash
# 推荐写法
function_name() {
    local param1=$1
    local param2=$2
    
    # 函数体
    echo "Processing $param1 and $param2"
    return 0  # 返回状态码
}

# 调用函数
function_name "arg1" "arg2"
```

### 函数参数处理

```bash
# 参数处理示例
greet() {
    local name=${1:-"World"}    # 默认参数
    echo "Hello, $name!"
}

# 处理可变参数
sum() {
    local total=0
    for num in "$@"; do
        ((total += num))
    done
    echo $total
}

# 检查参数数量
validate_args() {
    if [[ $# -lt 2 ]]; then
        echo "Error: Need at least 2 arguments"
        return 1
    fi
    echo "Arguments: $@"
}
```

### 函数返回值

```bash
# 通过 echo 返回值
get_sum() {
    local sum=$(( $1 + $2 ))
    echo $sum
}

result=$(get_sum 3 5)
echo "Result: $result"

# 通过引用传递返回多个值
get_user_info() {
    local -n name_ref=$1
    local -n age_ref=$2
    
    name_ref="Alice"
    age_ref=30
}

get_user_info name age
echo "Name: $name, Age: $age"

# 使用关联数组返回复杂数据
declare -A user_info
get_user_data() {
    user_info[name]="Bob"
    user_info[age]=25
    user_info[email]="bob@example.com"
}

get_user_data
echo "Name: ${user_info[name]}"
```

### 脚本结构最佳实践

```bash
#!/usr/bin/env bash

# 严格模式
set -euo pipefail

# 全局变量
readonly SCRIPT_DIR=$(dirname $(readlink -f ${BASH_SOURCE[0]}))
readonly SCRIPT_NAME=$(basename $0)

# 配置
DEBUG=false

# 函数定义
usage() {
    cat << EOF
Usage: $SCRIPT_NAME [OPTIONS]
    -h, --help      Show this help
    -d, --debug     Enable debug mode
EOF
}

log() {
    echo "[$(date +'%Y-%m-%d %H:%M:%S')] $*" >&2
}

debug() {
    $DEBUG && log "DEBUG: $*"
}

main() {
    # 参数解析
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                usage
                exit 0
                ;;
            -d|--debug)
                DEBUG=true
                ;;
            *)
                echo "Unknown option: $1"
                usage
                exit 1
                ;;
        esac
        shift
    done
    
    # 主逻辑
    log "Starting $SCRIPT_NAME"
    debug "Debug mode enabled"
    
    # 实际工作...
    
    log "Completed successfully"
}

# 脚本入口
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
```

---

## 文件操作

### 文件读取

```bash
# 最佳实践：使用 input redirection
script="$(< file)"              # 高效安全
content="$(cat file)"           # 启动外部进程

# 逐行读取
while IFS= read -r line; do
    echo "Line: $line"
done < file

# 读取到数组
mapfile -t lines < file
# 或
readarray -t lines < file

# 处理字段分隔的数据
while IFS=: read -r user pass uid gid gecos home shell; do
    echo "User: $user, Home: $home"
done < /etc/passwd
```

### 文件检查

```bash
check_file() {
    local file="$1"
    
    if [[ ! -e "$file" ]]; then
        echo "File does not exist: $file"
        return 1
    fi
    
    if [[ ! -r "$file" ]]; then
        echo "File is not readable: $file"
        return 1
    fi
    
    if [[ ! -s "$file" ]]; then
        echo "File is empty: $file"
        return 1
    fi
    
    echo "File is valid: $file"
    return 0
}
```

### Here Document 和 Here String

```bash
# Here Document
cat <<EOF > config.txt
[database]
host=localhost
port=3306
EOF

# Here String
wc -w <<< "Hello World"         # 计算单词数

# 变量 Here Document
mail_body="This is the message content"
mail -s "Subject" user@example.com <<EOF
$mail_body
EOF

# 不解析变量的 Here Document
cat <<'EOF'
$HOME will not be expanded
EOF
```

---

## 进程管理

### 后台进程

```bash
# 启动后台进程
command &
pid=$!

# 等待进程完成
wait $pid

# 检查进程是否存在
if kill -0 $pid 2>/dev/null; then
    echo "Process $pid is running"
else
    echo "Process $pid has stopped"
fi

# 批量后台处理
pids=()
for i in {1..5}; do
    sleep $i &
    pids+=($!)
done

# 等待所有后台进程
for pid in "${pids[@]}"; do
    wait $pid
done
```

### 信号处理

```bash
# 设置信号处理器
cleanup() {
    echo "Cleaning up..."
    rm -f /tmp/temp_file
    exit 0
}

trap cleanup EXIT INT TERM

# 动态调试控制
DEBUG=false

toggle_debug() {
    DEBUG=!DEBUG
    echo "Debug mode: $DEBUG"
}

trap 'toggle_debug' USR1

# 发送信号: kill -USR1 $$
```

### 进程监控

```bash
# 进程状态检查
check_process() {
    local process_name="$1"
    
    if pgrep -x "$process_name" >/dev/null; then
        echo "$process_name is running"
        return 0
    else
        echo "$process_name is not running"
        return 1
    fi
}

# 进程重启
restart_if_needed() {
    local service="$1"
    
    if ! check_process "$service"; then
        echo "Restarting $service..."
        systemctl restart "$service"
    fi
}
```

---

## 网络工具

### 网络连接测试

```bash
# 使用 curl 测试连接
test_connection() {
    local url="$1"
    local timeout=5
    
    if curl -I --connect-timeout $timeout "$url" >/dev/null 2>&1; then
        echo "Connection to $url successful"
        return 0
    else
        echo "Connection to $url failed"
        return 1
    fi
}

# 代理测试
test_proxy() {
    local proxy="$1"
    local target="https://www.google.com"
    
    if curl -I --connect-timeout 3 --proxy "$proxy" "$target" >/dev/null 2>&1; then
        echo "Proxy $proxy is working"
        return 0
    else
        echo "Proxy $proxy failed"
        return 1
    fi
}
```

### 网络信息获取

```bash
# 获取本机IP
get_local_ip() {
    hostname -I | awk '{print $1}'
}

# 获取公网IP
get_public_ip() {
    curl -s ifconfig.me
}

# 端口检查
check_port() {
    local host="$1"
    local port="$2"
    
    if nc -z "$host" "$port" 2>/dev/null; then
        echo "Port $port on $host is open"
        return 0
    else
        echo "Port $port on $host is closed"
        return 1
    fi
}
```

### Socket 操作

```bash
# Unix domain sockets

# 流式socket服务器
nc -lU /tmp/my.sock

# 流式socket客户端
nc -U /tmp/my.sock

# 数据报socket服务器
nc -lUu /tmp/my.sock

# 数据报socket客户端
nc -Uu /tmp/my.sock

# 使用 socat
# 服务器端
socat - UNIX-LISTEN:/tmp/my.sock

# 客户端
socat - UNIX-CONNECT:/tmp/my.sock

# 检查socket类型
ss -xa | grep my.sock
```

---

## 系统工具

### 系统信息

```bash
# 系统基本信息
get_system_info() {
    echo "Hostname: $(hostname)"
    echo "OS: $(uname -o)"
    echo "Kernel: $(uname -r)"
    echo "Architecture: $(uname -m)"
    echo "Uptime: $(uptime -p)"
}

# CPU 信息
get_cpu_info() {
    echo "CPU: $(grep 'model name' /proc/cpuinfo | head -1 | cut -d: -f2 | sed 's/^ *//')"
    echo "Cores: $(nproc)"
    echo "Load: $(cat /proc/loadavg | cut -d' ' -f1-3)"
}

# 内存信息
get_memory_info() {
    free -h | awk '/^Mem:/{print "Total:", $2, "Used:", $3, "Free:", $4}'
}

# 磁盘信息
get_disk_info() {
    df -h / | awk 'NR==2{print "Root partition:", $3"/"$2, "("$5" used)"}'
}
```

### 文件系统操作

```bash
# 查找文件
find_files() {
    local pattern="$1"
    local directory="${2:-.}"
    
    find "$directory" -name "$pattern" -type f
}

# 查找大文件
find_large_files() {
    local size="${1:-100M}"
    local directory="${2:-.}"
    
    find "$directory" -type f -size +"$size" -exec ls -lh {} \; | sort -k5 -hr
}

# 清理临时文件
cleanup_temp() {
    local temp_dirs=("/tmp" "/var/tmp")
    local days=7
    
    for dir in "${temp_dirs[@]}"; do
        find "$dir" -type f -mtime +$days -delete 2>/dev/null || true
    done
}
```

### 权限管理

```bash
# 检查用户权限
check_user() {
    if [[ $EUID -eq 0 ]]; then
        echo "Running as root"
        return 0
    else
        echo "Running as regular user: $(whoami)"
        return 1
    fi
}

# 检查sudo权限
has_sudo() {
    if sudo -n true 2>/dev/null; then
        echo "User has sudo access"
        return 0
    else
        echo "User does not have sudo access"
        return 1
    fi
}
```

---

## 实用技巧

### 命令分割与组合

```bash
# 长命令分割
long_command \
    --option1 value1 \
    --option2 value2 \
    --option3 value3

# 管道分割
cat /path/file.txt \
    | grep "pattern" \
    | sed 's/foo/bar/g' \
    > output.txt

# 条件分割
[[ -f /path/file.txt ]] \
    && echo "file exists" \
    || echo "file not found"

# 数组方式
args=(
    --verbose
    --input "/path/file"
    --output "/another/path"
)
my_command "${args[@]}"
```

### 字符串作为标准输入

```bash
# Here String (<<<)
cmd <<< "string"                # 传递单个字符串

# Here Document (<<)
cmd <<EOF
multi-line
content
EOF

# 管道
echo "string" | cmd

# 输入重定向
cmd < file.txt

# 实例
ncs_cli -u admin <<< 'show status'
ncs_cli -u admin <<EOF
show vmc status | t
EOF
echo 'show status' | ncs_cli -u admin
```

### 进程替换

```bash
# <(...) 进程替换语法
diff <(ls /dir1) <(ls /dir2)           # 比较两个目录
sort -m <(command1) <(command2)        # 合并排序

# 避免子shell问题
count=0
while IFS= read -r line; do
    ((count++))
done < <(cat /etc/passwd)
echo "Total lines: $count"

# 多重输出
command | tee >(grep "error" > errors.log) >(grep "info" > info.log) > output.log
```

### 颜色输出

```bash
# ANSI 颜色码
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
BOLD='\033[1m'
RESET='\033[0m'

# 或使用 tput
RED=$(tput setaf 1)
GREEN=$(tput setaf 2)
BOLD=$(tput bold)
RESET=$(tput sgr0)

# 日志函数
log_error() { echo -e "${RED}${BOLD}[ERROR]${RESET} $*" >&2; }
log_warn()  { echo -e "${YELLOW}[WARN]${RESET} $*" >&2; }
log_info()  { echo -e "${BLUE}[INFO]${RESET} $*"; }
log_ok()    { echo -e "${GREEN}[OK]${RESET} $*"; }

# 使用
log_error "Something went wrong"
log_ok "Operation completed"
```

### 配置文件解析

```bash
# 解析键值对配置
parse_config() {
    local file="$1"
    
    while IFS='=' read -r key value; do
        # 跳过空行和注释
        [[ -z "$key" || "$key" =~ ^# ]] && continue
        
        # 去除首尾空白
        key=$(echo "$key" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
        value=$(echo "$value" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
        
        # 导出变量
        export "$key=$value"
    done < "$file"
}

# 解析 JSON（需要 jq）
parse_json() {
    local file="$1"
    
    if ! command -v jq >/dev/null 2>&1; then
        echo "Error: jq not found" >&2
        return 1
    fi
    
    DB_HOST=$(jq -r '.database.host' "$file")
    DB_PORT=$(jq -r '.database.port' "$file")
    DEBUG=$(jq -r '.server.debug' "$file")
}
```

### 交互式脚本

```bash
# 目录切换后启动交互shell
interactive_dirs() {
    local dirs=("/path/dir1" "/path/dir2")
    
    for dir in "${dirs[@]}"; do
        (
            echo "Entering $dir..."
            cd "$dir" || exit 1
            bash -i  # 启动交互shell
            echo "Leaving $dir..."
        )
    done
}

# 使用 pushd/popd
navigate_dirs() {
    local dirs=("/path/dir1" "/path/dir2")
    
    for dir in "${dirs[@]}"; do
        pushd "$dir" >/dev/null || continue
        echo "Current dir: $(pwd)"
        $SHELL  # 启动默认shell
        popd >/dev/null
    done
}
```

---

## 最佳实践

### 脚本安全

```bash
# 严格模式
set -euo pipefail
# -e: 命令失败时退出
# -u: 使用未定义变量时退出  
# -o pipefail: 管道中任何命令失败时退出

# 安全的临时文件
temp_file=$(mktemp)
trap 'rm -f "$temp_file"' EXIT

# 安全的参数处理
process_file() {
    local file="$1"
    
    # 验证参数
    [[ -z "$file" ]] && { echo "Error: No file specified" >&2; return 1; }
    [[ ! -f "$file" ]] && { echo "Error: File not found: $file" >&2; return 1; }
    [[ ! -r "$file" ]] && { echo "Error: File not readable: $file" >&2; return 1; }
    
    # 处理文件...
}
```

### 错误处理

```bash
# 错误处理函数
error_exit() {
    echo "Error: $1" >&2
    exit "${2:-1}"
}

# 使用
[[ -f "$config_file" ]] || error_exit "Config file not found" 2

# 命令成功检查
run_command() {
    local cmd="$1"
    
    if ! $cmd; then
        error_exit "Command failed: $cmd" $?
    fi
}

# 重试机制
retry() {
    local max_attempts=3
    local delay=2
    local attempt=1
    
    while [[ $attempt -le $max_attempts ]]; do
        if "$@"; then
            return 0
        fi
        
        echo "Attempt $attempt failed. Retrying in ${delay}s..." >&2
        sleep $delay
        ((attempt++))
    done
    
    echo "All $max_attempts attempts failed" >&2
    return 1
}
```

### 参数解析

```bash
# getopts 短选项
parse_short_options() {
    while getopts "a:b:ch" opt; do
        case $opt in
            a) ARG_A="$OPTARG" ;;
            b) ARG_B="$OPTARG" ;;
            c) FLAG_C=true ;;
            h) usage; exit 0 ;;
            \?) echo "Invalid option: -$OPTARG" >&2; exit 1 ;;
            :) echo "Option -$OPTARG requires an argument" >&2; exit 1 ;;
        esac
    done
    shift $((OPTIND-1))
}

# 长选项处理
parse_long_options() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -a|--arg1)
                ARG_A="$2"
                shift 2
                ;;
            -b|--arg2)
                ARG_B="$2"
                shift 2
                ;;
            -c|--config)
                CONFIG_FILE="$2"
                shift 2
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            --)
                shift
                break
                ;;
            -*)
                echo "Unknown option: $1" >&2
                exit 1
                ;;
            *)
                break
                ;;
        esac
    done
}
```

### 日志记录

```bash
# 日志系统
LOG_LEVEL="INFO"
LOG_FILE=""

log() {
    local level="$1"
    shift
    local message="$*"
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    
    # 输出格式
    local log_entry="[$timestamp] [$level] $message"
    
    # 输出到文件和/或控制台
    if [[ -n "$LOG_FILE" ]]; then
        echo "$log_entry" >> "$LOG_FILE"
    fi
    
    # 根据级别决定是否输出到控制台
    case $level in
        ERROR) echo "$log_entry" >&2 ;;
        WARN|INFO) echo "$log_entry" ;;
        DEBUG) [[ "$LOG_LEVEL" == "DEBUG" ]] && echo "$log_entry" ;;
    esac
}

# 便捷函数
log_debug() { log "DEBUG" "$@"; }
log_info()  { log "INFO" "$@"; }
log_warn()  { log "WARN" "$@"; }
log_error() { log "ERROR" "$@"; }
```

### 并发处理

```bash
# 并行处理
parallel_process() {
    local max_jobs=4
    local job_count=0
    local pids=()
    
    for item in "${items[@]}"; do
        # 如果达到最大并发数，等待一个任务完成
        if [[ $job_count -ge $max_jobs ]]; then
            wait "${pids[0]}"
            pids=("${pids[@]:1}")  # 移除第一个PID
            ((job_count--))
        fi
        
        # 启动新任务
        process_item "$item" &
        pids+=($!)
        ((job_count++))
    done
    
    # 等待所有剩余任务
    for pid in "${pids[@]}"; do
        wait "$pid"
    done
}
```

---

## 调试技巧

### 调试选项

```bash
# 调试模式
set -x                          # 打印执行的命令
set +x                          # 关闭调试

# 或者运行时启用
bash -x script.sh

# 部分调试
debug_function() {
    set -x
    # 需要调试的代码
    set +x
}
```

### 调试函数

```bash
# 调试输出
debug_echo() {
    if [[ -n "${DEBUG:-}" ]]; then
        echo "[DEBUG] $*" >&2
    fi
}

# 变量检查
debug_vars() {
    if [[ -n "${DEBUG:-}" ]]; then
        echo "=== Debug Variables ===" >&2
        for var in "$@"; do
            echo "$var = ${!var}" >&2
        done
        echo "======================" >&2
    fi
}

# 使用示例
DEBUG=1
name="John"
age=30
debug_vars name age
```

### 错误追踪

```bash
# 错误追踪函数
trace_error() {
    local frame=0
    echo "Error occurred in:" >&2
    
    while caller $frame >&2; do
        ((frame++))
    done
}

# 设置错误处理
trap 'trace_error' ERR

# 详细错误信息
detailed_error() {
    echo "Error on line $1" >&2
    echo "Command: $2" >&2
    echo "Exit code: $3" >&2
}

trap 'detailed_error $LINENO "$BASH_COMMAND" $?' ERR
```

### 性能分析

```bash
# 时间测量
time_command() {
    local start=$(date +%s%N)
    "$@"
    local end=$(date +%s%N)
    local duration=$(( (end - start) / 1000000 ))
    echo "Command took ${duration}ms" >&2
}

# 内存使用监控
monitor_memory() {
    local pid=$1
    
    while kill -0 "$pid" 2>/dev/null; do
        ps -o pid,vsz,rss,comm -p "$pid"
        sleep 1
    done
}
```

### 测试框架

```bash
# 简单测试框架
test_count=0
test_passed=0
test_failed=0

assert_equals() {
    local expected="$1"
    local actual="$2"
    local test_name="$3"
    
    ((test_count++))
    
    if [[ "$expected" == "$actual" ]]; then
        echo "✓ PASS: $test_name"
        ((test_passed++))
    else
        echo "✗ FAIL: $test_name"
        echo "  Expected: '$expected'"
        echo "  Actual:   '$actual'"
        ((test_failed++))
    fi
}

# 测试报告
test_report() {
    echo
    echo "Test Results:"
    echo "  Total:  $test_count"
    echo "  Passed: $test_passed"
    echo "  Failed: $test_failed"
    
    if [[ $test_failed -eq 0 ]]; then
        echo "All tests passed!"
        return 0
    else
        echo "Some tests failed!"
        return 1
    fi
}

# 使用示例
assert_equals "5" "$(echo $((2 + 3)))" "Addition test"
assert_equals "hello" "$(echo hello)" "Echo test"
test_report
```

---

## 工具命令参考

### 核心工具

#### find
```bash
# 查找并执行操作
find . -name "*.txt" -exec cat {} \;      # 逐个执行
find . -name "*.txt" -exec cat {} +       # 批量执行
find . -name "*.cfg" -print0 | xargs -0 vim

# 时间过滤
find /var/log -name "*.log" -mtime +7     # 7天前修改的文件
find . -type f -newermt "2023-01-01"      # 指定日期后的文件

# 权限和大小
find . -type f -perm 644                  # 特定权限
find . -type f -size +100M                # 大于100MB的文件
```

#### xargs
```bash
# 基本用法
echo "file1 file2" | xargs cat            # 将输入作为参数
find . -name "*.tmp" -print0 | xargs -0 rm -f

# 批处理
cat files.txt | xargs -n 1 process_file   # 每次处理一个
seq 1 10 | xargs -P 4 -I {} process {}    # 并行处理

# 复杂命令
pidof process | xargs -I{} kill -9 {}
```

#### awk
```bash
# 字段处理
awk '{print $1, $3}' file                 # 打印第1和第3字段
awk -F: '{print $1}' /etc/passwd           # 指定分隔符

# 条件处理
awk '$3 > 1000' /etc/passwd                # 第3字段大于1000的行
awk '/pattern/ {print $0}' file            # 包含模式的行

# 使用变量
pattern="def"
echo "abc def gh" | awk -v pat="$pattern" '$0 ~ pat { print $3 }'

# 统计
awk '{sum += $1} END {print sum}' numbers.txt

# 复杂处理
awk 'BEGIN {FS=":"} {users[$3]++} END {for(u in users) print u, users[u]}' /etc/passwd
```

#### jq (JSON处理)
```bash
# 基本用法
jq '.' data.json                           # 格式化显示
jq '.name' data.json                       # 获取字段
jq '.users[]' data.json                    # 数组展开
jq -r '.users[].name' data.json            # 原始输出

# 过滤和映射
jq '.users[] | select(.age > 30)' data.json
jq '.users | map(.name)' data.json
jq '.users | map(select(.active == true)) | length' data.json

# 构造新对象
jq '{name: .users[0].name, count: (.users | length)}' data.json
```

---

## 高级示例

### 复杂脚本示例

```bash
#!/usr/bin/env bash

# 系统监控脚本示例
set -euo pipefail

readonly SCRIPT_DIR=$(dirname $(readlink -f ${BASH_SOURCE[0]}))
readonly LOG_FILE="/var/log/system_monitor.log"
readonly PID_FILE="/var/run/system_monitor.pid"

# 配置
MONITOR_INTERVAL=60
CPU_THRESHOLD=80
MEMORY_THRESHOLD=90
DISK_THRESHOLD=85

# 日志函数
log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" | tee -a "$LOG_FILE"
}

# 检查是否已在运行
check_running() {
    if [[ -f "$PID_FILE" ]] && kill -0 "$(cat "$PID_FILE")" 2>/dev/null; then
        echo "Script is already running (PID: $(cat "$PID_FILE"))"
        exit 1
    fi
    echo $$ > "$PID_FILE"
}

# 清理函数
cleanup() {
    log "Shutting down monitor..."
    rm -f "$PID_FILE"
    exit 0
}

# 获取系统指标
get_cpu_usage() {
    top -bn1 | grep "Cpu(s)" | awk '{print $2}' | cut -d'%' -f1
}

get_memory_usage() {
    free | grep Mem | awk '{printf("%.0f", $3/$2 * 100)}'
}

get_disk_usage() {
    df / | tail -1 | awk '{print $5}' | cut -d'%' -f1
}

# 告警函数
send_alert() {
    local metric="$1"
    local value="$2"
    local threshold="$3"
    
    local message="ALERT: $metric usage is ${value}% (threshold: ${threshold}%)"
    log "$message"
    
    # 这里可以添加邮件或其他通知机制
    # mail -s "System Alert" admin@example.com <<< "$message"
}

# 监控循环
monitor_system() {
    while true; do
        local cpu_usage=$(get_cpu_usage | cut -d',' -f1)
        local memory_usage=$(get_memory_usage)
        local disk_usage=$(get_disk_usage)
        
        log "CPU: ${cpu_usage}%, Memory: ${memory_usage}%, Disk: ${disk_usage}%"
        
        # 检查阈值
        (( $(echo "$cpu_usage > $CPU_THRESHOLD" | bc -l) )) && 
            send_alert "CPU" "$cpu_usage" "$CPU_THRESHOLD"
        
        (( memory_usage > MEMORY_THRESHOLD )) && 
            send_alert "Memory" "$memory_usage" "$MEMORY_THRESHOLD"
        
        (( disk_usage > DISK_THRESHOLD )) && 
            send_alert "Disk" "$disk_usage" "$DISK_THRESHOLD"
        
        sleep $MONITOR_INTERVAL
    done
}

# 主函数
main() {
    check_running
    trap cleanup EXIT INT TERM
    
    log "Starting system monitor (PID: $$)"
    log "Monitoring interval: ${MONITOR_INTERVAL}s"
    log "Thresholds - CPU: ${CPU_THRESHOLD}%, Memory: ${MEMORY_THRESHOLD}%, Disk: ${DISK_THRESHOLD}%"
    
    monitor_system
}

# 入口点
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
```

### 数据处理脚本

```bash
#!/usr/bin/env bash

# 日志分析脚本
process_logs() {
    local log_file="$1"
    local output_dir="${2:-./analysis}"
    
    mkdir -p "$output_dir"
    
    log "Processing log file: $log_file"
    
    # IP 访问统计
    awk '{print $1}' "$log_file" | sort | uniq -c | sort -rn > "$output_dir/ip_stats.txt"
    
    # 状态码统计
    awk '{print $9}' "$log_file" | grep -E '^[0-9]+$' | sort | uniq -c > "$output_dir/status_codes.txt"
    
    # 访问量时间分布
    awk '{print $4}' "$log_file" | sed 's/\[//g' | cut -d: -f2 | sort | uniq -c > "$output_dir/hourly_stats.txt"
    
    # 最常访问的页面
    awk '{print $7}' "$log_file" | sort | uniq -c | sort -rn | head -20 > "$output_dir/top_pages.txt"
    
    # 错误日志提取
    awk '$9 >= 400 {print}' "$log_file" > "$output_dir/errors.txt"
    
    log "Analysis complete. Results in: $output_dir"
}

# 生成报告
generate_report() {
    local analysis_dir="$1"
    local report_file="$2"
    
    cat > "$report_file" <<EOF
# 日志分析报告

## 生成时间: $(date)

## TOP 10 访问IP
$(head -10 "$analysis_dir/ip_stats.txt")

## HTTP状态码分布
$(cat "$analysis_dir/status_codes.txt")

## 小时访问量分布
$(cat "$analysis_dir/hourly_stats.txt")

## TOP 10 访问页面
$(head -10 "$analysis_dir/top_pages.txt")

## 错误统计
错误日志条数: $(wc -l < "$analysis_dir/errors.txt")
EOF
    
    log "Report generated: $report_file"
}
```

---

## Tips 集合

```bash
# 判断 命令 输出是否为空
# 示例：判断 grep 是否找到内容
if grep "关键词" 文件名; then
    echo "找到内容，输出不为空"
else
    echo "未找到，输出为空"
fi

if ! grep "关键词" 文件名; then
    echo "输出为空"
fi

# 把命令输出存到变量
result=$(grep "abc" test.txt)

# 判断变量是否为空
if [ -z "$result" ]; then
    echo "输出为空"
else
    echo "输出不为空：$result"
fi

# 没传参数就用默认值

ip="${1:-127.0.0.1}"
port="${2:-8080}"

echo "IP: $ip, PORT: $port"

# ===================== 默认配置 =====================
DEFAULT_HOST="127.0.0.1"
DEFAULT_PORT="3306"
DEFAULT_USER="root"
# =====================================================

# 参数传入
HOST="${1:-$DEFAULT_HOST}"
PORT="${2:-$DEFAULT_PORT}"
USER="${3:-$DEFAULT_USER}"


# 临时取消 / 屏蔽环境变量

# 方案 1：执行脚本时临时清空代理（推荐）
env -u HTTP_PROXY -u HTTPS_PROXY ./your_script.sh
unset HTTP_PROXY HTTPS_PROXY && ./your_script.sh

# 方案 2：在 bash 脚本内部自动关闭代理（最省心）
# 关闭代理（只在本脚本内生效）
unset HTTP_PROXY HTTPS_PROXY http_proxy https_proxy

# 方案 3：给单个命令临时禁用代理 (只想让脚本里某几个命令不走代理，其他命令继续用代理)
# 脚本里这个 curl 不走代理
env -u HTTP_PROXY -u HTTPS_PROXY curl https://example.com

# 脚本里下面的命令继续使用系统代理
curl https://google.com


# 快速查看文件头部
head -c 16 test.pcap | hexdump -C

# 分割长命令
long_command \
    --option1 value1 \
    --option2 value2 \
    --option3 value3

# 数组用于复杂参数
args=(
    --verbose
    --input "/path/file"  
    --output "/another/path"
)
my_command "${args[@]}"

# here document 多行输入
cat <<EOF
This is a long message
that spans multiple lines
without needing backslashes.
EOF

# 检查命令是否存在
if ! command -v jq &> /dev/null; then
    echo "Error: jq is not installed"
    exit 1
fi

# 等待后台任务完成
jobs_running() {
    jobs -r | wc -l
}

while [[ $(jobs_running) -gt 0 ]]; do
    sleep 1
done

# 文件系统检查
identify_filesystem() {
    local path="$1"
    df -T "$path" | tail -1 | awk '{print $2}'
}

readonly_check() {
    local path="$1"
    if mount | grep "$(df "$path" | tail -1 | awk '{print $1}')" | grep -q "ro,"; then
        echo "Read-only filesystem"
        return 0
    else
        echo "Read-write filesystem"
        return 1
    fi
}
```

---

这个重新组织的文档现在具有：

1. **清晰的目录结构** - 从基础到高级，逻辑递进
2. **分类明确** - 按功能分组，便于查找
3. **保留所有原内容** - 没有删除任何有价值的信息
4. **添加了导航** - 目录链接便于快速跳转
5. **实用示例丰富** - 每个部分都有实际可用的代码
6. **最佳实践集中** - 将经验和技巧整理成专门章节

现在这个文档既可以作为学习教程，也可以作为日常工作的参考手册使用。