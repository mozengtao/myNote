# 03 · 示例：游戏 NPC Decision Policy

> 前置阅读：[00-why-and-architecture.md](00-why-and-architecture.md)。代码位于 [code/03-npc-decision-policy/](code/03-npc-decision-policy/)。

## 1. Problem（问题）

一个游戏引擎已经实现好了 NPC 需要的所有底层能力：怎么播放攻击动画并计算伤害（战斗系统）、怎么寻路到掩体（导航系统）、怎么沿着预设路径点巡逻（AI 移动系统）。这些系统本身很复杂，一旦写好，改动频率很低，改错的代价很高（动画错位、寻路卡死、帧率下降）。

真正经常变的，是"NPC 在什么情况下应该选哪个动作"这条决策逻辑——策划可能这周想让 NPC 血量低于 20% 就逃跑，下周想改成 30%；这个星期想让 NPC 看到玩家就立刻攻击，下个星期想加上"距离太远先不攻击"的判断。如果这条决策逻辑写死在 C++ 战斗系统内部，策划每调一次数值/逻辑都要找程序员改代码、重新编译、重新测试整个游戏，迭代速度完全跟不上设计需要。

## 2. Why Lua（为什么用 Lua）

这是游戏行业采用 Lua 做 AI/技能/关卡脚本最经典的场景之一：把"状态判断到动作选择"这条决策逻辑从 C++ 里剥离出来，交给策划或者 AI 设计师直接编辑的 Lua 脚本。好处：

- 策划改一次决策阈值（比如把"血量危险线"从 20 调到 30），只需要改 `npc_policy.lua` 里的一个数字，不需要重新编译整个游戏引擎；
- 决策逻辑里如果有 bug（比如误把优先级写反，导致 NPC 一直站着不动），`lua_pcall` 会把它变成一次可捕获的错误，引擎可以给这个 NPC 一个安全的默认动作（比如继续 PATROL），不会导致整局游戏崩溃；
- 同一套引擎代码（`npc_engine.c`）可以给不同种类的 NPC 配不同的 `npc_policy.lua`，甚至同一个 NPC 在不同关卡换一套决策脚本，都不需要碰 C++ 代码。

## 3. Architecture（架构）

```text
NPC
 |
 +-- distance
 +-- HP
 +-- enemy state
 |
 v
Lua Policy: decide_action(distance, hp, enemy_visible)
 |
 +-- ATTACK
 +-- FLEE
 +-- PATROL
 |
 v
Engine: execute_action()
```

`distance`/`HP`/`enemy state` 是 Engine 在每一轮（可以理解成每个逻辑帧/每个 AI tick）维护并计算出来的状态快照，全部属于 Mechanism 层——物理引擎的碰撞查询、AI 感知系统的视野判断、战斗系统的伤害结算，都在 Engine 内部完成，Lua 从不接触这些系统的实现细节。Lua 唯一做的事情，是把这三个数值翻译成三个动作名字里的一个；Engine 拿到这个名字后，回到自己的 Mechanism 代码（动画/寻路/移动系统），真正执行对应的动作。

## 4. Host responsibilities（Host/Engine 的职责）

- 创建并管理 `lua_State` 的完整生命周期。
- 负责状态从哪里来：本示例用一个预设的多轮状态数组模拟（真实引擎里会来自物理引擎、感知系统、战斗系统的实时计算）。
- 提供 Policy API 调用点 `decide_action`：把当前状态压栈，调用 Lua，取回动作字符串。
- 拥有并执行真正的 Mechanism：`execute_action` 按动作字符串调用对应的动画/寻路/巡逻逻辑（本示例用 printf 模拟）。
- 处理 Lua 侧所有失败情况（脚本加载失败、`decide_action` 未定义、`lua_pcall` 报错、返回值不是字符串），并在失败时选择一个安全的默认处理（跳过这一轮，而不是让引擎状态陷入不确定）。

## 5. Lua responsibilities（Lua 的职责）

- 定义决策阈值：`HP_DANGER_THRESHOLD`（血量危险线）、`ATTACK_RANGE`（攻击距离）。
- 实现 `decide_action(distance, hp, enemy_visible)`：按优先级（保命 > 攻击 > 巡逻）判断，返回 `"ATTACK"`/`"FLEE"`/`"PATROL"` 三者之一。
- 不知道、也不需要知道"ATTACK"具体会触发哪个动画、"FLEE"具体会怎么寻路——这些都是 Engine 侧 Mechanism 的事情，Lua 只负责给出动作的名字。

## 6. Lifecycle（生命周期）

```text
luaL_newstate() + luaL_openlibs()
    |
    v
luaL_loadfile + lua_pcall 加载 npc_policy.lua
                (decide_action 全局函数就位)
    |
    v
for each round (共 6 轮):
    decide_action(distance, hp, enemy_visible)  <-- 每轮调用一次
    execute_action(action, round)
    |
    v
lua_close()
```

与示例 2（OpenResty）"每个请求重新执行一次脚本"不同，本示例的 `npc_policy.lua` 只在启动时被加载/执行一次（这一次执行只是定义了 `decide_action` 函数和两个阈值常量），之后每一轮都是**调用同一个已经定义好的函数**，不是重新加载脚本。这一点和示例 1（C Server Access Policy）的生命周期模式完全一致。

## 7. Data Flow（C -> Lua）

```text
C: npc_state_t { distance, hp, enemy_visible }
    |
    v
lua_pushnumber(L, distance)
lua_pushinteger(L, hp)
lua_pushboolean(L, enemy_visible)
    |
    v
Lua: decide_action(distance, hp, enemy_visible)
```

三个字段分别用三种不同的 push 函数跨过 C/Lua 边界：`distance` 是浮点数用 `lua_pushnumber`，`hp` 是整数用 `lua_pushinteger`（在 Lua 5.3+ 里对应一个真正的整数子类型，在更早版本里等价于 `lua_pushnumber`），`enemy_visible` 是 C 里的 `int`（0/1）用 `lua_pushboolean` 转换成 Lua 的布尔值——这一步转换很重要：如果直接用 `lua_pushinteger` 传 0/1，Lua 侧的 `enemy_visible` 就会是数字 `0`/`1` 而不是布尔值，虽然在 `if enemy_visible then` 这种写法下数字 `0` 和 `1` 都是"真"（Lua 里只有 `nil` 和 `false` 是假），逻辑上可能凑巧不出错，但类型语义是错的，容易在后续维护中引发困惑，所以显式用 `lua_pushboolean` 是更严谨的写法。

## 8. Control Flow（Lua -> C）

```text
Lua: return "ATTACK" / "FLEE" / "PATROL"
    |
    v
lua_pcall(L, 3, 1, 0) 返回，栈顶是动作字符串
    |
    v
C: const char *action = lua_tostring(L, -1)
    |
    v
snprintf 复制进调用者缓冲区（避免悬挂指针）
    |
    v
execute_action(action, round)   <-- 控制权已完全回到 C
```

`decide_action` 用 `return` 一个字符串结束，控制权和这个字符串值一起交还给 `lua_pcall` 的调用点。C 侧用 `lua_tostring` 取出这个字符串，但必须在对应的栈位置被 `lua_pop` 之前，把内容复制进自己的缓冲区——因为 Lua 字符串是被 GC 管理的对象，`lua_pop` 之后这块内存随时可能被回收或被后续的 Lua 代码复用。复制完成后，控制权完全回到 C，`execute_action` 按字符串内容分派到具体的 Mechanism 分支，这一步 Lua 完全不参与。

## 9. State（状态归属）

- **C/Engine 侧状态**：`g_rounds` 这组预设状态序列，代表"NPC 在世界中的真实处境"，属于 Engine 的运行时状态；真实引擎里这些数值来自物理引擎和感知系统，生命周期跟游戏世界本身绑定。
- **Lua 侧状态**：`HP_DANGER_THRESHOLD`、`ATTACK_RANGE` 两个模块级 `local` 常量，代表"决策使用的阈值参数"。它们是 Lua 侧的私有状态，Engine 不能直接读取或修改，只能通过修改 `npc_policy.lua` 文件来调整。
- 本示例里 `decide_action` 是一个**无状态**的纯函数——不依赖任何跨轮次保留的 Lua 变量，每一轮的决策只取决于这一轮传入的三个参数。如果要实现"记仇"这类需要跨轮次记忆的行为（比如"最近被攻击过的 NPC 更倾向于逃跑"），需要在 Lua 侧引入模块级的可变状态（例如一个记录"上次受伤时间"的 `local` 变量），这会让 Lua 侧从"纯 Policy"逐渐带上一点"内部状态"的色彩，但只要 Engine 依然只通过 `decide_action` 这一个入口交互，架构上仍然属于 Policy 模式。

## 10. Failure（失败路径）

- **脚本加载失败**（文件不存在/语法错误）：`luaL_loadfile` 返回非 `LUA_OK`，`main` 打印错误信息、`lua_close`、直接退出，这是启动阶段的致命错误，不应该带着坏掉的决策脚本继续跑游戏逻辑。
- **`decide_action` 未定义**：`decide_action` 函数里 `lua_isfunction` 检查失败，打印错误、返回 -1，`main` 循环里 `continue`，跳过这一轮的 `execute_action`（相当于"这一轮什么都不做"），下一轮继续尝试。
- **`decide_action` 运行时报错**（比如脚本里意外访问了一个 `nil` 值）：`lua_pcall` 返回非 `LUA_OK`，打印错误信息，同样跳过这一轮。
- **返回值不是字符串**（比如脚本改错返回了一个数字或者 `nil`）：`lua_tostring` 在这种情况下返回 `NULL`（`lua_tostring` 对无法转换成字符串的类型返回 `NULL`，例如 `table`/`nil`/`boolean`），代码显式检查并跳过这一轮。
- 统一原则：Policy 调用失败时，Engine 选择"跳过这一轮、维持 NPC 当前状态不变"而不是让程序崩溃，也不是随意猜测一个动作——**跳过本身就是一种安全默认值**，这一点和示例 1 里"失败就保守拒绝"的思路是同一个原则在不同场景下的体现：都是"失败时选择影响面最小的默认结果"。

## 11. Complete Host Code（完整 Host 代码）

对应文件：[code/03-npc-decision-policy/npc_engine.c](code/03-npc-decision-policy/npc_engine.c)

```c
/*
 * npc_engine.c
 *
 * 一个简化的游戏引擎状态循环：模拟一个 NPC 在若干轮（可以理解成
 * 若干个逻辑帧/若干个 AI tick）里的状态变化，每一轮把最新状态
 * （与玩家的 distance、自身 hp、玩家是否可见 enemy_visible）
 * 交给 Lua 侧的 decide_action() 函数，由它决定这一轮 NPC 应该
 * 做什么，engine 只负责"真正执行"这个决策代表的动作。
 *
 * 架构：
 *   main() 创建 Lua VM
 *     -> luaL_dofile 加载 npc_policy.lua
 *     -> for each round in 预设状态序列:
 *         -> decide_action(): lua_getglobal + lua_pushnumber/lua_pushinteger/
 *            lua_pushboolean 三个参数 + lua_pcall(L, 3, 1, 0) + lua_tostring
 *            取回动作字符串
 *         -> execute_action(): 按动作字符串真正执行对应的引擎逻辑
 *            （播放动画、寻路、巡逻），Lua 全程不接触动画系统/寻路系统
 *     -> lua_close 释放 VM
 *
 * Engine = Mechanism：谁负责维护 NPC 的状态（distance/hp/enemy_visible
 * 从哪里来）、谁负责把决策字符串翻译成真正的动画播放/寻路调用，
 * 这些都是 engine 的职责，Lua 完全不参与。
 * Lua = Decision Policy：只回答"给定这组状态，应该选 ATTACK / FLEE /
 * PATROL 里的哪一个"，不知道动画系统怎么播放、寻路系统怎么计算路径。
 *
 * 简化点：真实游戏引擎里，distance/hp/enemy_visible 来自物理引擎的
 * 碰撞查询、AI 感知系统（视野锥、遮挡检测）、战斗系统的实时计算，
 * 这里用一个固定的状态数组代替，只是为了让本示例的输出可复现、
 * 且能确定性地覆盖 ATTACK / FLEE / PATROL 三种决策。
 */

#include <stdio.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

typedef struct {
    double distance;    /* 与玩家的距离 */
    int hp;              /* NPC 当前生命值，满值假设为 100 */
    int enemy_visible;   /* 是否能看到玩家：0/1，作为 Lua boolean 传入 */
} npc_state_t;

/*
 * 预设的多轮状态序列。
 * 覆盖三种典型场景：
 *   round 1: 距离远、状态健康、看不到玩家           -> 期望 PATROL
 *   round 2: 距离近、状态健康、能看到玩家           -> 期望 ATTACK
 *   round 3: 血量危险（即使敌人可见且很近）         -> 期望 FLEE（保命优先）
 *   round 4: 能看到玩家但距离太远，attack 打不到     -> 期望 PATROL
 *   round 5: 距离很近、血量健康、能看到玩家         -> 期望 ATTACK
 *   round 6: 血量危险、看不到玩家                   -> 期望 FLEE
 */
static const npc_state_t g_rounds[] = {
    {10.0, 100, 0},
    { 3.0,  80, 1},
    { 2.0,  15, 1},
    { 8.0,  60, 1},
    { 1.5,  50, 1},
    { 4.0,  10, 0},
};
#define NUM_ROUNDS (int)(sizeof(g_rounds) / sizeof(g_rounds[0]))

/*
 * decide_action
 *
 * 调用 Lua 侧的 decide_action(distance, hp, enemy_visible) -> string。
 *
 * 调用协议：
 *   lua_getglobal(L, "decide_action")
 *   lua_pushnumber / lua_pushinteger / lua_pushboolean   三个参数依次入栈
 *   lua_pcall(L, 3, 1, 0)                                3 个参数，1 个返回值
 *   lua_tostring(L, -1)                                  取回动作字符串
 *
 * lua_tostring 返回的指针指向 Lua 内部字符串对象，一旦 lua_pop 之后
 * 就可能被 GC 回收，所以必须在 pop 之前用 snprintf 把内容复制进
 * 调用者提供的缓冲区，不能返回或长期持有这个指针本身。
 */
static int decide_action(lua_State *L, const npc_state_t *s, char *out_buf, size_t buf_size) {
    lua_getglobal(L, "decide_action");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        fprintf(stderr, "[Engine] 错误: decide_action 未定义\n");
        return -1;
    }

    lua_pushnumber(L, s->distance);
    lua_pushinteger(L, s->hp);
    lua_pushboolean(L, s->enemy_visible);

    if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
        fprintf(stderr, "[Engine] decide_action 执行失败: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return -1;
    }

    const char *action = lua_tostring(L, -1);
    if (!action) {
        fprintf(stderr, "[Engine] decide_action 返回值不是字符串\n");
        lua_pop(L, 1);
        return -1;
    }
    snprintf(out_buf, buf_size, "%s", action);
    lua_pop(L, 1);
    return 0;
}

/*
 * execute_action
 *
 * Engine 侧真正的 Mechanism 层：按 Lua 给出的动作字符串，
 * 调用对应的动画系统/寻路系统/战斗系统。这里用 printf 模拟。
 * Lua 只给了一个动作名字，怎么把这个名字变成真正的游戏行为，
 * 完全是 C 侧的职责，Lua 对动画/寻路系统的存在一无所知。
 */
static void execute_action(const char *action, int round) {
    if (strcmp(action, "ATTACK") == 0) {
        printf("[Engine] 第 %d 轮: 执行攻击逻辑，播放 attack.anim，对玩家造成伤害\n", round);
    } else if (strcmp(action, "FLEE") == 0) {
        printf("[Engine] 第 %d 轮: 执行逃跑逻辑，寻路到最近的掩体\n", round);
    } else if (strcmp(action, "PATROL") == 0) {
        printf("[Engine] 第 %d 轮: 执行巡逻逻辑，沿预设路径点移动\n", round);
    } else {
        printf("[Engine] 第 %d 轮: 未知动作 '%s'，忽略\n", round, action);
    }
}

int main(int argc, char **argv) {
    const char *script_path = (argc > 1) ? argv[1] : "npc_policy.lua";

    /* 1. 创建 Lua VM */
    lua_State *L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "无法创建 Lua VM\n");
        return 1;
    }

    /* 2. 打开标准库 */
    luaL_openlibs(L);

    /* 3. 加载并执行 npc_policy.lua（脚本只定义 decide_action 全局函数） */
    if (luaL_loadfile(L, script_path) != LUA_OK) {
        fprintf(stderr, "[Engine] 加载脚本失败: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }
    if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK) {
        fprintf(stderr, "[Engine] 执行脚本失败: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    printf("[Engine] NPC 决策引擎启动，共模拟 %d 轮\n", NUM_ROUNDS);

    /* 4. 逐轮驱动状态机：C 负责状态从哪里来、决策结果如何被执行，
     *    Lua 只负责中间这一步"选哪个动作"。 */
    for (int i = 0; i < NUM_ROUNDS; i++) {
        const npc_state_t *s = &g_rounds[i];
        printf("---\n第 %d 轮状态: distance=%.1f hp=%d enemy_visible=%s\n",
               i + 1, s->distance, s->hp, s->enemy_visible ? "true" : "false");

        char action[32];
        if (decide_action(L, s, action, sizeof(action)) != 0) {
            continue;
        }
        printf("[Engine] Lua Policy 决策: %s\n", action);
        execute_action(action, i + 1);
    }

    /* 5. 释放 Lua VM */
    lua_close(L);
    printf("---\n[Engine] 模拟结束，Lua VM 已释放。\n");
    return 0;
}
```

## 12. Complete Lua Code（完整 Lua 代码）

对应文件：[code/03-npc-decision-policy/npc_policy.lua](code/03-npc-decision-policy/npc_policy.lua)

```lua
-- npc_policy.lua
--
-- 这是 Engine（npc_engine.c）在启动时加载的 Lua 决策脚本。
-- 它只定义一个全局函数：
--
--   decide_action(distance, hp, enemy_visible) -> "ATTACK" | "FLEE" | "PATROL"
--
-- Engine 已经知道"怎么执行"每一种动作（Mechanism：动画系统、
-- 寻路系统、巡逻路径系统），不知道"这一轮该选哪一种"（Decision），
-- 这个函数只回答后者。
--
-- 决策优先级（从高到低）：
--   1. 血量危险 -> 无论敌人是否可见，优先逃跑（保命优先于战斗）
--   2. 敌人可见且距离足够近 -> 攻击
--   3. 其余情况 -> 巡逻（包括"看到敌人但太远打不到"的情况）

local HP_DANGER_THRESHOLD = 20
local ATTACK_RANGE = 5.0

function decide_action(distance, hp, enemy_visible)
    if hp <= HP_DANGER_THRESHOLD then
        return "FLEE"
    end

    if enemy_visible and distance <= ATTACK_RANGE then
        return "ATTACK"
    end

    return "PATROL"
end

print("[npc_policy.lua] 已加载，NPC decision policy 就位")
```

## 13. Execution（运行方式）

```bash
cd code/03-npc-decision-policy
make
./npc_engine
```

同样需要系统安装 Lua 5.4 开发包，`Makefile` 用 `pkg-config lua5.4` 探测编译参数，按需替换 `LUA_PKG`。

## 14. Step-by-step walkthrough（逐步执行走读）

完整输出：

```text
$ make
$ ./npc_engine
[npc_policy.lua] 已加载，NPC decision policy 就位
[Engine] NPC 决策引擎启动，共模拟 6 轮
---
第 1 轮状态: distance=10.0 hp=100 enemy_visible=false
[Engine] Lua Policy 决策: PATROL
[Engine] 第 1 轮: 执行巡逻逻辑，沿预设路径点移动
---
第 2 轮状态: distance=3.0 hp=80 enemy_visible=true
[Engine] Lua Policy 决策: ATTACK
[Engine] 第 2 轮: 执行攻击逻辑，播放 attack.anim，对玩家造成伤害
---
第 3 轮状态: distance=2.0 hp=15 enemy_visible=true
[Engine] Lua Policy 决策: FLEE
[Engine] 第 3 轮: 执行逃跑逻辑，寻路到最近的掩体
---
第 4 轮状态: distance=8.0 hp=60 enemy_visible=true
[Engine] Lua Policy 决策: PATROL
[Engine] 第 4 轮: 执行巡逻逻辑，沿预设路径点移动
---
第 5 轮状态: distance=1.5 hp=50 enemy_visible=true
[Engine] Lua Policy 决策: ATTACK
[Engine] 第 5 轮: 执行攻击逻辑，播放 attack.anim，对玩家造成伤害
---
第 6 轮状态: distance=4.0 hp=10 enemy_visible=false
[Engine] Lua Policy 决策: FLEE
[Engine] 第 6 轮: 执行逃跑逻辑，寻路到最近的掩体
---
[Engine] 模拟结束，Lua VM 已释放。
```

逐轮对照 `npc_policy.lua` 里的决策优先级：

1. **第 1 轮**：`hp=100` 远高于危险线，`enemy_visible=false` 直接排除 ATTACK 分支，落到默认的 PATROL。
2. **第 2 轮**：`hp=80` 健康，`enemy_visible=true` 且 `distance=3.0 <= ATTACK_RANGE(5.0)`，命中 ATTACK。
3. **第 3 轮**：`hp=15 <= HP_DANGER_THRESHOLD(20)`，优先级最高的"保命"规则命中，即使 `enemy_visible=true` 且 `distance=2.0`（本来足够发起攻击），依然选择 FLEE——这一轮直观地展示了"优先级"的意义：不是"满足哪个条件就执行哪个"，而是"按顺序检查，第一个满足的生效"。
4. **第 4 轮**：`hp=60` 健康，`enemy_visible=true` 但 `distance=8.0 > ATTACK_RANGE(5.0)`，ATTACK 条件里的距离子条件不满足，落到 PATROL——展示"看到敌人不代表一定攻击，还要看打不打得到"。
5. **第 5 轮**：`hp=50` 健康，`distance=1.5 <= 5.0` 且可见，命中 ATTACK。
6. **第 6 轮**：`hp=10 <= 20`，命中 FLEE，这一轮 `enemy_visible=false`，进一步说明 FLEE 这条规则的判断压根不依赖 `enemy_visible`——血量到了危险线就跑，不管看不看得到敌人。

六轮覆盖了 PATROL（第 1、4 轮）、ATTACK（第 2、5 轮）、FLEE（第 3、6 轮）三种动作各至少两次。

## 15. Mental Model（心智模型）

```text
Engine
 |
 +-- Mechanism: 动画系统 / 寻路系统 / 巡逻路径系统 / 状态采集
 |
 +-- Policy API: decide_action(distance, hp, enemy_visible)  (每轮调用一次)
         |
         v
        Lua
         |
         +-- 阈值: HP_DANGER_THRESHOLD / ATTACK_RANGE
         |
         +-- Decision: "ATTACK" / "FLEE" / "PATROL"
```

记忆点：策划改一次"血量低于多少就该逃跑"，动的是 `npc_policy.lua` 里的一个数字，不是 `npc_engine.c` 里的战斗系统代码——决策阈值和执行动画/寻路的代码，物理上分离在两个文件、两种语言里。

## 16. Why this is this Pattern（为什么这是 Policy 而不是别的模式）

这是 Policy，不是 Extension：`npc_engine.c` 在引入 Lua 之前，`execute_action` 里的 ATTACK/FLEE/PATROL 三个分支就已经存在、已经能正常执行对应的游戏逻辑，Lua 没有让 Engine 多学会一种新动作类型，只是把"这一轮该选哪个已有动作"这个判断从可能写死在状态机 C 代码里的逻辑，挪进了一个可以独立调整的脚本。

这也不是 Configuration：本示例返回的不是一份运行时参数表（比如"这个 NPC 的移动速度是多少""巡逻半径是多少"），而是每一轮都要重新计算一次的、依赖当前具体场景（这一刻的 distance/hp/enemy_visible）的实时判断结果——Configuration 通常只在加载时读一次，Policy 在这里每一帧/每一轮都会被重新调用。

- **如果 Lua 消失，Host 还能做什么？** Engine 依然能完整地播放攻击动画、执行寻路、沿路径点巡逻——这些 Mechanism 从未依赖 Lua。会消失的是"策划可以自己调整决策逻辑而不找程序员改代码"这一能力：`decide_action` 的逻辑必须写回 C/C++（例如恢复成一段状态机代码），任何阈值调整都需要重新编译整个游戏客户端，AI 行为的迭代速度会大幅下降。
- **如果把 Lua 换成 JSON/YAML/纯 C，这个 Pattern 还成立吗？** "状态判断到动作选择"这个 Pattern 本身依然成立。换成纯 C：能表达任意复杂的决策逻辑，但每次调阈值都要重新编译，策划无法自己动手。换成 JSON/YAML：如果决策逻辑本身能被固定成一种通用结构（比如"一组按优先级排列的条件-动作对，条件只是简单的数值比较"），完全可以把 `HP_DANGER_THRESHOLD`、`ATTACK_RANGE` 这类**阈值参数**，甚至"条件类型 + 比较符 + 阈值 + 动作名"这种结构化的规则表都外部化成 JSON/YAML，由 C++ 写一个通用的小解释器去读取和执行——这其实已经非常接近 [04-rule-engine/](../04-rule-engine/00-why-and-architecture.md) 里 Rule Engine 的思路（用统一结构表达一组条件-动作对）。但只要决策逻辑里出现"任意形状"的计算（比如"根据 NPC 的性格参数、和队友的距离、剩余弹药量做一个加权评分再选动作"），这种自由形式的计算逻辑就超出了 JSON/YAML 纯数据格式能表达的范围，Lua 作为"既能是数据、又能是代码"的载体，能在不改 C++ 的前提下继续承载这种复杂度，纯配置格式做不到。
