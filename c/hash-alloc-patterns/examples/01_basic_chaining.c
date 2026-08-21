/*
 * 01_basic_chaining.c
 *
 * 场景：按订单号(order id)管理一批"订单"记录。
 *
 * 模式：最基础的分离链接哈希表（separate chaining），每次 alloc 单独
 * malloc 一个节点，节点自带链表指针（侵入式链表），插入用头插法，
 * 删除用二级指针惯用法，不需要对"删除的是不是头节点"做特判。
 *
 * 编译： gcc -std=c11 -Wall -Wextra -O2 -o /tmp/ex1 01_basic_chaining.c
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
	NBUCKET = 16,	/* 必须是 2 的幂，才能用位运算代替取模 */
};

typedef struct Order Order;

struct Order {
	int    id;
	char   customer[32];
	double amount;
	Order *next;	/* 侵入式链表指针，就是哈希桶链的 next */
};

static Order *bucket[NBUCKET];

static unsigned
orderhash(int id)
{
	return (unsigned)id & (NBUCKET - 1);
}

/*
 * orderalloc - 分配一个新订单并插入哈希表
 */
Order*
orderalloc(int id, const char *customer, double amount)
{
	Order *o, **pp;

	o = malloc(sizeof(*o));
	if(o == NULL)
		return NULL;

	memset(o, 0, sizeof(*o));
	o->id = id;
	snprintf(o->customer, sizeof(o->customer), "%s", customer);
	o->amount = amount;

	pp = &bucket[orderhash(id)];
	o->next = *pp;
	*pp = o;

	return o;
}

/*
 * orderlook - 按订单号查找
 */
Order*
orderlook(int id)
{
	Order *o;

	for(o = bucket[orderhash(id)]; o != NULL; o = o->next) {
		if(o->id == id)
			break;
	}

	return o;
}

/*
 * orderfree - 从哈希表摘除并释放
 *
 * 返回 0 表示成功，-1 表示没有在表里找到（例如重复释放）
 */
int
orderfree(Order *o)
{
	Order *f, **pp;

	if(o == NULL)
		return 0;

	pp = &bucket[orderhash(o->id)];
	for(f = *pp; f != NULL; f = f->next) {
		if(f == o) {
			*pp = o->next;
			free(o);
			return 0;
		}
		pp = &f->next;
	}

	return -1;
}

int
main(void)
{
	Order *a, *b, *c;

	a = orderalloc(1001, "Alice", 19.99);
	b = orderalloc(1200, "Bob", 42.00);
	c = orderalloc(1001 + NBUCKET, "Carol", 5.50);	/* 故意撞 a 所在的那个桶 */

	assert(a != NULL && b != NULL && c != NULL);

	assert(orderlook(1001) == a);
	assert(orderlook(1200) == b);
	assert(orderlook(1001 + NBUCKET) == c);
	assert(orderlook(9999) == NULL);

	printf("lookup 1001 -> customer=%s amount=%.2f\n",
		orderlook(1001)->customer, orderlook(1001)->amount);

	assert(orderfree(b) == 0);
	assert(orderlook(1200) == NULL);
	assert(orderlook(1001) == a);		/* 同桶其它节点不受影响 */
	assert(orderlook(1001 + NBUCKET) == c);

	assert(orderfree(a) == 0);
	assert(orderfree(c) == 0);
	assert(orderfree(NULL) == 0);
	assert(orderfree(a) == -1);		/* 重复释放应该失败 */

	printf("01_basic_chaining: all assertions passed\n");
	return 0;
}

/*
 * 心智模型
 *
 * 1. 数组下标即哈希桶：bucket[NBUCKET] 本身就是整张哈希表，不需要
 *    额外的 Bucket 结构体。
 * 2. 侵入式链表优于外部链表：Order 结构体自带 next 字段，省掉了
 *    "链表节点包裹用户数据"的那一次额外分配和指针跳转。
 * 3. 删除是一个无特例的统一操作：用 Order **pp 记录"指向上一个 next
 *    字段的指针"，删除头节点和删除中间节点走的是同一段代码。
 */
