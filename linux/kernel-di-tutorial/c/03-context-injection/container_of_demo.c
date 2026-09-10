/*
 * container_of_demo.c
 *
 * 场景：手动实现 Kernel 的 container_of() 宏，并演示它如何解决
 * "Core 只拿到一个通用字段的指针，如何找回它所属的具体对象" 的问题。
 *
 * 这是 Context Injection 最底层的机制：Kernel 里几乎所有"从内嵌
 * 结构体指针反推外层对象"的场景都靠这一个宏（include/linux/kernel.h:659）。
 *
 *   struct device {
 *       ...
 *       struct kobject kobj;     <-- Core 只认识/操作这个内嵌字段
 *   };
 *
 *   struct kobject *kobj = ...;                     // Core 拿到的只是这个
 *   struct device *dev = container_of(kobj, struct device, kobj);  // 反推回外层对象
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -o container_of_demo container_of_demo.c
 * 运行：
 *   ./container_of_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>   /* offsetof */

/* ------------------------------------------------------------------ */
/* 1. 手写 container_of()，与 include/linux/kernel.h:659 完全同构      */
/* ------------------------------------------------------------------ */

#define container_of(ptr, type, member) ({			\
	const typeof(((type *)0)->member) *__mptr = (ptr);	\
	(type *)((char *)__mptr - offsetof(type, member)); })

/* ------------------------------------------------------------------ */
/* 2. Core 只认识的"通用嵌入字段"：类似 struct list_head / struct kobject */
/* ------------------------------------------------------------------ */

struct list_node {
	struct list_node *next;
	struct list_node *prev;
};

/* 一个极简双向链表，Core（list_*函数）只操作 struct list_node，
 * 完全不知道外层挂的是什么真实对象 —— 这正是 Kernel struct list_head 的设计 */

static void list_init(struct list_node *head)
{
	head->next = head;
	head->prev = head;
}

static void list_add_tail(struct list_node *head, struct list_node *n)
{
	n->prev = head->prev;
	n->next = head;
	head->prev->next = n;
	head->prev = n;
}

static void list_del(struct list_node *n)
{
	n->prev->next = n->next;
	n->next->prev = n->prev;
	n->next = n->prev = NULL;
}

#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

/* ------------------------------------------------------------------ */
/* 3. 具体对象 A：任务（task），把 list_node 嵌入自身                  */
/* ------------------------------------------------------------------ */

struct task {
	int id;
	char name[32];
	int priority;
	struct list_node node;   /* <-- 内嵌字段，Core 只操作这个 */
};

static struct task *task_from_node(struct list_node *n)
{
	/* container_of 的典型用法：从内嵌字段指针反推外层对象指针 */
	return container_of(n, struct task, node);
}

/* ------------------------------------------------------------------ */
/* 4. 具体对象 B：设备（device），演示同一个宏用在不同类型上            */
/* ------------------------------------------------------------------ */

struct device {
	char bus_name[32];
	int  vendor_id;
	struct list_node node;
};

static struct device *device_from_node(struct list_node *n)
{
	return container_of(n, struct device, node);
}

/* ------------------------------------------------------------------ */
/* 5. Core 逻辑：调度队列，只操作 list_node，靠 container_of 找回真实对象 */
/* ------------------------------------------------------------------ */

static void run_task_queue(struct list_node *head)
{
	struct list_node *pos;
	printf("[core] scanning task queue via generic list_node...\n");
	list_for_each(pos, head) {
		/* Core 手上只有一个 struct list_node*，
		 * 靠 container_of 才能拿到 task->name / task->priority */
		struct task *t = task_from_node(pos);
		printf("  [core] running task id=%d name='%s' priority=%d (node@%p task@%p, offset=%zu)\n",
		       t->id, t->name, t->priority, (void *)pos, (void *)t,
		       (size_t)((char *)pos - (char *)t));
	}
}

static void scan_device_list(struct list_node *head)
{
	struct list_node *pos;
	printf("[core] scanning device list via the *same* generic list_node code...\n");
	list_for_each(pos, head) {
		struct device *d = device_from_node(pos);
		printf("  [core] device bus='%s' vendor_id=0x%04x\n", d->bus_name, d->vendor_id);
	}
}

int main(void)
{
	struct list_node task_head, dev_head;
	struct task t1 = { .id = 1, .name = "flush-cache",  .priority = 5 };
	struct task t2 = { .id = 2, .name = "sync-disk",    .priority = 1 };
	struct task t3 = { .id = 3, .name = "network-poll", .priority = 9 };
	struct device d1 = { .bus_name = "pci", .vendor_id = 0x8086 };
	struct device d2 = { .bus_name = "usb", .vendor_id = 0x1d6b };

	printf("=== container_of_demo ===\n\n");

	list_init(&task_head);
	list_add_tail(&task_head, &t1.node);
	list_add_tail(&task_head, &t2.node);
	list_add_tail(&task_head, &t3.node);

	list_init(&dev_head);
	list_add_tail(&dev_head, &d1.node);
	list_add_tail(&dev_head, &d2.node);

	run_task_queue(&task_head);
	printf("\n");
	scan_device_list(&dev_head);

	printf("\n-- 演示：从队列中移除 t2，验证链表结构完整性 --\n");
	list_del(&t2.node);
	run_task_queue(&task_head);

	printf("\n=== done: 同一份 list_node 遍历代码 (Core)，\n"
	       "    通过 container_of() 服务了两种完全不同的对象类型 (task/device) ===\n");
	return 0;
}
