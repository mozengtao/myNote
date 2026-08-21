/*
 * 07_ordered_walk_and_cascade_free.c
 *
 * 场景：网关设备管理一批"会话"(session)，每个 session 名下又挂着
 * 若干条"连接"(connection)子资源；管理界面需要能够：
 *   (a) 按客户端地址快速查到某个 session（哈希查找）；
 *   (b) 按稳定顺序把所有活跃 session 分页导出（GetNext 风格游标遍历）；
 *   (c) 释放一个 session 时，必须连带释放它名下所有 connection。
 *
 * 模式（三合一）：
 *   1. session 来自一个定长数组池，其 id 直接用指针算术
 *      `s - sesspool` 算出来，不需要额外的 ID 分配器；
 *   2. session_walk_next() 直接按数组下标升序扫描，是一条独立于
 *      哈希查找的访问路径，专门服务"遍历全部、分页导出"这类需求；
 *   3. session_free() 是一个级联动作序列：先清空 connection 子链表，
 *      再从哈希表摘除，最后才放回 freelist。
 *
 * 编译： gcc -std=c11 -Wall -Wextra -O2 -o /tmp/ex7 07_ordered_walk_and_cascade_free.c
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
	NSESSION  = 8,	/* 静态定长池容量 */
	NSESSHASH = 8,
};

typedef struct Connection Connection;
typedef struct Session Session;

struct Connection {
	int         remote_port;
	Connection *link;	/* 挂在 session 的私有连接链表上 */
};

struct Session {
	int         id;		/* = 本对象在 sesspool 中的下标，指针算术得来 */
	uint32_t    client_addr;
	int         inuse;
	Connection *conns;
	Session    *poollink;	/* 复用：空闲时是 freelist 的 next */
	Session    *hash;	/* 按 client_addr 建的全局哈希桶链 */
};

static Session  sesspool[NSESSION];
static Session *sessfreelist;
static Session *sesshash[NSESSHASH];
static int       live_connections;	/* 只用于自测，证明级联释放没有漏 */

static unsigned
addrhash(uint32_t addr)
{
	return addr & (NSESSHASH - 1);
}

/*
 * session_pool_init - 一次性把 NSESSION 个 session 串成 freelist
 *
 * 每个槽位的 id 就是它在数组里的下标，初始化时用指针算术算一次，
 * 之后即便反复被分配/释放/再分配，id 始终等于这个下标。
 */
void
session_pool_init(void)
{
	int i;

	memset(sesspool, 0, sizeof(sesspool));

	sessfreelist = NULL;
	for(i = NSESSION - 1; i >= 0; i--) {
		sesspool[i].id = i;	/* i == (&sesspool[i] - sesspool) */
		sesspool[i].poollink = sessfreelist;
		sessfreelist = &sesspool[i];
	}

	memset(sesshash, 0, sizeof(sesshash));
	live_connections = 0;
}

/*
 * session_alloc - 从静态池分配一个 session，按 client_addr 挂进哈希表
 */
Session*
session_alloc(uint32_t client_addr)
{
	Session *s, **pp;

	s = sessfreelist;
	if(s == NULL)
		return NULL;	/* 静态池已耗尽 */
	sessfreelist = s->poollink;

	/* 用指针算术重新确认身份，不依赖任何额外的 ID 生成器 */
	s->id = (int)(s - sesspool);
	s->client_addr = client_addr;
	s->inuse = 1;
	s->conns = NULL;

	pp = &sesshash[addrhash(client_addr)];
	s->hash = *pp;
	*pp = s;

	return s;
}

/*
 * session_lookup_by_addr - 按 client_addr 的哈希查找，O(1)
 */
Session*
session_lookup_by_addr(uint32_t client_addr)
{
	Session *s;

	for(s = sesshash[addrhash(client_addr)]; s != NULL; s = s->hash) {
		if(s->inuse && s->client_addr == client_addr)
			break;
	}

	return s;
}

/*
 * session_walk_next - GetNext 风格的游标遍历
 *
 * 直接按下标升序扫描定长数组池，而不是走哈希桶链：遍历顺序稳定、
 * 可以用同一个 cursor 分页续接，这是哈希查找完全没有的能力，
 * 两条访问路径互相替代不了对方。
 */
Session*
session_walk_next(int *cursor)
{
	Session *s;

	while(*cursor < NSESSION) {
		s = &sesspool[*cursor];
		(*cursor)++;
		if(s->inuse)
			return s;
	}

	return NULL;
}

/*
 * connection_add - 给 session 挂一条新连接（动态分配，个数不定）
 */
int
connection_add(Session *s, int remote_port)
{
	Connection *c;

	if(s == NULL)
		return -1;

	c = malloc(sizeof(*c));
	if(c == NULL)
		return -1;

	c->remote_port = remote_port;
	c->link = s->conns;
	s->conns = c;
	live_connections++;

	return 0;
}

/*
 * connections_free_all - 级联释放的第一步：清空 session 名下所有 connection
 */
static void
connections_free_all(Session *s)
{
	Connection *c, *next;

	for(c = s->conns; c != NULL; c = next) {
		next = c->link;
		free(c);
		live_connections--;
	}
	s->conns = NULL;
}

/*
 * session_free - 级联释放：先释放全部 connection，再摘哈希表，最后回收槙位
 */
void
session_free(Session *s)
{
	Session *f, **pp;

	if(s == NULL || !s->inuse)
		return;

	connections_free_all(s);

	pp = &sesshash[addrhash(s->client_addr)];
	for(f = *pp; f != NULL; f = f->hash) {
		if(f == s) {
			*pp = s->hash;
			break;
		}
		pp = &f->hash;
	}

	s->inuse = 0;
	s->poollink = sessfreelist;
	sessfreelist = s;
}

int
main(void)
{
	Session *a, *b, *c, *walked;
	int cursor, count;

	session_pool_init();

	a = session_alloc(0xAAAA0001);
	b = session_alloc(0xAAAA0002);
	c = session_alloc(0xAAAA0003);
	assert(a != NULL && b != NULL && c != NULL);
	assert(a->id == 0 && b->id == 1 && c->id == 2);

	connection_add(a, 8001);
	connection_add(a, 8002);
	connection_add(b, 9001);
	assert(live_connections == 3);

	assert(session_lookup_by_addr(0xAAAA0002) == b);
	assert(session_lookup_by_addr(0xAAAA0009) == NULL);

	/* 游标遍历应该按下标升序，依次拿到 a, b, c */
	cursor = 0;
	walked = session_walk_next(&cursor);
	assert(walked == a);
	walked = session_walk_next(&cursor);
	assert(walked == b);
	walked = session_walk_next(&cursor);
	assert(walked == c);
	walked = session_walk_next(&cursor);
	assert(walked == NULL);
	printf("ordered walk visited sessions in id order: %d, %d, %d\n", a->id, b->id, c->id);

	/* 释放 a：必须级联释放它的 2 条 connection，且从哈希表里消失 */
	session_free(a);
	assert(live_connections == 1);	/* a 的 2 条被释放，只剩 b 的 1 条 */
	assert(session_lookup_by_addr(0xAAAA0001) == NULL);
	assert(session_lookup_by_addr(0xAAAA0002) == b);	/* b 不受影响 */

	/* 用满剩余槙位，验证静态池确实是定长的 */
	count = 0;
	for(;;) {
		Session *s = session_alloc(0xB0000000u + (uint32_t)count);
		if(s == NULL)
			break;
		count++;
	}
	printf("filled remaining pool with %d more sessions (NSESSION=%d)\n", count, NSESSION);
	assert(session_alloc(0xC0000000u) == NULL);	/* 池已满，必须失败 */

	printf("07_ordered_walk_and_cascade_free: all assertions passed\n");
	return 0;
}

/*
 * 心智模型
 *
 * 1. 索引遍历与哈希查找是两条正交的访问路径：session_walk_next() 靠
 *    数组下标保证"稳定顺序、可分页"，session_lookup_by_addr() 靠哈希
 *    保证"已知 key 时 O(1)"，两者不能相互替代，也不需要互相替代。
 * 2. 删除是一个动作序列，不是一次调用：session_free() 内部先做
 *    "清空子资源"（级联释放），再做"摘除索引"，最后才做"归还槙位"，
 *    顺序颠倒就会留下悬挂指针或者提前把槙位标记为可复用。
 * 3. 让内存布局本身承载身份（identity-from-layout）：session 的 id
 *    就是它在 sesspool 数组里的下标，用指针相减零成本得到，天然
 *    支持"按下标升序遍历"，不需要另外维护一个自增 ID 计数器。
 */
