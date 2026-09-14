/*
 * plugin_manager.c
 *
 * Pattern E · Plugin 的 C Host 示例：一个"插件管理器"。
 * 与 01-extension 的 main.c 最大的区别：
 *   - Extension 示例里只有一个 lua_State，所有命令共享同一个全局环境；
 *   - 这里每个插件拥有自己独立的 lua_State，插件之间在 Lua 层面完全隔离
 *     （全局变量、注册表引用互不可见），Manager 只在 C 层面统一调度。
 *
 * 生命周期：
 *   Discovery -> Load -> Register -> Lifecycle -> Event -> Unload
 *
 *   Discovery : 扫描当前目录，找出所有形如 plugin_*.lua 的文件
 *               （用 readdir，不是硬编码文件名列表，体现"运行时发现"）
 *   Load      : 为每个发现的插件文件创建一个独立的 lua_State，
 *               加载并执行脚本顶层代码（这一步会定义 plugin_init /
 *               plugin_shutdown / on_event 三个全局函数）
 *   Register  : 向该插件专属的 lua_State 里注册最小 Host API
 *               （host_log），随后调用 plugin_init(host) 完成插件
 *               自己的注册逻辑
 *   Lifecycle : 用一个插件表记录每个插件的 name / lua_State* / 状态
 *               （LOADED / RUNNING / FAILED / STOPPED）
 *   Event     : dispatch_event(name, payload) 遍历插件表，
 *               对每个 RUNNING 状态的插件调用其 on_event，用 lua_pcall
 *               保护，任何一个插件出错都不影响其他插件继续收到事件
 *   Unload    : 对仍处于 RUNNING 的插件调用 plugin_shutdown()，
 *               然后无条件 lua_close 释放其 lua_State
 *
 * main() 跑的序列：discover -> load 全部插件 -> dispatch 3 个事件
 *               （其中一个故意触发 plugin_b.lua 里的错误，用来演示
 *               错误隔离）-> unload 全部插件。
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define MAX_PLUGINS   16
#define NAME_LEN      128

/* ---------- 插件状态 ---------- */

typedef enum {
    PLUGIN_LOADED,   /* 脚本已加载执行，但尚未成功调用 plugin_init */
    PLUGIN_RUNNING,  /* plugin_init 成功返回，可以接收事件 */
    PLUGIN_FAILED,   /* plugin_init 报错，不再参与 Event 派发，但仍占有 lua_State */
    PLUGIN_STOPPED,  /* 已 Unload，lua_State 已释放 */
} plugin_state_t;

static const char *plugin_state_name(plugin_state_t s) {
    switch (s) {
        case PLUGIN_LOADED:  return "LOADED";
        case PLUGIN_RUNNING: return "RUNNING";
        case PLUGIN_FAILED:  return "FAILED";
        case PLUGIN_STOPPED: return "STOPPED";
    }
    return "UNKNOWN";
}

/* ---------- 插件表：Lifecycle 阶段维护的核心数据结构 ---------- */

typedef struct {
    char name[NAME_LEN];   /* 插件名字（文件名去掉 .lua 后缀），即 Identity */
    lua_State *L;          /* 该插件专属的 lua_State，插件之间互不共享 */
    plugin_state_t state;
} plugin_entry_t;

static plugin_entry_t g_plugins[MAX_PLUGINS];
static int g_plugin_count = 0;

/* ---------- Register 阶段：向插件专属 lua_State 注册的 Host API ---------- */

/*
 * host_log(msg)
 *
 * 插件唯一能调用的 Host 能力：打印一行日志，日志前缀里带上插件名，
 * 这样即便多个插件同时打日志也能区分是谁打的。
 *
 * 简化点：真实系统里 Host API 通常还包括"读取宿主状态"、"注册子回调"、
 * "访问共享资源"等；这里只留一个 host_log，聚焦 Plugin 生命周期本身。
 *
 * plugin_init(host) 的 host 参数在本示例里也简化成了插件名字符串，
 * 真实系统里往往是一个 userdata/table，携带更丰富的 Host 句柄。
 */
static int l_host_log(lua_State *L) {
    const char *msg = luaL_checkstring(L, 1);
    /* 插件名通过 upvalue 传入，见 register_host_api */
    const char *plugin_name = lua_tostring(L, lua_upvalueindex(1));
    printf("[Host] <%s> %s\n", plugin_name, msg);
    return 0;
}

static void register_host_api(lua_State *L, const char *plugin_name) {
    lua_pushstring(L, plugin_name);      /* upvalue 1: 插件名 */
    lua_pushcclosure(L, l_host_log, 1);
    lua_setglobal(L, "host_log");
}

/* ---------- Discovery 阶段 ---------- */

static int cmp_filenames(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

/*
 * discover_plugins
 *
 * 用 readdir 扫描当前目录，挑出文件名满足 "plugin_*.lua" 的文件
 * （前缀 "plugin_"，后缀 ".lua"）。这是"运行时发现"而不是在代码里
 * 硬编码 {"plugin_a.lua", "plugin_b.lua"}——新增一个插件文件不需要
 * 重新编译 Manager，这正是 Plugin 模式相比 Extension 模式更进一步的地方。
 *
 * readdir 返回的顺序依赖文件系统，不保证稳定，所以额外做一次
 * qsort，保证同一批文件每次运行的加载顺序都一致，方便复现输出。
 */
static int discover_plugins(char filenames[][NAME_LEN], int max_count) {
    int count = 0;
    DIR *dir = opendir(".");
    if (!dir) {
        perror("[Manager] opendir 失败");
        return 0;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && count < max_count) {
        const char *name = ent->d_name;
        size_t len = strlen(name);
        int has_prefix = (strncmp(name, "plugin_", 7) == 0);
        int has_suffix = (len > 4 && strcmp(name + len - 4, ".lua") == 0);
        if (has_prefix && has_suffix) {
            snprintf(filenames[count], NAME_LEN, "%s", name);
            count++;
        }
    }
    closedir(dir);

    qsort(filenames, (size_t)count, NAME_LEN, cmp_filenames);
    return count;
}

/* ---------- Load + Register 阶段 ---------- */

/*
 * load_plugin
 *
 * 为一个插件文件创建独立 lua_State，加载脚本，注册 Host API，
 * 调用 plugin_init。区分两级失败：
 *
 *   1. 脚本本身加载/执行失败（语法错误、顶层代码抛错）：
 *      此时插件连 plugin_init/plugin_shutdown/on_event 是否存在都
 *      无法确定，Manager 直接关闭这个 lua_State，也不把它计入插件表
 *      （连身份都没能建立起来，无法参与后续 Lifecycle）。
 *
 *   2. 脚本加载成功，但 plugin_init 运行时报错：
 *      插件已经有了合法的 name/lua_State，Manager 把它计入插件表，
 *      状态标记为 FAILED，仍然持有该 lua_State（后面 Unload 阶段统一
 *      lua_close，避免资源泄漏），但不再把它交给 dispatch_event。
 *      这就是"跳过该插件，其余插件继续正常工作"的具体实现。
 */
static void load_plugin(const char *filename) {
    if (g_plugin_count >= MAX_PLUGINS) {
        fprintf(stderr, "[Manager] 插件表已满，忽略 %s\n", filename);
        return;
    }

    lua_State *L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "[Manager] 无法为 %s 创建 lua_State\n", filename);
        return;
    }
    luaL_openlibs(L);

    /* Identity：插件名 = 文件名去掉 .lua 后缀 */
    char plugin_name[NAME_LEN];
    snprintf(plugin_name, sizeof(plugin_name), "%s", filename);
    char *dot = strstr(plugin_name, ".lua");
    if (dot) {
        *dot = '\0';
    }

    register_host_api(L, plugin_name);

    /* 加载并执行脚本顶层代码：定义 plugin_init/plugin_shutdown/on_event */
    if (luaL_loadfile(L, filename) != LUA_OK ||
        lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK) {
        fprintf(stderr, "[Manager] 加载插件 %s 失败: %s\n",
                filename, lua_tostring(L, -1));
        lua_close(L);
        return; /* 第 1 级失败：不计入插件表 */
    }

    plugin_entry_t *entry = &g_plugins[g_plugin_count++];
    snprintf(entry->name, sizeof(entry->name), "%s", plugin_name);
    entry->L = L;
    entry->state = PLUGIN_LOADED;

    /* Register: 调用 plugin_init(host)，host 简化为插件名字符串 */
    lua_getglobal(L, "plugin_init");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        fprintf(stderr, "[Manager] 插件 %s 没有定义 plugin_init，视为直接 RUNNING\n",
                entry->name);
        entry->state = PLUGIN_RUNNING;
        return;
    }

    lua_pushstring(L, entry->name);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        fprintf(stderr, "[Manager] 插件 %s 的 plugin_init 执行失败: %s\n",
                entry->name, lua_tostring(L, -1));
        lua_pop(L, 1);
        entry->state = PLUGIN_FAILED; /* 第 2 级失败：跳过，但保留在表里 */
        return;
    }

    entry->state = PLUGIN_RUNNING;
    printf("[Manager] 插件 %s 加载并初始化成功，状态 -> RUNNING\n", entry->name);
}

/* ---------- Lifecycle 阶段：查看插件表 ---------- */

static void print_plugin_table(void) {
    printf("[Manager] 当前插件表 (%d 个):\n", g_plugin_count);
    for (int i = 0; i < g_plugin_count; i++) {
        printf("  - %-12s state=%s\n", g_plugins[i].name,
               plugin_state_name(g_plugins[i].state));
    }
}

/* ---------- Event 阶段 ---------- */

/*
 * dispatch_event
 *
 * 遍历插件表，只给 RUNNING 状态的插件派发事件。核心教学点：
 * 用 lua_pcall 包裹每一次 on_event 调用，一个插件出错只打印错误、
 * continue 到下一个插件，绝不会让错误传播出这个循环、
 * 更不会让一个插件的 bug 拖垮 Manager 或者阻止其他插件收到同一事件。
 */
static void dispatch_event(const char *name, const char *payload) {
    printf("\n[Manager] ==> dispatch_event(name=\"%s\", payload=\"%s\")\n",
           name, payload);

    for (int i = 0; i < g_plugin_count; i++) {
        plugin_entry_t *entry = &g_plugins[i];
        if (entry->state != PLUGIN_RUNNING) {
            printf("[Manager] 跳过插件 %s（状态=%s，非 RUNNING）\n",
                   entry->name, plugin_state_name(entry->state));
            continue;
        }

        lua_State *L = entry->L;
        lua_getglobal(L, "on_event");
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 1);
            continue; /* 该插件没有实现 on_event，静默跳过 */
        }

        lua_pushstring(L, name);
        lua_pushstring(L, payload);
        if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
            fprintf(stderr, "[Manager] 插件 %s 的 on_event 执行失败: %s\n",
                    entry->name, lua_tostring(L, -1));
            lua_pop(L, 1);
            /* 关键：这里不 return、不终止整个 dispatch_event，
               只是这一次事件对这一个插件失效，循环继续给下一个插件派发 */
        }
    }
}

/* ---------- Unload 阶段 ---------- */

static void unload_all_plugins(void) {
    for (int i = 0; i < g_plugin_count; i++) {
        plugin_entry_t *entry = &g_plugins[i];
        if (!entry->L) {
            continue;
        }

        if (entry->state == PLUGIN_RUNNING) {
            lua_getglobal(entry->L, "plugin_shutdown");
            if (lua_isfunction(entry->L, -1)) {
                if (lua_pcall(entry->L, 0, 0, 0) != LUA_OK) {
                    fprintf(stderr, "[Manager] 插件 %s 的 plugin_shutdown 执行失败: %s\n",
                            entry->name, lua_tostring(entry->L, -1));
                    lua_pop(entry->L, 1);
                }
            } else {
                lua_pop(entry->L, 1);
            }
        } else {
            printf("[Manager] 插件 %s 状态为 %s，跳过 plugin_shutdown，仅释放 lua_State\n",
                   entry->name, plugin_state_name(entry->state));
        }

        lua_close(entry->L);
        entry->L = NULL;
        entry->state = PLUGIN_STOPPED;
        printf("[Manager] 插件 %s 已卸载 (lua_State 已释放)\n", entry->name);
    }
}

/* ---------- main：完整走一遍 Discovery -> Load -> Event -> Unload ---------- */

int main(void) {
    char filenames[MAX_PLUGINS][NAME_LEN];

    printf("== Discovery ==\n");
    int n = discover_plugins(filenames, MAX_PLUGINS);
    printf("[Manager] 发现 %d 个插件文件:\n", n);
    for (int i = 0; i < n; i++) {
        printf("  - %s\n", filenames[i]);
    }

    printf("\n== Load + Register ==\n");
    for (int i = 0; i < n; i++) {
        load_plugin(filenames[i]);
    }
    printf("\n");
    print_plugin_table();

    printf("\n== Event ==\n");
    dispatch_event("tick", "1");
    dispatch_event("boom", "oops");   /* 故意触发 plugin_b.lua 里的错误 */
    dispatch_event("tick", "2");

    printf("\n== Unload ==\n");
    unload_all_plugins();

    printf("\n[Manager] 全部完成。\n");
    return 0;
}
