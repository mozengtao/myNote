# 01 · 示例：Firewall Rule Engine（C Host + Lua 规则表，First Match）

> 前置阅读：[00-why-and-architecture.md](00-why-and-architecture.md)。本示例演示 Rule Engine 最基础的形态——`match/action` 规则表 + First Match 语义。完整代码见 [code/01-firewall-rule-engine/](code/01-firewall-rule-engine/)。

## 1. Problem

一个用 C 写的简易防火墙 Host，需要根据数据包的 `src`（源地址）、`dst`（目的地址）、`port`（端口）决定 `ALLOW`（放行）、`DROP`（丢弃）还是 `LOG`（记录日志但放行）。这组判断规则不是一次性写死的——运维人员会不断新增/调整规则（"临时封禁某个 IP"、"新开放一个端口"、"给某段内网访问加日志"），如果把这些规则直接写成 C 里的一串 `if/else`，会遇到 Pattern 概览里提到的典型问题：

- 每加一条规则都要重新编译、重新发布整个防火墙二进制；
- 规则之间的判断顺序、优先级混在 C 代码逻辑里，越堆越难读；
- 规则写错的代价是段错误或者判断逻辑整体失效，而不是一条可控的错误信息。

需要的是一种"规则本身是数据，规则的判断逻辑是可以运行时加载/替换的代码"的机制。

## 2. Why Lua

这正是 [00-why-and-architecture.md](00-why-and-architecture.md) 里 Rule Engine 抽象的直接落地：`Input -> Predicate -> Match -> Action`。Lua 在这里承担 Predicate（`match` 函数）和 Action（`action` 函数）的载体：

- 每条规则可以直接写成一个 Lua 函数闭包，不需要额外发明一套规则 DSL 和对应的 parser；
- 规则表本身就是一个 Lua table，新增/删除/调整一条规则只是编辑 `rules.lua` 里的一个数组元素，不涉及 Host 二进制；
- `lua_pcall` 保证某条规则的 `match`/`action` 写错（比如笔误访问了不存在的字段）只会让这一条规则报错，不会拖垮整个防火墙进程；
- Lua 的模式匹配（`string.match`）足够表达 `src`/`dst` 这类字符串前缀判断，不需要引入正则库。

## 3. Architecture

```text
                 Host (C)
                    |
                    v
             Packet { src, dst, port }
                    |
                    v
           Lua rules 数组（match+action）
                    |
        +-----------+-----------+-----------+
        |           |           |           |
   allow-ssh   drop-blacklist  log-internal  ...  default-drop
        |           |           |           |
        +-----------+-----------+-----------+
                    |
              First Match 命中
                    |
                    v
          "ALLOW" / "DROP" / "LOG"
                    |
                    v
                 Host (C)
```

Host 拥有 Mechanism（收包/转发/丢包的真实网络能力）和 Resource（网络接口、连接表），Lua 侧只持有一份纯数据+函数的 `rules` 表，不直接接触任何真实资源。

## 4. Host responsibilities

- **Mechanism**：真实的收包、丢包、转发逻辑（本示例用模拟的 `Packet` 数组代替真实网卡收包，但转换和调用套路与真实系统一致）。
- **Resource**：本示例没有引入真实资源，如果接到真实网卡，Host 还要负责 socket/网卡描述符的生命周期。
- **Lifecycle**：创建/销毁 `lua_State`，加载 `rules.lua` 的时机（本示例在 `main` 里一次性加载；真实系统通常还要支持"运维改了规则文件后热加载"）。
- **Stable API**：Host 与 Lua 之间约定好的 `packet` table 形状（`src`/`dst`/`port` 三个字段）和每条规则必须实现的 `match(packet)`/`action(packet)` 签名——这是 Host 对 Lua 单向承诺的契约，规则可以任意增删，但字段名和函数签名不能随便改。

## 5. Lua responsibilities

- **Rules**：`rules.lua` 里的整张 `rules` 数组，就是 Host/Lua 分工表里 Lua 侧的 Rules 职责——"输入满足什么条件，触发什么动作"的匹配表。
- 每条规则内部：`match` 是纯判断（读 `packet` 字段，返回布尔值，不产生副作用），`action` 是纯描述（返回一个字符串，描述"应该做什么"，不真正执行丢包/放行）。真正的副作用永远留给 Host。

## 6. Lifecycle

```text
Host 启动
  |
  v
luaL_newstate()          创建 lua_State
  |
  v
luaL_openlibs()          打开标准库（string.match 等需要）
  |
  v
luaL_dofile("rules.lua") 加载并立即执行，注册全局 rules 表
  |
  v
for each packet:
  evaluate_packet()      遍历 rules，pcall match/action
  |
  v
lua_close()              销毁 VM，进程退出前的最后一步
```

`rules.lua` 本身没有独立的生命周期概念（不像 Plugin 有 load/unload），它的"生命周期"完全依附于 `lua_State` 的生命周期：VM 活着，规则表就在；VM 销毁，规则表随之消失。

## 7. Data Flow（C -> Lua）

Host 侧的 `push_packet_table()` 把一个 C `Packet` 结构体拆成三个字段，逐个 `lua_setfield` 进一个新建的 Lua table：

```c
lua_newtable(L);
lua_pushstring(L, pkt->src);
lua_setfield(L, -2, "src");
lua_pushstring(L, pkt->dst);
lua_setfield(L, -2, "dst");
lua_pushinteger(L, pkt->port);
lua_setfield(L, -2, "port");
```

这是 C struct 无法直接塞进 Lua 值栈、必须由 Binding 层显式转换的典型例子（详见 [06-c-lua-embedding-mental-model.md](../06-c-lua-embedding-mental-model.md)）。每次调用 `match` 或 `action` 都会重新构造一份新的 `packet` table——简化点：真实系统里如果每秒要处理成千上万个包，这里应该考虑复用/池化 table，或者用 `userdata` + `metatable` 惰性读取字段，而不是每次都新建 table。

## 8. Control Flow（Lua -> C）

Lua 侧的 `match`/`action` 不会主动调用任何 Host 暴露的函数（这个示例里 Lua 没有反向调用 C），Control Flow 完全是"Host 主动发起"的单向调用：`lua_getglobal` 取 `rules` -> `lua_geti` 取第 i 条规则 -> `lua_getfield` 取 `match`/`action` -> `lua_pcall` 触发执行 -> `lua_toboolean`/`lua_tostring` 取回结果。Lua 只是被动地"被调用一下，返回一个值"，控制权始终留在 Host 手里。

## 9. State

- `rules` 数组本身是唯一的跨调用状态，存活在 `lua_State` 的全局表里，从加载到 `lua_close` 为止一直存在。
- 每条规则的 `match`/`action` 函数是无状态的纯函数（只读 `packet` 参数，不读写任何外部变量），这是有意为之的设计：规则之间互相独立，任意顺序执行都不会相互影响（除了 First Match 的"提前退出"这个由 Host 侧控制的顺序语义）。
- Host 侧没有为规则引擎维护额外的跨包状态（比如"这个 IP 最近 DROP 了几次"这类计数），如果要加，应该作为 Lua 侧的模块级变量或者 Host 侧的 C 结构，取决于这个状态更应该被谁"拥有"。

## 10. Failure

- **加载阶段**：`luaL_dofile` 失败（`rules.lua` 语法错误、文件不存在）会被 `if (... != LUA_OK)` 捕获，打印错误信息后 `lua_close` 并退出，不会让 Host 带着一个残缺的 VM 继续跑。
- **调用阶段**：`call_match`/`call_action` 内部的 `lua_pcall` 保证某条规则的 `match`/`action` 抛错（比如笔误 `packet.prot` 而不是 `packet.port`）只会让这一条规则被跳过（`matched < 0` -> `continue`），不会影响其余规则的判断，也不会导致 Host 进程崩溃。
- **契约违反**：如果某条规则忘了写 `match` 字段，或者 `match` 不是函数，`lua_isfunction` 检查会提前发现并打印 warning，同样只影响这一条规则。
- **兜底**：`default-drop` 规则（`match` 恒为 `true`）保证任何没有被前面规则命中的包都有一个确定的最终动作，不会出现"没有规则命中，Host 不知道该怎么办"的未定义状态。

## 11. Complete Host Code

`code/01-firewall-rule-engine/main.c`：

```c
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
```

## 12. Complete Lua Code

`code/01-firewall-rule-engine/rules.lua`：

```lua
-- rules.lua
-- Firewall Rule Engine：一组结构统一的规则，每条规则是 { name, match, action }
-- match(packet)  -> boolean          判断这个 packet 是否命中本条规则
-- action(packet) -> "ALLOW"/"DROP"/"LOG"  命中后应该执行的动作（只是描述，不产生真实副作用）
--
-- Host 端采用 First Match 语义：从上到下遍历，第一条 match 返回 true 的规则
-- 就决定了这个 packet 的最终动作，后面的规则不会再被判断。
-- 因此规则的先后顺序本身就是一种优先级，书写顺序很重要。

rules = {
    -- 规则 1：放行发往 22 端口（SSH）的流量
    {
        name = "allow-ssh",
        match = function(packet)
            return packet.port == 22
        end,
        action = function(packet)
            return "ALLOW"
        end
    },

    -- 规则 2：来自黑名单地址的流量直接丢弃（覆盖 src 字段的用法）
    {
        name = "drop-blacklisted-src",
        match = function(packet)
            return packet.src == "10.0.0.66"
        end,
        action = function(packet)
            return "DROP"
        end
    },

    -- 规则 3：内网（10.0.0.0/8）访问 DMZ（192.168.0.0/16）只记录日志，不拦截
    -- 覆盖 src + dst 两个字段联合判断的用法
    {
        name = "log-internal-to-dmz",
        match = function(packet)
            return packet.src:match("^10%.") ~= nil and packet.dst:match("^192%.168%.") ~= nil
        end,
        action = function(packet)
            return "LOG"
        end
    },

    -- 规则 4：放行 HTTP/HTTPS
    {
        name = "allow-http",
        match = function(packet)
            return packet.port == 80 or packet.port == 443
        end,
        action = function(packet)
            return "ALLOW"
        end
    },

    -- 规则 5：默认规则，兜底丢弃所有未被前面规则命中的流量
    -- 永远放在最后一条，match 恒为 true
    {
        name = "default-drop",
        match = function(packet)
            return true
        end,
        action = function(packet)
            return "DROP"
        end
    }
}
```

## 13. Execution

`code/01-firewall-rule-engine/Makefile`：

```makefile
# Makefile —— Firewall Rule Engine
#
# 依赖系统安装的 Lua 5.4 开发包（liblua5.4-dev / lua5.4-devel，视发行版而定）。
# 如果 pkg-config 里的包名不同，按需修改 LUA_PKG。

LUA_PKG ?= lua5.4

CC      ?= cc
CFLAGS  += -Wall -Wextra -std=c99 -g $(shell pkg-config --cflags $(LUA_PKG))
LDFLAGS += $(shell pkg-config --libs $(LUA_PKG))

TARGET  := firewall
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
cd code/01-firewall-rule-engine
make
./firewall
```

实际运行输出（5 个测试 packet，覆盖 ALLOW / DROP / LOG 三种 Action）：

```text
=== Firewall Rule Engine：逐包匹配结果（First Match）===

packet #1  src=192.168.1.10   dst=192.168.1.1    port=22     -> rule=allow-ssh                action=ALLOW
packet #2  src=10.0.0.66      dst=192.168.1.5    port=8080   -> rule=drop-blacklisted-src     action=DROP
packet #3  src=10.0.0.5       dst=192.168.2.20   port=53     -> rule=log-internal-to-dmz      action=LOG
packet #4  src=8.8.8.8        dst=192.168.1.100  port=443    -> rule=allow-http               action=ALLOW
packet #5  src=203.0.113.9    dst=192.168.1.50   port=12345  -> rule=default-drop             action=DROP
```

## 14. Step-by-step walkthrough

以 packet #3（`src=10.0.0.5, dst=192.168.2.20, port=53`）为例，走一遍完整调用链：

1. `main` 从 `packets[]` 里取出这个 packet，调用 `evaluate_packet`。
2. `evaluate_packet` 用 `lua_getglobal(L, "rules")` 把全局 `rules` 表压栈，`lua_rawlen` 得到长度 5。
3. `i=1`：`call_match` 取出 `rules[1]`（`allow-ssh`）的 `match`，压入 `packet` table，`lua_pcall` 执行 `packet.port == 22`，`53 ~= 22`，返回 `false`。
4. `i=2`：`rules[2]`（`drop-blacklisted-src`）的 `match` 判断 `packet.src == "10.0.0.66"`，`"10.0.0.5" ~= "10.0.0.66"`，返回 `false`。
5. `i=3`：`rules[3]`（`log-internal-to-dmz`）的 `match` 判断 `packet.src:match("^10%.")`（对 `"10.0.0.5"` 匹配成功）并且 `packet.dst:match("^192%.168%.")`（对 `"192.168.2.20"` 匹配成功），两个条件都成立，返回 `true`。
6. `evaluate_packet` 发现 `matched == 1`，取出 `rules[3].name`（`"log-internal-to-dmz"`）存入 `rule_name_out`。
7. 调用 `call_action`，执行 `rules[3].action(packet)`，Lua 侧直接返回字符串 `"LOG"`，`lua_tostring` 取回后存入 `action_out`。
8. `evaluate_packet` 立即 `return 1`，**不再判断 `rules[4]`、`rules[5]`**——这就是 First Match：即使后面可能还有别的规则理论上也能匹配这个包，一旦前面命中就不再往下看。
9. `main` 打印这一行结果：`rule=log-internal-to-dmz action=LOG`。

## 15. Mental Model

```text
Packet (C struct)
      |
      v  push_packet_table
Packet (Lua table)
      |
      v
for i = 1..N:
   rules[i].match(packet) --pcall--> true/false
      |
      | 第一个 true 处
      v
   rules[i].action(packet) --pcall--> "ALLOW"/"DROP"/"LOG"
      |
      v
Host 打印/执行这个动作，其余规则不再判断
```

记忆点：**规则表是数据，遍历规则表的循环是 Host 侧固定不变的 Mechanism**。改变防火墙行为，永远只需要编辑 `rules.lua` 这份数据（增删一条 `{match=..., action=...}`），不需要碰 `main.c` 里遍历/调用规则的那套逻辑。

## 16. Why this is this Pattern

这是 Rule Engine 而不是 Policy：因为这里不是"给定一个包，用一段任意逻辑决定一个答案"，而是"有一组结构统一的规则（`match`+`action`），逐条独立判断是否命中"。如果把 `rules` 数组换成一个单一的 `decide(packet)` 函数，直接在函数体里写一串 `if port==22 then return "ALLOW" elseif ... end`，那就退化成了 Policy——虽然效果类似，但失去了"规则是可以被单独增删、列举、审计的数据项"这个特征，这正是 Rule Engine 和 Policy 的分界线。

这也不是 Extension：Lua 没有给 Host 新增任何它原来不具备的能力（Host 一直就有"丢包/放行/记日志"这三种能力），Lua 只是在决定"这一次该用哪种已有能力"，但决定的方式是"匹配一组规则"而不是"运行一段任意决策逻辑"（后者才是 Policy）。

**如果 Lua 消失，Host 还能做什么？** Host 依然可以运行——防火墙的核心机制（收包、丢包、转发、记日志的能力）完全在 C 侧，不依赖 Lua。但会退回到最开始 Problem 里描述的困境：所有规则要写死在 C 代码里，每次改规则都要重新编译发布整个二进制，规则数量一多，`if/else` 链会变得极难维护和审计。Lua 消失后，Host 失去的是"规则可以作为独立于二进制的数据被运行时加载、审计、修改"这个能力，不是"判断和执行"的能力本身。

**如果把 Lua 换成 JSON/YAML/纯 C，这个 Pattern 还成立吗？** `Input -> Predicate -> Match -> Action` 这个抽象本身完全成立，只是 Predicate 和 Action 的表达能力不同：
- 换成 **JSON/YAML**：可以描述"字段等于某个值"这种简单条件（例如 `{"field": "port", "op": "==", "value": 22}`），但没法直接表达"内网访问 DMZ"这种需要字符串前缀匹配、多字段联合判断的逻辑，除非再发明一套条件表达式的 mini-DSL 并为它写一个专门的求值器——这本质上是把 Lua 解释器的活自己重新做一遍，只是表达力更弱。规则5（默认规则 `match` 恒为 `true`）用 JSON 也很容易描述，但规则3那种正则式前缀匹配就会明显吃力。
- 换成 **纯 C**：完全可行（很多真实防火墙内核模块就是纯 C/eBPF 实现的），但代价就是 Problem 里说的那些——改规则要重新编译、出错是内存问题不是可控错误、运维不能自己改规则。

因此选择 Lua 不是因为"规则匹配"这件事本身需要 Lua，而是因为 Lua 恰好在"表达力"和"运行时可安全替换"之间给出了比 JSON/YAML 更强、比纯 C 更安全的折中点。
