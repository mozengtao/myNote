/*
 * main.c —— Firewall Rule Engine 的 Host 侧代码
 *
 * 职责：
 *   1. 创建/销毁 lua_State（VM 生命周期完全由 Host 管理）
 *   2. 加载 rules.lua（Lua 侧只声明一份 rules 规则表，不做任何 I/O）
 *   3. 模拟一组 Packet（固定数组），逐个转换成 Lua table
 *   4. 遍历 rules 数组，对每条规则调用 match，命中后调用 action 取回结果
 *   5. 按 First Match 语义决定最终动作，并打印结果
 */

#include <stdio.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

/* 模拟的网络数据包结构，字段与 rules.lua 里 packet.src/dst/port 对应 */
typedef struct {
	const char *src;
	const char *dst;
	int port;
} Packet;

/*
 * 把一个 C 端 Packet 结构体转换成 Lua table，push 到栈顶。
 * 简化点：真实防火墙的包字段远不止 src/dst/port（还有协议号、flags、
 * payload 长度等），这里只保留三个字段用于演示 Host -> Lua 的数据转换套路。
 */
static void push_packet_table(lua_State *L, const Packet *pkt)
{
	lua_newtable(L);

	lua_pushstring(L, pkt->src);
	lua_setfield(L, -2, "src");

	lua_pushstring(L, pkt->dst);
	lua_setfield(L, -2, "dst");

	lua_pushinteger(L, pkt->port);
	lua_setfield(L, -2, "port");
}

/*
 * 对一个 packet 调用 rules 表里第 rule_index 条规则的 match(packet)。
 * 返回 1 表示命中，0 表示未命中，-1 表示调用出错（match 本身不是合法函数
 * 或运行时抛错）。
 *
 * 栈约定：调用前栈顶是 rules 表（不会被本函数改变）。
 */
static int call_match(lua_State *L, int rules_index, lua_Integer rule_index,
		       const Packet *pkt)
{
	int ok;

	/* rules[rule_index] -> 栈顶 */
	lua_geti(L, rules_index, rule_index);

	/* 取出这条规则的 match 字段 */
	lua_getfield(L, -1, "match");
	if (!lua_isfunction(L, -1)) {
		fprintf(stderr, "[warn] rule #%lld 没有合法的 match 函数，跳过\n",
			(long long)rule_index);
		lua_pop(L, 2); /* 弹出 match 字段 + 规则表 */
		return -1;
	}

	push_packet_table(L, pkt);

	if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
		fprintf(stderr, "[error] rule #%lld match 执行失败: %s\n",
			(long long)rule_index, lua_tostring(L, -1));
		lua_pop(L, 2); /* 弹出错误信息 + 规则表 */
		return -1;
	}

	ok = lua_toboolean(L, -1);
	lua_pop(L, 2); /* 弹出 match 返回值 + 规则表 */
	return ok ? 1 : 0;
}

/*
 * 对一个 packet 调用 rules 表里第 rule_index 条规则的 action(packet)，
 * 把返回的字符串复制进 out（调用方保证 out 足够大）。
 * 成功返回 1，失败返回 0。
 */
static int call_action(lua_State *L, int rules_index, lua_Integer rule_index,
			const Packet *pkt, char *out, size_t out_size)
{
	const char *result;

	lua_geti(L, rules_index, rule_index);
	lua_getfield(L, -1, "action");
	if (!lua_isfunction(L, -1)) {
		fprintf(stderr, "[warn] rule #%lld 没有合法的 action 函数\n",
			(long long)rule_index);
		lua_pop(L, 2);
		return 0;
	}

	push_packet_table(L, pkt);

	if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
		fprintf(stderr, "[error] rule #%lld action 执行失败: %s\n",
			(long long)rule_index, lua_tostring(L, -1));
		lua_pop(L, 2);
		return 0;
	}

	result = lua_tostring(L, -1);
	if (result == NULL)
		result = "UNKNOWN";
	snprintf(out, out_size, "%s", result);

	lua_pop(L, 2); /* 弹出 action 返回值 + 规则表 */
	return 1;
}

/*
 * 依次遍历 rules 表，采用 First Match 语义：
 * 第一条 match 返回 true 的规则决定最终动作，立刻停止遍历。
 *
 * 简化点：如果要支持 Multiple Match（例如既要 LOG 又要 DROP），
 * 应该把这里的 "找到即 return" 改成"累积进一个数组，遍历完整张表后
 * 一次性返回所有命中的动作"，具体做法见 03-example-device-alarm-rule-engine.md
 * ——那份示例的 Lua 侧直接返回一个"命中结果数组"，Host 侧遍历这个数组逐个处理。
 */
static int evaluate_packet(lua_State *L, const Packet *pkt, char *action_out,
			    size_t action_out_size, char *rule_name_out,
			    size_t rule_name_out_size)
{
	lua_Integer n, i;
	int rules_index;

	lua_getglobal(L, "rules");
	if (!lua_istable(L, -1)) {
		fprintf(stderr, "[error] 全局 rules 不是一个 table\n");
		lua_pop(L, 1);
		return 0;
	}
	rules_index = lua_gettop(L);
	n = (lua_Integer)lua_rawlen(L, rules_index);

	for (i = 1; i <= n; i++) {
		int matched = call_match(L, rules_index, i, pkt);

		if (matched < 0)
			continue; /* 出错的规则跳过，不影响后续规则判断 */

		if (matched == 1) {
			/* 取规则名，用于打印命中了哪一条规则 */
			lua_geti(L, rules_index, i);
			lua_getfield(L, -1, "name");
			snprintf(rule_name_out, rule_name_out_size, "%s",
				 lua_tostring(L, -1));
			lua_pop(L, 2);

			call_action(L, rules_index, i, pkt, action_out,
				    action_out_size);
			lua_pop(L, 1); /* 弹出 rules 表 */
			return 1;
		}
	}

	lua_pop(L, 1); /* 弹出 rules 表 */
	snprintf(action_out, action_out_size, "NOMATCH");
	snprintf(rule_name_out, rule_name_out_size, "-");
	return 0;
}

int main(void)
{
	lua_State *L;
	int i;

	Packet packets[] = {
		{"192.168.1.10", "192.168.1.1", 22},    /* 期望命中 allow-ssh -> ALLOW */
		{"10.0.0.66", "192.168.1.5", 8080},     /* 期望命中 drop-blacklisted-src -> DROP */
		{"10.0.0.5", "192.168.2.20", 53},       /* 期望命中 log-internal-to-dmz -> LOG */
		{"8.8.8.8", "192.168.1.100", 443},      /* 期望命中 allow-http -> ALLOW */
		{"203.0.113.9", "192.168.1.50", 12345}, /* 期望命中 default-drop -> DROP */
	};
	int npackets = (int)(sizeof(packets) / sizeof(packets[0]));

	/* 1. 创建 VM：VM 的生命周期从这里开始，完全由 Host 管理 */
	L = luaL_newstate();
	if (L == NULL) {
		fprintf(stderr, "无法创建 lua_State\n");
		return 1;
	}
	luaL_openlibs(L);

	/* 2. 加载并执行 rules.lua，把顶层的 rules = {...} 注册为全局变量。
	 *    加载和执行都必须用 pcall 语义兜底，脚本语法错误/运行时错误
	 *    不应该导致 Host 进程崩溃。 */
	if (luaL_dofile(L, "rules.lua") != LUA_OK) {
		fprintf(stderr, "加载 rules.lua 失败: %s\n", lua_tostring(L, -1));
		lua_close(L);
		return 1;
	}

	printf("=== Firewall Rule Engine：逐包匹配结果（First Match）===\n\n");

	for (i = 0; i < npackets; i++) {
		char action[32];
		char rule_name[64];

		evaluate_packet(L, &packets[i], action, sizeof(action),
				 rule_name, sizeof(rule_name));

		printf("packet #%d  src=%-14s dst=%-14s port=%-6d -> rule=%-24s action=%s\n",
		       i + 1, packets[i].src, packets[i].dst, packets[i].port,
		       rule_name, action);
	}

	/* 3. 销毁 VM：无论前面走到哪个分支，只要 VM 创建成功，最终都要 lua_close */
	lua_close(L);
	return 0;
}
