# read()/write() 与阻塞机制详解

## 🎯 学习目标
深入理解TTY的I/O操作机制，掌握canonical/raw模式下的读写行为，理解VMIN/VTIME参数和阻塞/非阻塞I/O的实现原理。

---

## 📊 TTY I/O 操作架构图

```
TTY I/O 完整架构:
┌─────────────────────────────────────────────────────────────────────────────┐
│                             User Space                                     │
│                                                                             │
│  ┌─────────────┐  read()   ┌─────────────┐  write()  ┌─────────────────┐   │
│  │ Application │─────────▶│    libc     │─────────▶│   Application   │   │
│  │  Process    │◀─────────│   Buffer    │◀─────────│    Process      │   │
│  └─────────────┘          └─────────────┘          └─────────────────┘   │
│        │                         │                         │               │
│        │ syscall                 │ syscall                 │ syscall       │
│        ▼                         ▼                         ▼               │
└─────────────────────────────────────────────────────────────────────────────┘
                   │                         │                         │
                   ▼                         ▼                         ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                             Kernel Space                                   │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                        VFS Layer                                    │   │
│  │  sys_read()                                    sys_write()          │   │
│  │       │                                             │                │   │
│  │       ▼                                             ▼                │   │
│  │  vfs_read()                                    vfs_write()           │   │
│  │       │                                             │                │   │
│  │       ▼                                             ▼                │   │
│  │  file->f_op->read()                           file->f_op->write()    │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                   │                                             │           │
│                   ▼                                             ▼           │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                        TTY Core                                     │   │
│  │                                                                     │   │
│  │  tty_read()                                       tty_write()       │   │
│  │       │                                                 │           │   │
│  │       ▼                                                 ▼           │   │
│  │  ┌─────────────────┐                         ┌─────────────────┐   │   │
│  │  │ 作业控制检查     │                         │ 作业控制检查     │   │   │
│  │  │ job_control()   │                         │ job_control()   │   │   │
│  │  │                │                         │                │   │   │
│  │  │ • 检查TOSTOP   │                         │ • 检查TOSTOP   │   │   │
│  │  │ • 后台进程写   │                         │ • 后台进程写   │   │   │
│  │  │   发送SIGTTOU  │                         │   发送SIGTTOU  │   │   │
│  │  └─────────────────┘                         └─────────────────┘   │   │
│  │       │                                                 │           │   │
│  │       ▼                                                 ▼           │   │
│  │  tty->ldisc->ops->read()                   tty->ldisc->ops->write() │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                   │                                             │           │
│                   ▼                                             ▼           │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                   Line Discipline (N_TTY)                          │   │
│  │                                                                     │   │
│  │  ┌─────────────────┐                         ┌─────────────────┐   │   │
│  │  │   n_tty_read()  │                         │  n_tty_write()  │   │   │
│  │  │                │                         │                │   │   │
│  │  │ Canonical模式:  │                         │ 输出处理:       │   │   │
│  │  │ • 等待换行符    │                         │ • OPOST处理     │   │   │
│  │  │ • 行编辑支持    │                         │ • 格式转换      │   │   │
│  │  │                │                         │ • 流控制       │   │   │
│  │  │ Raw模式:       │                         │               │   │   │
│  │  │ • VMIN/VTIME   │                         │               │   │   │
│  │  │ • 字符模式     │                         │               │   │   │
│  │  └─────────────────┘                         └─────────────────┘   │   │
│  │           │                                           │             │   │
│  │           ▼                                           ▼             │   │
│  │  ┌─────────────────────────────────────────────────────────────┐   │   │
│  │  │                    缓冲区管理                                │   │   │
│  │  │                                                             │   │   │
│  │  │  读缓冲区:                    写缓冲区:                      │   │   │
│  │  │  ┌─────────────────┐          ┌─────────────────────────┐   │   │   │
│  │  │  │  Input Buffer   │          │    Output Buffer        │   │   │   │
│  │  │  │                │          │                        │   │   │   │
│  │  │  │ • 环形缓冲区    │          │ • 写入队列             │   │   │   │
│  │  │  │ • read_head     │          │ • 流控制              │   │   │   │
│  │  │  │ • read_tail     │          │ • 阻塞等待            │   │   │   │
│  │  │  │ • canon_head    │          │                        │   │   │   │
│  │  │  │ • 等待队列      │          │                        │   │   │   │
│  │  │  └─────────────────┘          └─────────────────────────┘   │   │   │
│  │  └─────────────────────────────────────────────────────────────┘   │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                   │                                             │           │
│                   ▼                                             ▼           │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                    Hardware Driver                                  │   │
│  │                                                                     │   │
│  │  tty->ops->write()                          (读取来自硬件/PTY)      │   │
│  │       │                                             ▲               │   │
│  │       ▼                                             │               │   │
│  │  console_write()                           tty_insert_flip_char()   │   │
│  │  serial_write()                                     │               │   │
│  │  pty_write()                              tty_flip_buffer_push()    │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘

阻塞/非阻塞决策流程:
┌─────────────────────────────────────────────────────────────────────────────┐
│                          I/O 阻塞决策                                       │
│                                                                             │
│  ┌─────────────┐     O_NONBLOCK     ┌─────────────────────────────────────┐  │
│  │  用户调用   │─────set?──────────▶│         立即返回                     │  │
│  │  read()     │                    │       (EAGAIN/EWOULDBLOCK)          │  │
│  └─────────────┘                    └─────────────────────────────────────┘  │
│         │                                                                   │
│         │ O_NONBLOCK not set                                                │
│         ▼                                                                   │
│  ┌─────────────┐                                                           │
│  │ 检查数据可用 │                                                           │
│  │   情况      │                                                           │
│  └─────────────┘                                                           │
│         │                                                                   │
│         ▼                                                                   │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                      Canonical Mode                                │   │
│  │                                                                     │   │
│  │  有完整行可用? ────yes───▶ 立即返回行数据                            │   │
│  │         │                                                           │   │
│  │         no                                                          │   │
│  │         ▼                                                           │   │
│  │  add_wait_queue(&tty->read_wait)                                    │   │
│  │  等待换行符或EOF                                                     │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                       Raw Mode                                     │   │
│  │                                                                     │   │
│  │  ┌─────────────────┐                                               │   │
│  │  │ VMIN=0, VTIME=0 │ ──▶ 立即返回可用字符 (可能为0)                 │   │
│  │  └─────────────────┘                                               │   │
│  │                                                                     │   │
│  │  ┌─────────────────┐                                               │   │
│  │  │ VMIN>0, VTIME=0 │ ──▶ 等待至少VMIN个字符                        │   │
│  │  └─────────────────┘                                               │   │
│  │                                                                     │   │
│  │  ┌─────────────────┐                                               │   │
│  │  │ VMIN=0, VTIME>0 │ ──▶ 等待VTIME*0.1秒或有字符                   │   │
│  │  └─────────────────┘                                               │   │
│  │                                                                     │   │
│  │  ┌─────────────────┐                                               │   │
│  │  │ VMIN>0, VTIME>0 │ ──▶ 等待VMIN个字符，字符间超时VTIME*0.1秒      │   │
│  │  └─────────────────┘                                               │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
```

## 🏗️ 核心数据结构和算法

### N_TTY读取缓冲区管理

```c
// drivers/tty/n_tty.c - N_TTY数据结构
struct n_tty_data {
    /* 主读取缓冲区 */
    unsigned char *read_buf;            // 环形缓冲区
    size_t read_head;                   // 写入位置
    size_t read_tail;                   // 读取位置
    size_t read_cnt;                    // 可读字符数
    
    /* Canonical模式专用 */
    size_t canon_head;                  // canonical数据结尾
    size_t line_start;                  // 当前行开始位置
    
    /* 缓冲区状态标志 */
    bool no_room;                       // 缓冲区满
    bool lnext;                         // 下一个字符字面处理
    bool erasing;                       // 正在擦除
    bool raw;                           // 原始模式
    bool real_raw;                      // 真正的原始模式
    bool icanon;                        // canonical模式
    
    /* 等待队列 */
    wait_queue_head_t read_wait;        // 读等待队列
    
    /* 读取标志位 (每个字符一位) */
    unsigned long read_flags[N_TTY_BUF_SIZE / BITS_PER_LONG];
};

// 缓冲区操作宏和函数
#define N_TTY_BUF_SIZE      4096
#define WAKEUP_CHARS        256

// 环形缓冲区指针计算
static inline unsigned char read_buf(struct n_tty_data *ldata, size_t i)
{
    return ldata->read_buf[i & (N_TTY_BUF_SIZE - 1)];
}

static inline unsigned char *read_buf_addr(struct n_tty_data *ldata, size_t i)
{
    return &ldata->read_buf[i & (N_TTY_BUF_SIZE - 1)];
}

// 计算可用空间
static size_t receive_room(struct tty_struct *tty)
{
    struct n_tty_data *ldata = tty->disc_data;
    int room;
    
    if (I_PARMRK(tty)) {
        /* PARMRK模式需要额外空间标记错误 */
        room = N_TTY_BUF_SIZE - read_cnt(ldata) * 3 - 1;
    } else {
        room = N_TTY_BUF_SIZE - read_cnt(ldata) - 1;
    }
    
    if (room < 0)
        room = 0;
        
    return room;
}

// 读取计数
static inline size_t read_cnt(struct n_tty_data *ldata)
{
    return ldata->read_head - ldata->read_tail;
}
```

### Canonical模式读取实现

```c
// drivers/tty/n_tty.c - canonical模式读取
static ssize_t n_tty_read(struct tty_struct *tty, struct file *file,
                         unsigned char __user *buf, size_t nr)
{
    struct n_tty_data *ldata = tty->disc_data;
    unsigned char __user *b = buf;
    DEFINE_WAIT_FUNC(wait, woken_wake_function);
    int c;
    int minimum, time;
    ssize_t retval = 0;
    size_t tail;
    bool found = false;
    
    c = job_control(tty, file);
    if (unlikely(c < 0))
        return c;
        
    /* 确定最小字符数和超时 */
    minimum = time = 0;
    timeout = MAX_SCHEDULE_TIMEOUT;
    
    if (!ldata->icanon) {
        /* 非canonical模式：使用VMIN/VTIME */
        minimum = MIN_CHAR(tty);
        if (minimum) {
            time = (HZ / 10) * TIME_CHAR(tty);
            if (time)
                minimum = 1;
        } else {
            timeout = (HZ / 10) * TIME_CHAR(tty);
            minimum = 1;
        }
    }
    
    add_wait_queue(&tty->read_wait, &wait);
    
    while (nr) {
        /* 检查是否有数据可读 */
        if (ldata->icanon && !ldata->canon_data) {
            /* Canonical模式：等待完整行 */
            if (wait_woken(&wait, TASK_INTERRUPTIBLE, timeout) == 0)
                break;
            continue;
        }
        
        if (ldata->icanon) {
            /* Canonical模式：读取到行结束符 */
            size_t head = smp_load_acquire(&ldata->canon_head);
            
            while (ldata->read_tail != head && nr) {
                tail = ldata->read_tail & (N_TTY_BUF_SIZE - 1);
                c = read_buf(ldata, ldata->read_tail);
                ldata->read_tail++;
                
                /* 检查是否是行结束标志 */
                if (test_bit(tail, ldata->read_flags)) {
                    found = true;
                }
                
                /* 拷贝到用户空间 */
                if (put_user(c, b)) {
                    retval = -EFAULT;
                    break;
                }
                b++;
                nr--;
                
                /* 遇到行结束符，停止读取 */
                if (found && (c == '\n' || c == EOF_CHAR(tty)))
                    break;
            }
            
            if (found)
                clear_bit(ldata->read_tail - 1, ldata->read_flags);
                
        } else {
            /* 非canonical模式：字符模式读取 */
            size_t copy = min(read_cnt(ldata), nr);
            
            /* 批量拷贝 */
            if (copy) {
                tail = ldata->read_tail & (N_TTY_BUF_SIZE - 1);
                size_t head = ldata->read_head & (N_TTY_BUF_SIZE - 1);
                
                /* 处理环形缓冲区的边界 */
                size_t n = min(copy, N_TTY_BUF_SIZE - tail);
                
                if (copy_to_user(b, read_buf_addr(ldata, ldata->read_tail), n)) {
                    retval = -EFAULT;
                    break;
                }
                
                ldata->read_tail += n;
                b += n;
                nr -= n;
                copy -= n;
                
                /* 处理剩余部分 */
                if (copy) {
                    if (copy_to_user(b, ldata->read_buf, copy)) {
                        retval = -EFAULT;
                        break;
                    }
                    ldata->read_tail += copy;
                    b += copy;
                    nr -= copy;
                }
            }
            
            /* 检查最小字符数要求 */
            if (--minimum <= 0) {
                if (time)
                    timeout = time;
                minimum = 1;
            }
        }
        
        /* 检查是否满足退出条件 */
        if (b - buf >= minimum)
            break;
            
        /* 信号中断检查 */
        if (signal_pending(current)) {
            retval = -ERESTARTSYS;
            break;
        }
        
        /* 等待更多数据 */
        if (!timeout || wait_woken(&wait, TASK_INTERRUPTIBLE, timeout) == 0)
            break;
    }
    
    remove_wait_queue(&tty->read_wait, &wait);
    
    if (b - buf)
        retval = b - buf;
        
    /* 检查是否需要解除流控 */
    n_tty_check_unthrottle(tty);
    
    return retval;
}
```

### VMIN/VTIME控制逻辑

```c
// 非canonical模式的VMIN/VTIME处理逻辑
static int vmin_vtime_behavior(struct tty_struct *tty, 
                              unsigned char __user *buf, size_t nr)
{
    int vmin = MIN_CHAR(tty);    // VMIN值
    int vtime = TIME_CHAR(tty);  // VTIME值（单位：1/10秒）
    long timeout;
    ssize_t result;
    
    /*
     * VMIN/VTIME四种组合的行为：
     *
     * 1. VMIN>0, VTIME=0: 阻塞读取，至少返回VMIN个字符
     * 2. VMIN=0, VTIME>0: 超时读取，VTIME超时或有数据时返回
     * 3. VMIN>0, VTIME>0: 字符间超时，至少VMIN个字符，字符间隔不超过VTIME
     * 4. VMIN=0, VTIME=0: 非阻塞读取，立即返回可用数据
     */
     
    if (vmin == 0 && vtime == 0) {
        /* Case 4: 非阻塞读取 */
        return read_available_chars(tty, buf, nr);
    }
    
    if (vmin == 0) {
        /* Case 2: 纯超时读取 */
        timeout = vtime * HZ / 10;  // 转换为jiffies
        return read_with_timeout(tty, buf, nr, timeout);
    }
    
    if (vtime == 0) {
        /* Case 1: 纯阻塞读取 */
        return read_minimum_chars(tty, buf, nr, vmin);
    }
    
    /* Case 3: 字符间超时读取 */
    return read_with_inter_char_timeout(tty, buf, nr, vmin, vtime);
}

// 字符间超时读取实现
static ssize_t read_with_inter_char_timeout(struct tty_struct *tty,
                                           unsigned char __user *buf,
                                           size_t nr, int vmin, int vtime)
{
    struct n_tty_data *ldata = tty->disc_data;
    unsigned char __user *b = buf;
    long timeout = vtime * HZ / 10;
    int chars_read = 0;
    
    while (chars_read < vmin && (b - buf) < nr) {
        /* 等待字符或超时 */
        long remaining = wait_for_char_or_timeout(tty, timeout);
        
        if (remaining == 0) {
            /* 超时 */
            if (chars_read > 0) {
                /* 已经读取了一些字符，返回 */
                break;
            } else {
                /* 第一个字符超时，继续等待 */
                continue;
            }
        }
        
        /* 读取可用字符 */
        while (read_cnt(ldata) > 0 && (b - buf) < nr) {
            unsigned char c = read_buf(ldata, ldata->read_tail);
            ldata->read_tail++;
            
            if (put_user(c, b++))
                return -EFAULT;
                
            chars_read++;
            
            /* 重置字符间超时 */
            timeout = vtime * HZ / 10;
        }
    }
    
    return b - buf;
}
```

### 写入操作实现

```c
// drivers/tty/n_tty.c - TTY写入实现
static ssize_t n_tty_write(struct tty_struct *tty, struct file *file,
                          const unsigned char *buf, size_t nr)
{
    const unsigned char *b = buf;
    DEFINE_WAIT_FUNC(wait, woken_wake_function);
    int c;
    ssize_t retval = 0;
    
    /* 作业控制检查 */
    c = job_control(tty, file);
    if (unlikely(c < 0))
        return c;
        
    add_wait_queue(&tty->write_wait, &wait);
    
    while (1) {
        /* 信号检查 */
        if (signal_pending(current)) {
            retval = -ERESTARTSYS;
            break;
        }
        
        /* 检查TTY状态 */
        if (tty_hung_up_p(file) || (tty->link && !tty->link->count)) {
            retval = -EIO;
            break;
        }
        
        if (O_OPOST(tty)) {
            /* 输出后处理模式 */
            while (nr > 0) {
                ssize_t num = process_output_block(tty, b, nr);
                if (num < 0) {
                    if (num == -EAGAIN)
                        break;  /* 缓冲区满，需要等待 */
                    retval = num;
                    goto break_out;
                }
                b += num;
                nr -= num;
                if (nr == 0)
                    break;
                    
                c = *b;
                if (process_output(c, tty) < 0)
                    break;  /* 缓冲区满 */
                b++;
                nr--;
            }
            
            /* 刷新输出缓冲区 */
            if (tty->ops->flush_chars)
                tty->ops->flush_chars(tty);
                
        } else {
            /* 原始输出模式 */
            struct n_tty_data *ldata = tty->disc_data;
            
            while (nr > 0) {
                mutex_lock(&ldata->output_lock);
                c = tty->ops->write(tty, b, nr);
                mutex_unlock(&ldata->output_lock);
                
                if (c < 0) {
                    retval = c;
                    goto break_out;
                }
                if (!c)
                    break;  /* 缓冲区满，需要等待 */
                    
                b += c;
                nr -= c;
            }
        }
        
        if (!nr)
            break;  /* 全部写入完成 */
            
        /* 等待缓冲区有空间 */
        if (file->f_flags & O_NONBLOCK) {
            retval = -EAGAIN;
            break;
        }
        
        if (wait_woken(&wait, TASK_INTERRUPTIBLE, MAX_SCHEDULE_TIMEOUT) == 0)
            break;
    }
    
break_out:
    remove_wait_queue(&tty->write_wait, &wait);
    
    if (b - buf) {
        retval = b - buf;
        /* 如果有数据写入，更新访问时间 */
        if (tty->ops->write_wakeup)
            tty->ops->write_wakeup(tty);
    }
    
    return retval;
}

// 输出后处理
static int process_output(unsigned char c, struct tty_struct *tty)
{
    struct n_tty_data *ldata = tty->disc_data;
    int space, retval;
    
    mutex_lock(&ldata->output_lock);
    
    space = tty_write_room(tty);
    if (!space) {
        mutex_unlock(&ldata->output_lock);
        return -1;  /* 无空间，调用者需要等待 */
    }
    
    switch (c) {
    case '\n':
        if (O_ONLCR(tty)) {
            /* 换行转换为CR-LF */
            if (space < 2) {
                mutex_unlock(&ldata->output_lock);
                return -1;
            }
            ldata->column = 0;
            tty_put_char(tty, '\r');
            tty_put_char(tty, c);
        } else {
            ldata->column = 0;
            tty_put_char(tty, c);
        }
        break;
        
    case '\r':
        if (O_ONOCR(tty) && ldata->column == 0) {
            /* 在列0时不输出CR */
            mutex_unlock(&ldata->output_lock);
            return 0;
        }
        if (O_OCRNL(tty)) {
            /* CR转换为NL */
            c = '\n';
            if (O_ONLRET(tty))
                ldata->column = 0;
        } else {
            ldata->column = 0;
        }
        tty_put_char(tty, c);
        break;
        
    case '\t':
        spaces = 8 - (ldata->column & 7);
        if (O_TABDLY(tty) == XTABS) {
            /* 制表符扩展为空格 */
            if (space < spaces) {
                mutex_unlock(&ldata->output_lock);
                return -1;
            }
            ldata->column += spaces;
            for (int i = 0; i < spaces; i++)
                tty_put_char(tty, ' ');
        } else {
            ldata->column += spaces;
            tty_put_char(tty, c);
        }
        break;
        
    case '\b':
        if (ldata->column > 0)
            ldata->column--;
        tty_put_char(tty, c);
        break;
        
    default:
        if (!iscntrl(c)) {
            if (O_OLCUC(tty))
                c = toupper(c);
            if (!is_continuation_char(c))
                ldata->column++;
        }
        tty_put_char(tty, c);
        break;
    }
    
    mutex_unlock(&ldata->output_lock);
    return 0;
}
```

## 🔄 阻塞和非阻塞I/O机制

### 非阻塞I/O实现

```c
// 非阻塞读取的核心逻辑
static ssize_t tty_read_nonblock(struct tty_struct *tty, struct file *file,
                                unsigned char __user *buf, size_t count)
{
    struct n_tty_data *ldata = tty->disc_data;
    ssize_t retval;
    
    /* 检查是否有数据立即可用 */
    if (ldata->icanon) {
        /* Canonical模式：检查是否有完整行 */
        if (!ldata->canon_data) {
            return -EAGAIN;  /* 没有完整行可读 */
        }
    } else {
        /* 非canonical模式：检查是否有字符可读 */
        if (read_cnt(ldata) == 0) {
            if (MIN_CHAR(tty) == 0) {
                return 0;  /* VMIN=0时立即返回 */
            } else {
                return -EAGAIN;  /* VMIN>0但无数据 */
            }
        }
    }
    
    /* 执行实际读取 */
    retval = n_tty_read(tty, file, buf, count);
    
    return retval;
}

// 非阻塞写入的核心逻辑
static ssize_t tty_write_nonblock(struct tty_struct *tty, struct file *file,
                                 const unsigned char *buf, size_t count)
{
    int room;
    ssize_t ret;
    
    /* 检查写入缓冲区空间 */
    room = tty_write_room(tty);
    if (room == 0)
        return -EAGAIN;
        
    /* 限制写入数量为可用空间 */
    if (count > room)
        count = room;
        
    /* 执行实际写入 */
    ret = tty->ldisc->ops->write(tty, file, buf, count);
    
    return ret;
}
```

### 等待队列和唤醒机制

```c
// TTY等待队列管理
struct tty_struct {
    wait_queue_head_t read_wait;        // 读等待队列
    wait_queue_head_t write_wait;       // 写等待队列
    // ...
};

// 读操作等待
static void tty_wait_for_read(struct tty_struct *tty)
{
    DEFINE_WAIT_FUNC(wait, woken_wake_function);
    
    add_wait_queue(&tty->read_wait, &wait);
    
    while (!data_available(tty)) {
        if (signal_pending(current)) {
            remove_wait_queue(&tty->read_wait, &wait);
            return -ERESTARTSYS;
        }
        
        /* 进入睡眠等待 */
        wait_woken(&wait, TASK_INTERRUPTIBLE, MAX_SCHEDULE_TIMEOUT);
    }
    
    remove_wait_queue(&tty->read_wait, &wait);
}

// 数据到达时的唤醒
static void n_tty_receive_buf_wakeup(struct tty_struct *tty)
{
    struct n_tty_data *ldata = tty->disc_data;
    
    /* 唤醒读等待队列 */
    if (ldata->canon_data || (!ldata->icanon && read_cnt(ldata) >= MIN_CHAR(tty))) {
        wake_up_interruptible_poll(&tty->read_wait, EPOLLIN | EPOLLRDNORM);
    }
    
    /* 唤醒写等待队列（如果有空间） */
    if (tty_write_room(tty) > WAKEUP_CHARS) {
        wake_up_interruptible_poll(&tty->write_wait, EPOLLOUT | EPOLLWRNORM);
    }
}

// select/poll/epoll支持
static __poll_t n_tty_poll(struct tty_struct *tty, struct file *file,
                          poll_table *wait)
{
    struct n_tty_data *ldata = tty->disc_data;
    __poll_t mask = 0;
    
    poll_wait(file, &tty->read_wait, wait);
    poll_wait(file, &tty->write_wait, wait);
    
    /* 检查读就绪 */
    if (ldata->icanon && ldata->canon_data)
        mask |= EPOLLIN | EPOLLRDNORM;
    else if (!ldata->icanon && read_cnt(ldata) >= MIN_CHAR(tty))
        mask |= EPOLLIN | EPOLLRDNORM;
        
    /* 检查写就绪 */
    if (tty_write_room(tty) > 0)
        mask |= EPOLLOUT | EPOLLWRNORM;
        
    /* 检查异常条件 */
    if (tty_hung_up_p(file))
        mask |= EPOLLHUP;
    if (tty->packet && tty->link->ctrl_status)
        mask |= EPOLLPRI;
        
    return mask;
}
```

## 🧪 最小可运行实验

### 实验1：Canonical vs Raw模式I/O对比

```c
// io_modes_test.c - 对比不同I/O模式
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>

struct termios saved_termios;

void restore_terminal() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios);
}

void test_canonical_mode() {
    printf("=== Canonical模式测试 ===\n");
    printf("特点：行缓冲，支持编辑，等待换行符\n");
    
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    
    /* 确保canonical模式 */
    t.c_lflag |= ICANON;
    t.c_lflag |= ECHO;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
    
    char buffer[256];
    ssize_t n;
    struct timeval start, end;
    
    printf("请输入一行文字（支持退格编辑）: ");
    fflush(stdout);
    
    gettimeofday(&start, NULL);
    n = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
    gettimeofday(&end, NULL);
    
    if (n > 0) {
        buffer[n] = '\0';
        long elapsed_ms = (end.tv_sec - start.tv_sec) * 1000 + 
                         (end.tv_usec - start.tv_usec) / 1000;
        
        printf("读取结果:\n");
        printf("  字节数: %zd\n", n);
        printf("  内容: %s", buffer);
        printf("  耗时: %ld ms\n", elapsed_ms);
        printf("  最后字符: 0x%02x (%s)\n", 
               (unsigned char)buffer[n-1],
               buffer[n-1] == '\n' ? "换行符" : "其他");
    }
}

void test_raw_mode_basic() {
    printf("\n=== Raw模式基础测试 ===\n");
    printf("特点：字符模式，无编辑，无echo\n");
    
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    
    /* 设置raw模式 */
    t.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
    t.c_iflag &= ~(IXON | ICRNL | INPCK | ISTRIP | BRKINT);
    t.c_oflag &= ~OPOST;
    t.c_cflag |= CS8;
    t.c_cc[VMIN] = 1;   /* 至少读取1个字符 */
    t.c_cc[VTIME] = 0;  /* 无超时 */
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
    
    printf("Raw模式设置完成\n");
    printf("输入字符观察（输入'q'退出）:\n");
    
    unsigned char c;
    while (1) {
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n == 1) {
            printf("字符: 0x%02x", c);
            if (c >= 32 && c < 127) {
                printf(" ('%c')", c);
            } else if (c < 32) {
                printf(" (^%c)", c + '@');
            }
            printf(" [无回显，立即返回]\n");
            
            if (c == 'q') {
                printf("退出raw模式测试\n");
                break;
            }
        } else {
            printf("读取错误: %s\n", strerror(errno));
            break;
        }
    }
}

void test_vmin_vtime() {
    printf("\n=== VMIN/VTIME组合测试 ===\n");
    
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    
    /* 基础raw设置 */
    t.c_lflag &= ~(ICANON | ECHO | ISIG);
    t.c_iflag &= ~(IXON | ICRNL);
    t.c_oflag &= ~OPOST;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
    
    struct {
        int vmin, vtime;
        const char *description;
    } tests[] = {
        {0, 0, "非阻塞读取（立即返回）"},
        {1, 0, "阻塞读取（至少1个字符）"},
        {0, 50, "超时读取（5秒超时）"},
        {3, 20, "字符间超时（至少3个字符，字符间2秒超时）"}
    };
    
    for (int i = 0; i < 4; i++) {
        printf("\n--- 测试%d: VMIN=%d, VTIME=%d ---\n", 
               i+1, tests[i].vmin, tests[i].vtime);
        printf("描述: %s\n", tests[i].description);
        
        /* 设置VMIN/VTIME */
        tcgetattr(STDIN_FILENO, &t);
        t.c_cc[VMIN] = tests[i].vmin;
        t.c_cc[VTIME] = tests[i].vtime;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
        
        printf("开始读取...");
        fflush(stdout);
        
        char buffer[10];
        struct timeval start, end;
        gettimeofday(&start, NULL);
        
        ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
        
        gettimeofday(&end, NULL);
        long elapsed_ms = (end.tv_sec - start.tv_sec) * 1000 + 
                         (end.tv_usec - start.tv_usec) / 1000;
        
        printf("\n结果:\n");
        if (n > 0) {
            printf("  读取字节数: %zd\n", n);
            printf("  内容: ");
            for (ssize_t j = 0; j < n; j++) {
                if (buffer[j] >= 32 && buffer[j] < 127) {
                    printf("%c", buffer[j]);
                } else {
                    printf("\\x%02x", (unsigned char)buffer[j]);
                }
            }
            printf("\n");
        } else if (n == 0) {
            printf("  超时或EOF\n");
        } else {
            printf("  错误: %s\n", strerror(errno));
        }
        printf("  耗时: %ld ms\n", elapsed_ms);
        
        printf("按Enter继续下一个测试...");
        getchar();  /* 清除输入缓冲区 */
    }
}

void test_nonblocking_io() {
    printf("\n=== 非阻塞I/O测试 ===\n");
    
    /* 设置非阻塞模式 */
    int flags = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag &= ~ICANON;  /* 非canonical */
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
    
    printf("非阻塞模式设置完成\n");
    printf("连续读取演示（10次尝试）:\n");
    
    char buffer[256];
    for (int i = 0; i < 10; i++) {
        ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
        
        printf("第%d次读取: ", i + 1);
        if (n > 0) {
            buffer[n] = '\0';
            printf("读取%zd字节: %s", n, buffer);
        } else if (n == 0) {
            printf("无数据可读");
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("无数据（EAGAIN）");
            } else {
                printf("错误: %s", strerror(errno));
            }
        }
        printf("\n");
        
        usleep(500000);  /* 等待0.5秒 */
    }
    
    /* 恢复阻塞模式 */
    fcntl(STDIN_FILENO, F_SETFL, flags);
    printf("已恢复阻塞模式\n");
}

int main() {
    printf("=== TTY I/O模式对比实验 ===\n");
    
    /* 保存原始终端设置 */
    tcgetattr(STDIN_FILENO, &saved_termios);
    atexit(restore_terminal);
    
    test_canonical_mode();
    test_raw_mode_basic();
    test_vmin_vtime();
    test_nonblocking_io();
    
    printf("\n实验结束，终端设置已恢复\n");
    return 0;
}
```

### 实验2：select/poll/epoll与TTY I/O

```c
// io_multiplexing.c - I/O多路复用与TTY
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>

struct termios original_termios;

void restore_terminal() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
}

void setup_raw_mode() {
    struct termios raw;
    
    tcgetattr(STDIN_FILENO, &original_termios);
    atexit(restore_terminal);
    
    raw = original_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void test_select() {
    printf("=== select() 测试 ===\n");
    printf("输入字符观察select行为（输入'q'退出）:\n");
    
    char buffer[256];
    
    while (1) {
        fd_set readfds, writefds, exceptfds;
        struct timeval timeout;
        
        /* 准备文件描述符集合 */
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_ZERO(&exceptfds);
        
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(STDOUT_FILENO, &writefds);
        FD_SET(STDIN_FILENO, &exceptfds);
        
        /* 设置2秒超时 */
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        
        printf("等待I/O事件...");
        fflush(stdout);
        
        int result = select(STDIN_FILENO + 1, &readfds, &writefds, &exceptfds, &timeout);
        
        printf("\nselect返回: %d\n", result);
        
        if (result == -1) {
            perror("select");
            break;
        } else if (result == 0) {
            printf("超时，无I/O事件\n");
            continue;
        }
        
        /* 检查各个文件描述符 */
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            printf("STDIN可读\n");
            ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
            if (n > 0) {
                buffer[n] = '\0';
                printf("读取内容: %s", buffer);
                
                if (buffer[0] == 'q') {
                    printf("退出select测试\n");
                    break;
                }
            }
        }
        
        if (FD_ISSET(STDOUT_FILENO, &writefds)) {
            printf("STDOUT可写\n");
        }
        
        if (FD_ISSET(STDIN_FILENO, &exceptfds)) {
            printf("STDIN异常事件\n");
        }
    }
}

void test_poll() {
    printf("\n=== poll() 测试 ===\n");
    printf("输入字符观察poll行为（输入'q'退出）:\n");
    
    struct pollfd fds[2];
    char buffer[256];
    
    /* 设置poll结构 */
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN | POLLPRI;
    fds[0].revents = 0;
    
    fds[1].fd = STDOUT_FILENO;
    fds[1].events = POLLOUT;
    fds[1].revents = 0;
    
    while (1) {
        printf("等待poll事件...");
        fflush(stdout);
        
        /* 2秒超时 */
        int result = poll(fds, 2, 2000);
        
        printf("\npoll返回: %d\n", result);
        
        if (result == -1) {
            perror("poll");
            break;
        } else if (result == 0) {
            printf("超时，无I/O事件\n");
            continue;
        }
        
        /* 检查STDIN */
        if (fds[0].revents & POLLIN) {
            printf("STDIN: POLLIN事件\n");
            ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
            if (n > 0) {
                buffer[n] = '\0';
                printf("读取内容: %s", buffer);
                
                if (buffer[0] == 'q') {
                    printf("退出poll测试\n");
                    break;
                }
            }
        }
        
        if (fds[0].revents & POLLPRI) {
            printf("STDIN: POLLPRI事件（优先数据）\n");
        }
        
        if (fds[0].revents & POLLERR) {
            printf("STDIN: POLLERR事件\n");
        }
        
        if (fds[0].revents & POLLHUP) {
            printf("STDIN: POLLHUP事件\n");
        }
        
        /* 检查STDOUT */
        if (fds[1].revents & POLLOUT) {
            printf("STDOUT: POLLOUT事件（可写）\n");
        }
        
        /* 重置revents */
        fds[0].revents = 0;
        fds[1].revents = 0;
    }
}

void test_epoll() {
    printf("\n=== epoll 测试 ===\n");
    printf("输入字符观察epoll行为（输入'q'退出）:\n");
    
    int epfd = epoll_create1(0);
    if (epfd == -1) {
        perror("epoll_create1");
        return;
    }
    
    struct epoll_event ev, events[10];
    char buffer[256];
    
    /* 添加STDIN到epoll */
    ev.events = EPOLLIN | EPOLLPRI | EPOLLET;  /* 边缘触发 */
    ev.data.fd = STDIN_FILENO;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev) == -1) {
        perror("epoll_ctl: stdin");
        close(epfd);
        return;
    }
    
    /* 添加STDOUT到epoll */
    ev.events = EPOLLOUT;
    ev.data.fd = STDOUT_FILENO;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, STDOUT_FILENO, &ev) == -1) {
        perror("epoll_ctl: stdout");
        close(epfd);
        return;
    }
    
    while (1) {
        printf("等待epoll事件...");
        fflush(stdout);
        
        int nfds = epoll_wait(epfd, events, 10, 2000);  /* 2秒超时 */
        
        printf("\nepoll_wait返回: %d\n", nfds);
        
        if (nfds == -1) {
            perror("epoll_wait");
            break;
        } else if (nfds == 0) {
            printf("超时，无I/O事件\n");
            continue;
        }
        
        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;
            uint32_t ev_mask = events[i].events;
            
            printf("事件[%d]: fd=%d, events=0x%x ", i, fd, ev_mask);
            
            if (fd == STDIN_FILENO) {
                printf("(STDIN) ");
                
                if (ev_mask & EPOLLIN) {
                    printf("EPOLLIN ");
                    ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
                    if (n > 0) {
                        buffer[n] = '\0';
                        printf("\n读取内容: %s", buffer);
                        
                        if (buffer[0] == 'q') {
                            printf("退出epoll测试\n");
                            close(epfd);
                            return;
                        }
                    }
                }
                
                if (ev_mask & EPOLLPRI) printf("EPOLLPRI ");
                if (ev_mask & EPOLLERR) printf("EPOLLERR ");
                if (ev_mask & EPOLLHUP) printf("EPOLLHUP ");
                
            } else if (fd == STDOUT_FILENO) {
                printf("(STDOUT) ");
                if (ev_mask & EPOLLOUT) printf("EPOLLOUT ");
            }
            
            printf("\n");
        }
    }
    
    close(epfd);
}

void test_io_timing() {
    printf("\n=== I/O时序测试 ===\n");
    printf("比较不同I/O方式的响应时间\n");
    
    struct timeval start, end;
    char c;
    
    /* 测试1: 阻塞读取 */
    printf("\n1. 阻塞读取测试 - 输入一个字符: ");
    fflush(stdout);
    
    gettimeofday(&start, NULL);
    read(STDIN_FILENO, &c, 1);
    gettimeofday(&end, NULL);
    
    long elapsed = (end.tv_sec - start.tv_sec) * 1000000 + 
                   (end.tv_usec - start.tv_usec);
    printf("响应时间: %ld 微秒\n", elapsed);
    
    /* 测试2: select监听 */
    printf("\n2. select监听测试 - 输入一个字符: ");
    fflush(stdout);
    
    gettimeofday(&start, NULL);
    
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    
    select(STDIN_FILENO + 1, &readfds, NULL, NULL, NULL);
    if (FD_ISSET(STDIN_FILENO, &readfds)) {
        read(STDIN_FILENO, &c, 1);
    }
    
    gettimeofday(&end, NULL);
    elapsed = (end.tv_sec - start.tv_sec) * 1000000 + 
              (end.tv_usec - start.tv_usec);
    printf("响应时间: %ld 微秒\n", elapsed);
    
    /* 测试3: epoll监听 */
    printf("\n3. epoll监听测试 - 输入一个字符: ");
    fflush(stdout);
    
    int epfd = epoll_create1(0);
    struct epoll_event ev, events[1];
    
    ev.events = EPOLLIN;
    ev.data.fd = STDIN_FILENO;
    epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev);
    
    gettimeofday(&start, NULL);
    
    int nfds = epoll_wait(epfd, events, 1, -1);
    if (nfds > 0 && events[0].data.fd == STDIN_FILENO) {
        read(STDIN_FILENO, &c, 1);
    }
    
    gettimeofday(&end, NULL);
    elapsed = (end.tv_sec - start.tv_sec) * 1000000 + 
              (end.tv_usec - start.tv_usec);
    printf("响应时间: %ld 微秒\n", elapsed);
    
    close(epfd);
}

int main() {
    printf("=== TTY I/O多路复用实验 ===\n");
    
    setup_raw_mode();
    
    test_select();
    test_poll();
    test_epoll();
    test_io_timing();
    
    printf("\n实验结束，终端设置已恢复\n");
    return 0;
}
```

### 实验3：TTY缓冲区观察

```c
// buffer_analysis.c - TTY缓冲区分析
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

void print_buffer_info() {
    int input_count, output_count;
    
    /* 查询输入缓冲区字节数 */
    if (ioctl(STDIN_FILENO, FIONREAD, &input_count) == 0) {
        printf("输入缓冲区: %d 字节\n", input_count);
    } else {
        printf("输入缓冲区: 查询失败 (%s)\n", strerror(errno));
    }
    
    /* 查询输出缓冲区字节数 */
    if (ioctl(STDOUT_FILENO, TIOCOUTQ, &output_count) == 0) {
        printf("输出缓冲区: %d 字节\n", output_count);
    } else {
        printf("输出缓冲区: 查询失败 (%s)\n", strerror(errno));
    }
}

void test_input_buffering() {
    printf("=== 输入缓冲区测试 ===\n");
    printf("请快速输入多个字符但不按Enter，然后等待...\n");
    
    /* 设置非阻塞模式来观察缓冲 */
    int flags = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    
    sleep(3);  /* 给用户时间输入 */
    
    printf("\n输入完成后的缓冲区状态:\n");
    print_buffer_info();
    
    /* 逐个字符读取并观察缓冲区变化 */
    printf("\n逐字符读取过程:\n");
    char c;
    int read_count = 0;
    
    while (1) {
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n == 1) {
            read_count++;
            printf("读取字符%d: 0x%02x ('%c')\n", 
                   read_count, (unsigned char)c, 
                   (c >= 32 && c < 127) ? c : '?');
            print_buffer_info();
        } else if (n == 0) {
            printf("EOF\n");
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("缓冲区为空\n");
                break;
            } else {
                printf("读取错误: %s\n", strerror(errno));
                break;
            }
        }
    }
    
    /* 恢复阻塞模式 */
    fcntl(STDIN_FILENO, F_SETFL, flags);
}

void test_output_buffering() {
    printf("\n=== 输出缓冲区测试 ===\n");
    
    /* 写入大量数据观察缓冲行为 */
    char data[1024];
    memset(data, 'A', sizeof(data) - 1);
    data[sizeof(data) - 1] = '\0';
    
    printf("写入前缓冲区状态:\n");
    print_buffer_info();
    
    printf("写入1KB数据到stdout...\n");
    write(STDOUT_FILENO, data, strlen(data));
    
    printf("写入后缓冲区状态:\n");
    print_buffer_info();
    
    printf("调用fsync刷新...\n");
    fsync(STDOUT_FILENO);
    
    printf("刷新后缓冲区状态:\n");
    print_buffer_info();
}

void test_line_buffering() {
    printf("\n=== 行缓冲测试 ===\n");
    
    struct termios t, orig;
    tcgetattr(STDIN_FILENO, &orig);
    t = orig;
    
    /* 设置canonical模式 */
    t.c_lflag |= ICANON;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
    
    printf("Canonical模式 - 请输入文字但不要按Enter:\n");
    sleep(3);
    
    printf("未按Enter时的缓冲区:\n");
    print_buffer_info();
    
    printf("现在请按Enter...\n");
    
    char buffer[256];
    ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
    
    printf("按Enter后读取了 %zd 字节\n", n);
    if (n > 0) {
        buffer[n] = '\0';
        printf("内容: %s", buffer);
    }
    
    printf("读取后缓冲区:\n");
    print_buffer_info();
    
    /* 恢复原设置 */
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
}

void test_vmin_vtime_buffering() {
    printf("\n=== VMIN/VTIME缓冲行为测试 ===\n");
    
    struct termios t, orig;
    tcgetattr(STDIN_FILENO, &orig);
    t = orig;
    
    /* 设置raw模式 */
    t.c_lflag &= ~(ICANON | ECHO);
    t.c_iflag &= ~(ICRNL | INLCR);
    t.c_oflag &= ~OPOST;
    
    struct {
        int vmin, vtime;
        const char *desc;
    } configs[] = {
        {3, 0, "等待3个字符"},
        {0, 20, "2秒超时"},
        {2, 10, "等待2个字符，字符间1秒超时"}
    };
    
    for (int i = 0; i < 3; i++) {
        printf("\n--- 配置%d: VMIN=%d, VTIME=%d (%s) ---\n",
               i+1, configs[i].vmin, configs[i].vtime, configs[i].desc);
               
        t.c_cc[VMIN] = configs[i].vmin;
        t.c_cc[VTIME] = configs[i].vtime;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);
        
        printf("开始输入...\n");
        print_buffer_info();
        
        char buffer[10];
        ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
        
        printf("读取完成: %zd 字节\n", n);
        if (n > 0) {
            printf("内容: ");
            for (ssize_t j = 0; j < n; j++) {
                printf("0x%02x ", (unsigned char)buffer[j]);
            }
            printf("\n");
        }
        
        print_buffer_info();
        
        printf("按Enter继续...");
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);  /* 临时恢复 */
        getchar();
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);     /* 重设 */
    }
    
    /* 恢复原设置 */
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
}

int main() {
    printf("=== TTY缓冲区分析实验 ===\n");
    
    printf("初始状态:\n");
    print_buffer_info();
    
    test_input_buffering();
    test_output_buffering();
    test_line_buffering();
    test_vmin_vtime_buffering();
    
    printf("\n实验结束\n");
    return 0;
}
```

### 实验4：性能对比测试

```bash
#!/bin/bash
# io_performance.sh - I/O性能对比测试

echo "=== TTY I/O性能对比实验 ==="

# 编译测试程序
echo "编译测试程序..."
gcc -o io_modes_test io_modes_test.c
gcc -o io_multiplexing io_multiplexing.c  
gcc -o buffer_analysis buffer_analysis.c

echo -e "\n1. 字符读取性能测试:"

# 创建测试数据
echo "abcdefghijklmnopqrstuvwxyz" > test_input.txt

echo "测试1: Canonical模式"
time sh -c 'cat test_input.txt | ./io_modes_test > /dev/null 2>&1'

echo "测试2: Raw模式"
time sh -c 'echo "q" | timeout 5 ./io_modes_test > /dev/null 2>&1'

echo -e "\n2. 大量数据I/O测试:"

# 创建大文件
dd if=/dev/zero of=large_file bs=1M count=10 2>/dev/null
echo "创建10MB测试文件"

echo "测试: 大文件读取性能"
time cat large_file > /dev/null

echo "测试: 通过管道传输"  
time cat large_file | cat > /dev/null

echo -e "\n3. I/O多路复用性能测试:"

# 模拟多个I/O源
mkfifo fifo1 fifo2 fifo3 2>/dev/null || true

echo "创建测试数据..." &
echo "data1" > fifo1 &
echo "data2" > fifo2 &
echo "data3" > fifo3 &

echo "测试select性能:"
timeout 2 ./io_multiplexing > /dev/null 2>&1 || true

echo -e "\n4. 缓冲区效率测试:"

echo "测试不同缓冲区大小的影响:"
for size in 1 64 1024 4096 8192; do
    echo -n "缓冲区大小 ${size}字节: "
    time dd if=large_file of=/dev/null bs=$size 2>/dev/null
done

echo -e "\n5. 系统调用开销测试:"

echo "strace跟踪系统调用开销:"
echo "abcdef" | strace -c -e trace=read,write cat > /dev/null

echo -e "\n6. TTY vs 管道性能对比:"

echo "TTY读取:"
echo "test data" | time cat > /dev/null

echo "管道读取:"
echo "test data" > temp_file
time cat temp_file > /dev/null

echo -e "\n7. 实时响应性测试:"

echo "测试按键响应延迟 (需要手动输入):"
echo "请运行: time -p sh -c 'read -n 1 char && echo \$char'"

# 清理
rm -f test_input.txt large_file temp_file
rm -f fifo1 fifo2 fifo3 2>/dev/null || true
rm -f io_modes_test io_multiplexing buffer_analysis

echo -e "\n性能测试完成！"
```

## 🚨 常见坑 & Debug方法

### 1. VMIN/VTIME配置错误

**问题**: read()行为不符合预期
```c
// 检查当前VMIN/VTIME设置
void check_vmin_vtime() {
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    
    printf("当前设置:\n");
    printf("  ICANON: %s\n", (t.c_lflag & ICANON) ? "开启" : "关闭");
    printf("  VMIN: %d\n", t.c_cc[VMIN]);
    printf("  VTIME: %d\n", t.c_cc[VTIME]);
    
    if (t.c_lflag & ICANON) {
        printf("警告: Canonical模式下VMIN/VTIME无效\n");
    }
}
```

### 2. 非阻塞I/O误用

**问题**: 非阻塞读取返回EAGAIN时处理不当
```c
// 正确的非阻塞读取处理
ssize_t safe_nonblock_read(int fd, void *buf, size_t count) {
    ssize_t result = read(fd, buf, count);
    
    if (result == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* 无数据可读，这是正常情况 */
            return 0;  
        } else {
            /* 真正的错误 */
            perror("read");
            return -1;
        }
    }
    
    return result;
}
```

### 3. 缓冲区满导致的死锁

**问题**: 写入阻塞导致程序挂起
```bash
# 检查TTY缓冲区状态
echo "输入缓冲区字节数:"
python3 -c "
import fcntl, struct, sys
buf = fcntl.ioctl(sys.stdin.fileno(), 0x541B, b'\\x00' * 4)
print(struct.unpack('I', buf)[0])
"

# 检查输出缓冲区  
echo "输出缓冲区字节数:"
python3 -c "
import fcntl, struct, sys
try:
    buf = fcntl.ioctl(sys.stdout.fileno(), 0x5411, b'\\x00' * 4)  
    print(struct.unpack('I', buf)[0])
except OSError as e:
    print('不支持:', e)
"
```

### 4. select/poll/epoll误用

**问题**: 事件监听不正确
```c
// 检查文件描述符是否支持poll
void check_poll_support(int fd) {
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN | POLLOUT;
    pfd.revents = 0;
    
    int result = poll(&pfd, 1, 0);  /* 非阻塞检查 */
    
    printf("fd %d poll结果: %d\n", fd, result);
    if (result > 0) {
        printf("  事件: ");
        if (pfd.revents & POLLIN) printf("POLLIN ");
        if (pfd.revents & POLLOUT) printf("POLLOUT ");
        if (pfd.revents & POLLERR) printf("POLLERR ");
        if (pfd.revents & POLLHUP) printf("POLLHUP ");
        printf("\n");
    }
}
```

### 5. 使用strace调试I/O问题

```bash
# 跟踪read/write系统调用
strace -e trace=read,write,poll,select,epoll_wait -s 100 ./your_program

# 跟踪文件描述符操作
strace -e trace=openat,close,dup2,fcntl ./your_program

# 跟踪termios相关操作
strace -e trace=ioctl -s 200 ./your_program | grep -E "(TCGETS|TCSETS|FIONREAD|TIOCOUTQ)"
```

### 6. 内存和性能分析

```bash
# 使用valgrind检查内存问题
valgrind --tool=memcheck ./io_test

# 使用perf分析性能
sudo perf record -e syscalls:sys_enter_read,syscalls:sys_enter_write ./io_test
sudo perf report

# 查看进程I/O统计
cat /proc/$PID/io
# 字段说明:
# rchar: 读取字符数
# wchar: 写入字符数  
# syscr: 读系统调用次数
# syscw: 写系统调用次数
# read_bytes: 实际从存储读取字节数
# write_bytes: 实际写入存储字节数
```

## 📋 实际应用场景

### 1. 高性能终端模拟器实现

```c
// 终端模拟器的I/O处理核心
struct terminal_io {
    int master_fd;                     // PTY master
    char read_buffer[8192];            // 读缓冲区
    char write_buffer[8192];           // 写缓冲区
    size_t write_pos;                  // 写位置
    bool nonblock;                     // 非阻塞模式
};

int terminal_process_input(struct terminal_io *term) {
    /* 使用epoll高效处理I/O */
    struct epoll_event events[10];
    int nfds = epoll_wait(term->epoll_fd, events, 10, 1);
    
    for (int i = 0; i < nfds; i++) {
        if (events[i].data.fd == term->master_fd) {
            if (events[i].events & EPOLLIN) {
                /* 从PTY读取数据 */
                ssize_t n = read(term->master_fd, term->read_buffer, 
                               sizeof(term->read_buffer));
                if (n > 0) {
                    /* 更新终端显示 */
                    update_terminal_display(term->read_buffer, n);
                }
            }
            
            if (events[i].events & EPOLLOUT && term->write_pos > 0) {
                /* 向PTY写入数据 */
                ssize_t n = write(term->master_fd, term->write_buffer, 
                                term->write_pos);
                if (n > 0) {
                    memmove(term->write_buffer, term->write_buffer + n,
                           term->write_pos - n);
                    term->write_pos -= n;
                }
            }
        }
    }
    
    return 0;
}
```

### 2. 命令行工具的智能输入处理

```c
// 实现类似bash的行编辑功能
struct line_editor {
    char *line;                        // 当前行
    size_t pos;                        // 光标位置
    size_t len;                        // 行长度
    char **history;                    // 历史记录
    int history_pos;                   // 历史位置
};

int readline_with_editing(struct line_editor *ed) {
    struct termios orig, raw;
    
    /* 设置raw模式进行字符级处理 */
    tcgetattr(STDIN_FILENO, &orig);
    raw = orig;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    
    while (1) {
        unsigned char c;
        if (read(STDIN_FILENO, &c, 1) != 1)
            break;
            
        switch (c) {
        case '\n':
        case '\r':
            /* 回车：完成输入 */
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
            return 0;
            
        case 0x08:  /* Backspace */
        case 0x7f:  /* Delete */
            if (ed->pos > 0) {
                ed->pos--;
                ed->len--;
                memmove(ed->line + ed->pos, ed->line + ed->pos + 1,
                       ed->len - ed->pos);
                ed->line[ed->len] = '\0';
                /* 重绘行 */
                redraw_line(ed);
            }
            break;
            
        case 0x1b:  /* ESC序列（方向键等） */
            handle_escape_sequence(ed);
            break;
            
        default:
            if (c >= 32 && c < 127) {  /* 可打印字符 */
                insert_char(ed, c);
                redraw_line(ed);
            }
            break;
        }
    }
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
    return -1;
}
```

### 3. 实时数据流处理

```c
// 处理实时数据流（如日志监控）
struct stream_processor {
    int input_fd;
    char buffer[65536];
    size_t buffer_pos;
    time_t last_activity;
};

int process_realtime_stream(struct stream_processor *proc) {
    /* 使用非阻塞I/O + select实现实时处理 */
    fd_set readfds;
    struct timeval timeout;
    
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(proc->input_fd, &readfds);
        
        /* 100ms超时，保证实时响应 */
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;
        
        int result = select(proc->input_fd + 1, &readfds, NULL, NULL, &timeout);
        
        if (result > 0 && FD_ISSET(proc->input_fd, &readfds)) {
            /* 有数据到达 */
            ssize_t n = read(proc->input_fd, 
                           proc->buffer + proc->buffer_pos,
                           sizeof(proc->buffer) - proc->buffer_pos);
            
            if (n > 0) {
                proc->buffer_pos += n;
                proc->last_activity = time(NULL);
                
                /* 处理完整行 */
                process_complete_lines(proc);
            }
        } else if (result == 0) {
            /* 超时：检查是否需要刷新不完整的行 */
            if (time(NULL) - proc->last_activity > 1) {
                process_partial_line(proc);
            }
        }
    }
    
    return 0;
}
```

## 🎯 学习检查点

完成本模块后，你应该能够：

1. ✅ 理解canonical和raw模式下read()的不同行为
2. ✅ 掌握VMIN/VTIME参数的四种组合及其应用场景
3. ✅ 理解阻塞/非阻塞I/O的内核实现机制
4. ✅ 能够正确使用select/poll/epoll监听TTY I/O事件
5. ✅ 理解TTY缓冲区管理和流控制机制
6. ✅ 会调试和优化TTY I/O性能问题
7. ✅ 能够实现高效的终端应用程序

---

**下一步**: 学习 [SSH/Docker/Terminal 应用详解](08-applications.md)