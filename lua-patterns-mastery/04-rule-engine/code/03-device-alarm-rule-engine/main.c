/*
 * main.c —— Device Alarm Rule Engine 的 Host 侧代码
 *
 * 和 01-firewall-rule-engine 的差异（这正是本示例要展示的对比点）：
 *   1. Host 不自己遍历 rules 表，而是调用 Lua 侧封装好的 evaluate(metrics)
 *      入口函数，把“遍历 rules、逐条 match、收集命中结果”整个过程都交给
 *      Lua 完成，Host 只拿最终结果。
 *   2. evaluate() 返回的是一个数组，包含本轮所有命中规则的结果
 *      （Multiple Match），而不是像 firewall 示例那样只返回第一条命中
 *      规则的结果（First Match）。
 *   3. 每个命中结果是一个结构化 table { rule, level, message }，
 *      不是一个纯字符串。
 *
 * 职责：
 *   1. 创建/销毁 lua_State
 *   2. 加载 rules.lua
 *   3. 模拟多轮采集（collect metrics），每轮把一组指标转成 Lua table
 *   4. 调用 evaluate(metrics)，取回命中规则数组
 *   5. Host 侧“执行动作”——这里用打印模拟“发送告警邮件/记录日志”
 */

#include <stdio.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

/* 一轮采集到的设备指标 */
typedef struct {
	double temperature; /* 摄氏度 */
	double cpu;          /* 百分比 */
	double memory;        /* 百分比 */
	const char *link_state; /* "up" / "down" */
} Metrics;

/* 把一轮 Metrics 转换成 Lua table，push 到栈顶 */
static void push_metrics_table(lua_State *L, const Metrics *m)
{
	lua_newtable(L);

	lua_pushnumber(L, m->temperature);
	lua_setfield(L, -2, "temperature");

	lua_pushnumber(L, m->cpu);
	lua_setfield(L, -2, "cpu");

	lua_pushnumber(L, m->memory);
	lua_setfield(L, -2, "memory");

	lua_pushstring(L, m->link_state);
	lua_setfield(L, -2, "link_state");
}

/*
 * 模拟 Host 侧“执行动作”：真实系统里这里应该去调用邮件/短信/工单系统的
 * 客户端库；本示例只打印一行，用来代表“动作已经被 Host 执行”。
 * 简化点：真实告警系统通常还需要去抖（防止同一条规则反复触发）、
 * 限流、聚合等逻辑，这里为了聚焦 Rule Engine 本身而省略。
 */
static void execute_action(const char *rule, const char *level,
			    const char *message)
{
	if (strcmp(level, "ALARM") == 0)
		printf("    [执行动作] 发送告警邮件 | rule=%-22s level=%-7s %s\n",
		       rule, level, message);
	else
		printf("    [执行动作] 记录警告日志 | rule=%-22s level=%-7s %s\n",
		       rule, level, message);
}

/*
 * 调用 Lua 侧的 evaluate(metrics)，取回命中规则数组，逐个转发给
 * execute_action。返回命中的规则数量。
 */
static int evaluate_round(lua_State *L, const Metrics *m)
{
	lua_Integer n, i;
	int results_index;
	int hit_count = 0;

	lua_getglobal(L, "evaluate");
	if (!lua_isfunction(L, -1)) {
		fprintf(stderr, "[error] 全局 evaluate 不是一个函数\n");
		lua_pop(L, 1);
		return 0;
	}

	push_metrics_table(L, m);

	if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
		fprintf(stderr, "[error] evaluate 执行失败: %s\n",
			lua_tostring(L, -1));
		lua_pop(L, 1);
		return 0;
	}

	if (!lua_istable(L, -1)) {
		fprintf(stderr, "[error] evaluate 的返回值不是一个 table\n");
		lua_pop(L, 1);
		return 0;
	}

	results_index = lua_gettop(L);
	n = (lua_Integer)lua_rawlen(L, results_index);

	if (n == 0) {
		printf("    (本轮未命中任何规则，设备状态正常)\n");
	}

	for (i = 1; i <= n; i++) {
		const char *rule, *level, *message;

		lua_geti(L, results_index, i);

		lua_getfield(L, -1, "rule");
		rule = lua_tostring(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, -1, "level");
		level = lua_tostring(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, -1, "message");
		message = lua_tostring(L, -1);
		lua_pop(L, 1);

		execute_action(rule ? rule : "?", level ? level : "?",
			       message ? message : "");

		lua_pop(L, 1); /* 弹出 results[i] */
		hit_count++;
	}

	lua_pop(L, 1); /* 弹出 evaluate 的返回值 */
	return hit_count;
}

int main(void)
{
	lua_State *L;
	int round;

	/* 模拟 4 轮采集：分别覆盖“全部正常”“双 WARNING”“ALARM+WARNING 混合”
	 * “多条 ALARM 同时命中”四种情况，用于展示 Multiple Match。 */
	Metrics rounds[] = {
		{45.0, 30.0, 50.0, "up"},   /* 全部正常，预期 0 条命中 */
		{70.0, 80.0, 60.0, "up"},   /* 温度/CPU 双 WARNING */
		{88.0, 40.0, 95.0, "up"},   /* 温度 ALARM + 内存 WARNING */
		{50.0, 95.0, 70.0, "down"}, /* CPU ALARM + 链路 ALARM */
	};
	int nrounds = (int)(sizeof(rounds) / sizeof(rounds[0]));

	/* 1. 创建 VM */
	L = luaL_newstate();
	if (L == NULL) {
		fprintf(stderr, "无法创建 lua_State\n");
		return 1;
	}
	luaL_openlibs(L);

	/* 2. 加载 rules.lua，注册全局 rules 表和 evaluate 函数 */
	if (luaL_dofile(L, "rules.lua") != LUA_OK) {
		fprintf(stderr, "加载 rules.lua 失败: %s\n", lua_tostring(L, -1));
		lua_close(L);
		return 1;
	}

	printf("=== Device Alarm Rule Engine：多轮采集与规则匹配（Multiple Match）===\n\n");

	for (round = 0; round < nrounds; round++) {
		int hits;

		printf("--- 第 %d 轮采集: temperature=%.1f cpu=%.1f memory=%.1f link_state=%s ---\n",
		       round + 1, rounds[round].temperature, rounds[round].cpu,
		       rounds[round].memory, rounds[round].link_state);

		hits = evaluate_round(L, &rounds[round]);

		printf("    (本轮共命中 %d 条规则)\n\n", hits);
	}

	/* 3. 销毁 VM */
	lua_close(L);
	return 0;
}
