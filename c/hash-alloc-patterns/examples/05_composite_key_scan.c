/*
 * 05_composite_key_scan.c
 *
 * 场景：按 (类型, 优先级) 两个字段组成的复合 key 快速查找"规则"
 * (rule)，但有时候需要按一个完全不在 key 里的字段（目标端口）去找。
 *
 * 模式：把两个字段打包压缩成一个整数再取哈希，可以让"已知类型和
 * 优先级"的查找保持 O(1)；但哈希表天生只加速建表时选定的那个 key，
 * 一旦查询维度换成别的字段组合，就必须显式提供一个遍历全部桶的
 * 线性扫描函数作为兜底，而不是硬凑一个"万能索引"。
 *
 * 编译： gcc -std=c11 -Wall -Wextra -O2 -o /tmp/ex5 05_composite_key_scan.c
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

enum {
	NRULEHASH = 16,
};

typedef struct Rule Rule;

struct Rule {
	int   type;		/* 参与复合 key */
	int   priority;		/* 参与复合 key */
	int   target_port;	/* 不参与复合 key，只能靠全表扫描找 */
	int   action;
	Rule *next;
};

static Rule *rulehash[NRULEHASH];

/*
 * rulehashfn - 把 (type, priority) 揉进一个整数再取哈希
 *
 * 假设 priority 在 0..15 之内，用 4 bit 就能装下，这样 type 和
 * priority 打包后仍然分布得比较均匀。
 */
static unsigned
rulehashfn(int type, int priority)
{
	return (unsigned)((type << 4) | (priority & 0xF)) & (NRULEHASH - 1);
}

/*
 * rulealloc - 按复合 key 插入哈希表
 */
Rule*
rulealloc(int type, int priority, int target_port, int action)
{
	Rule *r, **pp;

	r = malloc(sizeof(*r));
	if(r == NULL)
		return NULL;

	r->type = type;
	r->priority = priority;
	r->target_port = target_port;
	r->action = action;

	pp = &rulehash[rulehashfn(type, priority)];
	r->next = *pp;
	*pp = r;

	return r;
}

/*
 * rulelook - 按建表用的复合 key (type, priority) 查找，O(1)
 */
Rule*
rulelook(int type, int priority)
{
	Rule *r;

	for(r = rulehash[rulehashfn(type, priority)]; r != NULL; r = r->next) {
		if(r->type == type && r->priority == priority)
			break;
	}

	return r;
}

/*
 * rulefree - 按复合 key 定位所在桶，然后二级指针摘链
 */
int
rulefree(Rule *r)
{
	Rule *f, **pp;

	if(r == NULL)
		return 0;

	pp = &rulehash[rulehashfn(r->type, r->priority)];
	for(f = *pp; f != NULL; f = f->next) {
		if(f == r) {
			*pp = r->next;
			free(r);
			return 0;
		}
		pp = &f->next;
	}

	return -1;
}

/*
 * rulematch_by_port - target_port 没有建索引，只能全表扫描兜底
 *
 * 这是分离链接哈希表的天然局限：只有建表用的那个 key 才能 O(1)；
 * 任何其它维度的查询都要显式退化为 O(N)。
 */
Rule*
rulematch_by_port(int target_port)
{
	int i;
	Rule *r;

	for(i = 0; i < NRULEHASH; i++) {
		for(r = rulehash[i]; r != NULL; r = r->next) {
			if(r->target_port == target_port)
				return r;
		}
	}

	return NULL;
}

int
main(void)
{
	Rule *r1, *r2, *r3;

	r1 = rulealloc(1, 5, 8080, 100);	/* type=1 priority=5  -> bucket ((1<<4)|5)&15 = 5  */
	r2 = rulealloc(2, 5, 9090, 200);	/* type=2 priority=5  -> bucket ((2<<4)|5)&15 = 5，与 r1 撞同一个桶 */
	r3 = rulealloc(3, 1, 22,   300);	/* type=3 priority=1  -> bucket ((3<<4)|1)&15 = 1，落在别的桶 */

	assert(r1 && r2 && r3);

	/* 按复合 key 直接命中 */
	assert(rulelook(1, 5) == r1);
	assert(rulelook(2, 5) == r2);
	assert(rulelook(3, 1) == r3);
	assert(rulelook(9, 9) == NULL);

	/* 按 target_port 查找：不在复合 key 里，必须全表扫描 */
	assert(rulematch_by_port(9090) == r2);
	assert(rulematch_by_port(22) == r3);
	assert(rulematch_by_port(65535) == NULL);
	printf("rulematch_by_port(9090) found rule with type=%d priority=%d\n",
		rulematch_by_port(9090)->type, rulematch_by_port(9090)->priority);

	assert(rulefree(r2) == 0);
	assert(rulelook(2, 5) == NULL);
	assert(rulematch_by_port(9090) == NULL);	/* 释放之后全表扫描也应该找不到 */
	assert(rulelook(1, 5) == r1);			/* 同桶的 r1 不受影响 */

	assert(rulefree(r1) == 0);
	assert(rulefree(r3) == 0);

	printf("05_composite_key_scan: all assertions passed\n");
	return 0;
}

/*
 * 心智模型
 *
 * 1. 哈希只加速"建表用的那个 key"：rulelook(type, priority) 是 O(1)，
 *    但同一批数据按 target_port 查，哈希表完全帮不上忙。
 * 2. 复合 key 只是"把多个字段打包成一个整数"这件事的自然延伸——本质
 *    上仍然是普通的哈希查找，不要指望它能同时优化所有查询维度。
 * 3. 坦然接受"某些查询就是 O(N)"，比强行给每个字段都建一份索引更
 *    简单也更省内存；只有当某个非主 key 的查询频率高到扛不住 O(N)
 *    时，才值得为它单独建一张索引表。
 */
