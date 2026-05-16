# termios & stty 详解

## 🎯 学习目标
深入理解termios结构体和stty命令，掌握终端属性控制机制，理解canonical和raw模式的本质区别。

---

## 📊 termios控制架构图

```
用户空间:
┌─────────────┐    stty命令    ┌─────────────┐    tcsetattr()    ┌─────────────┐
│   Shell     │──────────────▶│    stty     │──────────────────▶│   termios   │
│ (用户输入)   │               │   程序       │                   │    结构     │
└─────────────┘               └─────────────┘                   └─────────────┘
                                                                       │
                                                                   ioctl()
                                                                       │
内核空间:                                                               ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                           TTY Subsystem                                    │
│  ┌─────────────┐    ┌─────────────────┐    ┌─────────────────────────────┐│
│  │ tty_struct  │───▶│ ktermios (kernel│───▶│     Line Discipline         ││
│  │   .termios  │    │     copy)       │    │      (N_TTY)                ││
│  └─────────────┘    └─────────────────┘    │ - Input Processing          ││
│                                            │ - Output Processing         ││
│                                            │ - Signal Generation         ││
│                                            └─────────────────────────────┘│
└─────────────────────────────────────────────────────────────────────────────┘

termios 四大标志域:
┌─────────────────────────────────────────────────────────────────────────────┐
│                            termios 结构                                     │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐        │
│  │   c_iflag   │  │   c_oflag   │  │   c_cflag   │  │   c_lflag   │        │
│  │  输入模式    │  │  输出模式    │  │  控制模式    │  │  本地模式    │        │
│  │             │  │             │  │             │  │             │        │
│  │ IGNBRK      │  │ OPOST       │  │ CSIZE       │  │ ICANON      │        │
│  │ BRKINT      │  │ ONLCR       │  │ CSTOPB      │  │ ECHO        │        │
│  │ IGNPAR      │  │ OCRNL       │  │ CREAD       │  │ ECHOE       │        │
│  │ INPCK       │  │ TABDLY      │  │ PARENB      │  │ ISIG        │        │
│  │ ISTRIP      │  │ OXTABS      │  │ PARODD      │  │ NOFLSH      │        │
│  │ INLCR       │  │ OFILL       │  │ HUPCL       │  │ TOSTOP      │        │
│  │ IGNCR       │  │ OFDEL       │  │ CLOCAL      │  │ IEXTEN      │        │
│  │ ICRNL       │  │             │  │ CRTSCTS     │  │             │        │
│  │ IXON        │  │             │  │             │  │             │        │
│  │ IXOFF       │  │             │  │             │  │             │        │
│  │ IXANY       │  │             │  │             │  │             │        │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘        │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                     c_cc[NCCS] 特殊字符数组                          │   │
│  │  VINTR(^C)  VQUIT(^\)  VERASE(BS)  VKILL(^U)  VEOF(^D)             │   │
│  │  VSTART(^Q)  VSTOP(^S)  VSUSP(^Z)  VMIN  VTIME                     │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
```

## 🏗️ 核心数据结构详解

### termios结构体完整定义

```c
// include/uapi/asm-generic/termbits.h
struct termios {
    tcflag_t c_iflag;      // 输入标志
    tcflag_t c_oflag;      // 输出标志  
    tcflag_t c_cflag;      // 控制标志
    tcflag_t c_lflag;      // 本地标志
    cc_t c_line;           // 行规程 (通常不使用)
    cc_t c_cc[NCCS];       // 控制字符数组
    speed_t c_ispeed;      // 输入波特率
    speed_t c_ospeed;      // 输出波特率
};

// 内核版本 (include/linux/tty.h)
struct ktermios {
    tcflag_t c_iflag;      // 输入模式标志
    tcflag_t c_oflag;      // 输出模式标志
    tcflag_t c_cflag;      // 控制模式标志
    tcflag_t c_lflag;      // 本地模式标志
    cc_t c_line;           // 行规程
    cc_t c_cc[NCCS];       // 控制字符
    speed_t c_ispeed;      // 输入速度
    speed_t c_ospeed;      // 输出速度
};
```

### 四大标志域详解

#### c_iflag (输入模式标志)
```c
// 输入处理标志
#define IGNBRK   0000001   // 忽略BREAK条件
#define BRKINT   0000002   // BREAK时发送SIGINT
#define IGNPAR   0000004   // 忽略奇偶校验错误
#define PARMRK   0000010   // 标记奇偶校验错误
#define INPCK    0000020   // 启用输入奇偶校验
#define ISTRIP   0000040   // 去除第8位
#define INLCR    0000100   // NL转换为CR  
#define IGNCR    0000200   // 忽略CR
#define ICRNL    0000400   // CR转换为NL
#define IUCLC    0001000   // 大写转小写
#define IXON     0002000   // 启用输出软件流控制 (^Q/^S)
#define IXANY    0004000   // 任意字符重启输出
#define IXOFF    0010000   // 启用输入软件流控制
#define IMAXBEL  0020000   // 输入队列满时响铃
#define IUTF8    0040000   // 假设输入是UTF-8
```

#### c_oflag (输出模式标志)
```c  
// 输出处理标志
#define OPOST    0000001   // 启用输出处理
#define OLCUC    0000002   // 小写转大写
#define ONLCR    0000004   // NL转换为CR-NL
#define OCRNL    0000010   // CR转换为NL
#define ONOCR    0000020   // 列0时不输出CR
#define ONLRET   0000040   // NL执行CR功能
#define OFILL    0000100   // 使用填充字符延迟
#define OFDEL    0000200   // 填充字符为DEL，否则为NUL
#define TABDLY   0014000   // 制表符延迟掩码
#define TAB0     0000000   // 无制表符延迟
#define TAB1     0004000   // 制表符延迟 
#define TAB2     0010000   // 
#define TAB3     0014000   // 制表符扩展为空格
```

#### c_cflag (控制模式标志)
```c
// 硬件控制标志
#define CBAUD    0010017   // 波特率掩码
#define B0       0000000   // 挂断线路
#define B50      0000001   // 50波特
#define B75      0000002   // 75波特
#define B110     0000003   // 110波特
// ... 更多波特率设置

#define CSIZE    0000060   // 字符大小掩码
#define CS5      0000000   // 5位字符
#define CS6      0000020   // 6位字符
#define CS7      0000040   // 7位字符  
#define CS8      0000060   // 8位字符

#define CSTOPB   0000100   // 设置两个停止位
#define CREAD    0000200   // 启用接收器
#define PARENB   0000400   // 启用奇偶校验
#define PARODD   0001000   // 奇校验，清除为偶校验
#define HUPCL    0002000   // 最后关闭时挂断
#define CLOCAL   0004000   // 忽略调制解调器控制线
#define CRTSCTS  020000000000 // 启用RTS/CTS硬件流控制
```

#### c_lflag (本地模式标志) - 最重要
```c
// 本地处理标志
#define ISIG     0000001   // 识别特殊字符并生成信号
#define ICANON   0000002   // 启用规范模式（行缓冲）
#define ECHO     0000010   // 回显输入字符
#define ECHOE    0000020   // 如果ICANON设置，ERASE擦除前一字符
#define ECHOK    0000040   // 如果ICANON设置，KILL擦除当前行
#define ECHONL   0000100   // 如果ICANON设置，回显NL
#define NOFLSH   0000200   // 接收到INTR/QUIT/SUSP字符时不刷新
#define TOSTOP   0000400   // 后台进程尝试写入终端时发送SIGTTOU
#define IEXTEN   0100000   // 启用扩展处理
```

### 控制字符数组 c_cc[NCCS]

```c
// 控制字符索引定义
#define VINTR    0      // ^C - 中断字符
#define VQUIT    1      // ^\ - 退出字符
#define VERASE   2      // ^? - 擦除字符 (Backspace)
#define VKILL    3      // ^U - 删除行字符
#define VEOF     4      // ^D - 文件结束字符
#define VTIME    5      // 非规范模式下的超时 (决秒)
#define VMIN     6      // 非规范模式下的最小字符数
#define VSWTC    7      // 切换字符 (未使用)
#define VSTART   8      // ^Q - 开始字符 (XON)
#define VSTOP    9      // ^S - 停止字符 (XOFF)  
#define VSUSP    10     // ^Z - 挂起字符
#define VEOL     11     // 额外的行结束字符
#define VREPRINT 12     // ^R - 重打印字符
#define VDISCARD 13     // ^O - 丢弃字符
#define VWERASE  14     // ^W - 单词擦除字符
#define VLNEXT   15     // ^V - 字面意思的下一个字符
#define VEOL2    16     // 第二个额外行结束字符

#define NCCS     19     // 控制字符数组大小
```

## 🔄 canonical vs raw 模式数据流

### Canonical模式 (ICANON=1)

```
Input处理流程:
键盘输入 → TTY Driver → Line Discipline → 行缓冲区
                                   │
                                   ▼ 
                         ┌─────────────────────┐
                         │   特殊字符处理        │
                         │ - ^C → SIGINT       │
                         │ - ^Z → SIGTSTP      │  
                         │ - ^D → EOF          │
                         │ - Backspace → 擦除   │
                         │ - ^U → 删除行        │
                         └─────────────────────┘
                                   │
                                   ▼
                         ┌─────────────────────┐
                         │    Echo处理          │
                         │ 回显到终端显示        │
                         └─────────────────────┘
                                   │
                         等待换行符(\n)
                                   │
                                   ▼
                            read()返回整行

特点:
- 按行缓冲，read()阻塞直到遇到换行符
- 支持行编辑（退格、删除行等）
- 特殊字符会生成信号
- 自动echo
```

### Raw模式 (ICANON=0, ECHO=0, ISIG=0)

```
Input处理流程:
键盘输入 → TTY Driver → Line Discipline → 字符缓冲区
                                   │
                                   ▼
                         ┌─────────────────────┐
                         │   不处理特殊字符      │
                         │ ^C, ^Z 等当普通字符   │
                         │ 不生成信号           │
                         └─────────────────────┘
                                   │
                                   ▼
                         ┌─────────────────────┐
                         │     不Echo          │
                         │  不回显字符          │
                         └─────────────────────┘
                                   │
                                   ▼
                       read()立即返回可用字符
                       (根据VMIN/VTIME控制)

特点:
- 按字符缓冲，每个字符立即可用
- 不处理特殊字符
- 不自动echo
- 应用程序完全控制输入处理
```

## 🔧 关键系统调用和内核函数

### 用户态termios API

```c
#include <termios.h>
#include <unistd.h>

// 获取termios属性
int tcgetattr(int fd, struct termios *termios_p);

// 设置termios属性  
int tcsetattr(int fd, int optional_actions, const struct termios *termios_p);
// optional_actions:
// TCSANOW    - 立即改变
// TCSADRAIN  - 等待所有输出完成后改变
// TCSAFLUSH  - 等待所有输出完成，丢弃未读输入后改变

// 发送break
int tcsendbreak(int fd, int duration);

// 等待输出完成  
int tcdrain(int fd);

// 刷新I/O
int tcflush(int fd, int queue_selector);
// TCIFLUSH  - 刷新输入
// TCOFLUSH  - 刷新输出  
// TCIOFLUSH - 刷新输入和输出

// 流控制
int tcflow(int fd, int action);
// TCOOFF - 停止输出
// TCOON  - 重新开始输出
// TCIOFF - 发送STOP字符  
// TCION  - 发送START字符

// 设置波特率
int cfsetispeed(struct termios *termios_p, speed_t speed);
int cfsetospeed(struct termios *termios_p, speed_t speed);
```

### 内核实现函数

```c
// drivers/tty/tty_ioctl.c
long tty_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct tty_struct *tty = file_tty(file);
    
    switch (cmd) {
    case TCGETS:    // 获取termios
        return get_termios(tty, (struct termios __user *)arg);
    case TCSETS:    // 设置termios (立即)
        return set_termios(tty, (struct termios __user *)arg, TERMIOS_WAIT);
    case TCSETSW:   // 设置termios (等待输出完成)  
        return set_termios(tty, (struct termios __user *)arg, TERMIOS_WAIT | TERMIOS_FLUSH);
    case TCSETSF:   // 设置termios (等待+刷新)
        return set_termios(tty, (struct termios __user *)arg, TERMIOS_FLUSH);
    // ...
    }
}

// 设置termios的核心函数
static int set_termios(struct tty_struct *tty, void __user *arg, int opt)
{
    struct ktermios tmp_termios;
    
    // 从用户空间拷贝termios
    if (copy_from_user(&tmp_termios, arg, sizeof(tmp_termios)))
        return -EFAULT;
        
    // 等待输出完成
    if (opt & TERMIOS_WAIT) {
        if (wait_event_interruptible(tty->write_wait, 
                                     !tty_chars_in_buffer(tty)))
            return -ERESTARTSYS;
    }
    
    // 刷新输入
    if (opt & TERMIOS_FLUSH) {
        n_tty_flush_buffer(tty);
        tty_driver_flush_buffer(tty);
    }
    
    // 设置新的termios
    tty_set_termios(tty, &tmp_termios);
    
    return 0;
}
```

## 🧪 最小可运行实验

### 实验1：观察默认termios设置

```bash
# 查看当前termios完整设置
stty -a

# 典型输出：
# speed 38400 baud; rows 24; columns 80; line = 0;
# intr = ^C; quit = ^\; erase = ^?; kill = ^U; eof = ^D; eol = <undef>;
# eol2 = <undef>; swtch = <undef>; start = ^Q; stop = ^S; susp = ^Z;
# rprnt = ^R; werase = ^W; lnext = ^V; discard = ^O; min = 1; time = 0;
# -parenb -parodd -cmspar cs8 -hupcl -cstopb cread -clocal -crtscts
# -ignbrk -brkint -ignpar -parmrk -inpck -istrip -inlcr -igncr icrnl ixon
# -ixoff -iuclc -ixany -imaxbel -iutf8
# opost -olcuc -ocrnl onlcr -onocr -onlret -ofill -ofdel nl0 cr0 tab0 bs0
# vt0 ff0
# isig icanon iexten echo echoe echok -echonl -noflsh -xcase -tostop
# -echoprt echoctl echoke -flusho -extproc

# 保存当前设置  
stty -g > /tmp/termios_backup
```

### 实验2：canonical vs raw模式对比

```c
// termios_test.c - 对比canonical和raw模式
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>

void test_canonical_mode() {
    printf("=== Canonical模式测试 ===\n");
    printf("输入一些字符，按Enter结束: ");
    fflush(stdout);
    
    char buffer[100];
    if (fgets(buffer, sizeof(buffer), stdin)) {
        printf("读取到: %s", buffer);
    }
}

void test_raw_mode() {
    struct termios old_termios, raw_termios;
    
    // 获取当前termios设置
    if (tcgetattr(STDIN_FILENO, &old_termios) == -1) {
        perror("tcgetattr");
        return;
    }
    
    // 配置raw模式
    raw_termios = old_termios;
    raw_termios.c_lflag &= ~(ICANON | ECHO | ISIG);  // 关闭canonical, echo, signal
    raw_termios.c_cc[VMIN] = 1;   // 至少读取1个字符
    raw_termios.c_cc[VTIME] = 0;  // 无超时
    
    // 应用raw模式设置
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_termios) == -1) {
        perror("tcsetattr");
        return;
    }
    
    printf("=== Raw模式测试 ===\n");
    printf("输入字符(按'q'退出，字符不会回显): ");
    fflush(stdout);
    
    char c;
    while (read(STDIN_FILENO, &c, 1) == 1) {
        if (c == 'q') {
            break;
        }
        
        // 手动回显并显示字符信息
        printf("\n收到字符: '%c' (ASCII: %d, 0x%02x)\n", 
               isprint(c) ? c : '?', c, (unsigned char)c);
        
        if (c == '\003') {  // Ctrl+C
            printf("检测到Ctrl+C，但不会发送信号\n");
        }
        
        printf("继续输入(按'q'退出): ");
        fflush(stdout);
    }
    
    // 恢复原始termios设置
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_termios);
    printf("\n已恢复canonical模式\n");
}

int main() {
    test_canonical_mode();
    printf("\n");
    test_raw_mode();
    
    return 0;
}
```

编译运行：
```bash
gcc -o termios_test termios_test.c
./termios_test
```

### 实验3：特殊字符处理验证

```bash
# 保存当前设置
old_settings=$(stty -g)

# 测试1：关闭信号处理
echo "测试1：关闭信号处理 (ISIG=0)"
stty -isig
echo "现在按Ctrl+C不会中断程序"
sleep 5  # 按Ctrl+C试试
echo "sleep命令完成"

# 恢复设置  
stty $old_settings

# 测试2：关闭echo
echo -e "\n测试2：关闭echo"
stty -echo
echo "输入密码(不会显示): "
read password
echo "你输入的是: $password"

# 恢复设置
stty $old_settings

# 测试3：修改特殊字符
echo -e "\n测试3：修改中断字符为Ctrl+X"
stty intr '^X'
echo "现在按Ctrl+X来中断(Ctrl+C无效)"
sleep 10

# 恢复设置
stty $old_settings
```

### 实验4：VMIN/VTIME非规范模式控制

```c
// vmin_vtime_test.c - 测试非规范模式的VMIN/VTIME
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <sys/time.h>

void test_vmin_vtime(int vmin, int vtime) {
    struct termios old_termios, new_termios;
    
    printf("=== 测试 VMIN=%d, VTIME=%d ===\n", vmin, vtime);
    
    tcgetattr(STDIN_FILENO, &old_termios);
    new_termios = old_termios;
    
    // 设置非规范模式
    new_termios.c_lflag &= ~(ICANON | ECHO);
    new_termios.c_cc[VMIN] = vmin;
    new_termios.c_cc[VTIME] = vtime;
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_termios);
    
    char buffer[10];
    struct timeval start, end;
    
    printf("开始读取... ");
    fflush(stdout);
    
    gettimeofday(&start, NULL);
    ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer)-1);
    gettimeofday(&end, NULL);
    
    if (n > 0) {
        buffer[n] = '\0';
        printf("读取了 %zd 个字符\n", n);
        
        long elapsed_ms = (end.tv_sec - start.tv_sec) * 1000 + 
                         (end.tv_usec - start.tv_usec) / 1000;
        printf("耗时: %ld ms\n", elapsed_ms);
    } else {
        printf("读取超时或出错\n");
    }
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_termios);
    printf("\n");
}

int main() {
    printf("VMIN/VTIME 控制测试\n\n");
    
    // 测试1：VMIN=1, VTIME=0 (阻塞读取，至少1个字符)
    test_vmin_vtime(1, 0);
    
    // 测试2：VMIN=3, VTIME=0 (阻塞读取，至少3个字符)
    test_vmin_vtime(3, 0);
    
    // 测试3：VMIN=0, VTIME=50 (5秒超时)
    test_vmin_vtime(0, 50);
    
    // 测试4：VMIN=2, VTIME=30 (至少2个字符，或3秒超时)
    test_vmin_vtime(2, 30);
    
    return 0;
}
```

### 实验5：stty命令源码分析

```bash
# 查看stty的实际系统调用
strace -e trace=ioctl stty -a 2>&1 | grep TCGETS

# 修改设置时的系统调用
strace -e trace=ioctl stty -echo 2>&1 | grep TCSETS

# 使用ltrace查看库函数调用  
ltrace -e tcgetattr,tcsetattr stty -echo
```

## 🚨 常见坑 & Debug方法

### 1. termios设置丢失问题

**问题**: 程序崩溃后终端状态异常
**解决**:
```bash
# 应急恢复方法
reset
# 或
stty sane
# 或使用备份
stty $(cat /tmp/termios_backup)

# 编程中的防护措施:
void setup_signal_handler() {
    struct sigaction sa;
    sa.sa_handler = restore_termios_and_exit;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

void restore_termios_and_exit(int sig) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
    exit(1);
}
```

### 2. VMIN/VTIME理解错误

**问题**: 非规范模式下读取行为不符合预期

**Debug代码**:
```c
void debug_termios(int fd) {
    struct termios t;
    tcgetattr(fd, &t);
    
    printf("=== Termios Debug Info ===\n");
    printf("ICANON: %s\n", (t.c_lflag & ICANON) ? "ON" : "OFF");
    printf("ECHO: %s\n", (t.c_lflag & ECHO) ? "ON" : "OFF");
    printf("ISIG: %s\n", (t.c_lflag & ISIG) ? "ON" : "OFF");
    printf("VMIN: %d\n", t.c_cc[VMIN]);
    printf("VTIME: %d\n", t.c_cc[VTIME]);
}
```

### 3. 控制字符修改无效

**问题**: 修改了c_cc数组但特殊字符处理没有改变

**Debug**:
```bash
# 检查ISIG是否开启
stty -a | grep -E "(isig|-isig)"

# 检查具体字符设置
stty -a | grep intr
stty intr '^X'  # 修改中断字符
stty -a | grep intr  # 验证修改
```

### 4. 输出格式问题

**问题**: 输出格式混乱（换行、回车问题）

**分析**:
```bash
# 检查输出处理标志
stty -a | grep -E "(opost|onlcr|ocrnl)"

# ONLCR - NL转换为CR-NL
# OPOST - 启用输出处理

# 测试不同设置
stty -opost  # 关闭输出处理
printf "line1\nline2\n"  # 观察输出
stty opost   # 恢复输出处理
```

### 5. 使用strace调试termios

```bash
# 跟踪termios相关系统调用
strace -e trace=ioctl -s 1000 stty -echo

# 输出类似:
# ioctl(0, TCGETS, {B38400 opost isig icanon echo ...}) = 0  
# ioctl(0, TCSETS, {B38400 opost isig icanon -echo ...}) = 0

# 跟踪程序的termios操作
strace -e trace=ioctl ./termios_test
```

### 6. 内核调试termios

```bash
# 查看当前TTY的termios设置 (需要root)
cat /proc/$$/fd/0 | od -c  # 这不会直接显示termios

# 更好的方法：编写内核模块或使用现有工具
# 查看内核TTY结构 (需要调试内核)
echo 'file drivers/tty/n_tty.c +p' > /sys/kernel/debug/dynamic_debug/control
```

## 📋 高级应用场景

### 1. 密码输入实现

```c
// 安全的密码输入函数
char* getpass_secure(const char *prompt) {
    struct termios old, new;
    static char password[256];
    
    printf("%s", prompt);
    fflush(stdout);
    
    // 关闭echo
    tcgetattr(STDIN_FILENO, &old);
    new = old;
    new.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &new);
    
    // 读取密码
    if (fgets(password, sizeof(password), stdin)) {
        // 移除换行符
        password[strcspn(password, "\n")] = 0;
    }
    
    // 恢复echo
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &old);
    printf("\n");
    
    return password;
}
```

### 2. 终端游戏输入处理

```c
// 游戏按键处理（无需按Enter）
int get_game_key() {
    struct termios old, new;
    int key;
    
    // 设置原始模式
    tcgetattr(STDIN_FILENO, &old);
    new = old;
    new.c_lflag &= ~(ICANON | ECHO);
    new.c_cc[VMIN] = 1;
    new.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &new);
    
    key = getchar();
    
    // 恢复原设置
    tcsetattr(STDIN_FILENO, TCSANOW, &old);
    
    return key;
}
```

### 3. 进度条实现

```c
// 利用termios控制实现动态进度条
void show_progress(int percent) {
    // 关闭行缓冲，立即输出
    struct termios old, new;
    tcgetattr(STDOUT_FILENO, &old);
    new = old;
    new.c_lflag &= ~ICANON;  // 关闭行缓冲
    tcsetattr(STDOUT_FILENO, TCSANOW, &new);
    
    printf("\r[");
    int filled = percent / 2;  // 50字符宽度
    for (int i = 0; i < 50; i++) {
        printf(i < filled ? "=" : " ");
    }
    printf("] %d%%", percent);
    fflush(stdout);
    
    // 恢复设置
    tcsetattr(STDOUT_FILENO, TCSANOW, &old);
}
```

## 🎯 学习检查点

完成本模块后，你应该能够：

1. ✅ 理解termios四大标志域的作用和含义
2. ✅ 区分canonical和raw模式的本质差异
3. ✅ 掌握VMIN/VTIME在非规范模式下的控制机制
4. ✅ 能够编写控制终端属性的C程序
5. ✅ 理解stty命令的实现原理
6. ✅ 会调试和恢复异常的终端状态
7. ✅ 能够实现密码输入、游戏输入等特殊需求

---

**下一步**: 深入学习 [Line Discipline (N_TTY) 详解](04-line-discipline.md)