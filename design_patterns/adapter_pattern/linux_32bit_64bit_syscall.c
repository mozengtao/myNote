/*
场景说明
64 位 Linux 内核需要兼容 32 位用户态进程的系统调用：32 位进程的系统调用参数、寄存器使用、返回值格式与 64 位内核的原生系统调用接口不兼容。内核通过ia32_sys_call_table适配器表，将 32 位系统调用接口转换为 64 位内核的原生接口，实现 32 位进程的无缝运行。

核心说明
32 位系统调用表ia32_sys_call_table是适配器层，每个 32 位系统调用对应一个适配器函数；
适配器函数完成 “32 位参数→64 位参数” 的转换，调用 64 位内核原生系统调用；
32 位进程无需感知内核位数，适配器层屏蔽了接口差异，实现了跨位兼容。
*/

#include <stdio.h>

// 模拟64位内核原生系统调用接口（目标接口）
typedef long (*sys_call_64_t)(unsigned long arg1, unsigned long arg2);

// 模拟32位系统调用适配器接口（被适配者接口）
typedef int (*sys_call_32_t)(unsigned int arg1, unsigned int arg2);

// ---------------------- 被适配者：64位内核原生系统调用 ----------------------
// 64位原生read系统调用
long sys_read_64(unsigned long fd, unsigned long buf, unsigned long count) {
    printf("[64位原生sys_read] fd=%ld, buf=%p, count=%ld\n", fd, (void*)buf, count);
    return count; // 模拟读取count字节
}

// ---------------------- 适配器：32位调用适配64位原生接口 ----------------------
// 32位read系统调用适配器：转换参数类型，调用64位原生接口
int sys_read_32(unsigned int fd, unsigned int buf, unsigned int count) {
    // 接口转换：32位参数 → 64位参数
    return (int)sys_read_64((unsigned long)fd, (unsigned long)buf, (unsigned long)count);
}

// 32位系统调用表（适配器表）
static sys_call_32_t ia32_sys_call_table[] = {
    NULL,           // 0: 无效调用
    (sys_call_32_t)sys_read_32, // 3: read调用（适配器）
};

// ---------------------- 内核系统调用入口：适配32位进程 ----------------------
int ia32_syscall_handler(int nr, unsigned int arg1, unsigned int arg2, unsigned int arg3) {
    if (nr < 0 || nr >= sizeof(ia32_sys_call_table)/sizeof(ia32_sys_call_table[0])) {
        return -1;
    }
    printf("\n[32位系统调用适配器] 处理调用号%d\n", nr);
    return ia32_sys_call_table[nr](arg1, arg2); // 调用适配器
}

// 主函数测试（模拟32位进程调用read）
int main() {
    // 32位进程调用read系统调用（调用号3，参数为32位）
    ia32_syscall_handler(3, 0, 0x10000, 128);
    return 0;
}