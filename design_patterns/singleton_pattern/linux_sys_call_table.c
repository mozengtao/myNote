/*
场景说明
sys_call_table是内核中全局唯一的系统调用入口表，存储所有系统调用（如sys_read、sys_write）的函数指针，所有进程的系统调用均通过该表转发，仅能有一个实例

核心说明
sys_call_table是内核只读的静态全局数组（单例），编译时初始化；
所有系统调用的转发逻辑均依赖该表，保证全局一致性；
内核通过write_cr0修改 CR0 寄存器关闭写保护，才能修改该表（如系统调用劫持），进一步体现其 “唯一且受保护” 的单例特性。
*/

#include <stdio.h>

// 1. 系统调用函数指针类型（模拟）
typedef int (*sys_call_func)(void);

// 2. 系统调用表单例（全局唯一，饿汉式）
static sys_call_func sys_call_table[] = {
    NULL,               // 0: 无效系统调用
    (sys_call_func)1,   // 1: sys_exit
    (sys_call_func)2,   // 2: sys_fork
    (sys_call_func)3,   // 3: sys_read
    (sys_call_func)4    // 4: sys_write
};

// 3. 唯一访问接口：获取系统调用表实例
static inline sys_call_func* get_sys_call_table(void) {
    return sys_call_table;
}

// 4. 业务接口：执行系统调用
int do_syscall(int nr) {
    sys_call_func *table = get_sys_call_table();
    if (nr < 0 || nr >= sizeof(table)/sizeof(table[0])) {
        return -1; // 无效系统调用号
    }
    printf("执行系统调用 %d，入口地址：%p\n", nr, table[nr]);
    return 0;
}

// 测试
int main() {
    printf("系统调用表实例地址：%p\n", get_sys_call_table());
    do_syscall(3); // 执行sys_read
    do_syscall(4); // 执行sys_write
    return 0;
}