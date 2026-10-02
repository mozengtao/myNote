// platform_demo.c
// 用 platform bus 演示 Device Model 的 device/driver matching 机制
// 这不是一个真实硬件驱动——device 和 driver 都在同一个模块里手工注册，
// 只是为了单独观察 "matching 触发 probe()" 这件事本身。

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>

#define DEV_NAME "my_pdev"

static struct platform_device *my_pdev;

/*
 * my_probe - bus 完成 matching 后调用的函数
 *
 * 对应 Mental Model 里的: Device + Driver --match--> probe()
 * 这里只打印一条日志，证明 "matching -> probe()" 这条链路真的发生了。
 */
static int my_probe(struct platform_device *pdev)
{
	pr_info("platform_demo: my_probe() called for device '%s' -> MATCHED!\n",
		pdev->name);
	return 0;
}

/*
 * my_remove - driver 被卸载 (或 device 被移除) 时调用
 *
 * 注意: Linux 6.x 的 struct platform_driver.remove 返回值已经是 void
 * （早期版本是 int，返回值其实从来没被内核认真处理过，所以干脆改成 void）。
 */
static void my_remove(struct platform_device *pdev)
{
	pr_info("platform_demo: my_remove() called for device '%s'\n",
		pdev->name);
}

static struct platform_driver my_driver = {
	.driver = {
		.name = DEV_NAME,   /* <-- matching 时比较的就是这个字符串 */
		.owner = THIS_MODULE,
	},
	.probe  = my_probe,
	.remove = my_remove,
};

static int __init platform_demo_init(void)
{
	int ret;

	pr_info("platform_demo: registering platform_device '%s'\n", DEV_NAME);

	/* 第一步：注册一个 device。此刻还没有对应的 driver，bus 上没有 matching 发生 */
	my_pdev = platform_device_register_simple(DEV_NAME, -1, NULL, 0);
	if (IS_ERR(my_pdev))
		return PTR_ERR(my_pdev);

	pr_info("platform_demo: registering platform_driver '%s'\n", DEV_NAME);

	/* 第二步：注册 driver。此刻 bus 发现名字匹配的 device 已经存在 -> 触发 probe() */
	ret = platform_driver_register(&my_driver);
	if (ret) {
		platform_device_unregister(my_pdev);
		return ret;
	}

	return 0;
}

static void __exit platform_demo_exit(void)
{
	platform_driver_unregister(&my_driver);   /* 触发 my_remove() */
	platform_device_unregister(my_pdev);
}

module_init(platform_demo_init);
module_exit(platform_demo_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Experiment 0.2: device/driver matching on platform bus");
