/*
 * main.c
 *
 * 一个最小化的 C Host：启动时加载 config.lua，把 Lua 侧算出来的
 * `server` 表转换成 C struct ServerConfig，再用这份配置"启动"一个
 * 模拟 server（只打印启动信息，不真正监听端口）。
 *
 * 架构：
 *   main() 创建 Lua VM
 *     -> 注册 Host 提供的 C API: os_default_workers()（供 config.lua 调用）
 *     -> 注入只读全局变量 ENV（模拟运行环境）
 *     -> luaL_loadfile + lua_pcall 加载并执行 config.lua
 *     -> lua_getglobal("server") 取出配置表
 *     -> 逐个字段用 lua_getfield 读取，转换成 C struct（严格栈平衡）
 *     -> lua_close 释放 VM（此后配置已完全"脱离" Lua，是纯 C 数据）
 *     -> 用 C struct 启动模拟 server
 *
 * 错误处理覆盖三条路径：
 *   1. config.lua 不存在 / 语法错误  -> luaL_loadfile 失败
 *   2. config.lua 执行时抛错（例如脚本自己 error()）-> lua_pcall 失败
 *   3. server 表缺字段 / 字段类型不对 -> 逐字段读取时发现并报告
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/* 转换后的、纯 C 的运行时配置。此后程序完全不依赖 Lua VM。 */
typedef struct {
    int port;
    int workers;
    int timeout;
    int debug; /* 0 = false, 1 = true */
} ServerConfig;

/*
 * os_default_workers
 *
 * Host 暴露给 config.lua 的 C API：返回"建议的 worker 数量"。
 * 这是"配置具备计算能力"的关键一环——worker 数量不是 Lua 脚本里的
 * 一个常量，而是由 Host 在加载配置的那一刻现算出来的。
 *
 * 简化点：真实项目里这里可能读 /proc/cpuinfo 或调用
 * sysconf(_SC_NPROCESSORS_ONLN)；本示例为了输出稳定、便于讲解，
 * 固定返回一个"看起来像是算出来的"值。
 */
static int l_os_default_workers(lua_State *L) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    if (n < 1) {
        n = 4;
    }
    lua_pushinteger(L, (lua_Integer)n);
    return 1; /* 返回值个数 */
}

/*
 * get_int_field
 *
 * 从栈上 index 为 table_idx 的 table 中读取一个整数字段。
 *   required = 1：字段必须存在且是 number，否则失败并打印错误。
 *   required = 0：字段缺失时使用 default_value；但如果字段存在却
 *                 类型不对，仍然算失败（"写错了"和"没写"要区分对待）。
 *
 * 每次调用都严格保证栈平衡：lua_getfield 压入 1 个值，
 * 函数返回前一定会 pop 掉这 1 个值。
 */
static int get_int_field(lua_State *L, int table_idx, const char *field,
                          int required, int default_value, int *out) {
    lua_getfield(L, table_idx, field); /* 压栈: server[field] */
    int type = lua_type(L, -1);

    if (type == LUA_TNIL) {
        lua_pop(L, 1);
        if (required) {
            fprintf(stderr, "[Config] 缺少必填字段: server.%s\n", field);
            return 0;
        }
        *out = default_value;
        return 1;
    }

    if (type != LUA_TNUMBER) {
        fprintf(stderr, "[Config] 字段类型错误: server.%s 应为 number，实际是 %s\n",
                field, lua_typename(L, type));
        lua_pop(L, 1);
        return 0;
    }

    *out = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);
    return 1;
}

/* 与 get_int_field 同构，只是把 number 换成 boolean。 */
static int get_bool_field(lua_State *L, int table_idx, const char *field,
                           int required, int default_value, int *out) {
    lua_getfield(L, table_idx, field);
    int type = lua_type(L, -1);

    if (type == LUA_TNIL) {
        lua_pop(L, 1);
        if (required) {
            fprintf(stderr, "[Config] 缺少必填字段: server.%s\n", field);
            return 0;
        }
        *out = default_value;
        return 1;
    }

    if (type != LUA_TBOOLEAN) {
        fprintf(stderr, "[Config] 字段类型错误: server.%s 应为 boolean，实际是 %s\n",
                field, lua_typename(L, type));
        lua_pop(L, 1);
        return 0;
    }

    *out = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return 1;
}

/* 用读到的配置"启动"一个模拟 server：只打印信息，不真正监听端口。 */
static void start_server(const ServerConfig *cfg) {
    printf("========================================\n");
    printf("[Server] 正在启动（模拟，不会真正监听端口）...\n");
    printf("  port    = %d\n", cfg->port);
    printf("  workers = %d\n", cfg->workers);
    printf("  timeout = %ds\n", cfg->timeout);
    printf("  debug   = %s\n", cfg->debug ? "true" : "false");
    printf("[Server] 启动完成。\n");
    printf("========================================\n");
}

int main(int argc, char **argv) {
    const char *script_path = (argc > 1) ? argv[1] : "config.lua";

    /* 1. 创建 Lua VM */
    lua_State *L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "无法创建 Lua VM\n");
        return 1;
    }
    luaL_openlibs(L);

    /* 2. 注册 Host 提供的计算能力：os_default_workers() */
    lua_register(L, "os_default_workers", l_os_default_workers);

    /* 3. 注入只读全局变量 ENV，模拟"运行环境"这个外部输入。
     *    真实项目里通常改成 os.getenv("APP_ENV")，这里为了让
     *    示例自包含、边界清晰，改成由 Host 直接注入。 */
    const char *env = getenv("APP_ENV");
    lua_pushstring(L, env ? env : "development");
    lua_setglobal(L, "ENV");

    /* 4. 加载 config.lua ——失败路径 1：文件不存在 / 语法错误 */
    if (luaL_loadfile(L, script_path) != LUA_OK) {
        fprintf(stderr, "[Config] 加载失败: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    /* 5. 执行 config.lua ——失败路径 2：脚本运行时报错（例如内部 error()） */
    if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK) {
        fprintf(stderr, "[Config] 执行失败: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    /* 6. 取出全局变量 server，必须是一个 table */
    lua_getglobal(L, "server");
    if (lua_type(L, -1) != LUA_TTABLE) {
        fprintf(stderr, "[Config] 全局变量 'server' 不存在或不是 table\n");
        lua_pop(L, 1);
        lua_close(L);
        return 1;
    }
    int table_idx = lua_gettop(L);

    /* 7. 逐字段读取，转换成 C struct ——失败路径 3：字段缺失/类型错误 */
    ServerConfig cfg;
    int ok = 1;
    ok = ok && get_int_field(L, table_idx, "port", 1, 0, &cfg.port);
    ok = ok && get_int_field(L, table_idx, "workers", 1, 0, &cfg.workers);
    ok = ok && get_int_field(L, table_idx, "timeout", 0, 30, &cfg.timeout);
    int debug_flag = 0;
    ok = ok && get_bool_field(L, table_idx, "debug", 0, 0, &debug_flag);
    cfg.debug = debug_flag;

    lua_pop(L, 1); /* 弹出 server table，栈恢复到调用前的高度 */

    if (!ok) {
        fprintf(stderr, "[Config] 配置校验失败，程序退出\n");
        lua_close(L);
        return 1;
    }

    /* 8. 配置已经完全转换成纯 C 数据，Lua VM 可以安全释放了 */
    lua_close(L);

    /* 9. 用读到的配置启动模拟 server */
    start_server(&cfg);

    return 0;
}
