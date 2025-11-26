# sys_read 系统调用完整执行路径分析

普通文件读取的完整流程 (基于 Linux 3.2 内核)

---

## 目录

- [执行路径概览](#执行路径概览)
- [1. 用户态调用入口](#1-用户态调用入口)
- [2. 陷入内核的机制](#2-陷入内核的机制)
- [3. 实际处理函数](#3-实际处理函数)
- [4. 返回用户态的过程](#4-返回用户态的过程)
- [完整时序图](#完整时序图)
- [关键源码位置](#关键源码位置)

---

## 执行路径概览

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    sys_read 普通文件读取完整执行路径                          │
└─────────────────────────────────────────────────────────────────────────────┘

用户态                                     内核态
───────                                    ───────

read(fd, buf, count)                       
     │                                     
     ▼                                     
┌──────────────┐                           
│ glibc read() │                           
│ int $0x80    │ ──────────────────────────► system_call
└──────────────┘                                │
                                                ▼
                                          sys_read()
                                                │
                                                ▼
                                          fget_light(fd)
                                                │ 获取 struct file
                                                ▼
                                          vfs_read()
                                                │
                                                ▼
                                     ┌──────────┴──────────┐
                                     │                     │
                                     ▼                     ▼
                              f_op->read()          do_sync_read()
                              (如果定义)            (通用实现)
                                     │                     │
                                     │                     ▼
                                     │          f_op->aio_read()
                                     │                     │
                                     └──────────┬──────────┘
                                                │
                                                ▼
                                    generic_file_aio_read()
                                                │
                                                ▼
                                     do_generic_file_read()
                                                │
                                     ┌──────────┴──────────┐
                                     │                     │
                                     ▼                     ▼
                               页缓存命中             页缓存未命中
                                     │                     │
                                     │                     ▼
                                     │            readpage() 触发 I/O
                                     │                     │
                                     └──────────┬──────────┘
                                                │
                                                ▼
                                    copy_to_user(buf, page_data)
                                                │
                                                ▼
                                          返回读取字节数
                                                │
┌──────────────┐                                │
│ 返回值 = n   │ ◄──────────────────────────────┘
│ (读取字节数)  │
└──────────────┘
```

---

## 1. 用户态调用入口

### 1.1 用户程序调用

```c
#include <unistd.h>

int main() {
    int fd = open("/path/to/file", O_RDONLY);
    char buf[1024];
    ssize_t n = read(fd, buf, sizeof(buf));  // 用户态入口
    close(fd);
    return 0;
}
```

### 1.2 glibc 包装函数

```c
// glibc: sysdeps/unix/sysv/linux/read.c (简化)
ssize_t __libc_read(int fd, void *buf, size_t count)
{
    return INLINE_SYSCALL(read, 3, fd, buf, count);
}
weak_alias(__libc_read, read)
```

### 1.3 汇编层面 (x86-32)

```asm
# glibc 生成的系统调用代码
    movl    $3, %eax          # __NR_read = 3
    movl    fd, %ebx          # 第1个参数: 文件描述符
    movl    buf, %ecx         # 第2个参数: 用户缓冲区地址
    movl    count, %edx       # 第3个参数: 读取字节数
    int     $0x80             # 触发软中断 (或使用 sysenter)
    # 返回值在 %eax 中
```

### 1.4 系统调用号

```c
// arch/x86/include/asm/unistd_32.h
#define __NR_read    3

// arch/x86/kernel/syscall_table_32.S
ENTRY(sys_call_table)
    .long sys_restart_syscall   /* 0 */
    .long sys_exit              /* 1 */
    .long ptregs_fork           /* 2 */
    .long sys_read              /* 3 */  ← sys_read
    .long sys_write             /* 4 */
```

---

## 2. 陷入内核的机制

### 2.1 int $0x80 中断处理

```
用户态执行 int $0x80
         │
         ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                           CPU 硬件自动完成                                    │
│                                                                              │
│  1. 从 TSS 加载内核栈 (SS0:ESP0)                                             │
│  2. 切换到内核栈                                                              │
│  3. 压入用户态现场:                                                           │
│     SS → ESP → EFLAGS → CS → EIP                                            │
│  4. 从 IDT[0x80] 加载中断处理程序                                             │
│  5. 跳转到 system_call 入口                                                   │
│                                                                              │
│  特权级切换: Ring 3 → Ring 0                                                 │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 system_call 入口

```asm
// arch/x86/kernel/entry_32.S:499

ENTRY(system_call)
    RING0_INT_FRAME
    pushl_cfi %eax              # 保存系统调用号 (orig_eax = 3)
    SAVE_ALL                    # 保存所有寄存器到内核栈
    GET_THREAD_INFO(%ebp)       # 获取当前线程信息
    
    # 检查是否需要系统调用跟踪
    testl $_TIF_WORK_SYSCALL_ENTRY, TI_flags(%ebp)
    jnz syscall_trace_entry
    
    # 验证系统调用号
    cmpl $(nr_syscalls), %eax   # eax=3, 检查是否 < 350
    jae syscall_badsys

syscall_call:
    call *sys_call_table(,%eax,4)   # 调用 sys_call_table[3] = sys_read
    movl %eax, PT_EAX(%esp)         # 保存返回值
```

### 2.3 内核栈布局

```
进入 syscall_call 时的内核栈:

高地址
┌───────────────────┐
│  SS (用户态)      │ +0x40
├───────────────────┤
│  ESP (用户态)     │ +0x3C
├───────────────────┤
│  EFLAGS           │ +0x38
├───────────────────┤
│  CS (用户态)      │ +0x34
├───────────────────┤
│  EIP (返回地址)   │ +0x30
├───────────────────┤
│  orig_eax = 3     │ +0x2C  ← 系统调用号
├───────────────────┤
│  gs, fs, es, ds   │ +0x1C ~ +0x28
├───────────────────┤
│  eax = 3          │ +0x18  ← 返回值存放位置
├───────────────────┤
│  ebp = count      │ +0x14  ← (第6参数位置，此处未用)
├───────────────────┤
│  edi              │ +0x10
├───────────────────┤
│  esi              │ +0x0C
├───────────────────┤
│  edx = count      │ +0x08  ← 第3个参数
├───────────────────┤
│  ecx = buf        │ +0x04  ← 第2个参数
├───────────────────┤
│  ebx = fd         │ +0x00  ← 第1个参数 (ESP)
└───────────────────┘
低地址
```

---

## 3. 实际处理函数

### 3.1 sys_read 函数

```c
// fs/read_write.c:460

SYSCALL_DEFINE3(read, unsigned int, fd, char __user *, buf, size_t, count)
{
    struct file *file;
    ssize_t ret = -EBADF;
    int fput_needed;

    // 1. 根据 fd 获取 struct file
    file = fget_light(fd, &fput_needed);
    if (file) {
        // 2. 获取当前文件位置
        loff_t pos = file_pos_read(file);
        
        // 3. 调用 VFS 读取函数
        ret = vfs_read(file, buf, count, &pos);
        
        // 4. 更新文件位置
        file_pos_write(file, pos);
        
        // 5. 释放文件引用
        fput_light(file, fput_needed);
    }

    return ret;
}
```

### 3.2 文件描述符到 struct file

```c
// 从 fd 获取 struct file 的过程
file = fget_light(fd, &fput_needed)
         │
         ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                         文件描述符表查找                                      │
│                                                                              │
│   current->files                                                             │
│       │                                                                      │
│       ▼                                                                      │
│   struct files_struct                                                        │
│       │                                                                      │
│       └── struct fdtable *fdt                                               │
│               │                                                              │
│               └── struct file **fd                                          │
│                       │                                                      │
│                       └── fd[3] ──► struct file (目标文件)                   │
│                                         │                                    │
│                                         ├── f_op (file_operations)          │
│                                         ├── f_mapping (address_space)       │
│                                         ├── f_pos (当前位置)                 │
│                                         └── ...                              │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 3.3 vfs_read 函数

```c
// fs/read_write.c:364

ssize_t vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    ssize_t ret;

    // 1. 检查文件是否可读
    if (!(file->f_mode & FMODE_READ))
        return -EBADF;
    
    // 2. 检查是否有读取方法
    if (!file->f_op || (!file->f_op->read && !file->f_op->aio_read))
        return -EINVAL;
    
    // 3. 验证用户空间缓冲区
    if (unlikely(!access_ok(VERIFY_WRITE, buf, count)))
        return -EFAULT;

    // 4. 验证读取区域 (权限、锁等)
    ret = rw_verify_area(READ, file, pos, count);
    if (ret >= 0) {
        count = ret;
        
        // 5. 调用具体读取方法
        if (file->f_op->read)
            ret = file->f_op->read(file, buf, count, pos);
        else
            ret = do_sync_read(file, buf, count, pos);
        
        // 6. 文件访问通知
        if (ret > 0) {
            fsnotify_access(file);
            add_rchar(current, ret);
        }
        inc_syscr(current);
    }

    return ret;
}
```

### 3.4 调用链路分支

```
vfs_read()
    │
    ├── file->f_op->read 存在?
    │       │
    │       ├── 是 ──► 直接调用 f_op->read()
    │       │            例如: ext4_file_read() 或直接使用 do_sync_read
    │       │
    │       └── 否 ──► 调用 do_sync_read()
    │                       │
    │                       ▼
    │                  初始化 kiocb
    │                       │
    │                       ▼
    │                  f_op->aio_read()
    │                       │
    │                       ▼
    │              generic_file_aio_read()
    │
    └── 对于大多数文件系统 (ext4, ext3, xfs 等):
            最终都会调用 generic_file_aio_read()
```

### 3.5 do_sync_read 函数

```c
// fs/read_write.c:338

ssize_t do_sync_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
    // 1. 设置 iovec 结构
    struct iovec iov = { .iov_base = buf, .iov_len = len };
    struct kiocb kiocb;
    ssize_t ret;

    // 2. 初始化同步 I/O 控制块
    init_sync_kiocb(&kiocb, filp);
    kiocb.ki_pos = *ppos;
    kiocb.ki_left = len;
    kiocb.ki_nbytes = len;

    // 3. 调用异步读取接口 (同步等待完成)
    for (;;) {
        ret = filp->f_op->aio_read(&kiocb, &iov, 1, kiocb.ki_pos);
        if (ret != -EIOCBRETRY)
            break;
        wait_on_retry_sync_kiocb(&kiocb);
    }

    // 4. 等待 I/O 完成
    if (-EIOCBQUEUED == ret)
        ret = wait_on_sync_kiocb(&kiocb);
    
    *ppos = kiocb.ki_pos;
    return ret;
}
```

### 3.6 generic_file_aio_read 函数

```c
// mm/filemap.c:1409

ssize_t generic_file_aio_read(struct kiocb *iocb, const struct iovec *iov,
                              unsigned long nr_segs, loff_t pos)
{
    struct file *filp = iocb->ki_filp;
    ssize_t retval;
    size_t count;
    loff_t *ppos = &iocb->ki_pos;

    // 1. 检查 iovec 参数
    retval = generic_segment_checks(iov, &nr_segs, &count, VERIFY_WRITE);
    if (retval)
        return retval;

    // 2. O_DIRECT 直接 I/O 路径
    if (filp->f_flags & O_DIRECT) {
        struct address_space *mapping = filp->f_mapping;
        struct inode *inode = mapping->host;
        loff_t size = i_size_read(inode);
        
        if (pos < size) {
            // 刷新脏页
            retval = filemap_write_and_wait_range(mapping, pos, ...);
            if (!retval) {
                // 直接调用块设备 I/O
                retval = mapping->a_ops->direct_IO(READ, iocb, iov, pos, nr_segs);
            }
        }
        // ... 处理读取结果
    }

    // 3. 缓冲 I/O 路径 (普通读取)
    for (seg = 0; seg < nr_segs; seg++) {
        read_descriptor_t desc;
        desc.arg.buf = iov[seg].iov_base;
        desc.count = iov[seg].iov_len;
        
        // 调用核心读取函数
        do_generic_file_read(filp, ppos, &desc, file_read_actor);
        
        retval += desc.written;
    }

    return retval;
}
```

### 3.7 do_generic_file_read - 页缓存核心

```c
// mm/filemap.c:1104

static void do_generic_file_read(struct file *filp, loff_t *ppos,
                                 read_descriptor_t *desc, read_actor_t actor)
{
    struct address_space *mapping = filp->f_mapping;
    struct inode *inode = mapping->host;
    struct file_ra_state *ra = &filp->f_ra;
    pgoff_t index;          // 页索引
    unsigned long offset;   // 页内偏移

    // 计算起始页和偏移
    index = *ppos >> PAGE_CACHE_SHIFT;      // *ppos / 4096
    offset = *ppos & ~PAGE_CACHE_MASK;      // *ppos % 4096

    for (;;) {
        struct page *page;
        unsigned long nr, ret;

        cond_resched();  // 调度点

find_page:
        // ===== 1. 在页缓存中查找页面 =====
        page = find_get_page(mapping, index);
        
        if (!page) {
            // ===== 2. 页面不存在，触发同步预读 =====
            page_cache_sync_readahead(mapping, ra, filp,
                                      index, last_index - index);
            page = find_get_page(mapping, index);
            if (unlikely(page == NULL))
                goto no_cached_page;
        }

        // ===== 3. 检查是否需要异步预读 =====
        if (PageReadahead(page)) {
            page_cache_async_readahead(mapping, ra, filp, page,
                                       index, last_index - index);
        }

        // ===== 4. 检查页面数据是否有效 =====
        if (!PageUptodate(page)) {
            goto page_not_up_to_date;
        }

page_ok:
        // ===== 5. 页面有效，复制数据到用户空间 =====
        isize = i_size_read(inode);
        
        // 计算要复制的字节数
        nr = PAGE_CACHE_SIZE - offset;  // 本页剩余
        if (nr > desc->count)
            nr = desc->count;
        
        // 调用 actor 函数复制数据
        ret = actor(desc, page, offset, nr);
        // actor = file_read_actor → copy_to_user()
        
        offset += ret;
        index += offset >> PAGE_CACHE_SHIFT;
        offset &= ~PAGE_CACHE_MASK;
        
        page_cache_release(page);
        
        if (desc->count == 0)
            break;  // 读取完成
        
        continue;  // 继续下一页

page_not_up_to_date:
        // ===== 6. 页面数据无效，需要从磁盘读取 =====
        lock_page(page);
        
        if (!page->mapping) {
            // 页面被截断
            unlock_page(page);
            page_cache_release(page);
            goto find_page;
        }

readpage:
        // 调用文件系统的 readpage 方法
        error = mapping->a_ops->readpage(filp, page);
        // 例如: ext4_readpage → mpage_readpage → submit_bio
        
        if (!error) {
            wait_on_page_locked(page);  // 等待 I/O 完成
            if (PageUptodate(page))
                goto page_ok;
        }
        // ... 错误处理

no_cached_page:
        // ===== 7. 分配新页面并读取 =====
        page = page_cache_alloc_cold(mapping);
        error = add_to_page_cache_lru(page, mapping, index, GFP_KERNEL);
        if (error == 0)
            goto readpage;
    }

out:
    // 更新预读状态
    ra->prev_pos = *ppos;
    *ppos = ((loff_t)index << PAGE_CACHE_SHIFT) + offset;
}
```

### 3.8 页缓存查找与 I/O 流程

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          页缓存读取流程                                       │
└─────────────────────────────────────────────────────────────────────────────┘

do_generic_file_read()
         │
         ▼
find_get_page(mapping, index)
         │
         ├── 页面存在且有效 (PageUptodate)
         │       │
         │       ▼
         │   file_read_actor()
         │       │
         │       ▼
         │   copy_to_user(buf, page_data, nr)  ──► 返回
         │
         │
         ├── 页面存在但无效
         │       │
         │       ▼
         │   lock_page(page)
         │       │
         │       ▼
         │   mapping->a_ops->readpage(file, page)
         │       │
         │       ▼
         │   ┌─────────────────────────────────────────┐
         │   │          磁盘 I/O 路径                  │
         │   │                                         │
         │   │  readpage() (e.g., ext4_readpage)      │
         │   │       │                                 │
         │   │       ▼                                 │
         │   │  mpage_readpage()                      │
         │   │       │                                 │
         │   │       ▼                                 │
         │   │  submit_bio(READ, bio)                 │
         │   │       │                                 │
         │   │       ▼                                 │
         │   │  块设备层 → 磁盘驱动 → 硬件            │
         │   │       │                                 │
         │   │       ▼                                 │
         │   │  中断 → bio 完成回调                    │
         │   │       │                                 │
         │   │       ▼                                 │
         │   │  SetPageUptodate(page)                 │
         │   │  unlock_page(page)                     │
         │   └─────────────────────────────────────────┘
         │       │
         │       ▼
         │   wait_on_page_locked(page)  ── 等待 I/O
         │       │
         │       ▼
         │   copy_to_user()  ──► 返回
         │
         │
         └── 页面不存在
                 │
                 ▼
         page_cache_sync_readahead()  ── 触发预读
                 │
                 ▼
         分配页面并加入缓存
                 │
                 ▼
         goto readpage
```

### 3.9 copy_to_user - 数据复制

```c
// file_read_actor 函数
static int file_read_actor(read_descriptor_t *desc, struct page *page,
                           unsigned long offset, unsigned long size)
{
    char *kaddr;
    unsigned long left, count = desc->count;

    if (size > count)
        size = count;

    // 映射页面到内核地址空间
    kaddr = kmap_atomic(page, KM_USER0);
    
    // 复制数据到用户空间
    left = __copy_to_user(desc->arg.buf, kaddr + offset, size);
    
    kunmap_atomic(kaddr, KM_USER0);

    if (left) {
        size -= left;
        desc->error = -EFAULT;
    }
    
    desc->count -= size;
    desc->written += size;
    desc->arg.buf += size;
    
    return size;
}
```

---

## 4. 返回用户态的过程

### 4.1 返回值传递

```c
// sys_read 返回
SYSCALL_DEFINE3(read, ...)
{
    ...
    ret = vfs_read(file, buf, count, &pos);
    ...
    return ret;  // 返回读取的字节数或错误码
}
```

### 4.2 syscall_exit 处理

```asm
// arch/x86/kernel/entry_32.S

syscall_call:
    call *sys_call_table(,%eax,4)   # 调用 sys_read
    movl %eax, PT_EAX(%esp)         # 保存返回值到栈上

syscall_exit:
    LOCKDEP_SYS_EXIT
    DISABLE_INTERRUPTS(CLBR_ANY)    # 关中断
    TRACE_IRQS_OFF
    
    # 检查是否有待处理工作
    movl TI_flags(%ebp), %ecx
    testl $_TIF_ALLWORK_MASK, %ecx
    jne syscall_exit_work           # 有信号/调度等待处理
    
restore_all:
    TRACE_IRQS_IRET

restore_all_notrace:
    RESTORE_REGS 4                  # 恢复所有寄存器
                                    # eax = 返回值 (读取字节数)

irq_return:
    INTERRUPT_RETURN                # iret 返回用户态
```

### 4.3 iret 返回

```
iret 指令执行:
         │
         ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                           CPU 硬件自动完成                                    │
│                                                                              │
│  1. 从内核栈弹出: EIP, CS, EFLAGS                                            │
│  2. 检测到特权级切换 (Ring 0 → Ring 3)                                       │
│  3. 从内核栈弹出: ESP, SS                                                    │
│  4. 切换到用户栈                                                              │
│  5. 跳转到用户态 EIP 继续执行                                                 │
│                                                                              │
│  返回时: %eax = 读取的字节数 (或负数错误码)                                   │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
         │
         ▼
    用户程序继续执行
```

---

## 完整时序图

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                   read(fd, buf, 4096) 完整时序图                             │
└─────────────────────────────────────────────────────────────────────────────┘

用户态                                内核态
───────────────────────────────────────────────────────────────────────────────
   │
   │ read(fd=3, buf=0x7fff1000, count=4096)
   │
   ▼
┌──────────────────┐
│ glibc: read()    │
│ mov $3, %eax     │
│ mov $3, %ebx     │  (fd)
│ mov buf, %ecx    │
│ mov 4096, %edx   │
│ int $0x80        │ ────────────────────────────────────────┐
└──────────────────┘                                          │
                                                              ▼
                                   ┌──────────────────────────────────────────┐
                                   │ 1. CPU: 保存用户态现场到内核栈             │
                                   │    切换到 Ring 0                         │
                                   └──────────────────────────────────────────┘
                                                              │
                                                              ▼
                                   ┌──────────────────────────────────────────┐
                                   │ 2. system_call:                          │
                                   │    SAVE_ALL                              │
                                   │    call sys_read                         │
                                   └──────────────────────────────────────────┘
                                                              │
                                                              ▼
                                   ┌──────────────────────────────────────────┐
                                   │ 3. sys_read(fd=3, buf, count=4096):      │
                                   │    file = fget_light(3)                  │
                                   │    pos = file->f_pos                     │
                                   └──────────────────────────────────────────┘
                                                              │
                                                              ▼
                                   ┌──────────────────────────────────────────┐
                                   │ 4. vfs_read(file, buf, 4096, &pos):      │
                                   │    检查权限                               │
                                   │    调用 f_op->read 或 do_sync_read       │
                                   └──────────────────────────────────────────┘
                                                              │
                                                              ▼
                                   ┌──────────────────────────────────────────┐
                                   │ 5. do_sync_read():                       │
                                   │    初始化 kiocb                          │
                                   │    调用 f_op->aio_read()                 │
                                   └──────────────────────────────────────────┘
                                                              │
                                                              ▼
                                   ┌──────────────────────────────────────────┐
                                   │ 6. generic_file_aio_read():              │
                                   │    调用 do_generic_file_read()           │
                                   └──────────────────────────────────────────┘
                                                              │
                                                              ▼
                                   ┌──────────────────────────────────────────┐
                                   │ 7. do_generic_file_read():               │
                                   │    index = pos / 4096 = 页号             │
                                   │    offset = pos % 4096 = 页内偏移        │
                                   └──────────────────────────────────────────┘
                                                              │
                                          ┌───────────────────┴───────────────────┐
                                          │                                       │
                                          ▼                                       ▼
                           ┌─────────────────────────┐           ┌─────────────────────────┐
                           │ 8a. 页缓存命中           │           │ 8b. 页缓存未命中        │
                           │                         │           │                         │
                           │ page = find_get_page()  │           │ readpage() 触发磁盘 I/O │
                           │ PageUptodate? 是       │           │ submit_bio()            │
                           └───────────┬─────────────┘           │ wait_on_page_locked()   │
                                       │                         └───────────┬─────────────┘
                                       │                                     │
                                       └───────────────────┬─────────────────┘
                                                           │
                                                           ▼
                                   ┌──────────────────────────────────────────┐
                                   │ 9. file_read_actor():                    │
                                   │    kmap_atomic(page)                     │
                                   │    copy_to_user(buf, page_data, nr)      │
                                   │    kunmap_atomic()                       │
                                   └──────────────────────────────────────────┘
                                                              │
                                                              ▼
                                   ┌──────────────────────────────────────────┐
                                   │ 10. 返回路径:                             │
                                   │    do_generic_file_read 返回              │
                                   │    generic_file_aio_read 返回             │
                                   │    do_sync_read 返回                      │
                                   │    vfs_read 返回 (4096)                   │
                                   │    sys_read 返回 (4096)                   │
                                   └──────────────────────────────────────────┘
                                                              │
                                                              ▼
                                   ┌──────────────────────────────────────────┐
                                   │ 11. syscall_exit:                        │
                                   │    movl %eax, PT_EAX(%esp)  # 保存4096   │
                                   │    检查 TIF_ALLWORK_MASK                 │
                                   └──────────────────────────────────────────┘
                                                              │
                                                              ▼
                                   ┌──────────────────────────────────────────┐
                                   │ 12. restore_all:                         │
                                   │    RESTORE_REGS                          │
                                   │    iret                                  │
┌──────────────────┐               └──────────────────────────────────────────┘
│ %eax = 4096      │◄─────────────────────────────────────────┘
│ (读取的字节数)    │
│ buf 已填充数据    │
└──────────────────┘
   │
   ▼
继续执行用户程序
```

---

## 调用栈总结

```
用户态:
    read(fd, buf, count)
        │
        ▼
内核态:
    system_call                     [arch/x86/kernel/entry_32.S]
        │
        ▼
    sys_read(fd, buf, count)        [fs/read_write.c:460]
        │
        ├── fget_light(fd)          [获取 struct file]
        │
        └── vfs_read()              [fs/read_write.c:364]
                │
                ├── rw_verify_area() [权限检查]
                │
                └── do_sync_read()  [fs/read_write.c:338]
                        │
                        └── f_op->aio_read()
                                │
                                ▼
                    generic_file_aio_read()  [mm/filemap.c:1409]
                                │
                                └── do_generic_file_read() [mm/filemap.c:1104]
                                        │
                                        ├── find_get_page()     [页缓存查找]
                                        │
                                        ├── readpage()          [如需磁盘I/O]
                                        │       │
                                        │       └── submit_bio() [提交块I/O]
                                        │
                                        └── file_read_actor()
                                                │
                                                └── copy_to_user() [复制到用户空间]
```

---

## 关键源码位置

| 组件 | 文件 | 函数/行号 |
|------|------|-----------|
| 系统调用号 | `arch/x86/include/asm/unistd_32.h` | `__NR_read = 3` |
| 系统调用表 | `arch/x86/kernel/syscall_table_32.S` | 第 5 行 |
| 入口代码 | `arch/x86/kernel/entry_32.S` | `system_call` 第 499 行 |
| sys_read | `fs/read_write.c` | 第 460-475 行 |
| vfs_read | `fs/read_write.c` | 第 364-390 行 |
| do_sync_read | `fs/read_write.c` | 第 338-361 行 |
| generic_file_aio_read | `mm/filemap.c` | 第 1409-1500 行 |
| do_generic_file_read | `mm/filemap.c` | 第 1104-1280 行 |
| 页缓存查找 | `mm/filemap.c` | `find_get_page()` |
| 数据复制 | `arch/x86/lib/usercopy_32.c` | `copy_to_user()` |

---

## 关键数据结构

```c
// 文件对象
struct file {
    struct path         f_path;       // 文件路径
    const struct file_operations *f_op; // 操作函数表
    struct address_space *f_mapping;  // 页缓存
    loff_t              f_pos;        // 当前位置
    struct file_ra_state f_ra;        // 预读状态
    // ...
};

// 文件操作表 (普通文件)
const struct file_operations ext4_file_operations = {
    .read       = do_sync_read,
    .write      = do_sync_write,
    .aio_read   = generic_file_aio_read,
    .aio_write  = generic_file_aio_write,
    .mmap       = ext4_file_mmap,
    // ...
};

// 地址空间 (页缓存)
struct address_space {
    struct inode        *host;        // 所属 inode
    struct radix_tree_root page_tree; // 页面基数树
    const struct address_space_operations *a_ops;
    // ...
};
```

---

*本文档基于 Linux 3.2 内核源码分析*

