# Bash Here Document (`<<`) 与重定向 (`>`) 的工作原理及心智模型

## 一、问题

下面两种写法为什么结果完全一样？

### 写法一

```bash
cat > /tmp/t1.txt <<'EOF'
aa
bb
EOF
```

### 写法二

```bash
cat <<'EOF' > /tmp/t1.txt
aa
bb
EOF
```

执行结果都是：

```
/tmp/t1.txt
┌────┐
│ aa │
│ bb │
└────┘
```

很多人直觉会认为：

```
写法一：

>
再 <<
```

而

```
写法二：

<<
再 >
```

所以应该有区别。

实际上：

> **Bash 并不是按照命令中从左到右立即执行每一个重定向，而是先解析整个命令，再统一处理所有重定向。**

---

# 二、正确的心智模型

应该把 Bash 的执行理解成：

```
Shell Script
      │
      ▼
Lexical Analysis（词法分析）
      │
      ▼
Parser（语法解析）
      │
      ▼
Simple Command
      │
      ├──────── argv
      │
      └──────── Redirection List
      │
      ▼
收集 Here Document
      │
      ▼
准备所有 FD
      │
      ▼
fork（外部命令）
      │
      ▼
child 设置所有 dup2()
      │
      ▼
exec()
```

重点：

> **Parser 会先得到完整的 Command，而不是边解析边执行。**

---

# 三、Parser 如何理解这两个命令

## 第一种

```bash
cat > file <<EOF
```

Parser 得到：

```
Simple Command
│
├── argv
│      cat
│
└── Redirection List
       │
       ├── > file
       │
       └── <<EOF
```

---

## 第二种

```bash
cat <<EOF > file
```

Parser 得到：

```
Simple Command
│
├── argv
│      cat
│
└── Redirection List
       │
       ├── <<EOF
       │
       └── > file
```

唯一不同：

```
Redirection List 的顺序不同
```

不是：

```
执行顺序不同
```

---

# 四、Shell 真正的执行流程

对于：

```bash
cat <<EOF >file
aa
bb
EOF
```

Shell 的执行过程可以理解为：

```
            Shell
              │
              ▼
      解析整个命令
              │
              ▼
      发现有 Here Document
              │
              ▼
    继续读取直到 EOF
              │
              ▼
   得到 Here Document 内容

       aa
       bb

              │
              ▼
  创建一个临时输入（pipe 或临时文件）
              │
              ▼
        open(file)
              │
              ▼
      fork()
              │
              ▼
      child process
              │
      ┌───────┴────────┐
      │                │
dup2(stdin)      dup2(stdout)
      │                │
      ▼                ▼
here-doc          target file
      │                │
      └───────┬────────┘
              │
              ▼
          exec(cat)
```

因此：

当 `cat` 真正开始执行的时候：

```
stdin
↓

Here Document

stdout
↓

file
```

cat 根本不知道：

```
<<EOF 写在前面

还是

>file 写在前面
```

它看到的只是：

```
FD0
FD1
```

已经准备好了。

---

# 五、为什么结果完全一样？

对于：

```bash
cat >file <<EOF
```

最终 FD：

```
stdin
↓

Here Document

stdout
↓

file
```

对于：

```bash
cat <<EOF >file
```

最终 FD：

```
stdin
↓

Here Document

stdout
↓

file
```

最终完全一致：

```
             stdin
               │
               ▼
        +--------------+
        |     cat      |
        +--------------+
               │
               ▼
            stdout
               │
               ▼
             file
```

所以：

```
输出完全一致
```

---

# 六、为什么 Bash 可以这样做？

POSIX 对 Simple Command 的定义：

```
Simple Command

=

assignment*
word*
redirection*
```

重定向可以穿插在任何位置：

例如：

```bash
cat >a <b
```

```bash
>file cat <input
```

```bash
VAR=1 cat >a
```

```bash
cat <<EOF >a
```

全部合法。

Parser 最终都会整理成：

```
argv

cat

Redirection List

stdin
stdout
stderr
```

因此：

> **命令中的重定向位置只是语法，不代表执行顺序。**

---

# 七、真正决定命令行为的是 FD

Shell 最终关心的是三个文件描述符：

```
FD0
stdin

FD1
stdout

FD2
stderr
```

例如：

```
cat <<EOF >file
```

最终：

```
FD0

↓

Here Document

FD1

↓

file

FD2

↓

terminal
```

而：

```
cat >file <<EOF
```

最终：

```
FD0

↓

Here Document

FD1

↓

file

FD2

↓

terminal
```

完全一致。

---

# 八、什么时候顺序真的会影响结果？

很多人因此误认为：

> **所有 Redirection 都可以交换顺序。**

实际上：

**只有彼此独立的重定向才能交换。**

例如：

```
<<EOF
```

建立：

```
stdin
```

而：

```
>file
```

建立：

```
stdout
```

两者没有依赖。

所以：

```
<<EOF >file

与

>file <<EOF
```

没有区别。

---

但是：

```
2>&1
```

表示：

```
stderr

复制

当前 stdout
```

注意：

```
复制的是"当前"的 stdout
```

所以：

### 第一种

```bash
cmd >file 2>&1
```

执行过程：

```
stdout

↓

file

stderr

↓

复制 stdout

↓

file
```

最终：

```
stdout → file

stderr → file
```

---

### 第二种

```bash
cmd 2>&1 >file
```

执行过程：

```
stderr

↓

复制当前 stdout

↓

terminal

然后：

stdout

↓

file
```

最终：

```
stdout → file

stderr → terminal
```

因此：

```
>file 2>&1

≠

2>&1 >file
```

因为：

```
2>&1

依赖

FD1 当前的状态
```

所以顺序影响最终结果。

---

# 九、底层伪代码理解

可以把：

```bash
cat <<EOF >file
aa
bb
EOF
```

理解成：

```c
parse_command();

collect_here_document();

pid = fork();

if (pid == 0) {

    int fd = open("file",
                  O_WRONLY | O_CREAT | O_TRUNC);

    dup2(fd, STDOUT_FILENO);

    dup2(here_doc_fd, STDIN_FILENO);

    execvp("cat", argv);
}
```

而：

```bash
cat >file <<EOF
```

最终生成的流程几乎一样：

```c
parse_command();

collect_here_document();

pid = fork();

if (pid == 0) {

    int fd = open("file",
                  O_WRONLY | O_CREAT | O_TRUNC);

    dup2(fd, STDOUT_FILENO);

    dup2(here_doc_fd, STDIN_FILENO);

    execvp("cat", argv);
}
```

因此两者没有任何区别。

---

# 十、完整心智模型（推荐牢记）

```
Bash Command

      cat >file <<EOF
      ----------------------
             │
             ▼
      Parser 解析整个命令
             │
             ▼
      argv = cat
             │
             ▼
      Redirection List
             │
             ├── stdout -> file
             │
             └── stdin <- here-doc
             │
             ▼
      Shell 收集 Here Document
             │
             ▼
      Shell 准备所有 FD
             │
             ▼
      （外部命令）fork
             │
             ▼
      child 调用 dup2()
             │
             ▼
      exec(cat)
             │
             ▼
      cat 只看到：

      stdin  ← here-doc

      stdout → file
```

---

# 十一、核心结论

## 1. Bash 不会边解析边执行

Shell 会：

- 先完成整个命令的语法解析（Parser）
- 收集 Here Document 的全部内容
- 构建完整的重定向列表
- 最后统一设置文件描述符（FD）并执行命令

---

## 2. Here Document 在命令执行前就已准备好

`<<EOF` 的文本会在 `exec()` 之前由 Shell 全部读取并保存（通常通过管道或临时文件），命令启动后只是从标准输入读取这些数据。

---

## 3. 命令真正看到的是最终的文件描述符

对于外部命令而言，它不知道重定向的书写顺序，只能看到：

- `stdin` 指向哪里
- `stdout` 指向哪里
- `stderr` 指向哪里

因此：

```
cat >file <<EOF

与

cat <<EOF >file
```

最终完全等价。

---

## 4. 判断重定向顺序是否重要的原则

建立如下判断规则：

```
         两个 Redirection
                 │
                 ▼
          是否依赖同一个 FD 的当前状态？
                 │
        ┌────────┴────────┐
        │                 │
       否                是
        │                 │
        ▼                 ▼
 顺序通常无影响      顺序会影响最终结果
```

典型示例：

| 重定向组合 | 是否依赖 | 顺序是否影响 |
|------------|----------|--------------|
| `<<EOF` 与 `>file` | 否 | ❌ 不影响 |
| `<file` 与 `>file` | 否 | ❌ 不影响 |
| `2>&1` 与 `>file` | 是 | ✅ 影响 |
| `1>&2` 与 `2>file` | 是 | ✅ 影响 |

> **记忆口诀：**
>
> **重定向的位置只是语法；真正决定命令行为的是最终建立好的文件描述符（FD）。只有引用其他 FD 当前状态（如 `2>&1`）的重定向，书写顺序才会影响最终结果。**

---

# 十二、书写风格推荐：`<<EOF` 应放在 `>file` 之前

虽然两种写法在功能和性能上完全等价，但在可读性、可维护性和团队协作层面，推荐统一采用：

```bash
cat <<EOF > file
...
EOF
```

而不是：

```bash
cat > file <<EOF
...
EOF
```

---

## 原因一：符合"数据流"阅读顺序

Shell 命令的本质是数据流动：

```
      输入
       │
       ▼
   command
       │
       ▼
      输出
```

对于 `cat <<EOF > file`，阅读顺序与数据流方向一致：

```
  Here Document
        │
        ▼
      cat
        │
        ▼
      file
```

这与 Unix Pipeline 的阅读方式完全一致，例如：

```bash
grep foo < input.txt > output.txt
```

也是：

```
  input.txt  →  grep  →  output.txt
```

因此 `command <<INPUT >OUTPUT` 这种写法与 Unix "数据从左向右流动" 的思维一致。

---

## 原因二：Here Document 正文离声明更近

```bash
cat <<EOF > file
line1
line2
line3
EOF
```

阅读时看到 `<<EOF`，立即知道下面就是正文内容，视觉距离最短：

```
cat <<EOF > file
│
├── 从这里开始就是正文
├── line1
├── line2
└── EOF
```

而 `cat > file <<EOF` 的写法需要先越过 `> file`，再看到 `<<EOF`，多一次解析跳转。

---

## 原因三：符合"命令 + 输入 + 输出"的思维模型

可以把命令理解成：

```
command  <<  输入  >  输出
```

即：

```
cat <<EOF > file

对应：

command  ←输入←  ←输出→
```

同时附带多个重定向时也非常自然：

```bash
cat <<EOF > file 2> err.log
...
EOF
```

阅读顺序：

```
stdin → command → stdout → stderr
```

---

## 原因四：大型 Here Document 更易阅读

```bash
ssh server <<'EOF' > result.log
echo hello
hostname
date
EOF
```

阅读顺序清晰：

```
ssh server
    │
    ├── 下面是远程执行的脚本内容
    │
    └── 结果写入 result.log
```

而 `ssh server > result.log <<'EOF'` 则需要在看到脚本内容之前先处理输出目标，打断了阅读节奏。

---

## 原因五：官方及社区主流风格

`<<EOF > file` 的写法可以在以下地方频繁看到：

- Bash 官方文档示例
- Docker 官方脚本
- Kubernetes YAML 生成脚本
- GitHub 上大量 Shell 项目

近年来这种风格越来越普遍，逐渐成为事实上的社区惯例。

---

## 什么时候可以用 `> file <<EOF`？

如果需要强调"覆盖某个重要文件"，有人会选择：

```bash
cat > ~/.ssh/config <<EOF
...
EOF
```

这样 `> ~/.ssh/config` 一眼可见，视觉上更突出写入目标。

这是一种合理的阅读偏好，而非技术要求。

---

## 推荐心智模型

把整个命令理解成一个"数据流管道"：

```
        Here Document
              │
              ▼
      +---------------+
      |    command    |
      +---------------+
         │         │
         │         └────────► stderr（可选）
         ▼
      stdout
         │
         ▼
      输出文件
```

遵循如下经验法则：

```
command <<INPUT > OUTPUT
```

---

## 小结

| 写法 | 推荐度 | 原因 |
|------|--------|------|
| `cmd <<EOF > file` | ✅ 推荐 | 符合数据流方向，正文紧跟声明，阅读自然 |
| `cmd > file <<EOF` | 可用 | 强调输出目标时语义更突出 |

> 两种写法功能完全等价。团队中保持一种统一风格，比纠结选哪种更重要。

---

# 十三、Here Document 的正确心智模型与适用场景

## 核心心智模型

不要把 Here Document（`<<EOF`）看成一个"多行字符串"语法，而应理解为：

> **一种向命令提供标准输入（stdin）的机制。**

建立这个心智模型之后，判断何时使用 Here Document 就有了清晰的依据。

---

## 判断原则

先问自己一个问题：

> **"我要执行的这个命令，本来是不是就是从 stdin 读取数据？"**

```
命令是否从 stdin 读取数据？
          │
    ┌─────┴─────┐
    │           │
   是           否
    │           │
    ▼           ▼
Here Document  通常不适合使用
往往是最佳选择  考虑参数/配置文件/变量
```

---

## 推荐使用场景

### 场景一：生成文本文件（★★★★★）

这是最经典、最常见的用途。`cat` 本来就是 `stdin → stdout`，天然适合。

```bash
# 生成配置文件
cat <<EOF > /etc/myapp.conf
port=8080
host=127.0.0.1
debug=true
EOF

# 生成 YAML
cat <<EOF > deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: demo
EOF

# 生成 JSON
cat <<EOF > config.json
{
    "host": "127.0.0.1",
    "port": 8080
}
EOF

# 生成 SQL
cat <<EOF > schema.sql
CREATE TABLE user (
    id INT,
    name TEXT
);
EOF
```

数据流模型：

```
  Here Document
        │
        ▼
      cat
        │
        ▼
   目标文件
```

---

### 场景二：给交互式程序提供输入（★★★★★）

`sqlite3`、`mysql`、`psql`、`bc`、`python3`、`gdb`、`ftp` 等交互式程序本来就从 stdin 读取并解释执行，Here Document 与其设计完全契合。

```bash
# SQLite
sqlite3 demo.db <<EOF
CREATE TABLE t(id INT);
INSERT INTO t VALUES(1);
SELECT * FROM t;
EOF

# Python
python3 <<EOF
print("hello")
EOF

# bc 计算
bc <<EOF
10*20+30
EOF

# MySQL
mysql <<EOF
SELECT * FROM users;
EOF
```

---

### 场景三：SSH 执行远程脚本（★★★★★）

生产环境最常见用途之一，远比拼接引号字符串更易读。

```bash
ssh server <<EOF
hostname
date
uptime
EOF

# 复杂操作示例
ssh server <<EOF
systemctl stop nginx
cp /tmp/app.conf /etc/nginx/conf.d/
systemctl start nginx
EOF
```

数据流模型：

```
  Here Document
        │
        ▼
      ssh
        │
        ▼
  远程 shell
        │
        ▼
   执行脚本
```

---

### 场景四：一次性执行多条 Shell 命令（★★★★★）

```bash
bash <<EOF
echo hello
pwd
ls
EOF

sh <<EOF
echo "running in subshell"
date
EOF
```

---

### 场景五：构造多行模板（★★★★☆）

利用变量展开自动生成文本。若不希望展开变量，使用 `<<'EOF'`。

```bash
# 展开变量
cat <<EOF
Hello $USER
Today is $(date)
EOF

# 不展开变量（原样输出）
cat <<'EOF'
$HOME
EOF
# 输出：$HOME（不是 /home/xxx）
```

---

### 场景六：发送邮件（★★★★☆）

`mail` 命令本来就从 stdin 读取邮件正文。

```bash
mail -s "Backup Report" admin@example.com <<EOF
Backup completed.
Time: $(date)
EOF
```

---

### 场景七：curl POST 请求体（★★★★☆）

通过 `-d @-` 让 curl 从 stdin 读取请求体。

```bash
curl \
    -H "Content-Type: application/json" \
    -d @- \
    http://server/api \
<<EOF
{
    "name": "demo"
}
EOF
```

---

### 场景八：向管道提供数据（★★★★☆）

```bash
grep hello <<EOF
hello
world
hello linux
EOF

sort <<EOF
3
2
1
EOF

awk '{print $1}' <<EOF
a b
c d
EOF
```

---

## 不推荐使用的场景

### ① 只是定义一个多行字符串

```bash
# 不推荐
VAR=$(cat <<EOF
abc
def
EOF
)

# 推荐
VAR=$'abc\ndef'

# 或者（若确实需要多行变量）
read -r -d '' VAR <<EOF
abc
def
EOF
```

### ② 只是拼接变量

```bash
# 不推荐
cat <<EOF
$a$b$c
EOF

# 推荐
echo "$a$b$c"
```

### ③ 只有一两行输出

```bash
# 不推荐
cat <<EOF
hello
world
EOF

# 推荐
echo hello
echo world
```

### ④ 命令本来不读取 stdin

`mkdir`、`cp`、`mv`、`rm` 等命令依赖命令行参数而非标准输入，对其使用 Here Document 没有意义。

---

## 决策树

```
            是否需要多行文本？
                   │
         ┌─────────┴─────────┐
         │                   │
        否                  是
         │                   │
         ▼                   ▼
   使用 echo/printf    文本是否作为某命令的 stdin？
                               │
                   ┌───────────┴───────────┐
                   │                       │
                  是                      否
                   │                       │
                   ▼                       ▼
           使用 Here Document       使用变量、数组、
             (<<EOF)               独立模板文件或普通字符串
```

---

## 一句话总结

> Here Document 的本质不是"多行文本"，而是**"把一段内嵌文本作为标准输入提供给一个本来就会读取 stdin 的命令"**。当你的数据流模型是 `文本 → 命令(stdin) → 输出` 时，它通常就是最自然、最清晰的选择。