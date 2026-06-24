# Linux Pipeline 底层实现：系统调用、Pipe 对象与数据流动心智模型

以最经典的 Pipeline 为例：

```bash
cmd1 | cmd2 | cmd3
```

从 Linux Kernel 角度看，本质上是：

```text
Process
    │
    ▼
 write()

    │
    ▼

 Pipe Buffer

    │
    ▼

 read()

    │
    ▼

Process
```

核心思想：

```text
Pipeline = 多个进程 + Pipe对象 + FD重定向
```

Shell 负责：

```text
搭建拓扑结构
```

Kernel 负责：

```text
数据搬运
进程同步
阻塞唤醒
```

---

# 一、整体心智模型

## 用户视角

```bash
cmd1 | cmd2 | cmd3
```

---

## Shell视角

```text
1. 创建 Pipe1
2. 创建 Pipe2

3. fork cmd1
4. fork cmd2
5. fork cmd3

6. dup2重定向

7. exec各程序
```

---

## Kernel视角

```text
                  Kernel视角：cmd1 | cmd2 | cmd3

    +-----------+              +-----------+
    |  Pipe1    |              |  Pipe2    |
    +-----------+              +-----------+
    | Buffer    |              | Buffer    |
    | [Data]    |              | [Data]    |
    +-----------+              +-----------+
         ^  |                       ^  |
    write|  |read              write|  |read
         |  v                       |  v
    +--------+                 +--------+                 +--------+
    | cmd1   |                 | cmd2   |                 | cmd3   |
    |--------|                 |--------|                 |--------|
    |stdout--+                 |stdin <-+                 |stdin <-+
    |        |                 |stdout--+                 |stdout  |
    +--------+                 +--------+                 +--------+

数据流向：
cmd1 --write()--> Pipe1.buffer --read()--> cmd2 --write()--> Pipe2.buffer --read()--> cmd3

内核对象：
Pipe1 = pipe_inode_info { ring_buffer, wait_queue, readers=1, writers=1 }
Pipe2 = pipe_inode_info { ring_buffer, wait_queue, readers=1, writers=1 }
```

### 关键说明：

1. **管道创建过程**：
   - Shell调用 `pipe()` 系统调用创建两个管道对象
   - 每个管道包含一个 `pipe_inode_info` 结构和内核缓冲区
   - 返回读写文件描述符对：`[read_fd, write_fd]`

2. **进程间连接**：
   - cmd1的stdout重定向到Pipe1的写端
   - cmd2的stdin重定向到Pipe1的读端，stdout重定向到Pipe2的写端  
   - cmd3的stdin重定向到Pipe2的读端

3. **数据流动机制**：
   - `write()` 系统调用将数据从用户空间复制到内核管道缓冲区
   - `read()` 系统调用将数据从内核管道缓冲区复制到用户空间
   - 当缓冲区满时，写进程阻塞；当缓冲区空时，读进程阻塞

4. **同步与阻塞**：
   - 使用等待队列（wait_queue）实现进程同步
   - 写者在缓冲区满时休眠，读者消费后唤醒写者
   - 读者在缓冲区空时休眠，写者写入后唤醒读者

5. **生命周期管理**：
   - 写进程退出时关闭写端，读进程收到EOF（read返回0）
   - 读进程退出时关闭读端，写进程收到SIGPIPE信号

---

# 二、最核心的系统调用

Pipeline 本质上只依赖几个核心系统调用：

```text
pipe()
fork()
dup2()
close()
execve()
read()
write()
waitpid()
```

---

# 三、Pipe 创建过程

## Step1

Shell调用：

```c
int pipefd[2];

pipe(pipefd);
```

---

Kernel内部：

```text
sys_pipe()
    │
    ▼

do_pipe2()
    │
    ▼

create pipe inode
    │
    ▼

allocate pipe buffer
    │
    ▼

allocate fd
```

---

创建结果：

```text
Pipe Object

          Pipe
    ┌─────────────┐
    │             │
    │   Buffer    │
    │             │
    └─────────────┘
       ▲      ▲
       │      │

     fd0    fd1
    read   write
```

---

返回：

```c
pipefd[0]
pipefd[1]
```

对应：

```text
pipefd[0]
    ↓
Read End

pipefd[1]
    ↓
Write End
```

---

# 四、Shell 创建 cmd1 | cmd2

例如：

```bash
cmd1 | cmd2
```

---

## 创建Pipe

```c
pipe(pipefd);
```

结果：

```text
              Kernel

       +----------------+
       | Pipe Buffer    |
       +----------------+
          ▲         ▲
          │         │

       readfd    writefd
```

---

# 五、fork产生两个进程

Shell执行：

```c
fork();
```

---

形成：

```text
                Shell
                  │
        ┌─────────┴─────────┐
        │                   │

      Child1             Child2
```

---

fork之后：

```text
所有FD都会被复制
```

即：

```text
Shell FD Table

 3 -> pipe read
 4 -> pipe write
```

变成：

```text
Child1 FD Table

 3 -> pipe read
 4 -> pipe write


Child2 FD Table

 3 -> pipe read
 4 -> pipe write
```

注意：

```text
复制的是FD

不是复制Pipe Buffer
```

所有FD最终指向：

```text
同一个Pipe对象
```

---

# 六、dup2完成重定向

## cmd1

目标：

```text
stdout → pipe write end
```

执行：

```c
dup2(pipefd[1], STDOUT_FILENO);
```

即：

```text
dup2(4,1)
```

结果：

```text
cmd1

FD1(stdout)
      │
      ▼

pipe write end
```

---

## cmd2

目标：

```text
stdin ← pipe read end
```

执行：

```c
dup2(pipefd[0], STDIN_FILENO);
```

即：

```text
dup2(3,0)
```

结果：

```text
pipe read end
      │
      ▼

FD0(stdin)
```

---

最终：

```text
cmd1 stdout
      │
      ▼

 Pipe Buffer

      │
      ▼

cmd2 stdin
```

---

# 七、execve替换程序

此时进程还只是Shell子进程。

执行：

```c
execve(...)
```

---

cmd1子进程：

```text
bash child
      │
      ▼
 execve()

      │
      ▼

 cmd1
```

---

cmd2子进程：

```text
bash child
      │
      ▼
 execve()

      │
      ▼

 cmd2
```

---

最终：

```text
Shell
 │
 ├── cmd1
 │
 └── cmd2
```

---

# 八、数据真正如何流动

假设：

```bash
echo hello | cat
```

---

## cmd1

执行：

```c
write(1, "hello\n", 6);
```

由于：

```text
FD1
 ↓
Pipe Write End
```

所以实际变成：

```text
write()
    │
    ▼

Pipe Buffer
```

---

Kernel：

```text
copy_from_user()
```

把用户空间数据复制到：

```text
Pipe Buffer
```

---

结果：

```text
 Pipe Buffer

+------------------+
| hello\n          |
+------------------+
```

---

# 九、cmd2如何读取

cmd2执行：

```c
read(0, buf, 4096);
```

由于：

```text
FD0
 ↓
Pipe Read End
```

所以：

```text
read()

   │
   ▼

Pipe Buffer
```

---

Kernel：

```text
copy_to_user()
```

把数据拷贝回：

```text
cmd2用户空间
```

---

形成：

```text
cmd1

 write()
    │
    ▼

─────────────
 Kernel
─────────────

 Pipe Buffer

─────────────
 Kernel
─────────────

    │
    ▼

 read()

cmd2
```

---

# 十、Pipe Buffer结构

内核对象：

```text
struct pipe_inode_info
```

可理解为：

```text
pipe_inode_info

 ├─ ring buffer
 ├─ head
 ├─ tail
 ├─ readers
 ├─ writers
 ├─ wait queue
 └─ lock
```

---

心智模型：

```text
          head
            ▼

+---+---+---+---+---+---+
| D | A | T | A |   |   |
+---+---+---+---+---+---+

            ▲
           tail
```

本质：

```text
环形缓冲区(Ring Buffer)
```

---

# 十一、阻塞机制

## Reader先启动

```text
cmd2
  │
  ▼

read()
```

Pipe为空：

```text
Pipe Buffer

EMPTY
```

---

Kernel：

```text
sleep current task
```

进入：

```text
TASK_INTERRUPTIBLE
```

状态。

---

## Writer写入

```text
cmd1

write()
```

---

Kernel：

```text
copy data
```

然后：

```text
wake_up()
```

唤醒Reader。

---

过程：

```text
Reader
   │
   ▼

 Waiting Queue

   ▲
   │

 wake_up()

   │
   ▼

Writer
```

---

# 十二、Pipe满时发生什么

Pipe默认容量：

```text
64KB左右
(不同内核可能不同)
```

---

假设：

```text
cmd1很快
cmd2很慢
```

---

结果：

```text
cmd1

write()
  │
  ▼

+------------------+
| FULL FULL FULL   |
+------------------+

Pipe Buffer

  │
  ▼

cmd2
(慢)
```

---

Kernel行为：

```text
writer sleep
```

进入：

```text
TASK_INTERRUPTIBLE
```

等待Reader消费。

---

形成：

```text
Producer
     │
     ▼

 Buffer Full

     │
     ▼

 Producer Blocked
```

这就是：

```text
Back Pressure
背压机制
```

---

# 十三、EOF如何产生

最容易忽略的部分。

---

例如：

```bash
echo hello | cat
```

echo退出：

```text
关闭write end
```

即：

```c
close(pipefd[1]);
```

---

Kernel发现：

```text
writers == 0
```

即：

```text
没有写者了
```

---

此时：

```text
Pipe Empty
+
No Writers
```

---

read()返回：

```c
0
```

即：

```text
EOF
```

---

ASCII图：

```text
writer exit
    │
    ▼

close(write end)

    │
    ▼

writers = 0

    │
    ▼

reader read()

    │
    ▼

return 0
```

---

# 十四、SIGPIPE产生过程

例如：

```bash
yes | head -n 5
```

---

head先退出：

```text
close(read end)
```

---

此时：

```text
readers = 0
```

---

yes继续：

```c
write(...)
```

---

Kernel检查：

```text
reader count == 0
```

于是：

```text
send SIGPIPE
```

---

流程：

```text
yes

 write()

    │
    ▼

 Pipe

    │
    ▼

 no readers

    │
    ▼

 SIGPIPE

    │
    ▼

 process exit
```

---

# 十五、完整底层时序图

```text
Shell

 │
 │ pipe()
 ▼

Pipe1

 │
 │ fork()
 ▼

Child1
Child2

 │
 │ dup2()
 ▼

stdout -> Pipe1 Write
stdin  <- Pipe1 Read

 │
 │ execve()
 ▼

cmd1
cmd2

────────────────────────

cmd1

write()

 │
 ▼

Kernel

 │
 ▼

Pipe Buffer

 │
 ▼

Kernel

 │
 ▼

read()

cmd2

────────────────────────

cmd1 exit

 │
 ▼

close(write end)

 │
 ▼

writers=0

 │
 ▼

cmd2 read()

 │
 ▼

EOF

 │
 ▼

cmd2 exit
```

# 最终心智模型

```text
                Shell Layer

                  pipe()
                  fork()
                  dup2()
                  execve()

                     │
                     ▼

                Process Layer

           cmd1      cmd2      cmd3

                     │
                     ▼

                FD Layer

      stdout ─► Pipe ─► stdin

                     │
                     ▼

                Kernel Layer

        pipe_inode_info
             │
             ▼
         Ring Buffer
             │
             ▼
      wait queue + wakeup

                     │
                     ▼

                Syscalls

        write()
        read()
        close()
        poll()
        epoll()
```

一句话概括：

```text
Pipeline 的本质：

Shell 利用 pipe()+fork()+dup2()+execve()
搭建一个数据流图(Data Flow Graph)；

Kernel 利用 pipe_inode_info + Ring Buffer
实现进程间数据传输；

read()/write() 驱动数据流动；
wait queue + wake_up() 实现同步；
EOF/SIGPIPE 实现生命周期管理。
```