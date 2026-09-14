# 01 · 示例1：C + Lua Plugin Manager

> 前置阅读：[00-why-and-architecture.md](00-why-and-architecture.md)。本文用一个最小但完整的 C Host——"插件管理器"——把 Discovery / Load / Register / Lifecycle / Event / Unload 六步生命周期逐一落地成可运行的代码。

## 1. Problem

假设我们在写一个长期运行的 C 程序（可以想象成一个小型网关、一个后台服务），希望支持"插件"：运营人员往一个目录里丢一个 `.lua` 文件，程序**不重新编译、不重启**，下次启动时就能自动发现并加载这个插件；插件可以响应程序运行过程中发生的各种事件；某个插件写错了不能把整个程序拖垮；程序退出前要能干净地卸载所有插件。

如果不用 Plugin 机制，直觉的做法是在 C 代码里为每一种新行为写一个 `if/else` 分支，每加一个行为就要改一次 C 代码、重新编译。这正是 [00-mental-model-overview.md](../00-mental-model-overview.md) 里"易变需求 vs 稳定需求"矛盾的具体体现，只是这里的"易变需求"还多了一层——**数量本身也是易变的**：今天有 2 个插件，明天可能有 5 个，程序不应该为"有几个插件"这件事重新编译。

## 2. Why Lua

- **C API 优先设计 + 单一 `lua_State`**：每个插件可以拿到一个完全独立的 `lua_State`，创建和销毁都只是一次函数调用，天然适合"数量不固定、随时增删"的场景。
- **`lua_pcall` 把错误变成返回值**：插件是"不可信"的——它可能是运营人员随手写的、没经过严格测试的脚本。`lua_pcall` 让 Manager 可以放心地调用任何一个插件的任何一个回调，出错也只是拿到一个错误字符串，不会有段错误。
- **栈式调用协议**：Host 和插件之间只通过参数/返回值交换数据，不需要设计一套复杂的插件间通信协议。

## 3. Architecture

```text
                 main()
                   |
             Plugin Manager
                   |
        +----------+----------+
        |                     |
   plugin_a.lua          plugin_b.lua
        |                     |
  独立 lua_State A       独立 lua_State B
```

Plugin Manager 本身用纯 C 实现（`plugin_manager.c`），不包含任何插件的业务逻辑，只负责六步生命周期机制；`plugin_a.lua`/`plugin_b.lua` 各自跑在自己的 `lua_State` 里，彼此看不见对方的全局变量、看不见对方的 State。

## 4. Host responsibilities

- Discovery：`readdir` 扫描当前目录，挑出 `plugin_*.lua` 文件。
- Load：为每个插件文件调用 `luaL_newstate` + `luaL_openlibs` + `luaL_loadfile` + `lua_pcall`。
- Register：向每个插件的 `lua_State` 注册 `host_log`，随后调用该插件的 `plugin_init(host)`。
- Lifecycle：维护插件表（`plugin_entry_t` 数组），记录 name / `lua_State*` / 状态。
- Event：实现 `dispatch_event(name, payload)`，遍历插件表，逐个 `lua_pcall` 调用 `on_event`。
- Unload：对仍在 RUNNING 的插件调用 `plugin_shutdown()`，然后统一 `lua_close`。
- Host 完全不知道、也不关心 `plugin_a`/`plugin_b` 具体做了什么——它只认识"一个有 name/state/callback 的记录"。

## 5. Lua responsibilities

- 实现三个契约函数：`plugin_init(host)`、`plugin_shutdown()`、`on_event(name, payload)`。
- 维护自己的内部 State（`plugin_b.lua` 的 `event_count`），不依赖、不暴露给其他插件。
- 通过 Host 提供的 `host_log` 打印日志，不直接访问任何真实资源（本示例里"真实资源"被简化成了 `printf`）。
- 决定收到事件后要不要做出反应、怎么反应——这部分完全是 Lua 侧的自由。

## 6. Lifecycle

```text
Discovery
 |  readdir(".") 找到 plugin_a.lua / plugin_b.lua
 v
Load
 |  luaL_newstate + luaL_openlibs + luaL_loadfile + lua_pcall
 v
Register
 |  register_host_api(host_log) -> plugin_init(host) 成功
 v
Lifecycle: RUNNING
 |
 v
Event（可能发生 0~N 次）
 |  dispatch_event -> on_event，任意插件出错只影响自己
 v
Unload
 |  plugin_shutdown() -> lua_close
 v
Lifecycle: STOPPED
```

插件表里实际存在四个状态：`LOADED`（脚本已跑完顶层代码，`plugin_init` 还没成功）、`RUNNING`（`plugin_init` 成功，可以收事件）、`FAILED`（`plugin_init` 报错，被跳过但不释放）、`STOPPED`（已 Unload）。一个插件从 `LOADED` 出发，正常路径是走到 `RUNNING` 再走到 `STOPPED`；异常路径是直接从 `LOADED` 掉进 `FAILED`，永远不会经过 `RUNNING`，但最终仍会在 Unload 阶段被清理到"资源已释放"（只是不再单独设置成 `STOPPED`，代码里直接 `lua_close` 并置空指针）。

## 7. Data Flow（C -> Lua）

- Load 阶段：文件名（字符串）-> `luaL_loadfile` 编译成 Lua 函数 -> `lua_pcall` 执行，产出全局函数 `plugin_init`/`plugin_shutdown`/`on_event`。
- Register 阶段：插件名字符串通过 `lua_pushstring` + `lua_pcall(L, 1, 0, 0)` 作为 `host` 参数传入 `plugin_init(host)`。
- Event 阶段：事件名、payload 两个字符串通过 `lua_pushstring` 依次压栈，`lua_pcall(L, 2, 0, 0)` 传入 `on_event(name, payload)`。

所有跨越 C/Lua 边界的数据都是字符串，没有传递任何 C struct 指针——这是本示例的简化点之一，真实系统里 payload 往往是结构化数据（会用 table 或 userdata 表达）。

## 8. Control Flow（Lua -> C）

- 插件唯一能反向调用的 Host API 是 `host_log(msg)`：Lua 侧调用 `host_log("...")`，实际执行的是注册进对应 `lua_State` 的 C 函数 `l_host_log`。
- `l_host_log` 通过 `lua_upvalueindex(1)` 取出注册时绑定的插件名字符串（一个 Lua closure upvalue），这样一份 C 函数代码可以被多个插件共享，却仍能各自打印出正确的插件名前缀。
- 插件没有办法主动"注册自己想在将来某个时刻被调用"（没有提供类似 `register_timer` 的 API）——所有的调用时机都由 Manager 主动决定，插件只能被动响应。

## 9. State

- **Host 侧 State**：`g_plugins` 数组本身——每个插件的 name/`lua_State*`/状态，这是 C 层面唯一的、跨插件共享可见的状态。
- **Lua 侧 State**：每个插件的 `lua_State` 内部的全局变量/局部变量，`plugin_a.lua` 没有维护跨事件状态，`plugin_b.lua` 用一个模块级 `local event_count` 维护"收到过多少次成功事件"。
- 两者的隔离边界非常清晰：Host 侧的 State 是"插件的元数据"（这个插件叫什么、活着还是死了），Lua 侧的 State 是"插件自己的业务数据"，Host 完全看不见、也不需要看见 `event_count` 具体是多少。

## 10. Failure

需要处理两级不同的失败：

- **脚本加载/执行本身失败**（语法错误、顶层代码抛错）：此时插件连自己的名字对应的 `plugin_init` 是否存在都无法确定。`load_plugin` 直接 `lua_close` 并 `return`，**不把它计入插件表**——因为它从未真正建立起"身份"，谈不上进入 Lifecycle。

- **`plugin_init` 运行时报错**：插件已经有合法的 name 和 `lua_State`，但初始化逻辑本身出了问题。这种情况下 Manager 把状态标记为 `FAILED`，**保留在插件表里、保留 `lua_State`（留给 Unload 阶段统一 `lua_close`，避免资源泄漏），但从这一刻起永远不会再被 `dispatch_event` 选中**。也就是"跳过该插件，其余插件继续正常工作"，而不是让整个程序因为一个插件的 bug 而 `exit`。

  简化点：作为演示，本示例的两个插件文件都不会在 `plugin_init` 里报错，所以这条路径不会在真实运行输出里出现。如果想亲自验证，可以在 `code/01-c-plugin-manager/` 目录下临时新建一个文件，例如：

  ```lua
  -- plugin_c.lua（仅用于演示，未包含在正式代码目录中）
  function plugin_init(host_name)
      error("故意在 plugin_init 里抛出的错误")
  end
  ```

  重新运行会看到：

  ```text
  [Manager] 插件 plugin_c 的 plugin_init 执行失败: plugin_c.lua:3: 故意在 plugin_init 里抛出的错误
  ```

  随后 `plugin_c` 会一直停留在 `FAILED` 状态，`dispatch_event` 会在遍历到它时打印"跳过插件 plugin_c（状态=FAILED，非 RUNNING）"，`plugin_a`/`plugin_b` 完全不受影响，最后 Unload 阶段会跳过对它调用 `plugin_shutdown`，但仍会 `lua_close` 释放它的 `lua_State`。

- **`on_event` 运行时报错**（本示例真实会触发的路径，见 `plugin_b.lua` 对 `"boom"` 事件的处理）：`dispatch_event` 用 `lua_pcall` 包裹每一次调用，出错只打印一行错误日志、`continue` 到下一个插件，**不会跳过同一事件里的其他插件，也不会把这个插件标记为 `FAILED`**——一次事件处理失败被认为是"这一次的偶发错误"，不代表插件永久失效，插件在下一次事件里仍会被正常调用（`plugin_b.lua` 在 `"boom"` 之后的 `tick` 事件里恢复正常计数就是证据）。

  这条"一个插件的错误绝不能影响其他插件"的隔离原则，是 Plugin 模式相对 Extension 模式**格外重要**的一条设计原则：Extension 通常只有一个，出错了本来就只影响这一个能力；Plugin 天生要管理"数量不定的一批模块"，如果不做错误隔离，一个质量差的第三方插件就能拖垮所有其他插件甚至整个 Host。

## 11. Complete Host Code

```c
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
```

## 12. Complete Lua Code

`plugin_a.lua`：

```lua
-- plugin_a.lua
--
-- Plugin A：全篇最简单的插件实现，只做一件事——把生命周期回调和
-- 收到的事件打印成日志，不维护任何跨事件的内部状态。
-- 用来和 plugin_b.lua（维护 event_count 计数器）做对比，
-- 体现"两个插件即使实现同一套契约，State 也完全互相独立"。

function plugin_init(host_name)
    host_log(string.format("plugin_init 被调用，host 传入的标识: %s", host_name))
end

function on_event(name, payload)
    host_log(string.format(
        "收到事件 on_event(%s, %s)，Plugin A 只打印日志，不维护状态",
        name, payload))
end

function plugin_shutdown()
    host_log("plugin_shutdown 被调用，Plugin A 即将卸载")
end
```

`plugin_b.lua`：

```lua
-- plugin_b.lua
--
-- Plugin B：与 plugin_a.lua 共用同一份 Host API（host_log），
-- 但维护了一个模块级私有状态 event_count，每次 on_event 累加。
-- 这体现"每个插件的 lua_State 互相独立，State 也互相独立"——
-- 即使 Manager 同时给 plugin_a 和 plugin_b 派发同一个事件，
-- plugin_a 完全不知道 plugin_b 内部计数器的存在，反之亦然。
--
-- on_event 里还故意对 name == "boom" 抛出一个错误，用来配合
-- plugin_manager.c 演示 Event 阶段的错误隔离：这一次 dispatch
-- 对 plugin_b 失败，但不影响 plugin_a 继续收到同一个事件，
-- 也不影响 plugin_b 在下一次事件里恢复正常。

local event_count = 0

function plugin_init(host_name)
    event_count = 0
    host_log(string.format(
        "plugin_init 被调用，host 传入的标识: %s，计数器已初始化为 0",
        host_name))
end

function on_event(name, payload)
    if name == "boom" then
        -- 简化点：这里用 error() 模拟插件内部的 bug（例如访问了一个 nil 字段），
        -- 真实场景中错误可能来自任何一行 Lua 代码，不一定是显式 error()。
        error("Plugin B 故意在 on_event 里抛出的错误，用于演示 Manager 的错误隔离")
    end
    event_count = event_count + 1
    host_log(string.format(
        "收到事件 on_event(%s, %s)，这是第 %d 次收到事件",
        name, payload, event_count))
end

function plugin_shutdown()
    host_log(string.format(
        "plugin_shutdown 被调用，Plugin B 共处理了 %d 次事件后卸载",
        event_count))
end
```

## 13. Execution

```bash
cd 05-plugin/code/01-c-plugin-manager
make
./pluginmgr
```

`Makefile` 用 `pkg-config` 探测 `lua5.4`；如果本机是别的版本，把 `LUA_PKG` 改成 `lua5.3`/`lua5.1`/`lua` 即可（见 `Makefile` 顶部注释）。因为 `discover_plugins` 用 `readdir(".")` 扫描当前工作目录，必须在 `code/01-c-plugin-manager/` 目录下运行，才能扫到同目录里的 `plugin_a.lua`/`plugin_b.lua`。

完整示例输出：

```text
== Discovery ==
[Manager] 发现 2 个插件文件:
  - plugin_a.lua
  - plugin_b.lua

== Load + Register ==
[Host] <plugin_a> plugin_init 被调用，host 传入的标识: plugin_a
[Manager] 插件 plugin_a 加载并初始化成功，状态 -> RUNNING
[Host] <plugin_b> plugin_init 被调用，host 传入的标识: plugin_b，计数器已初始化为 0
[Manager] 插件 plugin_b 加载并初始化成功，状态 -> RUNNING

[Manager] 当前插件表 (2 个):
  - plugin_a     state=RUNNING
  - plugin_b     state=RUNNING

== Event ==

[Manager] ==> dispatch_event(name="tick", payload="1")
[Host] <plugin_a> 收到事件 on_event(tick, 1)，Plugin A 只打印日志，不维护状态
[Host] <plugin_b> 收到事件 on_event(tick, 1)，这是第 1 次收到事件

[Manager] ==> dispatch_event(name="boom", payload="oops")
[Host] <plugin_a> 收到事件 on_event(boom, oops)，Plugin A 只打印日志，不维护状态
[Manager] 插件 plugin_b 的 on_event 执行失败: plugin_b.lua:27: Plugin B 故意在 on_event 里抛出的错误，用于演示 Manager 的错误隔离

[Manager] ==> dispatch_event(name="tick", payload="2")
[Host] <plugin_a> 收到事件 on_event(tick, 2)，Plugin A 只打印日志，不维护状态
[Host] <plugin_b> 收到事件 on_event(tick, 2)，这是第 2 次收到事件

== Unload ==
[Host] <plugin_a> plugin_shutdown 被调用，Plugin A 即将卸载
[Manager] 插件 plugin_a 已卸载 (lua_State 已释放)
[Host] <plugin_b> plugin_shutdown 被调用，Plugin B 共处理了 2 次事件后卸载
[Manager] 插件 plugin_b 已卸载 (lua_State 已释放)

[Manager] 全部完成。
```

## 14. Step-by-step walkthrough

1. `main()` 先调用 `discover_plugins`，`readdir` 扫描当前目录，只挑出 `plugin_a.lua`、`plugin_b.lua` 两个文件名（`plugin_manager.c` 后缀是 `.c`，`Makefile` 不带 `plugin_` 前缀，都不匹配）。`qsort` 保证输出顺序稳定。
2. 对每个文件名调用 `load_plugin`：分别创建两个**互相独立**的 `lua_State`，各自 `luaL_openlibs`，各自注册一份带有不同 upvalue（插件名）的 `host_log`。
3. `luaL_loadfile` + `lua_pcall(L, 0, LUA_MULTRET, 0)` 执行脚本顶层代码——此时 `plugin_a.lua`/`plugin_b.lua` 里的 `function plugin_init(...) ... end` 这类语句被求值，产生的是"定义了一个叫 `plugin_init` 的全局函数"这个副作用，脚本本身不会打印任何东西（顶层没有裸的 `print`/`host_log` 调用）。
4. Manager 把这个插件加进 `g_plugins` 表，状态设为 `LOADED`，再 `lua_getglobal` 取出 `plugin_init`、传入插件名字符串、`lua_pcall` 调用。两个插件的 `plugin_init` 都成功执行并打印了一行 `host_log`，状态被推进到 `RUNNING`。
5. `print_plugin_table` 打印出两条 `state=RUNNING` 的记录，确认 Lifecycle 阶段的状态跟踪是有效的。
6. `dispatch_event("tick", "1")`：遍历插件表，两个都是 `RUNNING`，依次调用各自的 `on_event("tick", "1")`。`plugin_a` 只打日志；`plugin_b` 判断 `name ~= "boom"`，把 `event_count` 从 0 累加到 1，打印"这是第 1 次收到事件"。
7. `dispatch_event("boom", "oops")`：`plugin_a` 正常收到并打日志；`plugin_b` 的 `on_event` 判断 `name == "boom"`，执行 `error(...)`，`lua_pcall` 返回非 `LUA_OK`，Manager 打印一行 `[Manager] 插件 plugin_b 的 on_event 执行失败: ...`，**循环继续**（并不会跳过 `plugin_a`，因为 `plugin_a` 已经在这次遍历里先被处理过；如果换一个顺序，后面的插件也不会被这次错误影响）。
8. `dispatch_event("tick", "2")`：两个插件都正常收到；`plugin_b` 的 `event_count` 从 1 累加到 2——**证明上一次的错误没有破坏它的内部状态**，Lua 侧的 `error()` 只是中断了那一次函数调用，不会导致 `event_count` 变成脏数据或者让 `lua_State` 进入不可用状态。
9. `unload_all_plugins`：两个插件都还是 `RUNNING`，Manager 依次调用它们的 `plugin_shutdown()`——`plugin_a` 打印告别日志；`plugin_b` 打印"共处理了 2 次事件"（印证第 8 步的计数结果），随后各自 `lua_close`，指针置空，状态改成 `STOPPED`。

## 15. Mental Model

```text
Host（main.c 的 main 函数）
 |
 +-- Mechanism   : readdir 扫描 / for 循环调度 / 插件表的增删查
 +-- Resource    : 每个插件专属的 lua_State*（Host 创建、Host 销毁）
 +-- Lifecycle   : LOADED -> RUNNING -> (FAILED |) -> STOPPED 状态机
 +-- Stable API  : host_log（唯一暴露给插件的 Host 能力）
          |
          v
         Lua（plugin_a.lua / plugin_b.lua，各自一个 lua_State）
          |
 +-- Behavior    : on_event 里具体做什么（打日志 / 累加计数器）
 +-- State       : plugin_b.lua 的 event_count（模块级私有，跨插件互不可见）
 +-- Extension   : 两个插件本身就是两个独立注册进 Manager 的"新能力"
```

Manager 是这份心智模型里第一次出现的"Host 内部的一个中间管理层"——它自己不是最终的业务逻辑，也不是 Lua，而是**专门负责把 N 个 Extension 组织成可发现、可调度、可隔离失败的模块**的机制。理解这个 Manager 在做什么，比理解某一个具体插件在做什么更重要。

## 16. Why this is this Pattern

这个例子之所以是 **Plugin** 而不是普通的 Extension，关键在于三点，缺了任何一点都会退化成 Extension：

1. **数量不固定，靠运行时 Discovery 确定**——不是在代码里写死 `require("plugin_a")`，`plugin_a`/`plugin_b` 是被 `readdir` 找出来的，理论上可以是 0 个、2 个，也可以是 20 个。
2. **每个插件有自己独立的身份、状态机和资源（`lua_State`）**——`g_plugins` 数组是显式的 Lifecycle 跟踪结构，`LOADED`/`RUNNING`/`FAILED`/`STOPPED` 是显式的状态枚举，这些在单个 Extension 的例子里都不需要存在。
3. **有显式的、统一的 Unload 步骤**——`plugin_shutdown()` + `lua_close()` 对每一个插件单独执行，而不是"程序退出，一切自然消失"。

**如果 Lua 消失，Host 还能做什么？** Host（Manager 这部分 C 代码）本身完全不认识任何具体插件的行为，Lua 消失后 Manager 的六步生命周期机制（Discovery/Load/Register/Lifecycle/Event/Unload）作为**代码骨架**仍然成立，但没有了可以被"发现并加载"的东西——`discover_plugins` 会扫到一堆 `.lua` 文件却无法执行它们，整个程序退化成一个空的调度框架，什么实际行为都提供不了。这恰好说明 Plugin 模式里 Host 提供的是"骨架"，行为的"肉"完全来自 Lua。

**如果把 Lua 换成 JSON/YAML/纯 C，这个 Pattern 还成立吗？** Discovery/Load/Lifecycle/Unload 这套骨架依然成立——操作系统内核模块用 `.ko` + `insmod`/`rmmod`、浏览器扩展用打包好的 JS + manifest.json、很多插件系统用 `dlopen` 加载编译好的 `.so`，走的都是同一套六步生命周期。但如果换成纯 JSON/YAML，会立刻丧失 `on_event` 这种"运行时任意逻辑"的能力——JSON/YAML 只能描述静态数据（配置项、字段映射），无法表达"收到 `boom` 事件时该执行什么样的判断和副作用"，那样的话 Event 阶段就必须退化成"用 JSON 里的固定字段查表"，本质上从 Plugin 退化成了纯 Configuration/Rule Engine。如果换成纯 C（用 `dlopen` 加载编译好的共享库），Pattern 本身依然成立、甚至运行更快，但会失去 Lua 版本"改一行脚本立即生效、写错也不崩溃、无需处理 C ABI 兼容"这几个关键优势——这也是为什么很多"轻量级、允许第三方随手写、容错要求高"的插件系统（游戏 Mod、编辑器扩展）宁可选 Lua/JS 这类脚本语言，也不愿意直接开放 `.so` 插件接口。
