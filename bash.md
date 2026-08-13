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
- [shell-workshop](https://github.com/kamalmarhubi/shell-workshop)
- [Shell Style Guide](https://google.github.io/styleguide/shellguide.html)
- [BashPitfalls](https://mywiki.wooledge.org/BashPitfalls)
- [Bash Reference Manual](https://www.gnu.org/software/bash/manual/bash.html)
- [GNU Bash Reference Manual](https://www.linuxtopia.org/online_books/bash_reference_guide/index.html)
- [Bash Features](https://www.gnu.org/software/bash/manual/html_node/)
- [Shell Parameter Expansion](https://www.gnu.org/software/bash/manual/html_node/Shell-Parameter-Expansion.html)
- [Shell Expansions](https://www.gnu.org/software/bash/manual/html_node/Shell-Expansions.html)
- [Bash Variables](https://www.gnu.org/software/bash/manual/html_node/Bash-Variables.html)
> Let's build a shell!
- [Classic Shell Scripting](http://www.nylxs.com/docs/classicshellscripting.pdf)
- [The Unix Programming Environment](https://scis.uohyd.ac.in/~apcs/itw/UNIXProgrammingEnvironment.pdf)
- []()
- []()
- []()
- []()

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

- [Shell 最佳实践](bash/shell_best_practice.md)
- [start-stop-daemon 命令](bash/start-stop-daemon.md)
- [Bash命令替换 - command substitution](bash/command_substitution.md)
- [命令行参数解析 - command line arguments](bash/parse_cmd_args.md)
- [Bash 并行执行任务的方法 - run parallel functions](bash/run_parallel_functions.md)
- [匹配指定pattern及其后所有行 - pattern and rest lines](bash/get_pattern_rest_lines.md)
- [bash 读取文件中的命令行 - read command line file](bash/read_cmd_line_file.md)
- [read 命令](bash/read.md)
- [后台运行进程 - run service at background](bash/run_service_at_background.md)
- [Heredoc 用法](bash/heredoc.md)
- [Bash Array](bash/array.md)
- [ps 命令](bash/ps.md)
- [grep 命令](bash/grep.md)
- [string 操作](bash/string.md)
- [htop 命令](bash/htop.md)
- [通用 Bash 模板：模板 × 变量集合 = 任务展开执行器](bash/template_var_executor.md)
- [Bash / awk / sed 工程级实践指南](bash/bash_awk_sed.md)
- [Shell `{}` + `time` + 重定向 的工作模型](bash/time_redirection_model.md)
- [多层命令执行链（multi-layer command execution pipeline）的工程化实践与核心认知](bash/multi_layer_cmd_exec_pipeline.md)
- [SSH 中 `-t` / `-T` 与 PTY 深度总结](bash/ssh_option_t_T.md)
- [Bash 工程化示例](bash/evc_ncs_cli.sh)
- [Bash 中 "构造命令" 的方式总结](bash/bash_cmd_construct.md)
- [Linux Shell Script 最佳实践（工程级指南）](bash/shell_script_guide.md)
- [Linux Shell (Bash) 数组最佳实践指南](bash/shell_array_guide.md)
- [Linux Pipeline 底层实现：系统调用、Pipe 对象与数据流动心智模型](bash/how_pipe_works.md)
- [ Linux Process Substitution：>(cmd) 与 <(cmd) 的底层实现及心智模型](bash/how_process_sub_works.md)
- [Shell 下读取文件并逐行解析的最佳实践](bash/howto_read_file_line_by_line.md)
- [Shell 下读取文件并按行执行命令](bash/howto_read_file_and_execute_line.md)
- [Linux Pipeline（管道）工作原理及心智模型](bash/how_pipeline_works.md)
- [Shell 下并行执行（Parallel Execution）最佳实践](bash/parallel_execution.md)
- [Shell 函数执行与后台执行的心智模型](bash/how_function_call_works.md)
- [Bash 命令查找顺序（Command Resolution Order）心智模型](bash/cmd_resolve_order.md)
- [Linux / Bash 执行模型心智模式](bash/execute_model.md)
- [Bash Here Document (`<<`) 与重定向 (`>`) 的工作原理及心智模型](bash/here_doc_and_redirection.md)
- [Shell 脚本最佳实践 —— "数据"与"处理逻辑"分离](bash/data_logic_seperation.md)
- [Shell 脚本数据遍历最佳实践及心智模式](bash/data_traverse_buide.md)
- [复杂 Shell 命令的数据流心智模型（Data Flow Mental Model）](bash/cmd_io_stream.md)
- [复杂 Shell 脚本的组织心智模型与通用模板](bash/shell_script_organization.md)
- [大型 Bash 项目的组织方式——以「网站发布」为例](bash/howto_organize_shell_script.md)
- [大型 Bash 项目组织实战：可运行的分层发布 demo](bash/deploy_demo.md)
- [大型 Bash 项目最佳实践：配置加载、配置校验与命令抽象的心智模式](bash/bash_project_workflow.md)
- [Bash Primitive（命令原语）的设计哲学、最佳实践与心智模式](bash/bash_primitive_guide.md)
- [Shell 通用模板](bash/bash_general_template.sh)
- [Shell 构造命令流（Generate Command Stream）最佳实践与心智模型](bash/generate_cmd_stream.md)
- [围绕五大心智模型的 Shell 知识体系](bash/shell_mental_models.md)
- [Python 自动化项目代码组织与心智模式（完整示例）](bash/python_automation_guide.md)
- [Python 自动化示例项目实战：可运行的分层 network_audit demo](bash/python_automation_demo.md)
- [三种语言心智模型对比（Shell/Python/Go 日志聚合）](lang_mental_models/README.md)
- [Shell 示例：Text Stream 文本流驱动](lang_mental_models/shell/README.md)
- [Python 示例：Object Flow 对象流驱动](lang_mental_models/python/README.md)
- [Go 示例：Service/Message 服务消息流驱动](lang_mental_models/go/README.md)
- [Bash [[ ]] 字符串全操作语法](bash/string_oper.md)
- [Bash Template String（模板字符串）最佳实践与心智模式](bash/template_str.md)
- [Shell 中关于路径（Path）的黄金法则及心智模式](bash/shell_path_guide.md)
- [Bash 长命令拆分：自动续行（Line Continuation）完整总结](bash/line_cont.md)
- [Bash 并行执行方法总结](bash/bash-parallel-execution.md)
- [周期任务（Periodic Task）的心智模式（黄金法则）](bash/periodic_task.md)
- [Bash `printf` 多行模板：引号使用、格式保持与最佳实践](bash/printf_guide.md)
- [Bash `exec` 命令核心用法总结](bash/exec.md)
- []()
- []()
- []()
---

## 基础知识

### Shebang 与脚本执行

```bash
# 使用 env 工具（推荐）
#!/usr/bin/env bash
```

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
# find 默认跟随软链接，在 sysfs 虚拟文件系统里递归遍历被内核限制、目录读取不完整
R470-1:/tmp$ find /sys/class/net/ -name sriov_totalvfs
# 采用glob 通配直接遍历
R470-1:/tmp$ ls /sys/class/net/*/device/sriov_totalvfs
/sys/class/net/enp16495np0/device/sriov_totalvfs  /sys/class/net/enp16795np0/device/sriov_totalvfs
/sys/class/net/enp16505np1/device/sriov_totalvfs  /sys/class/net/enp16805np1/device/sriov_totalvfs

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

# ls 命令只列出文件夹
ls -d */
```
