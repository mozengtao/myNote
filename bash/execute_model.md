# Linux / Bash 执行模型心智模式
> **一句话总结：**
>
> **执行环境（Execution Context）决定是否 `fork()`；命令类型（Command Type）决定是否 `exec()`。**
>
> 这是理解 Bash 执行模型最重要的原则。

---

# 一、整体架构

Bash 执行一条命令时，可以拆分为两个独立的问题：

```
             用户输入一条命令
                    │
                    ▼
        +-----------------------+
        | 1. 执行环境(Context)   |
        +-----------------------+
                    │
        是否需要创建新的Shell进程？
                    │
          Yes               No
           │                 │
           ▼                 ▼
       fork()           当前Shell继续执行
                    │
                    ▼
        +-----------------------+
        | 2. 命令解析(Resolution) |
        +-----------------------+
                    │
    Alias → Function → Builtin → External Command
                    │
                    ▼
           External Command ?
                 │
           Yes          No
            │            │
            ▼            ▼
         execve()    当前Bash解释执行
```

因此：

```
Execution Context
        │
        ├── 决定是否 fork()
        │
        ▼
Command Resolution
        │
        ├── 决定是否 execve()
```

---

# 二、fork() 与 execve() 是两个独立概念

很多初学者容易误认为：

```
fork
=
exec
```

实际上：

```
fork()
```

作用：

```
创建新的进程
```

```
execve()
```

作用：

```
用新的程序替换当前进程
```

二者没有必然关系。

ASCII 图：

```
fork()

Parent
   │
   ├─────────────► Parent
   │
   └─────────────► Child
```

```
execve()

Current Process (bash)
          │
          ▼
Current Process (grep)

PID 不变
程序变了
```

---

# 三、执行环境（Execution Context）

执行环境决定：

```
是否需要fork一个新的Shell
```

常见执行环境：

```
当前Shell
后台任务 (&)
Pipeline (|)
Subshell ( )
Command Substitution ($())
新启动bash
```

---

## 1. 当前Shell

例如：

```bash
foo
```

执行：

```
Current Bash
      │
      ▼
foo()
```

没有：

```
fork
exec
```

---

## 2. 后台执行

```bash
foo &
```

执行：

```
Parent Bash
      │
      ▼
fork()
      │
      ▼
Child Bash
      │
      ▼
foo()
```

特点：

```
fork
有

exec
没有
```

---

## 3. Subshell

```bash
(
    foo
)
```

执行：

```
Parent Bash
      │
      ▼
fork()
      │
      ▼
Child Bash
      │
      ▼
foo()
```

---

## 4. Pipeline

```bash
foo | cat
```

通常：

```
Parent Bash
      │
      ├──────────────┐
      ▼              ▼
 Child Bash      Child Process
     │                │
     ▼                ▼
   foo()             cat
```

注意：

```
foo()
虽然是函数

但是
运行在fork出来的Shell
```

---

## 5. Command Substitution

```bash
x=$(foo)
```

执行：

```
Parent Bash
      │
      ▼
fork()
      │
      ▼
Child Bash
      │
      ▼
foo()
      │
stdout
      │
      ▼
Parent Bash
```

---

# 四、命令解析（Command Resolution）

当确定执行环境之后，

Bash 开始解析命令。

顺序固定：

```
Alias
   │
   ▼
Function
   │
   ▼
Builtin
   │
   ▼
PATH Search
   │
   ▼
External Command
```

ASCII：

```
           command
               │
               ▼
      +----------------+
      | Alias ?        |
      +----------------+
               │
         No    ▼ Yes
               Execute
               │
               ▼
      +----------------+
      | Function ?     |
      +----------------+
               │
         No    ▼ Yes
               Execute
               │
               ▼
      +----------------+
      | Builtin ?      |
      +----------------+
               │
         No    ▼ Yes
               Execute
               │
               ▼
      +----------------+
      | PATH Search    |
      +----------------+
               │
               ▼
           execve()
```

---

# 五、Function、Builtin、External Command 的区别

## Function

例如：

```bash
foo() {
    echo hello
}

foo
```

执行：

```
Current Bash
      │
      ▼
foo()
```

特点：

```
没有fork
没有exec
```

---

## Builtin

例如：

```bash
cd /tmp
```

执行：

```
Current Bash
      │
      ▼
builtin cd
```

特点：

```
没有fork
没有exec
```

Builtin 必须运行在当前Shell。

例如：

```
cd
export
read
wait
jobs
alias
```

否则：

```
无法修改当前Shell环境
```

---

## External Command

例如：

```bash
grep root /etc/passwd
```

执行：

```
Bash
 │
 ▼
fork()
 │
 ▼
execve("/usr/bin/grep")
```

特点：

```
通常：

fork
+
exec
```

---

# 六、真正决定fork的是执行环境

例如：

```bash
foo
```

```
Current Shell
      │
      ▼
foo()
```

---

```bash
foo &
```

```
Parent Shell
      │
fork()
      │
      ▼
Child Shell
      │
      ▼
foo()
```

---

```bash
(foo)
```

```
Parent Shell
      │
fork()
      │
      ▼
Child Shell
      │
      ▼
foo()
```

---

```bash
$(foo)
```

```
Parent Shell
      │
fork()
      │
      ▼
Child Shell
      │
      ▼
foo()
```

因此：

```
函数并不是不会fork。

函数只是不会主动fork。

真正fork的是：

执行环境(Context)
```

---

# 七、真正决定exec的是命令类型

例如：

```
foo
```

```
Bash解释执行
```

没有：

```
exec
```

---

```
cd
```

```
Builtin
```

没有：

```
exec
```

---

```
grep
```

```
External Command
```

必须：

```
execve()
```

加载：

```
/usr/bin/grep
```

---

# 八、四种典型组合

## 1. 不fork，不exec

例如：

```bash
foo
cd
export
read
wait
```

执行：

```
Current Bash
      │
      ▼
Builtin / Function
```

---

## 2. fork，不exec

例如：

```bash
foo &
(foo)
$(foo)
foo | cat
```

执行：

```
Parent Bash
      │
fork()
      │
      ▼
Child Bash
      │
      ▼
Function/Builtin
```

---

## 3. fork + exec

例如：

```bash
grep file
ssh host
awk
sed
python
```

执行：

```
Parent Bash
      │
fork()
      │
      ▼
Child Process
      │
execve()
      │
      ▼
grep
```

这是最常见的模式。

---

## 4. 不fork，只exec

例如：

```bash
exec grep file
```

执行：

```
bash(PID=1000)
      │
execve()
      ▼
grep(PID=1000)
```

特点：

```
PID保持不变

bash
变成
grep
```

---

# 九、工程实践示例

例如：

```bash
worker() {
    ssh "$1" hostname
}

for host in host1 host2 host3
do
    worker "$host" &
done

wait
```

执行过程：

```
Parent Bash
      │
      ├──────────────┐
      ▼              ▼
 Child Bash1    Child Bash2 ...
      │              │
      ▼              ▼
 worker()       worker()
      │              │
      ▼              ▼
fork+exec       fork+exec
      │              │
      ▼              ▼
    ssh            ssh
```

可以看到有两层：

```
第一层：

Bash
fork多个子Shell

第二层：

每个子Shell
再exec ssh
```

---

# 十、最终心智模型

牢记下面这张图：

```
                 Bash
                  │
                  ▼
        ┌─────────────────┐
        │ Execution Context│
        └─────────────────┘
                  │
          是否需要fork？
                  │
        ┌─────────┴─────────┐
        │                   │
      当前Shell         Child Shell
        │                   │
        └─────────┬─────────┘
                  ▼
        ┌─────────────────┐
        │ Command Lookup  │
        └─────────────────┘
                  │
Alias → Function → Builtin → External
                                   │
                                   ▼
                              execve()
```

可以浓缩成三句话：

```
① 执行环境（Execution Context）
   决定是否 fork()。

② 命令类型（Command Type）
   决定是否 execve()。

③ fork() 与 execve() 相互独立，
   它们分别解决：
   "在哪里执行？" 和 "执行什么程序？"
```

最终记忆口诀：

```
Context 决定 fork
Command 决定 exec

fork 创建执行环境
exec 装载程序镜像
```