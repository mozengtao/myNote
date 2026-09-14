# 03 · 示例：Device Alarm Rule Engine（C Host + Lua 规则表，Multiple Match）

> 前置阅读：[00-why-and-architecture.md](00-why-and-architecture.md)、[01-example-firewall-rule-engine.md](01-example-firewall-rule-engine.md)。本示例与示例1 形成直接对比：示例1 是 First Match + 纯字符串 Action，本示例是 **Multiple Match + 结构化 Action**，并且把"遍历规则表"这件事从 Host 侧搬进了 Lua 侧的一个统一入口函数。完整代码见 [code/03-device-alarm-rule-engine/](code/03-device-alarm-rule-engine/)。

## 1. Problem

一个设备监控 Host，每隔一段时间采集一轮设备指标（`temperature`、`cpu`、`memory`、`link_state`），需要根据这些指标判断要不要告警。和防火墙不同的是，设备告警场景里**一轮采集数据完全可能同时违反多条规则**（温度过高的同时 CPU 也过高），如果只按 First Match 语义处理，命中第一条规则后就不再检查其余规则，会丢失掉同一轮里其他同样重要的告警信息——温度告警和 CPU 告警是两个独立的问题，都应该被上报，不能因为先命中了温度规则就放过 CPU 异常。

## 2. Why Lua

这里除了复用示例1"规则是数据，可运行时增删"的理由外，还多了一个动机：告警信息不能只是一个 `"ALARM"` 字符串，通常需要携带级别（`level`）和具体描述（`message`，例如"温度 88.0 C 超过阈值 80 C"），这是一个结构化结果。Lua table 可以直接把 `{level=..., message=...}` 这种结构作为 `action` 的返回值，Host 侧用 `lua_getfield` 逐个字段取出即可，不需要像纯字符串那样再做一次解析/拼接。同时，把"遍历 rules、收集所有命中结果"这段逻辑整体写成一个 Lua 函数 `evaluate(metrics)`，比在 C 侧写这段循环更简洁——毕竟这段逻辑本质上是在操作 Lua 侧的数据结构（regs 表、结果数组），用 Lua 原生的 `ipairs`/`table.insert` 表达比来回横跨 C/Lua 边界的 `lua_geti`/`lua_pcall` 组合自然得多。

## 3. Architecture

```text
             Host (C)
               |
               v
     采集一轮 metrics（temperature/cpu/memory/link_state）
               |
               v
        Metrics (Lua table)
               |
               v
        evaluate(metrics)  <- Lua 侧统一入口
               |
        +------+------+------+------+------+
        |      |      |      |      |      |
     temp-  temp-   cpu-   cpu-  memory- link-
     critical warning critical warning warning down
        |      |      |      |      |      |
        +------+------+------+------+------+
               |
       本轮所有命中规则的结果数组
       [{rule, level, message}, ...]
               |
               v
             Host (C)
               |
               v
       逐条执行动作（发邮件/记日志）
```

和示例1 的关键区别：示例1 里 Host 在 C 侧持有"遍历 rules、逐条 pcall match/action"的循环；本示例里 Host 只调用一次 `evaluate(metrics)`，遍历循环完全在 Lua 侧完成，C 侧只负责取回结果数组并执行。

## 4. Host responsibilities

- **Mechanism**：真实的指标采集（本示例用固定数组模拟 4 轮采集）、真实的告警执行渠道（发邮件、发短信、写工单系统）。
- **Resource**：如果接入真实系统，这里的资源是采集探针的连接、邮件/短信网关的客户端连接。
- **Lifecycle**：创建/销毁 `lua_State`，加载 `rules.lua` 的时机，以及"每轮采集之后调用一次 `evaluate`"这个调用节奏。
- **Stable API**：Host 与 Lua 之间约定的 `metrics` table 形状（四个字段）和 `evaluate(metrics)` 函数签名，以及返回值数组里每个元素必须具备的 `rule`/`level`/`message` 三个字段——这是本示例的契约边界。

## 5. Lua responsibilities

- **Rules**：`rules` 数组仍然是"输入满足什么条件，触发什么动作"的规则表，与示例1 结构一致（`{name, match, action}`）。
- **调度逻辑**：`evaluate(metrics)` 承担了在示例1 里由 C 侧完成的"遍历 + 收集命中结果"这部分工作，这是本示例把更多调度职责交给 Lua 的体现——Rule Engine 的调度逻辑本身也可以选择放在 Host 侧或 Lua 侧，取决于哪一侧更适合表达这段逻辑。
- **结构化 Action**：每条规则的 `action` 返回 `{level, message}`，比示例1 的纯字符串携带更丰富的信息。

## 6. Lifecycle

```text
Host 启动
  |
  v
luaL_newstate() + luaL_openlibs()
  |
  v
luaL_dofile("rules.lua")   注册全局 rules 表和全局 evaluate 函数
  |
  v
for each round in 4 轮采集:
  push_metrics_table()      构造这一轮的 metrics table
  lua_getglobal("evaluate") + lua_pcall   调用一次，取回结果数组
  遍历结果数组，逐条 execute_action()
  |
  v
lua_close()   销毁 VM
```

`evaluate` 函数本身没有独立的生命周期，它和 `rules` 表一样依附于 `lua_State`；不同的是它每轮都会被调用一次（而 `rules` 表只在加载时构造一次，之后只读）。

## 7. Data Flow（C -> Lua）

`push_metrics_table()` 把一轮 `Metrics` 结构体的四个字段转换成 Lua table：

```c
lua_newtable(L);
lua_pushnumber(L, m->temperature);
lua_setfield(L, -2, "temperature");
lua_pushnumber(L, m->cpu);
lua_setfield(L, -2, "cpu");
lua_pushnumber(L, m->memory);
lua_setfield(L, -2, "memory");
lua_pushstring(L, m->link_state);
lua_setfield(L, -2, "link_state");
```

和示例1 的 `push_packet_table` 套路完全一致——这正说明"C struct -> Lua table"的转换套路本身与具体业务无关，是 Binding 层可以复用的固定模式。

## 8. Control Flow（Lua -> C）

与示例1 相同，Lua 侧不主动调用 Host，Control Flow 仍然是 Host 主动发起的单向调用：`lua_getglobal(L, "evaluate")` -> `push_metrics_table` 压参数 -> `lua_pcall` -> 取回一个 table（结果数组）。区别在于本示例里 Host 只调用一次 `evaluate`，而不是像示例1 那样对每条规则分别调用 `match`/`action`——调用粒度从"每条规则一次调用"收敛成"每轮采集一次调用"，中间的规则遍历完全被封装进了这一次调用里。

## 9. State

- `rules` 数组和示例1 一样是无状态的规则数据 + 纯函数。
- `evaluate` 函数内部的 `results` 是每次调用时新建的局部变量，调用结束后连同这个数组一起被当作返回值传回 C 侧，取值完毕后即可被 Lua GC 回收，不会跨轮次残留。
- Host 侧的 `rounds[]` 数组是本示例里唯一的跨轮次状态，纯粹存在于 C 侧，Lua 完全不知道"这是第几轮采集"——如果要做"连续 3 轮温度都超阈值才报警"这种跨轮次判断，就需要在 Lua 侧或 Host 侧引入额外的状态（例如一个"最近 N 轮命中历史"的表），这是本示例未覆盖的扩展方向。

## 10. Failure

- 加载阶段：`luaL_dofile` 失败会被捕获并安全退出，与示例1 一致。
- 调用阶段：`evaluate_round` 里的 `lua_pcall` 捕获 `evaluate` 函数内部任何一条规则的 `match`/`action` 抛出的错误——注意这里的错误粒度变粗了：因为整个 `evaluate` 是一次调用，如果某条规则的 `match` 里有笔误导致运行时错误，整轮 `evaluate` 调用都会失败（返回错误而不是部分结果），这是相比示例1"单条规则出错只影响这一条"更粗粒度的失败模式，是把调度逻辑收敛进 Lua 侧的代价。
- 类型检查：`evaluate_round` 会检查 `evaluate` 是否是函数、返回值是否是 table，任何一个检查失败都会打印错误并返回 0（当作本轮 0 条命中处理），不会让 Host 崩溃或读到脏数据。
- 兜底：本示例没有"默认规则"（不像防火墙一定要有个 `default-drop`），因为"这一轮没有任何异常"本身就是一个合法且常见的结果（对应 `n == 0` 的分支，打印"设备状态正常"）。

## 11. Complete Host Code

`code/03-device-alarm-rule-engine/main.c`：

```c
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
```

## 12. Complete Lua Code

`code/03-device-alarm-rule-engine/rules.lua`：

```lua
-- rules.lua
-- Device Alarm Rule Engine：与 firewall 示例的区别在于两点
--   1. Action 不是一个纯字符串，而是一个结构化 table { level, message }
--   2. Host 侧采用 Multiple Match 语义：本文件对外暴露一个 evaluate(metrics)
--      函数，一次性返回“这一轮所有命中规则的结果”组成的数组，而不是只返回
--      第一条命中规则的结果。
--
-- 设备指标字段：temperature（摄氏度）、cpu（百分比）、memory（百分比）、
-- link_state（"up"/"down"）。

rules = {
    -- IF temperature > 80 THEN ALARM
    {
        name = "temperature-critical",
        match = function(metrics)
            return metrics.temperature > 80
        end,
        action = function(metrics)
            return {
                level = "ALARM",
                message = string.format(
                    "设备温度过高: %.1f C（阈值 80 C）", metrics.temperature)
            }
        end
    },

    -- IF 60 < temperature <= 80 THEN WARNING
    {
        name = "temperature-warning",
        match = function(metrics)
            return metrics.temperature > 60 and metrics.temperature <= 80
        end,
        action = function(metrics)
            return {
                level = "WARNING",
                message = string.format(
                    "设备温度偏高: %.1f C（关注阈值 60 C）", metrics.temperature)
            }
        end
    },

    -- IF cpu > 90 THEN ALARM
    {
        name = "cpu-critical",
        match = function(metrics)
            return metrics.cpu > 90
        end,
        action = function(metrics)
            return {
                level = "ALARM",
                message = string.format(
                    "CPU 占用过高: %.1f%%（阈值 90%%）", metrics.cpu)
            }
        end
    },

    -- IF 75 < cpu <= 90 THEN WARNING
    {
        name = "cpu-warning",
        match = function(metrics)
            return metrics.cpu > 75 and metrics.cpu <= 90
        end,
        action = function(metrics)
            return {
                level = "WARNING",
                message = string.format(
                    "CPU 占用偏高: %.1f%%（关注阈值 75%%）", metrics.cpu)
            }
        end
    },

    -- IF memory > 90 THEN WARNING
    {
        name = "memory-warning",
        match = function(metrics)
            return metrics.memory > 90
        end,
        action = function(metrics)
            return {
                level = "WARNING",
                message = string.format(
                    "内存占用偏高: %.1f%%（关注阈值 90%%）", metrics.memory)
            }
        end
    },

    -- IF link_state == "down" THEN ALARM
    {
        name = "link-down",
        match = function(metrics)
            return metrics.link_state == "down"
        end,
        action = function(metrics)
            return {
                level = "ALARM",
                message = "链路已断开（link_state = down）"
            }
        end
    }
}

--[[
 evaluate(metrics) 是 Host 调用的统一入口。
 metrics 是一个 Lua table，字段来自 C 侧的一轮采集：
   { temperature = <number>, cpu = <number>, memory = <number>,
     link_state = "up"/"down" }

 返回值：一个数组（从下标 1 开始的 table），每个元素是
   { rule = "<规则名>", level = "ALARM"/"WARNING", message = "<描述>" }
 本轮命中了几条规则，数组就有几个元素；一条都没命中则返回空数组 {}。
 这就是 Multiple Match：与 firewall 示例的 First Match 不同，这里不会在
 找到第一条命中规则后就停止，而是把 rules 表完整遍历一遍。
--]]
function evaluate(metrics)
    local results = {}
    for _, rule in ipairs(rules) do
        if rule.match(metrics) then
            local result = rule.action(metrics)
            result.rule = rule.name
            table.insert(results, result)
        end
    end
    return results
end
```

## 13. Execution

`code/03-device-alarm-rule-engine/Makefile`：

```makefile
# Makefile —— Device Alarm Rule Engine
#
# 依赖系统安装的 Lua 5.4 开发包（liblua5.4-dev / lua5.4-devel，视发行版而定）。
# 如果 pkg-config 里的包名不同，按需修改 LUA_PKG。

LUA_PKG ?= lua5.4

CC      ?= cc
CFLAGS  += -Wall -Wextra -std=c99 -g $(shell pkg-config --cflags $(LUA_PKG))
LDFLAGS += $(shell pkg-config --libs $(LUA_PKG))

TARGET  := device_alarm
SRCS    := main.c

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
```

编译运行：

```bash
cd code/03-device-alarm-rule-engine
make
./device_alarm
```

实际运行输出（4 轮采集，覆盖"无告警" / "双 WARNING" / "ALARM+WARNING 混合" / "多条 ALARM 同时命中"四种场景）：

```text
=== Device Alarm Rule Engine：多轮采集与规则匹配（Multiple Match）===

--- 第 1 轮采集: temperature=45.0 cpu=30.0 memory=50.0 link_state=up ---
    (本轮未命中任何规则，设备状态正常)
    (本轮共命中 0 条规则)

--- 第 2 轮采集: temperature=70.0 cpu=80.0 memory=60.0 link_state=up ---
    [执行动作] 记录警告日志 | rule=temperature-warning    level=WARNING 设备温度偏高: 70.0 C（关注阈值 60 C）
    [执行动作] 记录警告日志 | rule=cpu-warning            level=WARNING CPU 占用偏高: 80.0%（关注阈值 75%）
    (本轮共命中 2 条规则)

--- 第 3 轮采集: temperature=88.0 cpu=40.0 memory=95.0 link_state=up ---
    [执行动作] 发送告警邮件 | rule=temperature-critical   level=ALARM   设备温度过高: 88.0 C（阈值 80 C）
    [执行动作] 记录警告日志 | rule=memory-warning         level=WARNING 内存占用偏高: 95.0%（关注阈值 90%）
    (本轮共命中 2 条规则)

--- 第 4 轮采集: temperature=50.0 cpu=95.0 memory=70.0 link_state=down ---
    [执行动作] 发送告警邮件 | rule=cpu-critical           level=ALARM   CPU 占用过高: 95.0%（阈值 90%）
    [执行动作] 发送告警邮件 | rule=link-down              level=ALARM   链路已断开（link_state = down）
    (本轮共命中 2 条规则)
```

## 14. Step-by-step walkthrough

以第 3 轮采集（`temperature=88.0, cpu=40.0, memory=95.0, link_state="up"`）为例：

1. `main` 从 `rounds[]` 取出这一轮数据，调用 `evaluate_round`。
2. `evaluate_round` 用 `lua_getglobal(L, "evaluate")` 取出全局函数，`push_metrics_table` 构造 `metrics` table，`lua_pcall` 触发执行。
3. Lua 侧的 `evaluate(metrics)` 开始 `for _, rule in ipairs(rules)` 遍历全部 6 条规则：
   - `temperature-critical`：`88.0 > 80` 成立，命中，`action` 返回 `{level="ALARM", message="设备温度过高: 88.0 C（阈值 80 C）"}`，加上 `result.rule = "temperature-critical"` 后 `table.insert` 进 `results`。
   - `temperature-warning`：`88.0 > 60 and 88.0 <= 80`，后半部分 `88.0 <= 80` 为 `false`，不命中。
   - `cpu-critical`：`40.0 > 90` 为 `false`，不命中。
   - `cpu-warning`：`40.0 > 75` 为 `false`，不命中。
   - `memory-warning`：`95.0 > 90` 成立，命中，`action` 返回 `{level="WARNING", message="内存占用偏高: 95.0%（关注阈值 90%）"}`，加入 `results`。
   - `link-down`：`"up" == "down"` 为 `false`，不命中。
4. `evaluate` 遍历完整张 `rules` 表后返回 `results`（长度为 2 的数组），**注意这里没有在 `temperature-critical` 命中后就提前 `return`**——这正是 Multiple Match 与示例1 First Match 的核心区别。
5. `evaluate_round` 拿到返回的 table，`lua_rawlen` 得到 `n=2`，循环两次，分别用 `lua_geti` + `lua_getfield` 取出 `results[1]`（`rule/level/message`）和 `results[2]`，each 调用一次 `execute_action`。
6. `execute_action` 根据 `level` 是否为 `"ALARM"` 决定打印"发送告警邮件"还是"记录警告日志"，最终打印出两行动作日志。
7. `main` 打印"本轮共命中 2 条规则"，进入下一轮。

## 15. Mental Model

```text
Metrics (C struct, 一轮采集)
      |
      v  push_metrics_table
Metrics (Lua table)
      |
      v
evaluate(metrics)  <- 一次调用，内部完整遍历 rules
      |
      v
results = { {rule, level, message}, {rule, level, message}, ... }
      |
      v  lua_geti 逐个取出
Host 逐条 execute_action（发邮件 / 记日志）
```

记忆点：**First Match 是"找到答案就停"，Multiple Match 是"收集所有答案再一起处理"**；把"遍历规则"这段逻辑放在 Lua 侧还是 Host 侧，是一个独立的设计选择，不是 Multiple Match 本身要求的，本示例选择放在 Lua 侧只是因为这样表达更简洁。

## 16. Why this is this Pattern

这是 Rule Engine 而不是 Policy：设备状态判断由一组结构统一、彼此独立的规则驱动（温度、CPU、内存、链路各自有专门的规则，互不依赖对方的判断结果），新增一条"磁盘使用率 > 95% 则 WARNING"的规则只需要在 `rules` 表里加一条，不需要改 `evaluate` 的调度逻辑。如果整个判断逻辑写成一个 `check_device(metrics)` 函数，内部依次判断各个指标并直接拼接告警字符串，就退化成了 Policy——虽然效果相近，但规则不再是可以被单独列举、审计、按条禁用的独立数据项。

这也不是 Configuration：这里描述的是"IF 温度超过阈值 THEN 告警"这种条件-动作关系，不是"阈值应该是多少"这种参数声明（尽管 `80`、`90` 这些阈值本身很适合被抽成 Configuration，作为 Rule Engine 之上再叠加 Configuration 的组合用法，可以参考 [07-cross-pattern-comparison.md](../07-cross-pattern-comparison.md) 里关于 Pattern 组合关系的讨论）。

**如果 Lua 消失，Host 还能做什么？** Host 依然可以完成设备采集和真正的告警发送（这两者是 Mechanism，在 C 侧）。但判断逻辑必须写死在 C 代码里：每新增一条规则、调整一个阈值都要重新编译发布监控程序；而且用纯 C 实现"收集本轮所有命中规则"这种动态长度的结果集合，需要手写动态数组管理代码，远不如 Lua 的 `table.insert` 简洁，代码复杂度会明显上升。

**如果把 Lua 换成 JSON/YAML/纯 C，这个 Pattern 还成立吗？** 
- 换成 **JSON/YAML**：像 `{"field": "temperature", "op": ">", "value": 80, "level": "ALARM"}` 这种简单阈值规则，用声明式配置完全可以表达，很多真实监控系统（Prometheus Alertmanager 的规则、Zabbix 的触发器表达式）就是这么做的；但本示例里 `temperature-warning` 需要"大于 60 且小于等于 80"这种区间判断，`message` 还需要按具体数值格式化字符串（`string.format`），这类"轻量计算 + 格式化"用纯声明式配置需要专门设计表达式语法或者模板引擎，复杂度会向 Lua 解释器靠近。
- 换成 **纯 C**：完全可行，效果上等价于把 `evaluate` 函数用 C 重写成一串 `if`，但每次调整阈值或者新增判断都要重新编译告警程序——这正是 Problem 一开始想避免的。

因此本示例选 Lua 的现实原因是：规则数量不多、条件里有需要计算/格式化的部分（区间判断 + 数值格式化拼接消息），比起单纯的"字段-操作符-阈值"三元组更适合用一段可执行代码表达，而不只是一条声明式配置。
