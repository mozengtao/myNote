/*
 * 06_lookup_or_create.c
 *
 * 场景：网络流量分析系统第一次看到一个"设备"(以整数 key 标识，
 * 可以想象成压缩后的 MAC/IP)时，需要给它建一条记录；之后每次
 * 看到同一个 key，只需要找到已有记录并更新统计信息。
 *
 * 模式：查找或创建（get-or-create / 幂等分配）。把"look 一下，没有
 * 就 alloc"这个高频调用序列收编进同一个函数签名 lookup(key, create)，
 * 用一个 flag 显式表达调用意图，调用方不需要自己拼接两次调用、也
 * 不用处理"look 完到 alloc 之间可能发生了什么"的中间状态。
 *
 * 编译： gcc -std=c11 -Wall -Wextra -O2 -o /tmp/ex6 06_lookup_or_create.c
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
	NDEVHASH = 16,
};

typedef struct Device Device;

struct Device {
	int     key;
	long    seen_count;
	Device *next;
};

static Device *devhash[NDEVHASH];

static unsigned
devhashfn(int key)
{
	return (unsigned)key & (NDEVHASH - 1);
}

/*
 * device_lookup - 查找或创建
 *
 * create == 0：纯查找，找不到返回 NULL，不产生任何副作用。
 * create != 0：找不到就分配一个新对象并插入哈希表；如果已经存在，
 *              直接返回已有对象——多次以同一个 key、create=1 调用
 *              是幂等的，绝不会创建出第二个对象。
 */
Device*
device_lookup(int key, int create)
{
	Device *d, **pp;

	for(d = devhash[devhashfn(key)]; d != NULL; d = d->next) {
		if(d->key == key)
			return d;
	}

	if(!create)
		return NULL;

	d = malloc(sizeof(*d));
	if(d == NULL)
		return NULL;

	memset(d, 0, sizeof(*d));
	d->key = key;

	pp = &devhash[devhashfn(key)];
	d->next = *pp;
	*pp = d;

	return d;
}

/*
 * device_free - 按 key 摘除并释放，与 alloc/free/lookup 三元组保持一致
 */
int
device_free(int key)
{
	Device *f, **pp;

	pp = &devhash[devhashfn(key)];
	for(f = *pp; f != NULL; f = f->next) {
		if(f->key == key) {
			*pp = f->next;
			free(f);
			return 0;
		}
		pp = &f->next;
	}

	return -1;
}

int
main(void)
{
	Device *d1, *d2, *d3;
	int i;

	/* 第一次看到 key=42：create=1，应该新建一条记录 */
	d1 = device_lookup(42, 1);
	assert(d1 != NULL);
	d1->seen_count++;

	/* 模拟同一个设备后续又被看到 9 次：每次都是 create=1，但不应该重复创建 */
	for(i = 0; i < 9; i++) {
		d2 = device_lookup(42, 1);
		assert(d2 == d1);	/* 幂等：拿到的是同一个对象 */
		d2->seen_count++;
	}
	assert(d1->seen_count == 10);
	printf("device 42 seen_count=%ld after 10 lookups with create=1\n", d1->seen_count);

	/* create=0 是纯查找：不存在的 key 必须返回 NULL，不能悄悄创建 */
	assert(device_lookup(99, 0) == NULL);

	/* 现在显式创建 key=99 */
	d3 = device_lookup(99, 1);
	assert(d3 != NULL);
	assert(device_lookup(99, 0) == d3);

	assert(device_free(42) == 0);
	assert(device_lookup(42, 0) == NULL);
	assert(device_free(42) == -1);	/* 重复释放应该失败 */

	assert(device_free(99) == 0);

	printf("06_lookup_or_create: all assertions passed\n");
	return 0;
}

/*
 * 心智模型
 *
 * 1. "查找即创建"是一种意图声明：create 参数把"调用方到底想不想在
 *    找不到时创建一个新对象"这件事，显式放进了函数签名里，而不是让
 *    调用方自己在外面拼接 if(!look()) alloc()，从而消灭了两次调用
 *    之间可能出现的状态空档（例如另一个事件先创建了同一个 key）。
 * 2. 幂等性是这个接口最重要的契约：不管调用多少次 create=1，只要
 *    key 相同，永远只会存在一个对象——这让"高频重复触发"的调用方
 *    （例如收到一个数据包就调一次）完全不需要自己维护"是否已创建"
 *    的状态。
 */
