# Shell 函数执行与后台执行的心智模型

---

# 一、先建立正确认知

很多人刚学习 Shell 时容易产生两个误解：

误解1：

```text
函数(Function)
=
一个独立进程
```

误解2：

```text
foo &
=
后台执行函数
```

实际上：

```text
函数不是进程

函数只是Shell中的一段代码
```

更准确地说：

```text
Shell Script
    │
    ├── Variables
    ├── Functions
    ├── Builtins
    └── External Commands
```

函数只是 Shell 解释器管理的代码块。

例如：

```bash
foo() {
    echo hello
}
```

本质上类似：

```c
void foo()
{
    printf("hello\n");
}
```

而不是：

```c
fork();
exec();
```

---

# 二、Shell 函数的本质

例如：

```bash
#!/bin/bash

foo() {
    echo "inside foo"
}

foo
```

执行过程：

```text
bash
 │
 │ 读取脚本
 │
 ├── 注册函数 foo()
 │
 └── 调用 foo()
        │
        ▼
    在当前Shell解释执行函数体
```

ASCII 图：

```text
+----------------+
| Bash Process   |
+----------------+
         │
         ▼
    foo()
         │
         ▼
echo "inside foo"
```

没有创建任何新进程。

---

# 三、验证函数不会创建新进程

脚本：

```bash
#!/bin/bash

foo() {
    echo "foo pid=$BASHPID"
}

echo "shell pid=$BASHPID"

foo
```

输出：

```text
shell pid=1000
foo pid=1000
```

说明：

```text
函数运行在当前Shell
```

而不是：

```text
Shell
    │
    └── Child Process
            foo
```

---

# 四、函数为什么能够修改当前Shell环境

例如：

```bash
foo() {
    cd /tmp
}

pwd

foo

pwd
```

输出：

```text
/home/user
/tmp
```

说明：

```text
foo()
直接运行在当前Shell
```

否则：

```text
Parent Shell
      │
      └── Child Process
              cd /tmp
```

子进程退出后：

```text
父进程目录不会变化
```

显然与实际不符。

---

# 五、函数修改变量为什么有效

示例：

```bash
counter=0

foo() {
    counter=100
}

foo

echo "$counter"
```

输出：

```text
100
```

执行过程：

```text
Current Shell

counter=0

    │
    ▼

foo()

    │
    ▼

counter=100
```

因为：

```text
函数和变量属于同一个Shell进程
```

---

# 六、函数内部执行外部命令时发生了什么

例如：

```bash
foo() {
    ls /tmp
}
```

执行：

```bash
foo
```

过程：

```text
Shell
 │
 ▼
foo()
 │
 ▼
ls
 │
 ▼
fork()
 │
 ▼
exec(ls)
```

ASCII 图：

```text
Bash
 │
 ▼
foo()
 │
 ▼
+--------+
| fork() |
+--------+
     │
     ▼
+--------+
| exec() |
+--------+
     │
     ▼
   ls
```

注意：

```text
函数没有fork

ls产生了新进程
```

---

# 七、函数后台执行的本质

很多脚本：

```bash
foo &
```

看起来像：

```text
后台执行函数
```

实际上更接近：

```text
后台启动一个Shell

由这个Shell执行函数
```

---

## 示例

```bash
foo() {
    echo "pid=$BASHPID"
}

echo "parent=$BASHPID"

foo &
wait
```

输出：

```text
parent=1000
pid=1001
```

执行过程：

```text
Parent Shell
      │
      │ fork
      ▼
Child Shell
      │
      ▼
    foo()
```

ASCII 图：

```text
+------------------+
| Parent Bash 1000 |
+------------------+
          │
          │ fork()
          ▼
+------------------+
| Child Bash 1001  |
+------------------+
          │
          ▼
        foo()
```

---

# 八、为什么后台函数修改变量无效

示例：

```bash
counter=0

foo() {
    counter=100
}

foo &

wait

echo "$counter"
```

输出：

```text
0
```

---

执行过程

父进程：

```text
counter=0
```

fork：

```text
Parent
counter=0

Child
counter=0
```

执行函数：

```text
Child
counter=100
```

退出：

```text
Child Exit
```

最终：

```text
Parent
counter=0
```

变量没有同步回来。

---

# 九、后台函数与Linux fork模型

可以把：

```bash
foo &
```

理解成：

```c
pid = fork();

if (pid == 0)
{
    foo();
    exit(0);
}
```

因此：

```text
&
导致fork

不是函数导致fork
```

这是最重要的理解。

---

# 十、多个后台函数

例如：

```bash
worker() {
    sleep 10
}

worker &
worker &
worker &

wait
```

执行结构：

```text
Parent Shell
      │
      ├── Child1
      │      worker
      │
      ├── Child2
      │      worker
      │
      └── Child3
             worker
```

ASCII 图：

```text
                Parent Shell
                       │
      ┌────────────────┼────────────────┐
      │                │                │
      ▼                ▼                ▼
 Child Shell1    Child Shell2    Child Shell3
      │                │                │
      ▼                ▼                ▼
   worker()         worker()         worker()
```

---

# 十一、工程实践：并发SSH

例如：

```bash
worker() {
    local host=$1

    ssh "$host" hostname
}

for host in host1 host2 host3
do
    worker "$host" &
done

wait
```

执行结构：

```text
Main Shell
      │
      ├── worker(host1)
      │       │
      │       └── ssh host1
      │
      ├── worker(host2)
      │       │
      │       └── ssh host2
      │
      └── worker(host3)
              │
              └── ssh host3
```

进一步展开：

```text
Main Bash
     │
     ├── Bash Child
     │      worker(host1)
     │            │
     │            ▼
     │         ssh process
     │
     ├── Bash Child
     │      worker(host2)
     │            │
     │            ▼
     │         ssh process
     │
     └── Bash Child
            worker(host3)
                  │
                  ▼
               ssh process
```

---

# 十二、wait 的作用

示例：

```bash
worker() {
    sleep 5
}

worker &
worker &
worker &

wait

echo done
```

执行：

```text
Parent Shell
      │
      ├── Child1
      ├── Child2
      └── Child3

           wait
             │
             ▼
等待所有后台任务结束
```

ASCII 图：

```text
worker1 ------
              \
worker2 --------> wait ---> done
              /
worker3 ------
```

---

# 十三、后台函数与Pipeline的关系

Pipeline：

```bash
foo | cat
```

通常：

```text
Parent Shell
      │
      ├── Child Shell
      │       foo
      │
      └── Child Shell
              cat
```

---

后台：

```bash
foo &
```

通常：

```text
Parent Shell
      │
      └── Child Shell
              foo
```

因此：

```text
Pipeline
Subshell
Background Job

本质都属于：

Shell创建新的执行环境
```

---

# 十四、Shell执行环境总表

| 场景 | 是否新Shell | 是否能修改父Shell变量 |
|--------|--------|--------|
| foo | 否 | 可以 |
| source file.sh | 否 | 可以 |
| . file.sh | 否 | 可以 |
| foo & | 是 | 不可以 |
| ( foo ) | 是 | 不可以 |
| foo \| cat | 通常是 | 不可以 |
| x=$(foo) | 是 | 不可以 |
| bash script.sh | 是 | 不可以 |

---

# 十五、最终心智模型

```text
函数(Function)
=
Shell中的代码块
=
不是进程
```

执行：

```bash
foo
```

等价于：

```text
当前Shell
解释执行函数体
```

结构：

```text
Shell
   │
   ▼
 foo()
```

---

执行：

```bash
foo &
```

等价于：

```text
fork一个新的Shell

新的Shell执行函数体
```

结构：

```text
Parent Shell
      │
      ▼
fork()
      │
      ▼
Child Shell
      │
      ▼
foo()
```

---

牢记一句话：

```text
Shell函数本身永远只是代码。

真正决定是否创建新进程的，
不是函数(Function)，
而是函数所处的执行环境：

    foo
    foo &
    (foo)
    foo | cat
    $(foo)

这些上下文决定了
Bash是否需要fork新的Shell进程来执行函数。
```