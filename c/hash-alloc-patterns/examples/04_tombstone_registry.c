/*
 * 04_tombstone_registry.c
 *
 * 场景：按名字注册的"策略"(policy)表，策略可以被"停用"，但停用
 * 不等于立刻从内存里抹掉。
 *
 * 模式：字符串哈希 + 状态位软删除（墓碑/tombstone）。查找时除了
 * 比较 key，还要检查一个 state 标志位；停用只是翻转标志位，真正
 * 的物理摘链交给一个独立的 purge 操作，两者解耦。
 *
 * 编译： gcc -std=c11 -Wall -Wextra -O2 -o /tmp/ex4 04_tombstone_registry.c
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
	NPOLICYHASH = 16,
	NAMELEN     = 24,
};

typedef enum {
	POLICY_ACTIVE   = 1,
	POLICY_INACTIVE = 2,
} PolicyState;

typedef struct Policy Policy;

struct Policy {
	char        name[NAMELEN];
	int         priority;
	PolicyState state;
	Policy     *next;
};

static Policy *policyhash[NPOLICYHASH];

static unsigned
namehash(const char *name)
{
	unsigned h;

	h = (unsigned char)name[0];
	while(*name != '\0')
		h ^= (unsigned char)*name++;

	return h & (NPOLICYHASH - 1);
}

/*
 * policyalloc - 注册一条新策略，初始状态为 ACTIVE
 */
Policy*
policyalloc(const char *name, int priority)
{
	Policy *p, **pp;

	p = malloc(sizeof(*p));
	if(p == NULL)
		return NULL;

	memset(p, 0, sizeof(*p));
	snprintf(p->name, sizeof(p->name), "%s", name);
	p->priority = priority;
	p->state = POLICY_ACTIVE;

	pp = &policyhash[namehash(name)];
	p->next = *pp;
	*pp = p;

	return p;
}

/*
 * policylook - 只有 ACTIVE 状态的条目才算"存在"
 */
Policy*
policylook(const char *name)
{
	Policy *p;

	for(p = policyhash[namehash(name)]; p != NULL; p = p->next) {
		if(strcmp(p->name, name) == 0 && p->state == POLICY_ACTIVE)
			break;
	}

	return p;
}

/*
 * policy_deactivate - 软删除：只翻转状态位，不摘链、不释放内存
 */
void
policy_deactivate(const char *name)
{
	Policy *p;

	for(p = policyhash[namehash(name)]; p != NULL; p = p->next) {
		if(strcmp(p->name, name) == 0) {
			p->state = POLICY_INACTIVE;
			return;
		}
	}
}

/*
 * policy_purge - 真正的物理删除，只清理已经被标记为 INACTIVE 的条目
 *
 * 返回 0 表示删掉了一条，-1 表示没有找到可清理的墓碑
 */
int
policy_purge(const char *name)
{
	Policy *f, **pp;

	pp = &policyhash[namehash(name)];
	for(f = *pp; f != NULL; f = f->next) {
		if(strcmp(f->name, name) == 0 && f->state == POLICY_INACTIVE) {
			*pp = f->next;
			free(f);
			return 0;
		}
		pp = &f->next;
	}

	return -1;
}

/*
 * policy_count_all - 遍历全表统计"物理上还存在"的条目数（ACTIVE+INACTIVE）
 *
 * 只用来在自测里证明"停用"确实没有物理删除数据。
 */
static int
policy_count_all(void)
{
	int i, n;
	Policy *p;

	n = 0;
	for(i = 0; i < NPOLICYHASH; i++) {
		for(p = policyhash[i]; p != NULL; p = p->next)
			n++;
	}

	return n;
}

int
main(void)
{
	Policy *gold, *silver;

	gold = policyalloc("gold", 10);
	silver = policyalloc("silver", 20);
	assert(gold != NULL && silver != NULL);
	assert(policy_count_all() == 2);

	assert(policylook("gold") == gold);
	assert(policylook("silver") == silver);

	policy_deactivate("silver");

	/* 停用之后：逻辑上查不到了，但物理上还在表里 */
	assert(policylook("silver") == NULL);
	assert(policy_count_all() == 2);
	printf("silver deactivated: policylook()==NULL but still physically present\n");

	/* 停用一个不存在的名字，应该安静地什么都不做 */
	policy_deactivate("no-such-policy");
	assert(policy_count_all() == 2);

	assert(policy_purge("silver") == 0);
	assert(policy_count_all() == 1);
	assert(policy_purge("silver") == -1);	/* 已经被清理，第二次找不到墓碑 */

	/* gold 应该完全不受影响 */
	assert(policylook("gold") == gold);

	/* 名字可以在墓碑被真正清理之后重新使用 */
	silver = policyalloc("silver", 99);
	assert(silver != NULL);
	assert(policylook("silver")->priority == 99);

	printf("04_tombstone_registry: all assertions passed\n");
	return 0;
}

/*
 * 心智模型
 *
 * 1. 软删除 != 物理删除：业务语义上的"删除"（停用、下线）往往只需要
 *    让条目在查找时"隐身"，而不需要立刻释放内存或摘链——这样可以保留
 *    审计痕迹，或者避免和正在进行的遍历产生竞争。
 * 2. 命中 key 只是必要条件：一个条目"存在"与否，是 key 匹配 AND 状态
 *    合法两个条件的合取，任何只比较 key 的查找逻辑在这种表上都是错的。
 * 3. 真正的资源回收可以延迟、可以独立触发（policy_purge），把"何时
 *    让用户看不到"和"何时真正释放内存"解耦，是很多垂直系统里
 *    "先下线、再回收"两阶段删除的通用做法。
 */
