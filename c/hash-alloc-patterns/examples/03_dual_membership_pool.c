/*
 * 03_dual_membership_pool.c
 *
 * 场景：一台"设备"(Device)拥有若干"端口"(Port)，端口同时需要被
 * 全局哈希表按 port id 快速查找。
 *
 * 模式：初始化时一次性分配好定长静态池(NPORT 个)，运行期绝不再向
 * 系统申请内存；一个 Port 同时具有两种独立的从属关系——"属于哪个
 * Device"（私有链表，用 link 字段）和"能被全局按 id 找到"（哈希桶链，
 * 用 hash 字段）——两条链表不能像 02 示例那样合并成一个字段，因为
 * 分配之后两条链表会同时存在。link 字段则可以在"空闲"时兼职当
 * freelist 的 next（因为空闲的 Port 不属于任何 Device）。
 *
 * 编译： gcc -std=c11 -Wall -Wextra -O2 -o /tmp/ex3 03_dual_membership_pool.c
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum {
	NPORT     = 6,	/* 静态定长池容量：worst case 一次性锁死 */
	NPORTHASH = 8,
};

typedef struct Device Device;
typedef struct Port Port;

struct Port {
	int     id;
	int     speed_mbps;
	Device *owner;
	Port   *hash;	/* 仅用于全局哈希桶链，分配后才有意义 */
	Port   *link;	/* 空闲时是 freelist 的 next；分配后是 owner 的私有链表 next */
};

struct Device {
	char  name[16];
	Port *ports;	/* 该设备名下端口的私有链表头 */
	int   nports;
};

static Port  portpool[NPORT];
static Port *portfreelist;
static Port *porthash[NPORTHASH];

static unsigned
porthashfn(int id)
{
	return (unsigned)id & (NPORTHASH - 1);
}

/*
 * port_pool_init - 一次性把 NPORT 个 Port 串成 freelist
 */
void
port_pool_init(void)
{
	int i;

	memset(portpool, 0, sizeof(portpool));

	portfreelist = NULL;
	for(i = NPORT - 1; i >= 0; i--) {
		portpool[i].link = portfreelist;
		portfreelist = &portpool[i];
	}

	memset(porthash, 0, sizeof(porthash));
}

/*
 * portalloc - 从静态池分配一个端口，同时挂上 owner 的私有链表和全局哈希表
 */
Port*
portalloc(Device *owner, int id, int speed_mbps)
{
	Port *p, **pp;

	if(owner == NULL)
		return NULL;

	p = portfreelist;
	if(p == NULL)
		return NULL;	/* 静态池已耗尽，绝不向系统申请更多内存 */
	portfreelist = p->link;

	p->id = id;
	p->speed_mbps = speed_mbps;
	p->owner = owner;

	/* 挂上 owner 的私有链表 */
	p->link = owner->ports;
	owner->ports = p;
	owner->nports++;

	/* 挂上全局哈希表 */
	pp = &porthash[porthashfn(id)];
	p->hash = *pp;
	*pp = p;

	return p;
}

/*
 * portlook - 按 id 在全局哈希表里查找端口，与它属于哪个 Device 无关
 */
Port*
portlook(int id)
{
	Port *p;

	for(p = porthash[porthashfn(id)]; p != NULL; p = p->hash) {
		if(p->id == id)
			break;
	}

	return p;
}

/*
 * portfree - 必须同时从两条链表上摘除：全局哈希表 + owner 的私有链表
 */
int
portfree(Port *p)
{
	Device *owner;
	Port *f, **pp;

	if(p == NULL)
		return 0;

	owner = p->owner;

	pp = &porthash[porthashfn(p->id)];
	for(f = *pp; f != NULL; f = f->hash) {
		if(f == p) {
			*pp = p->hash;
			break;
		}
		pp = &f->hash;
	}
	if(f == NULL)
		return -1;	/* 不在全局哈希表里，状态已经不一致，拒绝继续 */

	pp = &owner->ports;
	for(f = *pp; f != NULL; f = f->link) {
		if(f == p) {
			*pp = p->link;
			break;
		}
		pp = &f->link;
	}
	if(f == NULL)
		return -1;	/* 不在 owner 的私有链表里 */
	owner->nports--;

	p->owner = NULL;
	p->link = portfreelist;
	portfreelist = p;

	return 0;
}

int
main(void)
{
	Device devA, devB;
	Port *p1, *p2, *p3, *p4, *p5, *p6, *p7;

	port_pool_init();
	memset(&devA, 0, sizeof(devA));
	memset(&devB, 0, sizeof(devB));
	snprintf(devA.name, sizeof(devA.name), "switchA");
	snprintf(devB.name, sizeof(devB.name), "switchB");

	p1 = portalloc(&devA, 1, 1000);
	p2 = portalloc(&devA, 2, 1000);
	p3 = portalloc(&devA, 3, 10000);
	p4 = portalloc(&devB, 4, 100);
	p5 = portalloc(&devB, 5, 100);
	p6 = portalloc(&devB, 6, 40000);

	assert(p1 && p2 && p3 && p4 && p5 && p6);
	assert(devA.nports == 3);
	assert(devB.nports == 3);

	/* 静态池刚好用满 NPORT=6 个，第 7 次分配必须失败 */
	p7 = portalloc(&devA, 7, 1000);
	assert(p7 == NULL);
	printf("pool exhausted as expected after %d ports\n", NPORT);

	assert(portlook(4)->owner == &devB);
	assert(portlook(2)->owner == &devA);
	assert(portlook(999) == NULL);

	/* 释放 devA 的端口 2：必须同时从全局哈希表和 devA 的私有链表消失 */
	assert(portfree(p2) == 0);
	assert(portlook(2) == NULL);
	assert(devA.nports == 2);
	assert(portlook(1)->owner == &devA);	/* 同一个 owner 的其它端口不受影响 */
	assert(portlook(3)->owner == &devA);

	/* freelist 复用：现在应该能再分配成功一个端口了 */
	p7 = portalloc(&devA, 7, 1000);
	assert(p7 != NULL);
	assert(portlook(7)->owner == &devA);
	assert(devA.nports == 3);

	printf("03_dual_membership_pool: all assertions passed\n");
	return 0;
}

/*
 * 心智模型
 *
 * 1. 所有权(ownership)与索引(indexing)是两个正交的概念：owner->ports
 *    这条私有链表回答"这个端口归谁管、谁该在释放时把它带走"；
 *    porthash[] 这条全局哈希桶链回答"任何模块只知道 id，怎么最快找到它"。
 *    两个问题的答案互不影响，所以各自需要独立的链表字段。
 * 2. 静态内存预算：NPORT 在编译期就锁死了系统能同时存在的端口总数，
 *    用一次性预分配换取"运行期绝不会因为端口对象而向 OS 要内存"的确定性。
 * 3. link 字段的双重身份（空闲时是 freelist next，分配后是 owner 链表 next）
 *    依然遵守"生命周期状态互斥才能复用字段"的原则；而 hash 字段则不能
 *    这样复用，因为分配后 hash 和 link 要同时生效。
 */
