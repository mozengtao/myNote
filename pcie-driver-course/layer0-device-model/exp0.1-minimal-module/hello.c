// hello.c
// 最小内核模块：只做一件事——证明 module_init/module_exit 被正确调用

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

/*
 * hello_init - 模块加载时的入口函数
 *
 * 由 insmod/modprobe 触发。返回值的含义：
 *   0      : 初始化成功，模块进入 LOADED 状态
 *   非 0   : 初始化失败，内核会立刻卸载模块（不会调用 exit 函数）
 */
static int __init hello_init(void)
{
	pr_info("hello: module_init() called, module is now LOADED\n");
	return 0;
}

/*
 * hello_exit - 模块卸载时的出口函数
 *
 * 由 rmmod 触发。只有成功进入过 LOADED 状态的模块才会执行到这里。
 */
static void __exit hello_exit(void)
{
	pr_info("hello: module_exit() called, module is now UNLOADING\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PCIe Driver Course");
MODULE_DESCRIPTION("Experiment 0.1: minimal kernel module lifecycle");
