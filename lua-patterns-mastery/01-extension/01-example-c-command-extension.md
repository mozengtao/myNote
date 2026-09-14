# 01-1 · 示例：C Application + Lua Command Extension

> Pattern：Extension（参见 [00-why-and-architecture.md](00-why-and-architecture.md)）。
> 完整代码：[code/01-c-command-extension/](code/01-c-command-extension/)（`main.c` / `extension.lua` / `Makefile`）。

## 1. Problem

我们要写一个最小化的 C 命令行 Host：它从标准输入读取一行行命令，执行对应的动作。传统写法是用一个 `if/else` 或 `switch` 把命令名硬编码在 C 代码里——每新增一个命令，就要改 C 代码、重新编译、重新分发整个二进制。

我们想要的效果是：C Host 编译一次之后，"有哪些命令、每个命令具体做什么"完全由外部的 Lua 脚本决定，增删命令不需要重新编译 Host。

## 2. Why Lua

- Lua 的 C API 是"为被嵌入而生"的，`register_command` 这种"C 函数注册进 `lua_State`，Lua 反过来调用它"的模式是 Lua 的标准玩法，不需要额外的 IPC/RPC 框架。
- 命令回调这种"短小、经常改、允许出错"的逻辑，非常适合放进一个可以被 `lua_pcall` 兜底的解释器：脚本写错了，只是这一条命令失败，不会导致整个命令行程序段错误退出。
- Lua 的 varargs（`...`）和动态类型，天然适合表达"命令参数数量、类型不固定"这种场景，不需要在 C 侧为每个命令单独定义参数结构体。

## 3. Architecture

```text
main.c (Host)
    |
    +-- Core: 命令行循环 / stdin 解析 / 命令表(command_entry_t[])
    |
    +-- API: register_command(name, callback)
    |
    +-- Lua Runtime: lua_State *L
           |
           +-- extension.lua
                  |
                  +-- register_command("hello", ...)
                  +-- register_command("add", ...)
                  +-- register_command("boom", ...)
                  +-- register_command("state", ...)
```

`main.c` 只认识"命令名字符串 -> 一个 Lua 函数引用"这条映射，完全不知道 `hello`/`add`/`boom`/`state` 各自做了什么。

## 4. Host responsibilities

- 创建并管理 `lua_State` 的完整生命周期（创建 -> 加载脚本 -> 多次调用 -> 关闭）；
- 提供并注册 `register_command` 这一个 C API，作为 Lua 唯一的注册入口；
- 维护命令名到 Lua 回调引用（`LUA_REGISTRYINDEX` 里的 ref）的映射表；
- 负责真正的 I/O：从 stdin 读取一行、按空白字符切分成命令名+参数；
- 负责调用协议：把参数压栈、`lua_pcall`、取返回值/取错误信息；
- 处理两类失败：命令不存在（C 侧直接报错，不进入 Lua）、Lua 回调运行时报错（`lua_pcall` 返回非 `LUA_OK`）；
- 进程退出前调用 `lua_close` 释放整个 Lua VM。

## 5. Lua responsibilities

- 定义"某个命令名对应做什么"这段具体逻辑（`function(...) ... end`）；
- 调用 `register_command` 把自己注册进 Host；
- 在回调内部完成参数校验，参数不合法时用 `error()` 主动抛出错误，交给 Host 的 `lua_pcall` 兜底；
- 维护 Extension 自身需要的局部状态（例如 `state` 命令里的调用计数器 `call_count`，是 `extension.lua` 里的一个闭包变量，Host 完全看不到它）。

## 6. Lifecycle

```text
main() 启动
  |
  v
luaL_newstate + luaL_openlibs
  |
  v
lua_register("register_command", ...)
  |
  v
luaL_loadfile("extension.lua") + lua_pcall  <-- 脚本执行，内部调用 register_command 4 次
  |
  v
进入 stdin 命令行循环（等待用户输入）
  |
  v
用户输入一行 -> dispatch_command() -> lua_pcall 调用对应回调
  |
  v
（循环，直到用户输入 quit/exit 或 EOF）
  |
  v
lua_close(L)
  |
  v
main() 返回
```

## 7. Data Flow（C -> Lua）

用户输入 `add 3 5` 之后：

1. `main()` 用 `fgets` 读到一行字符串 `"add 3 5\n"`；
2. `tokenize` 用 `strtok` 按空白拆成 `["add", "3", "5"]`；
3. `dispatch_command` 用 `find_command("add")` 查到对应的 `lua_ref`；
4. `lua_rawgeti(L, LUA_REGISTRYINDEX, ref)` 把 Lua 回调函数压回栈顶；
5. `lua_pushstring(L, "3")`、`lua_pushstring(L, "5")` 把两个参数依次压栈——**注意这里传进 Lua 的是字符串，不是数字**，`add` 回调内部要用 `tonumber` 转换，这是 C/Lua 边界"只能传值栈能表达的类型"的直接体现；
6. `lua_pcall(L, 2, 1, 0)` 触发调用，2 个参数、期望 1 个返回值。

## 8. Control Flow（Lua -> C）

`add` 回调内部：

1. `local a = tonumber(args[1])`、`local b = tonumber(args[2])`：把 C 传来的字符串参数转换成 Lua number；
2. 如果转换失败（比如用户输错成 `add foo bar`），调用 `error("...")`——这是 Lua 主动把控制权连同一个错误值"抛"回给 C；
3. `lua_pcall` 在 C 侧捕获到这次 `error()`，返回值不是 `LUA_OK`，栈顶被替换成错误信息字符串；
4. `dispatch_command` 用 `lua_tostring(L, -1)` 取出错误信息并打印，然后 `lua_pop` 清理栈，**函数正常返回，不会让整个 C 进程崩溃**；
5. 如果一切正常，Lua 回调 `return true`，`lua_pcall` 返回 `LUA_OK`，C 侧从栈顶取回这个返回值（本示例里只是演示性地 `lua_pop` 掉，未强制使用）。

## 9. State

- **Host 侧 state**：`g_commands[]` 数组（命令名 + 引用 ID），这是纯 C 内存，随进程存在，直到 `lua_close` 才连带清空注册表引用；
- **Lua 侧 state**：`extension.lua` 里 `state` 命令用到的 `call_count`，是一个 Lua 闭包变量，生命周期跟随整个 `lua_State`（只要 VM 不关闭，这个变量就一直存在、持续累加）；
- 两边状态互不可见：C 不知道 `call_count` 的存在，Lua 也拿不到 `g_commands[]` 这个 C 数组，唯一的桥梁是 `register_command` 时换来的整数 `lua_ref`。

## 10. Failure

本示例演示两条明确的失败路径，均不会导致 C 进程崩溃：

1. **命令不存在**：用户输入一个未注册的命令名（例如 `nosuch`），`find_command` 返回 `NULL`，`dispatch_command` 直接打印错误并返回，完全不触碰 Lua VM；
2. **Lua 回调运行时报错**：`add` 参数非法、或 `boom` 命令故意 `error()`，`lua_pcall` 返回非 `LUA_OK`，C 侧打印 `lua_tostring(L, -1)` 里的错误信息，`lua_pop` 清理栈后继续下一轮循环。

两条路径都保证：**一次命令失败，只影响这一次命令，Host 进程和 Lua VM 都继续存活**。

## 11. Complete Host Code

与 [code/01-c-command-extension/main.c](code/01-c-command-extension/main.c) 完全一致：

```c
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
```

## 12. Complete Lua Code

与 [code/01-c-command-extension/extension.lua](code/01-c-command-extension/extension.lua) 完全一致：

```lua
-- extension.lua
-- 这是 Host（main.c）在启动时加载的 Lua 脚本。
-- 它的职责只有一件事：调用 Host 暴露的 register_command()，
-- 把"新命令该怎么做"这件事注册进 Host，而不修改 Host 的任何一行 C 代码。

-- 命令 1：hello
-- 用法：hello <name>
register_command("hello", function(...)
    local args = { ... }
    local name = args[1] or "world"
    print(string.format("Hello, %s! (来自 Lua 的问候)", name))
    return true
end)

-- 命令 2：add
-- 用法：add <a> <b>  -- 把两个数字相加，演示参数解析 + 简单错误处理
register_command("add", function(...)
    local args = { ... }
    local a = tonumber(args[1])
    local b = tonumber(args[2])
    if not a or not b then
        -- 用 error() 主动抛出一个 Lua 运行时错误，
        -- Host 侧的 lua_pcall 会捕获它，转换成"命令执行失败"而不会让进程崩溃。
        error("add 需要两个数字参数，例如: add 3 5")
    end
    print(string.format("%g + %g = %g", a, b, a + b))
    return true
end)

-- 命令 3：boom
-- 用法：boom  -- 故意抛出错误，用来演示"Lua 回调运行时报错"这条失败路径
register_command("boom", function(...)
    error("boom 命令故意抛出的错误：模拟脚本 bug")
end)

-- 命令 4：state
-- 用法：state  -- 演示 Lua 侧的模块级局部状态（闭包变量），每次调用递增
local call_count = 0
register_command("state", function(...)
    call_count = call_count + 1
    print(string.format("state 命令被调用了 %d 次", call_count))
    return true
end)

print("[extension.lua] 已加载，注册了 4 个命令：hello / add / boom / state")
```

## 13. Execution

`Makefile`（见 [code/01-c-command-extension/Makefile](code/01-c-command-extension/Makefile)，核心规则如下，文件末尾还附带了编译/运行命令和示例交互输出的注释）使用 `pkg-config --cflags --libs lua5.4` 探测编译参数：

```makefile
# Makefile
#
# 用 pkg-config 探测 lua5.4 的头文件路径和链接参数。
# 如果当前环境没有 lua5.4.pc（例如系统只装了 lua5.1/liblua），
# 可以把下面这一行替换成对应版本：
#   LUA_PKG = lua5.3   或者   LUA_PKG = lua-5.4   或者   LUA_PKG = lua
# 也可以完全不用 pkg-config，手动指定：
#   CFLAGS += -I/usr/include/lua5.4
#   LDLIBS += -llua5.4
LUA_PKG := lua5.4

CC := cc
CFLAGS := -Wall -Wextra -std=c11 $(shell pkg-config --cflags $(LUA_PKG))
LDLIBS := $(shell pkg-config --libs $(LUA_PKG))

TARGET := cmdhost
SRC := main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDLIBS)

run: $(TARGET)
	./$(TARGET) extension.lua

clean:
	rm -f $(TARGET)
```

编译与运行命令：

```bash
cd code/01-c-command-extension
make
./cmdhost
```

示例交互输出：

```text
$ make
$ ./cmdhost
[C Host] 已注册命令: hello
[C Host] 已注册命令: add
[C Host] 已注册命令: boom
[C Host] 已注册命令: state
[extension.lua] 已加载，注册了 4 个命令：hello / add / boom / state
[C Host] 命令行已就绪，共有 4 个命令。输入 'quit' 退出。
> hello world
Hello, world! (来自 Lua 的问候)
> add 3 5
3 + 5 = 8
> add foo bar
[C Host] 命令 'add' 执行失败: extension.lua:24: add 需要两个数字参数，例如: add 3 5
> boom
[C Host] 命令 'boom' 执行失败: extension.lua:33: boom 命令故意抛出的错误：模拟脚本 bug
> state
state 命令被调用了 1 次
> state
state 命令被调用了 2 次
> nosuch
[C Host] 错误: 命令不存在: nosuch
> quit
[C Host] 已退出，Lua VM 已释放。
```

调用链总图：

```text
User
 |
 v
C main()
 |
 v
Lua script
 |
 v
register_command()
 |
 v
C stores callback
 |
 v
User command
 |
 v
C calls Lua callback
 |
 v
Lua executes
 |
 v
Result returns to C
```

## 14. Step-by-step walkthrough

以 `add foo bar` 这一次失败调用为例，完整走一遍：

1. 用户在 `> ` 提示符后输入 `add foo bar` 并回车；
2. `fgets` 读到 `"add foo bar\n"`；
3. `tokenize` 拆分成 `tokens = ["add", "foo", "bar"]`；
4. `dispatch_command(L, "add", {"foo", "bar"}, 2)` 被调用；
5. `find_command("add")` 命中之前注册的记录，取到 `lua_ref`；
6. `lua_rawgeti` 把 `add` 对应的 Lua 函数压回栈顶；
7. 依次 `lua_pushstring(L, "foo")`、`lua_pushstring(L, "bar")`；
8. `lua_pcall(L, 2, 1, 0)` 触发执行；Lua 侧 `tonumber("foo")` 返回 `nil`，条件 `not a or not b` 成立，执行 `error("add 需要两个数字参数...")`；
9. Lua VM 把这次 `error()` 转换成 `lua_pcall` 的非 `LUA_OK` 返回值，栈顶被替换为形如 `extension.lua:24: add 需要两个数字参数，例如: add 3 5` 的字符串（Lua 自动在错误信息前加上了文件名和行号）；
10. C 侧 `status != LUA_OK` 成立，`lua_tostring(L, -1)` 取出这条信息并 `fprintf(stderr, ...)`；
11. `lua_pop(L, 1)` 清空栈，`dispatch_command` 返回；
12. 主循环继续，打印下一个 `> ` 提示符——**整个过程中 C 进程和 Lua VM 都没有被销毁**。

## 15. Mental Model

```text
Host = "一张空的命令登记表 + 一台读卡机"
Lua  = "往登记表里贴标签的人"

main.c 从来不写死"add 是什么"，
它只写死"怎么登记一个命令"和"怎么按名字查表再执行"。
```

把 `main.c` 想象成一台只认"插卡执行"的机器，`register_command` 就是插卡口；`extension.lua` 每次调用 `register_command` 就是往机器里插一张新卡。机器本身永远不变，卡可以随时换、加、删。

## 16. Why this is this Pattern

这是 Extension，而不是 Policy/Configuration/Rule Engine/Plugin，因为：

- Host 原本**不存在** `hello`/`add`/`boom`/`state` 这几个命令，它们是纯粹凭空新增的能力，符合 Extension 的核心定义；
- 不是 Policy：Lua 没有在 Host 已有的几条分支里做选择，而是自己定义了全新的分支；
- 不是 Configuration：Lua 没有只返回一份静态参数表给 Host 用，而是注册了一段会被反复执行的行为逻辑；
- 不是 Rule Engine：没有"一堆条件表达式匹配到一个动作"的表结构，每个命令名直接一一对应一个行为，不存在条件匹配层；
- 不是 Plugin：整个 `extension.lua` 和 `main.c` 共用同一个 `lua_State` 生命周期，没有独立的加载/卸载单元，`state` 命令的计数器和 Host 进程同生共死，不具备 Plugin 那种"可以单独卸载某一部分"的边界。

**如果 Lua 消失，Host 还能做什么？**
`main.c` 依然能启动，依然能读 stdin、切分命令、维护一个空的命令表，甚至依然能打印"命令不存在"——但它注册不进任何命令，因为唯一的注册来源（`extension.lua`）不存在了。Host 退化为一台"完全没有插卡"的读卡机：机制（Mechanism）都还在，能力（Behavior）归零。这恰好证明了 Extension Pattern 里"Host 只提供机制，能力完全来自 Lua"这个判断。

**如果把 Lua 换成 JSON/YAML/纯 C，这个 Pattern 还成立吗？**
- 换成 JSON/YAML：不成立。JSON/YAML 只能描述静态数据（字符串、数字、列表），描述不了"收到参数后做什么计算、怎么格式化输出、什么时候该报错"这种**行为**。用 JSON/YAML 最多能配置"命令名对应哪个预先写好的 C 函数指针"，那已经退化成 Configuration Pattern，不再是 Extension——新命令的具体逻辑仍然必须提前用 C 写好、编译进二进制。
- 换成纯 C（例如做成动态库 + `dlopen`）：Pattern 本身依然成立（本质都是"运行时注册新行为"），但失去了 Lua 提供的错误隔离——C 扩展一旦有内存越界或空指针，`dlopen` 加载进来的代码可以直接让主进程崩溃，不存在类似 `lua_pcall` 那样"错误变成一个可控返回值"的安全网，也失去了"改一行脚本不用重新编译"的敏捷性。
