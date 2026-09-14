/*
 * main.c
 *
 * Game Host：加载 game_config.lua，读取其中的 player 表（单层表）
 * 和 weapons 表（"表的表"），转换成 C struct / struct 数组，
 * 再用这些数据"初始化游戏"（打印初始化后的玩家/武器参数）。
 *
 * 架构：
 *   main() 创建 Lua VM
 *     -> 注入只读全局变量 difficulty（模拟存档/命令行传入的难度等级）
 *     -> luaL_loadfile + lua_pcall 加载并执行 game_config.lua
 *     -> lua_getglobal("player")  -> 逐字段 lua_getfield 读取
 *     -> lua_getglobal("weapons") -> lua_pushnil + lua_next 遍历
 *        每个 weapon 子表，转换成 WeaponConfig 数组
 *     -> lua_close 释放 VM
 *     -> 用 C struct 初始化游戏（打印结果）
 *
 * 简化点：lua_next 的遍历顺序由 Lua table 的内部哈希布局决定，
 * 不保证与源码里书写的顺序一致；生产代码如果需要稳定顺序，
 * 应该额外维护一个数组（例如 `weapon_order = {"sword","bow","staff"}`）
 * 或者让配置本身用数组形式书写。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define MAX_WEAPONS 16

typedef struct {
    double speed;
    int jump_height;
    int max_hp;
} PlayerConfig;

typedef struct {
    char name[32];
    double damage;
    double speed;
    int has_mana_cost; /* 是否存在 mana_cost 这个可选字段 */
    double mana_cost;
} WeaponConfig;

/* 从 table_idx 指向的 table 中读取一个 number 字段，语义同示例 1 的 get_int_field。 */
static int get_number_field(lua_State *L, int table_idx, const char *field,
                             int required, double default_value, double *out) {
    lua_getfield(L, table_idx, field);
    int type = lua_type(L, -1);

    if (type == LUA_TNIL) {
        lua_pop(L, 1);
        if (required) {
            fprintf(stderr, "[Config] 缺少必填字段: %s\n", field);
            return 0;
        }
        *out = default_value;
        return 0; /* 明确告知调用方：用的是默认值，字段本身不存在 */
    }

    if (type != LUA_TNUMBER) {
        fprintf(stderr, "[Config] 字段类型错误: %s 应为 number，实际是 %s\n",
                field, lua_typename(L, type));
        lua_pop(L, 1);
        *out = default_value;
        return -1; /* 明确的错误，调用方必须视为失败 */
    }

    *out = lua_tonumber(L, -1);
    lua_pop(L, 1);
    return 1; /* 成功读到真实值 */
}

static int get_int_field(lua_State *L, int table_idx, const char *field,
                          int required, int default_value, int *out) {
    double d;
    int rc = get_number_field(L, table_idx, field, required, (double)default_value, &d);
    if (rc < 0) {
        return 0; /* 类型错误 */
    }
    if (rc == 0 && required) {
        return 0; /* 必填但缺失 */
    }
    *out = (int)d;
    return 1;
}

/* 读取 player 表 */
static int load_player_config(lua_State *L, PlayerConfig *out) {
    lua_getglobal(L, "player");
    if (lua_type(L, -1) != LUA_TTABLE) {
        fprintf(stderr, "[Config] 全局变量 'player' 不存在或不是 table\n");
        lua_pop(L, 1);
        return 0;
    }
    int idx = lua_gettop(L);

    int ok = 1;
    double speed = 0;
    int rc = get_number_field(L, idx, "speed", 1, 0, &speed);
    ok = ok && (rc == 1);
    out->speed = speed;

    ok = ok && get_int_field(L, idx, "jump_height", 1, 0, &out->jump_height);
    ok = ok && get_int_field(L, idx, "max_hp", 0, 100, &out->max_hp);

    lua_pop(L, 1); /* 弹出 player table */
    return ok;
}

/*
 * 读取 weapons 表（表的表）：
 *   lua_pushnil 压入一个"起始 key"
 *   每次 lua_next(L, tbl_idx) 弹出旧 key、压入下一对 (key, value)
 *   直到 lua_next 返回 0（表示遍历完毕，且已经把最后的 key 弹出）
 */
static int load_weapons_config(lua_State *L, WeaponConfig *out, int max_out, int *out_count) {
    lua_getglobal(L, "weapons");
    if (lua_type(L, -1) != LUA_TTABLE) {
        fprintf(stderr, "[Config] 全局变量 'weapons' 不存在或不是 table\n");
        lua_pop(L, 1);
        return 0;
    }
    int tbl_idx = lua_gettop(L);

    int ok = 1;
    int count = 0;
    lua_pushnil(L); /* 第一个 key */
    while (lua_next(L, tbl_idx) != 0) {
        /* 栈: ... weapons(tbl_idx) key(-2) value(-1) */
        if (lua_type(L, -2) != LUA_TSTRING) {
            fprintf(stderr, "[Config] weapons 表里出现非字符串 key，已跳过\n");
            lua_pop(L, 1); /* 只弹出 value，把 key 留给下一次 lua_next 当输入 */
            continue;
        }
        const char *wname = lua_tostring(L, -2);

        if (lua_type(L, -1) != LUA_TTABLE) {
            fprintf(stderr, "[Config] weapons.%s 不是 table，已跳过\n", wname);
            lua_pop(L, 1);
            continue;
        }

        if (count >= max_out) {
            fprintf(stderr, "[Config] 武器数量超过上限 %d，'%s' 被丢弃\n", max_out, wname);
            lua_pop(L, 1);
            continue;
        }

        int weapon_idx = lua_gettop(L);
        WeaponConfig *w = &out[count];
        snprintf(w->name, sizeof(w->name), "%s", wname);

        double damage = 0, speed = 0, mana = 0;
        int rc_damage = get_number_field(L, weapon_idx, "damage", 1, 0, &damage);
        int rc_speed = get_number_field(L, weapon_idx, "speed", 1, 0, &speed);
        if (rc_damage != 1 || rc_speed != 1) {
            fprintf(stderr, "[Config] weapons.%s 缺少必填字段或类型错误，已跳过\n", wname);
            lua_pop(L, 1);
            continue;
        }
        w->damage = damage;
        w->speed = speed;

        int rc_mana = get_number_field(L, weapon_idx, "mana_cost", 0, -1, &mana);
        w->has_mana_cost = (rc_mana == 1);
        w->mana_cost = mana;

        count++;
        lua_pop(L, 1); /* 弹出 value（weapon 子表），只留 key 给下一次 lua_next */
    }

    lua_pop(L, 1); /* 弹出 weapons table */
    *out_count = count;
    return ok;
}

static void init_game(const PlayerConfig *player, const WeaponConfig *weapons, int weapon_count) {
    printf("========================================\n");
    printf("[Game] 正在初始化（模拟）...\n");
    printf("[Player] speed=%.2f jump_height=%d max_hp=%d\n",
           player->speed, player->jump_height, player->max_hp);
    printf("[Weapons] 共 %d 件:\n", weapon_count);
    for (int i = 0; i < weapon_count; i++) {
        const WeaponConfig *w = &weapons[i];
        if (w->has_mana_cost) {
            printf("  - %-8s damage=%-6.2f speed=%-4.2f mana_cost=%.2f\n",
                   w->name, w->damage, w->speed, w->mana_cost);
        } else {
            printf("  - %-8s damage=%-6.2f speed=%-4.2f\n", w->name, w->damage, w->speed);
        }
    }
    printf("[Game] 初始化完成。\n");
    printf("========================================\n");
}

int main(int argc, char **argv) {
    const char *script_path = (argc > 1) ? argv[1] : "game_config.lua";

    /* 1. 创建 Lua VM */
    lua_State *L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "无法创建 Lua VM\n");
        return 1;
    }
    luaL_openlibs(L);

    /* 2. 注入只读全局变量 difficulty，模拟"来自存档/命令行的难度等级"。
     *    可以用环境变量 GAME_DIFFICULTY 覆盖，默认是 2（普通难度）。 */
    int difficulty = 2;
    const char *diff_env = getenv("GAME_DIFFICULTY");
    if (diff_env) {
        difficulty = atoi(diff_env);
    }
    lua_pushinteger(L, difficulty);
    lua_setglobal(L, "difficulty");

    /* 3. 加载 game_config.lua ——失败路径 1：文件不存在 / 语法错误 */
    if (luaL_loadfile(L, script_path) != LUA_OK) {
        fprintf(stderr, "[Config] 加载失败: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    /* 4. 执行 game_config.lua ——失败路径 2：脚本运行时报错 */
    if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK) {
        fprintf(stderr, "[Config] 执行失败: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    /* 5. 读取 player 表 ——失败路径 3：字段缺失/类型错误 */
    PlayerConfig player;
    if (!load_player_config(L, &player)) {
        fprintf(stderr, "[Config] player 配置校验失败，程序退出\n");
        lua_close(L);
        return 1;
    }

    /* 6. 读取 weapons 表（表的表） */
    WeaponConfig weapons[MAX_WEAPONS];
    int weapon_count = 0;
    if (!load_weapons_config(L, weapons, MAX_WEAPONS, &weapon_count) || weapon_count == 0) {
        fprintf(stderr, "[Config] weapons 配置校验失败或为空，程序退出\n");
        lua_close(L);
        return 1;
    }

    /* 7. 配置已经完全转换成纯 C 数据，释放 Lua VM */
    lua_close(L);

    /* 8. 用 C struct 初始化游戏 */
    init_game(&player, weapons, weapon_count);

    return 0;
}
