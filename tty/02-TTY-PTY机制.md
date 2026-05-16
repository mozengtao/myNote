# TTY/PTY 机制详解

## 🎯 学习目标
深入理解TTY（物理终端）和PTY（伪终端）的本质区别，掌握PTY的创建和使用流程。

---

## 📊 TTY vs PTY 架构对比图

```
物理终端 (TTY):
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│  Keyboard   │───▶│ TTY Driver  │───▶│    Shell    │
│   Screen    │◀───│ (/dev/tty1) │◀───│   Process   │
└─────────────┘    └─────────────┘    └─────────────┘
                         │
                    Hardware IRQ
                         │
                   ┌─────────────┐
                   │   Kernel    │
                   │ TTY Subsys  │
                   └─────────────┘

伪终端 (PTY):
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   Terminal  │    │ PTY Master  │    │ PTY Slave   │    │    Shell    │
│  Emulator   │◀──▶│(/dev/ptmx)  │◀──▶│(/dev/pts/0) │◀──▶│   Process   │
│  (xterm)    │    │             │    │             │    │   (bash)    │
└─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘
                         │                   │
                    User Space          Kernel Space
                         │                   │
                   ┌─────────────────────────────────┐
                   │        Kernel PTY Driver        │
                   │      (drivers/tty/pty.c)        │
                   └─────────────────────────────────┘

SSH场景中的PTY:
Local Machine:                    Remote Machine:
┌─────────────┐    Network    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   Terminal  │──────────────▶│    SSHD     │───▶│ PTY Master  │───▶│ PTY Slave   │
│   (local)   │◀──────────────│             │◀───│             │◀───│    bash     │
└─────────────┘               └─────────────┘    └─────────────┘    └─────────────┘
```

## 🏗️ 核心数据结构

### PTY相关内核结构

```c
// PTY特有结构 (drivers/tty/pty.c)
struct pty_struct {
    struct tty_struct *tty;        // 关联的tty
    spinlock_t lock;               // 同步锁
    struct pty_struct *link;       // 指向配对的另一端
};

// PTY驱动结构
static struct tty_driver *ptm_driver;     // PTY Master驱动
static struct tty_driver *pts_driver;     // PTY Slave驱动

// PTY操作函数集
static const struct tty_operations ptm_unix98_ops = {
    .lookup = ptm_unix98_lookup,
    .install = pty_unix98_install,
    .remove = pty_unix98_remove,
    .open = pty_open,
    .close = pty_close,
    .write = pty_write,
    .write_room = pty_write_room,
    .flush_buffer = pty_flush_buffer,
    .chars_in_buffer = pty_chars_in_buffer,
    .unthrottle = pty_unthrottle,
    .ioctl = pty_unix98_ioctl,
    .compat_ioctl = pty_unix98_compat_ioctl,
    .resize = pty_resize,
    .cleanup = pty_cleanup
};
```

### 设备节点映射

```c
// 设备号定义 (include/uapi/linux/major.h)  
#define TTYAUX_MAJOR    5          // /dev/tty, /dev/console等
#define TTY_MAJOR       4          // /dev/tty0, /dev/tty1等 
#define UNIX98_PTY_MASTER_MAJOR 128 // PTY master设备
#define UNIX98_PTY_SLAVE_MAJOR  136 // PTY slave设备

// 设备节点关系:
// /dev/ptmx        -> (5, 2)  主设备号5，次设备号2
// /dev/pts/0       -> (136, 0) 主设备号136，次设备号0
// /dev/pts/1       -> (136, 1) 主设备号136，次设备号1
// /dev/tty0        -> (4, 0)   主设备号4，次设备号0
```

## 🔄 PTY创建和数据流

### PTY创建流程详解

```c
// PTY创建完整流程:

1. open("/dev/ptmx", O_RDWR)
   ↓
2. ptmx_open() -> pty_unix98_install()
   - 分配master和slave的tty_struct
   - 创建PTY pair链接关系
   - 分配pts设备号
   ↓  
3. grantpt(master_fd)  
   - 设置slave设备权限
   - chown /dev/pts/N to current user
   ↓
4. unlockpt(master_fd)
   - 清除LOCK标志，允许slave被打开
   ↓
5. ptsname(master_fd)  
   - 返回 "/dev/pts/N" 路径
   ↓
6. fork() + open("/dev/pts/N") 
   - 子进程打开slave端
```

### PTY数据传输机制

```
Master Write -> Slave Read:
┌─────────────┐    write()    ┌─────────────┐    pty_write()    ┌─────────────┐
│   Master    │──────────────▶│    Kernel   │──────────────────▶│    Slave    │
│   Process   │               │ PTY Driver  │                   │ tty buffer  │
└─────────────┘               └─────────────┘                   └─────────────┘
                                     │                                │
                              tty_flip_buffer_push()                read()
                                     │                                │
                                     ▼                                ▼
                               ┌─────────────────────────────────────────────┐
                               │        n_tty_receive_buf()                  │
                               │     (Line Discipline处理)                   │
                               └─────────────────────────────────────────────┘

Slave Write -> Master Read:
┌─────────────┐    write()    ┌─────────────┐    pty_write()    ┌─────────────┐
│    Slave    │──────────────▶│   Kernel    │──────────────────▶│   Master    │  
│   Process   │               │ PTY Driver  │                   │ tty buffer  │
└─────────────┘               └─────────────┘                   └─────────────┘
```

## 🔧 关键系统调用和内核函数

### 用户态PTY API

```c
#include <stdlib.h>
#include <fcntl.h> 
#include <unistd.h>
#include <sys/ioctl.h>

// 标准POSIX PTY接口
int posix_openpt(int flags);               // 打开 /dev/ptmx
int grantpt(int fd);                       // 修改slave权限  
int unlockpt(int fd);                      // 解锁slave
char *ptsname(int fd);                     // 获取slave路径

// 非标准但常用的接口
int openpty(int *amaster, int *aslave,     // 一次性创建PTY对
           char *name, struct termios *termp,
           struct winsize *winp);

int forkpty(int *amaster, char *name,      // fork + PTY创建
           struct termios *termp, 
           struct winsize *winp);

// 底层ioctl操作
#define TIOCSPTLCK  _IOW('T', 0x31, int)   // 锁定/解锁slave
#define TIOCGPTN    _IOR('T', 0x30, unsigned int) // 获取pts编号
```

### 内核PTY实现函数

```c  
// PTY Master操作 (drivers/tty/pty.c)
static int ptmx_open(struct inode *inode, struct file *filp)
{
    struct tty_struct *tty;
    struct inode *slave_inode;
    int retval;
    
    // 分配tty结构
    tty = tty_init_dev(ptm_driver, 0);
    if (IS_ERR(tty))
        return PTR_ERR(tty);
        
    // 设置文件操作
    filp->private_data = tty;
    
    return 0;
}

// PTY写操作
static int pty_write(struct tty_struct *tty, const unsigned char *buf, int c)
{
    struct tty_struct *to = tty->link;  // 获取配对端
    
    if (!to || tty_throttled(to))
        return 0;
        
    // 将数据推送到配对端的接收缓冲区
    return tty_insert_flip_string(to->port, buf, c);
}

// PTY安装
static int pty_unix98_install(struct tty_driver *driver, struct tty_struct *tty)
{
    return pty_common_install(driver, tty, false);
}
```

## 🧪 最小可运行实验

### 实验1：手动创建PTY对

```c
// create_pty.c - 完整PTY创建示例
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

int main() {
    int master_fd, slave_fd;
    char *slave_name;
    pid_t pid;
    char buffer[256];
    
    // 1. 打开PTY master
    master_fd = posix_openpt(O_RDWR);
    if (master_fd == -1) {
        perror("posix_openpt");
        exit(1);
    }
    
    // 2. 设置slave权限
    if (grantpt(master_fd) == -1) {
        perror("grantpt");
        exit(1);
    }
    
    // 3. 解锁slave
    if (unlockpt(master_fd) == -1) {
        perror("unlockpt");
        exit(1);
    }
    
    // 4. 获取slave设备路径
    slave_name = ptsname(master_fd);
    if (!slave_name) {
        perror("ptsname");
        exit(1);
    }
    
    printf("PTY created:\n");
    printf("  Master fd: %d\n", master_fd);
    printf("  Slave device: %s\n", slave_name);
    
    // 5. Fork子进程使用slave
    pid = fork();
    if (pid == 0) {
        // 子进程：打开slave端
        slave_fd = open(slave_name, O_RDWR);
        if (slave_fd == -1) {
            perror("open slave");
            exit(1);
        }
        
        // 从slave读取数据
        printf("[Child] Reading from slave...\n");
        ssize_t n = read(slave_fd, buffer, sizeof(buffer) - 1);
        if (n > 0) {
            buffer[n] = '\0';
            printf("[Child] Received: %s", buffer);
        }
        
        // 向slave写入数据
        write(slave_fd, "Hello from slave\n", 17);
        close(slave_fd);
        exit(0);
    } else {
        // 父进程：使用master端
        sleep(1);  // 等待子进程准备
        
        // 向master写入数据（会被slave读到）
        printf("[Parent] Writing to master...\n");
        write(master_fd, "Hello from master\n", 18);
        
        sleep(1);  // 等待子进程写入
        
        // 从master读取数据（来自slave）  
        ssize_t n = read(master_fd, buffer, sizeof(buffer) - 1);
        if (n > 0) {
            buffer[n] = '\0';
            printf("[Parent] Received: %s", buffer);
        }
        
        wait(NULL);  // 等待子进程结束
        close(master_fd);
    }
    
    return 0;
}
```

编译运行：
```bash
gcc -o create_pty create_pty.c
./create_pty
```

### 实验2：观察PTY设备

```bash
# 运行前查看pts设备
echo "Before:"
ls -l /dev/pts/

# 打开新终端
gnome-terminal &

# 再次查看pts设备
echo "After:"
ls -l /dev/pts/

# 查看PTY统计
cat /proc/tty/driver/pty_slave | head -5
# pty_slave            /dev/pts      136   0-1048575 system:/dev/ptmx
# 0: uart:16550A port:00000000 irq:0 tx:1234 rx:5678 RTS|CTS|DTR

# 查看当前shell使用的PTY
tty
readlink /proc/$$/fd/0  # stdin
readlink /proc/$$/fd/1  # stdout  
readlink /proc/$$/fd/2  # stderr
```

### 实验3：SSH中的PTY机制

```bash
# 在本地执行（需要SSH服务器）
ssh localhost 'tty; ps -p $$ -o pid,ppid,pgid,sid,tty,cmd'

# 对比本地直接执行
tty; ps -p $$ -o pid,ppid,pgid,sid,tty,cmd

# 查看SSH进程的文件描述符
ps aux | grep sshd
# 假设找到sshd进程PID为1234
ls -l /proc/1234/fd/
```

### 实验4：Docker中的PTY

```bash
# 不分配PTY运行容器
docker run --rm -it alpine sh -c 'tty'
# not a tty

# 分配PTY运行容器  
docker run --rm -it alpine sh -c 'tty; ps -o pid,ppid,pgid,sid,tty,cmd'
# /dev/pts/0 或类似输出

# 检查Docker的PTY分配
docker run --rm -it alpine sh -c 'ls -l /proc/self/fd/'
```

## 🚨 常见坑 & Debug方法

### 1. PTY权限问题

**问题**: 无法打开slave设备，Permission denied
```bash
# 检查pts设备权限
ls -l /dev/pts/
# 应该看到类似：
# crw--w---- 1 user tty 136, 0 May 16 15:30 0

# 检查用户是否在tty组
groups $USER | grep tty

# 调试权限设置
strace -e trace=openat,chmod,chown ./create_pty
```

### 2. Master/Slave混淆

**问题**: 分不清哪端是master哪端是slave
```c
// Debug技巧：检查设备类型
#include <sys/stat.h>

void check_device_type(const char *path) {
    struct stat st;
    if (stat(path, &st) == -1) {
        perror("stat");
        return;  
    }
    
    printf("Device: %s\n", path);
    printf("  Major: %d, Minor: %d\n", major(st.st_rdev), minor(st.st_rdev));
    
    // PTY master: major = 5 (对于/dev/ptmx)
    // PTY slave:  major = 136
    if (major(st.st_rdev) == 5) {
        printf("  Type: PTY Master\n");
    } else if (major(st.st_rdev) == 136) {
        printf("  Type: PTY Slave\n");  
    }
}
```

### 3. 数据流方向理解错误

**问题**: 不清楚数据如何在master/slave之间流动

**Debug工具**：
```bash
# 使用socat创建PTY并观察数据流
socat -d -d pty,raw,echo=0 pty,raw,echo=0

# 在另一个终端中观察
echo "test" > /dev/pts/1  # 写入slave
cat /dev/pts/2            # 从master读取
```

### 4. 使用strace分析PTY操作

```bash
# 跟踪PTY相关系统调用
strace -e trace=openat,read,write,ioctl -o pty.trace ./create_pty

# 查看ioctl调用详情
strace -e trace=ioctl -s 100 -v ./create_pty

# 重点关注的ioctl命令:
# TIOCGPTN  - 获取pts号码
# TIOCSPTLCK - 锁定/解锁操作
```

### 5. 内核调试信息

```bash  
# 查看TTY子系统状态
cat /proc/tty/drivers
# pty_slave            /dev/pts      136   0-1048575 system:/dev/ptmx
# pty_master           /dev/ptmx     5   2 system
# serial               /dev/ttyS     4  64-95 serial

# 查看具体PTY使用情况
cat /proc/tty/driver/pty_slave
cat /proc/tty/driver/pty_master

# 开启内核TTY调试（需要root）
echo 'file drivers/tty/pty.c +p' > /sys/kernel/debug/dynamic_debug/control
```

## 📋 实际应用场景

### SSH服务器PTY使用

```c
// SSH服务器中PTY创建的简化流程
void ssh_create_pty(struct ssh_session *session) {
    int master_fd, slave_fd;
    char *slave_name;
    pid_t pid;
    
    // 创建PTY
    master_fd = posix_openpt(O_RDWR);
    grantpt(master_fd);
    unlockpt(master_fd);
    slave_name = ptsname(master_fd);
    
    // Fork shell进程
    pid = fork();
    if (pid == 0) {
        // 子进程：成为session leader
        setsid();
        
        // 打开slave作为控制终端
        slave_fd = open(slave_name, O_RDWR);
        
        // 重定向标准输入输出
        dup2(slave_fd, 0);
        dup2(slave_fd, 1);
        dup2(slave_fd, 2);
        
        // 执行shell
        execl("/bin/bash", "bash", NULL);
    } else {
        // 父进程：处理网络数据和master_fd之间的转发
        session->pty_master = master_fd;
        handle_pty_data(session);
    }
}
```

### Terminal Emulator PTY使用

```c
// 终端模拟器中PTY使用
struct terminal_window {
    int master_fd;
    pid_t shell_pid;
    // GUI相关字段...
};

struct terminal_window *create_terminal() {
    struct terminal_window *term = malloc(sizeof(*term));
    
    // 创建PTY
    term->master_fd = posix_openpt(O_RDWR);
    grantpt(term->master_fd);
    unlockpt(term->master_fd);
    
    char *slave_name = ptsname(term->master_fd);
    
    // Fork shell
    term->shell_pid = fork();
    if (term->shell_pid == 0) {
        int slave_fd = open(slave_name, O_RDWR);
        
        // 设置为控制终端
        setsid();
        ioctl(slave_fd, TIOCSCTTY, 1);
        
        // 重定向I/O
        dup2(slave_fd, 0);
        dup2(slave_fd, 1);
        dup2(slave_fd, 2);
        
        execl("/bin/bash", "bash", NULL);
    }
    
    return term;
}
```

## 🎯 学习检查点

完成本模块后，你应该能够：

1. ✅ 明确区分TTY和PTY的使用场景
2. ✅ 完整理解PTY创建的五步流程  
3. ✅ 理解PTY master/slave数据流向
4. ✅ 知道SSH/Docker/Terminal如何使用PTY
5. ✅ 能够编写创建和使用PTY的C程序
6. ✅ 会使用strace调试PTY相关问题

---

**下一步**: 学习 [termios & stty 详解](03-termios-stty.md)