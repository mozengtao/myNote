/*
场景说明
内核的 PID 管理器负责全局 PID 的分配与回收，系统中仅能有一个实例，否则会导致 PID 重复分配，进程标识混乱。内核通过pid_max、next_pid等全局单例变量实现

核心说明
内核采用饿汉式单例：PID 管理器在系统启动时就初始化（全局变量静态初始化），无需懒加载；
自旋锁替代互斥锁：内核态无线程调度，自旋锁更高效；
全局唯一：pid_mgr为static全局变量，内核所有模块仅能通过pid_manager_get()访问。
*/

#include <stdio.h>
#include <linux/spinlock.h> // 内核自旋锁（替代pthread_mutex）

// 内核自旋锁定义（模拟）
typedef struct { int locked; } spinlock_t;
#define spin_lock(lock) (lock)->locked = 1
#define spin_unlock(lock) (lock)->locked = 0
#define SPIN_LOCK_UNLOCKED {0}

// 1. PID管理器单例结构体（全局唯一）
typedef struct {
    unsigned int next_pid;    // 下一个待分配的PID
    unsigned int pid_max;     // 最大PID值（默认32768）
    spinlock_t lock;          // 内核自旋锁（多核安全）
} PIDManager;

// 2. 静态全局单例实例（饿汉式：内核启动时初始化）
static PIDManager pid_mgr = {
    .next_pid = 1000,         // 跳过0-999的系统进程PID
    .pid_max = 32768,
    .lock = SPIN_LOCK_UNLOCKED
};

// 3. 唯一访问接口：获取PID管理器实例（饿汉式无需懒加载）
static inline PIDManager* pid_manager_get(void) {
    return &pid_mgr; // 直接返回全局单例
}

// 4. 业务接口：分配PID（内核核心逻辑）
unsigned int alloc_pid(void) {
    PIDManager *mgr = pid_manager_get();
    unsigned int pid;
    // 内核自旋锁保护（多核安全）
    spin_lock(&mgr->lock);
    if (mgr->next_pid >= mgr->pid_max) {
        mgr->next_pid = 1000; // 循环分配
    }
    pid = mgr->next_pid++;
    spin_unlock(&mgr->lock);
    printf("内核分配PID：%u\n", pid);
    return pid;
}

// 测试
int main() {
    printf("PID管理器实例地址：%p\n", pid_manager_get());
    alloc_pid();
    alloc_pid();
    alloc_pid();
    return 0;
}