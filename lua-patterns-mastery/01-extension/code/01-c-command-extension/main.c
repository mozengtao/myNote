/*
 * main.c
 *
 * 一个最小化的 C Host：命令行程序，从 stdin 逐行读取命令，
 * 命令的具体行为完全由 Lua 脚本（extension.lua）注册，
 * main.c 本身不认识 "hello" / "add" / "boom" / "state" 这些具体命令名。
 *
 * 架构：
 *   main() 创建 Lua VM
 *     -> 注册 C API: register_command(name, callback)
 *     -> luaL_dofile 加载 extension.lua（脚本内部调用 register_command 完成注册）
 *     -> 进入命令行循环，读取一行输入，解析出命令名 + 参数
 *     -> 查表找到对应的 Lua 回调，用 lua_pcall 调用
 *     -> 处理"命令不存在"和"Lua 回调运行时报错"两种失败路径
 *     -> 退出前 lua_close 释放 VM
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define MAX_COMMANDS 32
#define MAX_LINE 256
#define MAX_ARGS 8

/*
 * 命令表：每个已注册命令保存
 *   - name: 命令名（C 字符串）
 *   - lua_ref: 该命令对应的 Lua 回调函数在 LUA_REGISTRYINDEX 里的引用
 *
 * 为什么不能只存"函数指针"？
 * Lua 的函数是 lua_State 栈上的一个值（closure 对象），不是 C 意义上的
 * 函数指针，也不能直接转换成任何 C 类型长期持有。它的生命周期由 Lua 的 GC
 * 管理。如果只是临时把它 pop 出来看一眼，下一次 GC 就可能把它回收掉。
 *
 * 正确做法：用 lua_pushvalue 把该函数值复制一份到栈顶，
 * 再用 luaL_ref(L, LUA_REGISTRYINDEX) 把它放进"注册表"（一个 Lua 侧、
 * C 侧都不会意外清空的全局 table），换回一个整数引用 ID。
 * 这个整数 ID 是纯 C 数据，可以随意存进结构体、数组、长期持有；
 * 需要调用该函数时，用 lua_rawgeti(L, LUA_REGISTRYINDEX, ref) 把函数值
 * 重新压回栈顶即可，Lua GC 会因为注册表持有引用而不回收它。
 */
typedef struct {
    char name[64];
    int lua_ref;
} command_entry_t;

static command_entry_t g_commands[MAX_COMMANDS];
static int g_command_count = 0;

/*
 * register_command(name, callback)
 *
 * Host 暴露给 Lua 的 C API，签名从 Lua 侧看是:
 *   register_command("hello", function(...) ... end)
 *
 * 这是整个 Extension Pattern 的核心：Host 只提供"注册的机制"，
 * 不关心具体注册了什么行为——行为完全由 Lua 决定。
 */
static int l_register_command(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    if (g_command_count >= MAX_COMMANDS) {
        return luaL_error(L, "命令表已满，最多支持 %d 个命令", MAX_COMMANDS);
    }

    /* 把第 2 个参数（Lua 函数）复制到栈顶，再存进注册表，换回引用 ID */
    lua_pushvalue(L, 2);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    command_entry_t *entry = &g_commands[g_command_count++];
    snprintf(entry->name, sizeof(entry->name), "%s", name);
    entry->lua_ref = ref;

    printf("[C Host] 已注册命令: %s\n", name);
    return 0; /* 没有返回值给 Lua */
}

/* 在命令表中按名字查找，找不到返回 NULL */
static command_entry_t *find_command(const char *name) {
    for (int i = 0; i < g_command_count; i++) {
        if (strcmp(g_commands[i].name, name) == 0) {
            return &g_commands[i];
        }
    }
    return NULL;
}

/*
 * dispatch_command
 *
 * 收到用户输入的一行命令后：
 *   1. 查表，找不到 -> 打印"命令不存在"（失败路径 1）
 *   2. 找到 -> 用 lua_rawgeti 把回调函数重新压回栈顶
 *   3. 把参数逐个 push 成 Lua 字符串
 *   4. lua_pcall 调用；如果 Lua 回调内部 error() 了 -> 打印错误信息，
 *      但整个 C 进程继续运行（失败路径 2，进程不崩溃）
 */
static void dispatch_command(lua_State *L, const char *name, char **args, int argc) {
    command_entry_t *entry = find_command(name);
    if (!entry) {
        fprintf(stderr, "[C Host] 错误: 命令不存在: %s\n", name);
        return;
    }

    /* 把保存的 Lua 回调函数重新压回栈顶 */
    lua_rawgeti(L, LUA_REGISTRYINDEX, entry->lua_ref);

    /* 把参数依次压栈，作为 Lua 回调的 varargs (...) */
    for (int i = 0; i < argc; i++) {
        lua_pushstring(L, args[i]);
    }

    /* 调用：argc 个参数，期望 1 个返回值，errfunc = 0（不用自定义错误处理函数） */
    int status = lua_pcall(L, argc, 1, 0);
    if (status != LUA_OK) {
        /* lua_pcall 失败时，栈顶是错误信息（字符串） */
        const char *err_msg = lua_tostring(L, -1);
        fprintf(stderr, "[C Host] 命令 '%s' 执行失败: %s\n", name, err_msg);
        lua_pop(L, 1); /* 弹出错误信息 */
        return;
    }

    /* 成功：栈顶是返回值，这里只是演示性地弹掉，不强制使用 */
    lua_pop(L, 1);
}

/* 把一行输入按空格拆分成 argv 风格的 tokens：tokens[0] 是命令名 */
static int tokenize(char *line, char **tokens, int max_tokens) {
    int count = 0;
    char *p = strtok(line, " \t\r\n");
    while (p && count < max_tokens) {
        tokens[count++] = p;
        p = strtok(NULL, " \t\r\n");
    }
    return count;
}

int main(int argc, char **argv) {
    const char *script_path = (argc > 1) ? argv[1] : "extension.lua";

    /* 1. 创建 Lua VM */
    lua_State *L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "无法创建 Lua VM\n");
        return 1;
    }

    /* 2. 打开标准库（string/table/math/... 均可在 extension.lua 里使用） */
    luaL_openlibs(L);

    /* 3. 注册 Host 提供的 C API: register_command */
    lua_register(L, "register_command", l_register_command);

    /* 4. 加载并执行 extension.lua
     *
     * luaL_dofile 内部等价于:
     *   luaL_loadfile(L, path)  // 编译脚本，压入一个 Lua 函数
     *   lua_pcall(L, 0, LUA_MULTRET, 0) // 执行它
     * 这里手动展开，方便分别处理"加载失败"（语法错误/文件不存在）
     * 和"执行失败"（脚本运行时 error()）两种情况。
     */
    if (luaL_loadfile(L, script_path) != LUA_OK) {
        fprintf(stderr, "[C Host] 加载脚本失败: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }
    if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK) {
        fprintf(stderr, "[C Host] 执行脚本失败: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    printf("[C Host] 命令行已就绪，共有 %d 个命令。输入 'quit' 退出。\n", g_command_count);

    /* 5. 命令行主循环：从 stdin 读取一行，解析并分发 */
    char line[MAX_LINE];
    char *tokens[MAX_ARGS];
    while (1) {
        printf("> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) {
            break; /* EOF (Ctrl-D) */
        }

        char line_copy[MAX_LINE];
        snprintf(line_copy, sizeof(line_copy), "%s", line);
        int n = tokenize(line_copy, tokens, MAX_ARGS);
        if (n == 0) {
            continue; /* 空行 */
        }

        if (strcmp(tokens[0], "quit") == 0 || strcmp(tokens[0], "exit") == 0) {
            break;
        }

        dispatch_command(L, tokens[0], tokens + 1, n - 1);
    }

    /* 6. 释放 Lua VM：所有注册表引用、闭包、全局表都随之销毁 */
    lua_close(L);
    printf("[C Host] 已退出，Lua VM 已释放。\n");
    return 0;
}
