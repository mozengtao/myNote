# Linux Process Substitution：>(cmd) 与 <(cmd) 的底层实现及心智模型

很多人已经理解了：

```bash
cmd1 | cmd2
```

这是：

```text
stdout → stdin
```

的数据流连接。

但是看到：

```bash
>(cmd)
<(cmd)
```

往往会觉得神秘。

实际上：

```text
>(cmd)
<(cmd)
```

本质上是：

```text
Shell把一个进程伪装成一个文件
```

(Process Substitution)

---

# 一、首先理解它解决什么问题

Pipeline只能连接：

```text
stdout
   │
   ▼
stdin
```

即：

```text
Producer → Consumer
```

模型。

例如：

```bash
cat file | grep abc
```

---

但很多Linux程序并不接受stdin。

例如：

```bash
diff file1 file2
```

要求：

```text
两个文件名
```

而不是：

```text
stdin
```

---

问题：

```bash
diff \
    <(cmd1) \
    <(cmd2)
```

为什么可以？

因为：

```text
Shell把cmd1输出伪装成一个文件

Shell把cmd2输出伪装成一个文件
```

然后：

```text
diff 看到的仍然是文件路径
```

---

# 二、最重要的心智模型

## 普通文件

```text
Program

open("/tmp/file")

      │
      ▼

Kernel

      │
      ▼

File
```

---

## Process Substitution

```text
Program

open("/dev/fd/63")

      │
      ▼

Kernel

      │
      ▼

Pipe

      │
      ▼

Process
```

即：

```text
文件名是假象

背后其实是Pipe
```

---

# 三、<(cmd) 的心智模型

例如：

```bash
diff <(ls dir1) <(ls dir2)
```

---

脑中应该自动转换成：

```text
          Pipe1
ls dir1 ───────►

          Pipe2
ls dir2 ───────►

          │
          ▼

       diff
```

---

Shell实际看到：

```bash
diff /dev/fd/63 /dev/fd/64
```

---

diff以为：

```text
自己在读文件
```

实际上：

```text
读的是Pipe
```

---

# 四、<(cmd) 底层实现

## Step1

Shell创建Pipe

```c
pipe(pipefd);
```

得到：

```text
pipefd[0]
pipefd[1]
```

---

## Step2

fork

```c
fork()
```

生成：

```text
child
```

执行：

```bash
cmd
```

---

## Step3

重定向stdout

```c
dup2(pipefd[1], STDOUT_FILENO);
```

变成：

```text
cmd stdout
      │
      ▼
Pipe Write End
```

---

## Step4

Shell保留读端

```text
pipefd[0]
```

---

然后构造：

```text
/dev/fd/63
```

例如：

```text
63 -> pipefd[0]
```

---

最终执行：

```bash
diff /dev/fd/63 ...
```

---

ASCII图：

```text
          child

           cmd
            │
            ▼

         stdout

            │
            ▼

       Pipe Write

──────────────────────────

       Pipe Read

            │
            ▼

        /dev/fd/63

            │
            ▼

          diff
```

---

# 五、真实展开过程

用户输入：

```bash
diff <(date) <(uptime)
```

---

Shell内部：

```text
pipe1
pipe2
```

---

启动：

```bash
date
```

输出：

```text
Thu ...
```

写入：

```text
pipe1
```

---

启动：

```bash
uptime
```

输出：

```text
15:30 ...
```

写入：

```text
pipe2
```

---

Shell最终执行：

```bash
diff \
    /dev/fd/63 \
    /dev/fd/64
```

---

diff读取：

```text
fd63
fd64
```

实际上：

```text
pipe1
pipe2
```

---

# 六、>(cmd) 的心智模型

这是反方向。

例如：

```bash
echo hello > >(cat)
```

---

脑中应该理解成：

```text
echo

stdout
   │
   ▼

Pipe

   │
   ▼

cat
```

即：

```text
文件位置被一个进程接管
```

---

# 七、>(cmd) 底层实现

例如：

```bash
echo hello > >(cat)
```

---

Shell：

```c
pipe(pipefd);
```

---

启动：

```bash
cat
```

---

cat：

```text
stdin ← pipe read end
```

即：

```c
dup2(pipefd[0], STDIN_FILENO);
```

---

Shell返回：

```text
/dev/fd/63
```

对应：

```text
pipe write end
```

---

最终变成：

```bash
echo hello > /dev/fd/63
```

---

而：

```text
fd63
 ↓
pipe write end
```

---

形成：

```text
echo

stdout
   │
   ▼

pipe write

   │
   ▼

pipe read

   │
   ▼

cat stdin
```

---

# 八、为什么叫 Process Substitution

因为：

```text
文件位置
```

被：

```text
进程
```

替换了。

---

普通情况：

```bash
diff file1 file2
```

```text
file1
file2
```

是真文件。

---

Process Substitution：

```bash
diff <(cmd1) <(cmd2)
```

```text
file1位置
↓
cmd1

file2位置
↓
cmd2
```

---

因此：

```text
Process Substitution
```

即：

```text
用进程替代文件
```

---

# 九、典型应用场景

# 场景1：比较两个命令输出

最经典。

```bash
diff \
    <(ip route) \
    <(ssh host ip route)
```

---

数据流：

```text
ip route
     │
     ▼
 Pipe1

ssh host
     │
     ▼
 Pipe2

     │
     ▼

    diff
```

---

# 场景2：比较配置文件生成结果

```bash
diff \
    <(sort old.txt) \
    <(sort new.txt)
```

无需临时文件。

---

传统写法：

```bash
sort old.txt > /tmp/a
sort new.txt > /tmp/b

diff /tmp/a /tmp/b
```

---

Process Substitution：

```bash
diff \
    <(sort old.txt) \
    <(sort new.txt)
```

---

# 场景3：让只能读文件的程序读命令输出

例如：

```bash
vim <(git show HEAD~1:file.txt)
```

---

vim以为：

```text
打开文件
```

实际上：

```text
打开pipe
```

---

# 场景4：tee到多个消费者

非常常见。

```bash
command | tee >(grep ERROR) >(wc -l)
```

---

心智模型：

```text
                tee

                  │
         ┌────────┴────────┐
         ▼                 ▼

   grep ERROR          wc -l
```

---

# 场景5：日志分流

```bash
make 2> >(tee error.log)
```

---

数据流：

```text
stderr

   │
   ▼

Pipe

   │
   ▼

tee

   ├─ terminal
   └─ error.log
```

---

# 十、FIFO实现模式

某些Shell没有：

```text
/dev/fd
```

支持。

会退化成：

```c
mkfifo()
```

---

例如：

```text
/tmp/sh-np-12345
```

---

结构：

```text
cmd
 │
 ▼

FIFO

 │
 ▼

consumer
```

---

所以：

```text
<(cmd)
```

可能是：

```text
Pipe
```

也可能是：

```text
FIFO
```

取决于Shell实现。

---

# 十一、与普通Pipeline的区别

## Pipeline

```bash
cmd1 | cmd2
```

模型：

```text
stdout
   │
   ▼
stdin
```

只能连接：

```text
Producer → Consumer
```

---

## <(cmd)

模型：

```text
Producer
   │
   ▼

伪装成文件
   │
   ▼

Program
```

---

## >(cmd)

模型：

```text
Program
   │
   ▼

伪装成文件
   │
   ▼

Consumer
```

---

# 最终心智模型

## <(cmd)

```text
                cmd

                 │
                 ▼

             stdout

                 │
                 ▼

               Pipe

                 │
                 ▼

            /dev/fd/N

                 │
                 ▼

              Program
```

记忆一句话：

```text
<(cmd)

把命令输出伪装成文件供别人读取。
```

---

## >(cmd)

```text
Program

    │
    ▼

/dev/fd/N

    │
    ▼

 Pipe

    │
    ▼

 cmd
```

记忆一句话：

```text
>(cmd)

把文件输出位置替换成一个进程进行消费。
```

---

## 三者统一模型

```text
Pipeline

cmd1 | cmd2

cmd1.stdout ─────► cmd2.stdin


Input Process Substitution

<(cmd)

cmd.stdout ─────► fake file ─────► program


Output Process Substitution

>(cmd)

program ─────► fake file ─────► cmd.stdin
```

本质上：

```text
|      = stdin/stdout连接

<(cmd) = Producer伪装成文件

>(cmd) = Consumer伪装成文件
```

Shell 通过：

pipe()
fork()
dup2()
execve()

以及：

/dev/fd/*

或

mkfifo()

把「进程」伪装成「文件」，从而突破 Pipeline 只能连接 stdin/stdout 的限制。