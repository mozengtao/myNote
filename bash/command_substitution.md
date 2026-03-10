## how `text=$(command)` works
1. Fork（分叉）： 主 Shell 调用 fork() 创建一个与自己完全一样的副本进程。  
2. Exec（执行）： 在子进程中执行括号内的 command。  
3. Pipe（管道）： 子进程的标准输出（stdout）通过管道重定向回主 Shell。  
4. Wait（等待）： 主 Shell 等待子进程结束，并捕获管道中的数据。  
5. Strip（剥离）： 删掉输出结果末尾的所有换行符。  
6. Assign（赋值）： 将最终结果存入变量中。  

## 对当前 Shell 变量的影响
```bash
# 子 Shell, 无法直接修改主 Shell 已有的变量
var=$(echo "hi")

# 子 Shell, 无效（因为 read 在管道右侧的子 Shell 里，变量 var 随子进程结束而消失）
echo "hi" | read var

# 当前 Shel, 有效（使用 Here-string 不产生子 Shell，var 会保留在当前环境）
read var <<< "hi"

# 在循环中读取文件并修改主 Shell 的变量

# 错误做法(使用了管道，管道右侧运行在子 Shell 中)
count=0
cat file.txt | while read line; do
    ((count++)) # 这里的 count 在子 Shell 里
done
echo $count # 结果还是 0！

# 正确做法（使用输入重定向）
count=0
while read line; do
    ((count++))
done < file.txt
echo $count # 结果正确，因为 while 在当前 Shell 运行

# 进程替换语法：将命令输出伪装成文件
while read -r line; do
    ((count++))
done < <(cat t1.txt)
echo "正确结果: $count"
```

### `< <(...)`
`< <(...)` 由两个独立的 Bash 特性组合而成的

1. `<(command)` (进程替换/Process Substitution)：
    - Bash 执行括号内的 command。
    - 关键点： 它不会把输出通过管道传给下一个进程，而是将输出映射到一个临时文件描述符（通常是 /dev/fd/63 这种虚拟文件）。
    - 这个表达式的结果就是一个“文件名” (`echo <(cat /etc/passwd)`)。

2. `<` (输入重定向)：
    - 将左侧的文件（也就是那个虚拟文件）作为输入传给主命令。

## Bash 命令分组对比：( ) vs { }
在 Bash 中，`( )` 和 `{ }` 都能将一组命令组合执行，但它们的底层实现和对环境的影响完全不同

### 本质区别表

| 特性 | `( list )` - 圆括号 | `{ list; }` - 大括号 |
| :--- | :--- | :--- |
| **运行环境** | **子 Shell (Subshell)** | **当前 Shell (Current Shell)** |
| **进程模型** | 调用 `fork()` 创建新进程 | 在当前进程内顺序执行 |
| **变量作用域** | **隔离**：内部修改不影响外部 | **共享**：内部修改直接改变外部 |
| **语法要求** | 较宽松，末尾无需分号 | **严格**：`{` 后须有空格，`}` 前须有分号 `;` |
| **性能开销** | 较高（涉及进程创建） | 极低（仅逻辑分组） |

### 语法避坑指南
- 圆括号 `()`
    它们是元字符，Bash 见到它们就知道要 fork 子进程，所以语法非常灵活

- 大括号 `{}` 的“三要素”：
    1. 左空格： { 后面必须有一个空格
    2. 右分号： 最后一个命令后面必须跟一个分号 ;（或者换行）
    3. 独立性： { 和 } 是关键字，不是元字符

### 应用场景
- 使用 ( )：
    当你需要临时切换目录执行任务，或者执行一些会污染环境变量的操作，但又不希望手动再切换回来或还原变量时

- 使用 { }：
    批量重定向： { cmd1; cmd2; } > output.txt（比开子进程高效）
    在当前环境生效： 需要在代码块中修改全局变量或定义函数

```bash
var=10; (var=20); echo $var # 10

var=10; { var=20; }; echo $var # 20

# 批量重定向示例
{
    echo "Starting at $(date)"
    ./my_script.sh
    echo "Finished at $(date)"
} >> task.log

# 临时捕获变量修改（并保留结果）
# 假设从文件中读取配置并更新状态
[ -f config.txt ] && {
    read -r user_id < config.txt
    status="Initialized"
    log_msg="User $user_id is ready."
}
# 此时 $user_id, $status, $log_msg 在此处全部可用
echo "Current Status: $status"

# 逻辑组合中的“短路运算”
# 这是运维脚本中最常见的写法。利用 &&（与）或 ||（或）来触发一组复杂的动作
# 如果备份失败，执行一系列恢复和通知操作
tar -czf backup.tar.gz /data || {
    echo "备份失败！"
    rm -f backup.tar.gz.tmp
    send_alert_email "admin@example.com"
    exit 1
}
# 原理： { } 将多行命令打包成一个逻辑单元。如果 tar 返回非零值，整个大括号块会被执行

# 定义“内联”过滤逻辑
# 当你不想为了一个简单的功能去专门写一个 function 函数名时，可以用 { } 快速实现
# 将输出内容统一加时间戳
{ date; df -h; free -m; } | awk '{print "[LOG] " $0}'

# 解决“管道丢失变量”的替代方案
# 典型场景： 在管道内部处理并使用变量
cat data.txt | {
    read -r first_line
    read -r second_line
    # 在这个大括号内部，变量是共享的
    echo "前两行是: $first_line 和 $second_line"
    # 你甚至可以继续在这里处理逻辑...
}
# 注意：离开大括号后，因为管道产生了子 Shell，变量依然会消失。

```

### 快速文件读取（Fast File Reading）: `$(< file)`
$(< file) 是 Bash 提供的一个高度优化的特殊语法，它的功能等同于 $(cat file)，但底层实现逻辑完全不同

- 工作原理：绕过外部命令
    当你执行 text=$(cat file) 时，Bash 需要执行以下繁琐步骤：
    1. Fork：创建一个子进程。
    2. Search：在 $PATH 中寻找 cat 可执行文件。
    3. Exec：加载并运行 cat。
    4. Read/Write：cat 读取文件并通过管道传回。

    而当你使用 $(< file) 时：
    内置化：Bash 识别出这个语法，不调用任何外部程序。
    直接读取：它直接在当前的子 Shell 环境中打开文件并读取其内容到内存缓冲区。
    效率：省去了进程创建和二进制文件加载的开销。

### 读取文件内容到变量
- Here-document (Here-doc): 将多行文本作为一个整体传递给某个命令的标准输入（stdin）
- 基本用法
```bash
# 如果不加引号：Bash 会解析文本里的变量
cat <<EOF
Line 1
Line 2
...
EOF

# 加了单引号：Bash 会禁用变量替换，原封不动地保留文本
cat <<'EOF'
Line 1
Line 2
...
EOF
```

- 进阶技巧：使用 <<- 忽略制表符
```bash
cat <<-EOF
Line 1
Line 2
...
EOF
```

**注意**：`<<EOF` 和 `<<-EOF` 在语法上是一个整体，结束标志都是 `EOF`

- 示例
```bash
# 1
PYTHON_SCRIPT=$(cat <<'PYTHON_SCRIPT'

import pexpect
import sys
import time
import signal

...

PYTHON_SCRIPT
)

# 2 忽略制表符
if true; then
    cat <<-EOF
	这前面的制表符（Tab）会被自动忽略
	这样代码依然可以保持缩进对齐
	EOF
fi

# 3
python3 <<'EOF'
print("这是直接交给 Python 执行的代码")
EOF

# 快速创建/覆盖文件
cat <<'EOF' > config.yaml
server:
  port: 8080
  debug: true
EOF

# 快速追加内容到文件末尾
sudo cat <<'EOF' >> /etc/hosts
127.0.0.1  my.local.test
EOF

# "清空" 一个文件（而不删除它）
: > access.log
# 或者
> access.log

# 生成Shell脚本
cat <<'EOF' > test.sh
#!/bin/bash
echo "Hello World"
EOF

# 追加多行内容
cat <<'EOF' >> ~/.bashrc
alias ll='ls -alF'
export PATH=$PATH:/usr/local/bin
EOF

# 快速清空文件
> filename.log
```

## `cat file.txt` 和 `cat <<EOF` 的执行差异
- 核心区别：参数 vs. 标准输入

- `cat file.txt`: file.txt 作为参数
    逻辑： 你把 file.txt 作为一个位置参数（Argument） 传给了 cat。

    cat 的内部逻辑：
    1.  cat 启动后，检查自己的参数列表。
    2.  它发现参数列表里有一个字符串 file.txt。
    3.  cat 的程序代码里写死了：“如果参数不是以 - 开头，就把它当作文件名去 open()。”
    4.  于是，cat 调用系统接口去硬盘里找这个文件。

- `cat <<EOF`: 作为标准输入
    逻辑： 你使用了 重定向符号 <<。

    Shell 的预处理：
    1.  在 cat 运行之前，Shell 先看到了 <<。
    2.  Shell 明白：cat 并不需要知道文件名，它只需要从它的 “嘴巴”（标准输入 stdin） 等着吃数据就行了。
    3.  Shell 把 EOF 之间的数据打包，直接塞进 cat 的标准输入。
    4.  cat 的内部逻辑： 它发现参数列表是空的，于是它就张开嘴（读取 stdin），有什么吃什么。

- cat 对不同输入的处理逻辑

| 命令形式 | 输入类型 | 谁在起作用 | cat 的理解 |
| :--- | :--- | :--- | :--- |
| `cat file.txt` | **位置参数** | cat 程序内部逻辑 | "这是一个路径，我去硬盘 open 它。" |
| `cat < file.txt` | **输入重定向** | Shell 环境 | "参数为空，我只管从 stdin 读数据。" |
| `cat <<EOF` | **Here-doc** | Shell 环境 | "参数为空，我只管从 Shell 塞给我的流里读。" |
| `echo "file.txt"` | **字符串** | echo 程序内部逻辑 | "我只管把参数原样打印出来。" |

```bash
# 读取 file1.txt -> 从标准输入读取 -> (Ctrl + D) -> 读取 file2.txt
cat file1.txt - file2.txt
```

## `$text` 是否添加双引号的差异
###  不加双引号 `$text`
- 如果你执行 echo $text：
    1. Bash 会查看环境变量 $IFS（默认是 空格、制表符、换行符）。
    2. 它把 $text 中所有匹配 IFS 的字符都视为分隔符。
    3. 原始内容被拆成多个独立的“单词”。
    4. 这些单词作为多个参数传给命令。
    5. echo 接收到多个参数后，默认用单个空格把它们连起来输出。

    结果： 所有的换行、多个连续空格、制表符，通通变成了单个空格。
```bash
local jobs=($(nomad status | grep "^evc" | awk '{print $1}'))

最终 jobs 数组的内容就是:
jobs[0]="evc-artist6"
jobs[1]="evc-banker"
jobs[2]="evc-barber"
...
jobs[22]="evc-yogi"


如果想只按换行符分割（更严谨的写法），可以用 mapfile：
mapfile -t jobs < <(nomad status | grep "^$grep_filter" | awk '{print $1}')
```

### 添加双引号 `"$text"`
- 如果你执行 echo "$text"：
    1. 双引号告诉 Bash：“把这整个变量看作一个整体，不要去管 IFS”
    2. 变量内部的换行、空格、制表符被原样保留并传给命令。