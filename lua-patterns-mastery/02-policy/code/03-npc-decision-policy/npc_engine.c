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
