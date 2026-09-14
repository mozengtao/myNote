# 01 · C Application + Lua Configuration

代码目录：[`code/01-c-app-configuration/`](code/01-c-app-configuration/)（`main.c` / `config.lua` / `Makefile`）

## 1. Problem（问题定义）

一个 C 写的应用程序（这里用一个"server 启动器"作为最小载体）需要在启动时读取一批参数：监听端口、worker 数量、超时时间、是否开启 debug 模式。如果用传统 `.ini`/`.env` 文件，会遇到：

- 生产环境和开发环境端口不一样，只能维护两份文件，或者在部署脚本里做字符串替换；
- worker 数量"理想情况下"应该跟着机器的 CPU 核数走，但 `.ini` 没有计算能力，只能写死一个数字，换了机器就得手动改；
- debug 开关本质上是环境的派生值（"非生产环境就该开"），但传统配置只能再单独写一行，容易和环境字段"忘记同步"。

## 2. Why Lua（为什么用 Lua）

Lua 可以把"配置"重新定义成"一段在启动时执行一次、产出一份参数表的程序"：

- 表达式和条件分支：`port = (ENV == "production") and 80 or 8080`；
- 调用 Host 注入的计算函数：`workers = os_default_workers and os_default_workers() or 4`；
- 布尔派生：`debug = (ENV ~= "production")`。

这些能力都不需要引入模板引擎或部署脚本预处理，直接写在配置文件里，而且出错时（语法错误、字段缺失）Host 可以用 `lua_pcall` + 显式字段校验捕获，不会让整个程序崩溃。

## 3. Architecture（架构）

```text
main.c (Host)
 |
 +-- os_default_workers()   Config API：Host 注入给 Lua 的计算函数
 +-- ENV                    Config API：Host 注入给 Lua 的只读全局变量
 |
 +-- Lua VM
       |
       v
   config.lua
       |
       v
   ServerConfig (C struct)
       |
       v
  start_server() 模拟启动
```

## 4. Host responsibilities（Host 的职责）

- 创建/销毁 `lua_State`（`luaL_newstate` / `lua_close`）；
- 注册 Config API：一个计算函数 `os_default_workers`、一个只读变量 `ENV`；
- 加载并执行 `config.lua`，处理"文件不存在/语法错误"和"运行时报错"两类失败；
- 用 `lua_getfield` 把 Lua table 逐字段转换成 C struct `ServerConfig`，处理"字段缺失/类型错误"；
- 转换完成后 `lua_close` 释放 Lua VM，用纯 C struct 驱动后续逻辑（"启动"模拟 server）。

## 5. Lua responsibilities（Lua 的职责）

- 只做一件事：计算并声明一张全局表 `server`，包含 `port`/`workers`/`timeout`/`debug` 四个字段；
- 允许使用条件分支和函数调用来"计算"这些字段的值，但不反过来调用 Host 做任何有副作用的事情（这是 Configuration 和 Extension 的关键区别，见 [00-why-and-architecture.md](00-why-and-architecture.md) 第 4 节）。

## 6. Lifecycle（生命周期）

```text
main() 开始
 |
 v
luaL_newstate + luaL_openlibs -------- 创建 VM，打开标准库
 |
 v
lua_register(os_default_workers) ----- 注册 Config API（函数）
lua_setglobal(ENV) ------------------- 注册 Config API（变量）
 |
 v
luaL_loadfile(config.lua) ------------ 编译脚本；失败 -> 报错退出
 |
 v
lua_pcall(0, MULTRET, 0) ------------- 执行脚本；失败 -> 报错退出
 |
 v
lua_getglobal("server") + 逐字段读取 -- 转换成 C struct；失败 -> 报错退出
 |
 v
lua_close ----------------------------- 配置已转成纯 C 数据，VM 生命周期结束
 |
 v
start_server(&cfg) -------------------- 用纯 C 数据驱动后续逻辑
```

整个 Lua VM 的生命周期只覆盖"加载配置"这一小段，比 Policy/Plugin 的生命周期短得多——这正是 Configuration 的典型特征：一次性、用完即关。

## 7. Data Flow（C -> Lua）

```text
load config.lua
 |
 v
read Lua table
 |
 v
convert to C struct
 |
 v
start server
```

具体到本例：

1. `luaL_loadfile` + `lua_pcall` 加载并执行 `config.lua`，`ENV` 和 `os_default_workers` 是 Host 在这一步之前就已经注入的输入；
2. `lua_getglobal(L, "server")` 把 Lua 侧算好的表压到栈上；
3. `get_int_field` / `get_bool_field` 用 `lua_getfield` 逐个读取字段，每次读取后立刻 `lua_pop`，保证栈平衡；
4. 读到的值被写进 `ServerConfig` 这个纯 C struct；
5. `lua_close` 之后，`ServerConfig` 完全独立于 Lua 存在。

## 8. Control Flow（Lua -> C）

本例里 Lua 到 C 方向的"控制流"非常弱，只有一次：`config.lua` 在计算 `workers` 字段时，调用了 Host 注入的 `os_default_workers()`。这是 Configuration Pattern 里少见的、允许存在的"Lua 调 C"——但注意这个调用**没有副作用**，只是取值/计算，不会修改 Host 的任何状态。如果 Lua 侧开始调用带副作用的 Host 函数（比如 `open_socket()`、`register_route()`），这份"配置"实际上就越界成了 Extension 或 Plugin。

## 9. State（状态）

- Lua 侧没有需要跨调用保留的状态——`config.lua` 从加载到执行完毕是一次性的，脚本级的局部变量（如果有）在 `lua_close` 之后连同整个 VM 一起消失；
- C 侧的状态就是转换后的 `ServerConfig`，它的生命周期和 Lua VM 完全无关，比 VM 活得更久（一直存活到 `main` 结束）。

## 10. Failure（失败处理）

本例覆盖三条独立的失败路径，任何一条触发都会打印明确的错误信息、正确调用 `lua_close`、返回非 0 退出码，不会有内存泄漏或未定义行为：

1. **`config.lua` 不存在 / 语法错误**：`luaL_loadfile` 返回非 `LUA_OK`，栈顶是错误信息字符串（例如 `cannot open nope.lua: No such file or directory`，或 `unexpected symbol near <eof>`）。
2. **`config.lua` 执行时报错**：`lua_pcall` 返回非 `LUA_OK`（例如脚本里手写了 `error(...)`，或者引用了不存在的全局变量导致的运行时错误）。
3. **字段缺失或类型错误**：`server.port`/`server.workers` 是必填字段，缺失会报"缺少必填字段"；如果某字段存在但类型不对（例如把 `port` 写成字符串 `"8080"`），会报"字段类型错误"，两者都被明确区分，不会被静默地当成默认值处理。

## 11. Complete Host Code（完整 Host 代码）

```c
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
```

## 12. Complete Lua Code（完整 Lua 代码）

```lua
-- config.lua
--
-- 这是一份"可计算"的配置文件，用来对比传统 key=value 静态配置：
--   1. Host（main.c）在加载它之前，通过全局变量/全局函数注入了两样东西：
--        ENV                  -- 当前运行环境（"development" / "production"）
--        os_default_workers   -- 一个 C 函数，返回"建议的 worker 数量"
--   2. config.lua 内部利用这两样东西做条件判断和函数调用，
--      而不是把计算结果写死成常量，体现"配置具备计算能力"。
--
-- 简化点：真实项目里 ENV 通常来自命令行参数或 os.getenv("APP_ENV")，
-- 这里为了让示例自包含、边界清晰，改成由 Host 在 luaL_loadfile 之前
-- 直接注入一个只读全局变量。

server = {
    -- 条件分支：生产环境固定用 80 端口，其余环境用 8080，
    -- 这一行在传统 key=value 配置里是不可能做到的。
    port = (ENV == "production") and 80 or 8080,

    -- 函数调用：worker 数量不是写死的常量，而是由 Host 提供的
    -- C 函数在"加载配置的这一刻"动态计算出来的。
    -- `os_default_workers and os_default_workers() or 4` 是 Lua 里
    -- 常见的"安全调用"写法：如果 Host 没有注入这个函数，就退回默认值 4。
    workers = os_default_workers and os_default_workers() or 4,

    -- 纯数据字段，和传统配置没有区别。
    timeout = 30,

    -- 布尔表达式：debug 开关由 ENV 推导得出，
    -- 不需要在配置里再手动维护一个容易和 ENV 字段"忘记同步"的开关。
    debug = (ENV ~= "production"),
}
```

## 13. Execution（执行）

```bash
cd 03-configuration/code/01-c-app-configuration
make
./app_config_demo config.lua
```

默认（开发环境，`workers` 的具体数值取决于运行示例的机器核数，这里以 4 核机器为例）：

```text
========================================
[Server] 正在启动（模拟，不会真正监听端口）...
  port    = 8080
  workers = 4
  timeout = 30s
  debug   = true
[Server] 启动完成。
========================================
```

切换到生产环境（`APP_ENV=production`）：

```bash
APP_ENV=production ./app_config_demo config.lua
```

```text
========================================
[Server] 正在启动（模拟，不会真正监听端口）...
  port    = 80
  workers = 4
  timeout = 30s
  debug   = false
[Server] 启动完成。
========================================
```

失败路径演示（脚本不存在）：

```bash
./app_config_demo nope.lua
```

```text
[Config] 加载失败: cannot open nope.lua: No such file or directory
```

失败路径演示（字段类型错误，例如把 `config.lua` 里的 `port` 改成字符串 `"8080"`）：

```text
[Config] 字段类型错误: server.port 应为 number，实际是 string
[Config] 配置校验失败，程序退出
```

## 14. Step-by-step walkthrough（逐步走读）

1. `main()` 用 `luaL_newstate` + `luaL_openlibs` 创建一个全新的 Lua VM，此时 VM 里只有标准库，没有任何和本程序相关的东西。
2. `lua_register(L, "os_default_workers", l_os_default_workers)` 把一个 C 函数注册成 Lua 全局函数——这是 Config API 的"计算能力"那一半。
3. `lua_pushstring` + `lua_setglobal(L, "ENV")` 注入一个只读字符串全局变量——这是 Config API 的"环境信息"那一半。
4. `luaL_loadfile` 编译 `config.lua` 成一个 Lua 函数并压栈；如果文件不存在或语法错误，这一步直接失败，栈顶留下错误信息。
5. `lua_pcall(L, 0, LUA_MULTRET, 0)` 执行这个函数——也就是真正跑一遍 `config.lua` 里的代码，`server = {...}` 这行代码在这一步才真正执行，`port`/`workers`/`debug` 的表达式在这一刻被求值。
6. `lua_getglobal(L, "server")` 把执行结果（一张 table）从全局表里取出来压栈。
7. `get_int_field`/`get_bool_field` 依次对 `port`/`workers`/`timeout`/`debug` 做"取值 + 类型检查 + 弹栈"，任何一步失败都会记录到 `ok` 里，但不会立刻中断（这样可以一次性报告尽可能多的问题，而不是报一个错就退出）。
8. 所有字段读完后 `lua_pop(L, 1)` 弹出 `server` table，栈恢复到读取前的高度。
9. 根据 `ok` 决定是报错退出还是继续。
10. `lua_close(L)`——从这一刻起，`ServerConfig cfg` 是唯一还活着的、和这次配置加载相关的数据，Lua VM 及其内部的一切（包括 `config.lua` 里定义的所有局部变量）都被销毁。
11. `start_server(&cfg)` 用纯 C 数据完成"启动"。

## 15. Mental Model（心智模型）

```text
config.lua 不是"数据文件"，是"参数计算器"
它只运行一次，产出一份 C struct，然后就"退休"了
```

把 `config.lua` 想象成一个**只被调用一次的纯函数**：输入是 Host 注入的 `ENV` 和 `os_default_workers`，输出是全局变量 `server` 这张表。Host 调用这个"函数"、把返回值转换成 C 数据、然后彻底忘记 Lua 的存在——这和"每次请求都要问一次"的 Policy、"常驻并被反复回调"的 Plugin 形成鲜明对比。

## 16. Why this is this Pattern（为什么这是 Configuration 而不是别的）

- 它不是 Extension：`config.lua` 没有给 Host 新增任何"能做的事"，`start_server` 这个能力在 Lua 加载之前就已经存在于 C 代码里，Lua 只是决定了调用它时用什么参数。
- 它不是 Policy：Host 只在启动时读一次配置，运行期间不会因为某个事件发生就重新去问 Lua"现在该怎么做"。
- 它不是 Rule Engine：`server` 表里没有"一堆 IF/THEN 规则"，只有少量派生字段，条件分支的数量是常数级的，不会随业务增长。
- 它不是 Plugin：`config.lua` 没有独立的生命周期钩子（没有 `on_load`/`on_unload`），加载完就可以 `lua_close`，不需要在 Host 主循环里保留一个常驻的 `lua_State`。

**如果 Lua 消失，Host 还能做什么？** 完全可以运行，只是需要把 `port`/`workers`/`timeout`/`debug` 换成命令行参数或写死在代码里的常量；`start_server` 的实现一行都不需要改，因为它接收的始终是纯 C 的 `ServerConfig`，从来不知道这份数据以前是从 Lua 算出来的。这恰好说明 Configuration Pattern 里 Lua 和 Host 核心逻辑之间的耦合极低。

**如果把 Lua 换成 JSON/YAML/纯 C，这个 Pattern 还成立吗？** 换成纯静态的 JSON/YAML 之后，"Pattern"本身会退化——`port`/`debug` 之间的派生关系、`workers` 的动态计算都做不到了，要么打散成多份文件由外部脚本合并，要么退回"改代码重新编译"。换成纯 C（把配置直接写成 C 常量或者一个返回 struct 的 C 函数），计算能力还在，但代价是每次改一个参数都要重新编译、重新发布整个二进制，也失去了"运营人员可以自己改配置文件、不需要开发介入"的能力。所以 Configuration Pattern 的价值恰好卡在"比静态数据格式更灵活，比重新编译更轻量"这个中间地带。
