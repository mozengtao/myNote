# 01 · 示例：C Server + Lua Access Policy

> 前置阅读：[00-why-and-architecture.md](00-why-and-architecture.md)。代码位于 [code/01-c-server-access-policy/](code/01-c-server-access-policy/)。

## 1. Problem（问题）

设想一个最简化的网络服务器：它已经会 accept 连接、解析出请求里的 `user`（发起者）和 `resource`（想访问的资源路径），也已经会执行一次请求（返回资源内容）或者拒绝一次请求（返回一个拒绝响应）。这些都是 Mechanism，C 代码早就写好了。

真正麻烦的是"访问控制规则"这一层：哪些用户能访问哪些资源，这份名单几乎肯定会经常变（新增一个黑名单用户、给某个资源加上权限要求），而且这种改动往往不是由写 C 代码的工程师决定的，可能是运营/安全团队在运行时就要调整。如果把这份规则直接写进 C 的 if/else，每改一条规则就要重新编译、重新发布整个服务器进程，而且一旦规则里出现逻辑错误，最坏情况是段错误或者错误地放行了本该拒绝的请求。

## 2. Why Lua（为什么用 Lua）

这正是 [00-mental-model-overview.md](../00-mental-model-overview.md) 里讲的"Host 内部两类需求，变化速度差 10x-100x"的具体体现：accept/parse/execute 这类 Mechanism 几年不改，访问规则这类 Policy 可能每天都要改。把 Policy 部分交给 Lua：

- 改一条规则只需要改 `policy.lua` 这一个文件，不需要重新编译 C 代码；
- `allow_request` 内部即使写错逻辑（比如访问了一个不存在的表字段），也只会在 `lua_pcall` 里变成一个可捕获的错误返回值，C Server 进程不会崩溃，本条请求可以按照"保守拒绝"处理，服务器可以继续处理下一条请求；
- 规则本身用 Lua 的 table 表达（黑名单表、需要权限的资源表），比在 C 里维护动态数组或者写一堆字符串比较的 if/else 更直接。

## 3. Architecture（架构）

```text
Request
 |
 v
C Server (accept + parse)
 |
 v
Lua Policy: allow_request(user, resource)
 |
 +---- true  ----> C: execute_action(ALLOW)
 |
 +---- false ----> C: call_deny_reason() -> C: execute_action(DENY)
```

`accept + parse` 由 C 完成（本示例里简化成遍历一个固定请求数组）；`allow_request` 是唯一进入 Lua 的入口，输入两个字符串，输出一个布尔值；`execute_action` 回到 C 侧，按布尔值真正打印"执行"或"拒绝"的结果。DENY 分支多了一步 `call_deny_reason`，只在需要展示拒绝原因时才调用，用来在同一个示例里演示"取回布尔值"和"取回字符串值"两条不同的 Lua 调用路径。

## 4. Host responsibilities（Host 的职责）

- 创建并管理 `lua_State` 的整个生命周期（创建、加载脚本、多次调用、最终销毁）。
- 模拟"receive request"：维护一份请求列表（`g_requests`），这部分本该来自真实的 socket 读取和协议解析，这里退化成遍历一个固定数组。
- 提供两个 Policy API 调用点：`call_allow_request`（取回 ALLOW/DENY）、`call_deny_reason`（取回拒绝原因文本）。
- 拥有并执行真正的 Mechanism：`execute_action` 打印"执行请求"或"拒绝请求"的结果——这一步不关心"为什么"，只关心"做还是不做"。
- 处理 Lua 侧所有可能的失败（脚本加载失败、脚本执行失败、`allow_request` 未定义、`lua_pcall` 报错），并在任何失败情况下选择"保守拒绝"而不是"默默放行"。

## 5. Lua responsibilities（Lua 的职责）

- 维护两份规则数据：黑名单用户表 `blacklist_users`、需要 admin 权限的资源表 `admin_only_resources`，以及拥有 admin 权限的用户表 `admin_users`。这些表对 C 完全不可见。
- 实现 `allow_request(user, resource)`：读取上述规则表，返回一个布尔值。
- 实现 `deny_reason(user, resource)`：用同样的规则重新判断一次命中了哪条规则，返回对应的说明文字。
- 不做任何 I/O、不持有任何真实资源（socket、文件），只做纯粹的判断和字符串拼接。

## 6. Lifecycle（生命周期）

```text
luaL_newstate()          创建一个空的 lua_State
    |
    v
luaL_openlibs()           打开标准库（policy.lua 用到 string.format）
    |
    v
luaL_loadfile + lua_pcall  加载并执行 policy.lua
                            (此时 allow_request / deny_reason 两个全局函数就位)
    |
    v
for each request:
    call_allow_request()  Host 主动发起调用，Lua 临时获得控制权
    [call_deny_reason()]  仅 DENY 时发生
    |
    v
lua_close()                销毁 VM，所有闭包、全局表一并释放
```

`policy.lua` 本身没有独立的"卸载"概念——它不是 Pattern E（Plugin）意义上的可插拔模块，只是随着 Host 进程的启动而加载、随着 Host 进程的退出（`lua_close`）而消亡。每一次 `allow_request` 调用之间，Lua 侧没有任何状态保留的必要（本示例里两个规则表是常量表，不会被修改）。

## 7. Data Flow（C -> Lua）

一次决策请求携带的数据流：

```text
C: request_t { user, resource }
    |
    v
lua_pushstring(L, user)
lua_pushstring(L, resource)
    |
    v
Lua: allow_request(user, resource)
```

`user`/`resource` 在 C 侧是普通的 `const char *`，跨过 C/Lua 边界的唯一方式是 `lua_pushstring`——把这两个 C 字符串复制成 Lua 的字符串值，压入 Lua 的值栈。Lua 函数的两个形参 `user`、`resource` 直接对应栈上这两个值，Lua 侧看到的就是两个普通的 Lua 字符串，不知道也不需要知道它们在 C 侧曾经是什么类型。

## 8. Control Flow（Lua -> C）

```text
Lua: return true / return false
    |
    v
lua_pcall(L, 2, 1, 0) 返回，栈顶是决策结果
    |
    v
C: int allowed = lua_toboolean(L, -1)
```

`allow_request` 用 `return true` 或 `return false` 结束，这个返回值被 `lua_pcall` 放在栈顶。C 侧用 `lua_toboolean` 把它转换成一个 C 的 `int`（0 或非 0）。控制权在 `lua_pcall` 调用期间短暂交给 Lua，函数返回之后立刻交还给 C——C 从这一刻开始决定"做什么"，Lua 已经不再参与。

如果需要拒绝原因，控制权会再一次短暂地交给 Lua（`call_deny_reason` 里的第二次 `lua_pcall`），这次用 `lua_tostring` 取回一个字符串而不是布尔值，取值方式不同，但控制权转移的模式完全一样。

## 9. State（状态归属）

- **C 侧状态**：请求列表 `g_requests`（谁在发起请求这件事本身，属于 Host 的运行时状态，Lua 看不到也不需要看到）。
- **Lua 侧状态**：`blacklist_users`、`admin_only_resources`、`admin_users` 三张规则表。它们是 Lua 脚本级别的局部变量（用 `local` 声明），生命周期跟随 `lua_State` 本身；C 无法直接读取或修改这些表的内容，只能通过调用 `allow_request`/`deny_reason` 间接观察它们产生的效果。
- 本示例里没有"跨多次请求累积的状态"（比如"连续失败 3 次就永久拉黑"），如果要加这种状态，会是 Lua 侧一个新的 `local` 变量（例如一个按 user 计数的 table），依然完全不需要 C 侧知晓。

## 10. Failure（失败路径）

- **脚本文件不存在/语法错误**：`luaL_loadfile` 返回非 `LUA_OK`，C 打印错误信息、`lua_close`、进程以非 0 状态退出——这种失败发生在服务器启动阶段，属于"配置错误"，应该让进程直接失败退出，而不是带着一个坏掉的策略继续跑。
- **`allow_request` 未定义**（脚本被改错、函数名写错）：`lua_isfunction` 检查失败，`call_allow_request` 直接返回 0（拒绝），进程继续运行，但这一条请求以及后续所有请求都会被拒绝，行为是可预测的"fail closed"，不是随机行为。
- **`allow_request` 内部运行时报错**（例如访问了一个不存在的字段导致 `nil` 索引错误）：`lua_pcall` 返回非 `LUA_OK`，栈顶是错误信息字符串，C 打印这条信息、同样保守拒绝，进程不崩溃，可以继续处理下一条请求。
- **`deny_reason` 未定义或执行失败**：不影响决策本身（决策已经是 DENY 了），只是拒绝原因文本退化成一个占位说明（`"(未提供拒绝原因)"` 或 `"(获取拒绝原因失败: ...)"`）。

失败路径的统一原则：**Policy 相关的任何失败，都必须映射到"拒绝"这个更安全的默认值**，而不是意外地"放行"。这是访问控制类 Policy 的通用设计准则,不是 Lua 本身的特性。

## 11. Complete Host Code（完整 Host 代码）

对应文件：[code/01-c-server-access-policy/server.c](code/01-c-server-access-policy/server.c)

```c
/*
 * server.c
 *
 * 一个最小化的 C Host：模拟一个网络服务器收到一批请求，
 * 每个请求携带 user（发起者）和 resource（要访问的资源路径）两个字段。
 *
 * server.c 自己完全不知道"alice 能不能访问 /api/admin"这类具体规则，
 * 它只知道：
 *   receive request -> parse request -> 调用 Lua 决策 -> 按决策结果 execute/reject
 *
 * "是否放行"这一步，被完整地委托给 Lua 侧的 policy.lua 里的
 * allow_request(user, resource) 函数决定，C 不包含任何一条具体的
 * 访问控制规则。
 *
 * 架构：
 *   main() 创建 Lua VM
 *     -> luaL_dofile 加载 policy.lua（脚本只定义两个全局函数，无副作用）
 *     -> 遍历模拟请求列表
 *         -> call_allow_request(): lua_getglobal + lua_pushstring x2
 *            + lua_pcall(L, 2, 1, 0) + lua_toboolean 取回 ALLOW/DENY
 *         -> 如果 DENY，再调用 call_deny_reason() 取回人类可读的拒绝原因
 *            （lua_pcall(L, 2, 1, 0) + lua_tostring）
 *         -> execute_action() 打印真正执行/拒绝的结果（Mechanism 层）
 *     -> lua_close 释放 VM
 *
 * 简化点：真实网络服务器的 receive/parse 是从 socket 上读字节流、
 * 解析协议（HTTP 请求行、header 等），这里用一个固定的请求数组代替，
 * 只是为了让"决策"这一步的输入是确定的、示例输出可复现。
 */

#include <stdio.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

typedef struct {
    const char *user;
    const char *resource;
} request_t;

/*
 * 模拟收到的请求列表。
 * 覆盖两类场景：应该被放行的请求、应该被拒绝的请求（且拒绝原因不同：
 * 一条因为用户在黑名单，一条因为资源需要更高权限）。
 */
static const request_t g_requests[] = {
    {"alice", "/api/reports"}, /* 普通用户访问普通资源 -> 应该 ALLOW */
    {"bob",   "/api/admin"},   /* 普通用户访问管理员资源 -> 应该 DENY */
    {"eve",   "/etc/passwd"},  /* 黑名单用户 -> 应该 DENY */
    {"admin", "/api/admin"},   /* 管理员访问管理员资源 -> 应该 ALLOW */
};
#define NUM_REQUESTS (int)(sizeof(g_requests) / sizeof(g_requests[0]))

/*
 * call_allow_request
 *
 * 调用 Lua 侧的 allow_request(user, resource) -> boolean。
 *
 * 调用协议：
 *   lua_getglobal(L, "allow_request")  取出函数值，压栈
 *   lua_pushstring x2                  压入 user、resource 两个参数
 *   lua_pcall(L, 2, 1, 0)              2 个参数，期望 1 个返回值
 *   lua_toboolean(L, -1)               取回布尔决策
 *
 * 失败路径：找不到 allow_request，或者 pcall 报错，都按"保守拒绝"处理，
 * 这是访问控制类 Policy 的通用原则——决策函数本身出问题时，
 * 默认行为必须是 DENY，不能是 ALLOW。
 */
static int call_allow_request(lua_State *L, const char *user, const char *resource) {
    lua_getglobal(L, "allow_request");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        fprintf(stderr, "[C Host] 错误: allow_request 未定义\n");
        return 0;
    }

    lua_pushstring(L, user);
    lua_pushstring(L, resource);

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        fprintf(stderr, "[C Host] allow_request 执行失败: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return 0; /* 决策函数本身出错 -> 保守拒绝 */
    }

    int allowed = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return allowed;
}

/*
 * call_deny_reason
 *
 * 只在决策为 DENY 时才调用，取回一段人类可读的拒绝原因字符串。
 * 调用协议同样是 lua_pcall(L, 2, 1, 0)，但取值方式换成 lua_tostring，
 * 用来在同一个示例里演示"取回布尔值"和"取回字符串值"两条不同路径。
 *
 * 简化点：更常见的做法是让 allow_request 一次性返回
 * (boolean, string) 两个值（用 lua_pcall(L, 2, 2, 0) 一次取回），
 * 这里特意拆成两个独立函数、两次独立调用，是为了让 call_allow_request
 * 和 call_deny_reason 各自保持"单一职责、返回值类型单一"，
 * 便于第一次接触 Lua C API 的读者分别理解 lua_toboolean 和 lua_tostring
 * 两条取值路径，不代表这是唯一或最优的接口设计。
 *
 * lua_tostring 返回的指针指向 Lua 内部字符串对象的内存，一旦对应的
 * 栈位置被 lua_pop 弹出，这块内存就可能被 GC 回收，因此必须在 pop 之前
 * 用 snprintf 把内容复制进调用者提供的 buf，不能长期持有这个指针。
 */
static void call_deny_reason(lua_State *L, const char *user, const char *resource,
                              char *buf, size_t buf_size) {
    lua_getglobal(L, "deny_reason");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        snprintf(buf, buf_size, "%s", "(未提供拒绝原因)");
        return;
    }

    lua_pushstring(L, user);
    lua_pushstring(L, resource);

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        snprintf(buf, buf_size, "(获取拒绝原因失败: %s)", lua_tostring(L, -1));
        lua_pop(L, 1);
        return;
    }

    const char *reason = lua_tostring(L, -1);
    snprintf(buf, buf_size, "%s", reason ? reason : "(未知原因)");
    lua_pop(L, 1);
}

/*
 * execute_action
 *
 * Host 侧真正的 Mechanism 层：ALLOW 时执行请求本身该做的事情
 * （查数据库、返回文件内容……），DENY 时返回一个拒绝响应。
 * 这里用 printf 模拟，真实系统里这一段完全不关心"为什么"，
 * 只关心"做还是不做"，判断"为什么"的逻辑已经在上一步的 Lua 里完成了。
 */
static void execute_action(int allowed, const char *user, const char *resource,
                            const char *reason) {
    if (allowed) {
        printf("[C Host] ALLOW  user=%-6s resource=%-14s -> 执行请求\n", user, resource);
    } else {
        printf("[C Host] DENY   user=%-6s resource=%-14s -> 拒绝, 原因: %s\n",
               user, resource, reason);
    }
}

int main(int argc, char **argv) {
    const char *script_path = (argc > 1) ? argv[1] : "policy.lua";

    /* 1. 创建 Lua VM */
    lua_State *L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "无法创建 Lua VM\n");
        return 1;
    }

    /* 2. 打开标准库（policy.lua 里用到了 string.format） */
    luaL_openlibs(L);

    /* 3. 加载并执行 policy.lua。
     *    脚本本身只定义 allow_request/deny_reason 两个全局函数，
     *    不产生任何副作用，所以这一步执行完之后，两个函数就已经可用。
     *    分两步展开 luaL_dofile，方便分别处理"加载失败"（语法错误/
     *    文件不存在）和"执行失败"两种情况。 */
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

    printf("[C Host] Access Policy Server 已启动，共有 %d 个模拟请求待处理\n", NUM_REQUESTS);

    /* 4. 依次处理每个模拟请求：
     *    C 只负责 receive/parse（这里退化成遍历数组）和最终 execute，
     *    "是否放行"这一步完全交给 Lua 决定。 */
    for (int i = 0; i < NUM_REQUESTS; i++) {
        const request_t *req = &g_requests[i];
        printf("---\n[C Host] 收到请求 #%d: user=%s resource=%s\n",
               i + 1, req->user, req->resource);

        int allowed = call_allow_request(L, req->user, req->resource);

        char reason[128] = {0};
        if (!allowed) {
            call_deny_reason(L, req->user, req->resource, reason, sizeof(reason));
        }

        execute_action(allowed, req->user, req->resource, reason);
    }

    /* 5. 释放 Lua VM：所有闭包、全局表都随之销毁 */
    lua_close(L);
    printf("---\n[C Host] 已处理完 %d 个请求，Lua VM 已释放。\n", NUM_REQUESTS);
    return 0;
}
```

## 12. Complete Lua Code（完整 Lua 代码）

对应文件：[code/01-c-server-access-policy/policy.lua](code/01-c-server-access-policy/policy.lua)

```lua
-- policy.lua
--
-- 这是 Host（server.c）在启动时加载的 Lua 决策脚本。
-- 它只定义两个全局函数，不做任何 I/O、不持有任何真实资源：
--
--   allow_request(user, resource) -> boolean
--       决定这一次请求应不应该被放行，这是 Policy 的核心：
--       C 已经知道"怎么执行一个请求"（Mechanism），
--       不知道"这一次该不该执行"（Decision），后者由这个函数回答。
--
--   deny_reason(user, resource) -> string
--       只在 allow_request 返回 false 时被调用，给出人类可读的拒绝原因，
--       方便 Host 打印日志/返回给客户端，不影响决策本身。
--
-- 两个函数内部依赖的规则数据（黑名单用户表、需要管理员权限的资源表）
-- 全部是 Lua 侧的局部变量，C 完全不知道这些表的存在，
-- 也无法在不修改 policy.lua 的情况下看到或修改这些规则。

-- 黑名单用户：无论访问什么资源都直接拒绝
local blacklist_users = {
    eve = true,
}

-- 需要 admin 权限才能访问的资源
local admin_only_resources = {
    ["/api/admin"] = true,
}

-- 拥有 admin 权限的用户
local admin_users = {
    admin = true,
}

function allow_request(user, resource)
    if blacklist_users[user] then
        return false
    end

    if admin_only_resources[resource] and not admin_users[user] then
        return false
    end

    return true
end

function deny_reason(user, resource)
    if blacklist_users[user] then
        return string.format("用户 '%s' 在黑名单中", user)
    end

    if admin_only_resources[resource] and not admin_users[user] then
        return string.format("资源 '%s' 需要 admin 权限，当前用户 '%s' 不具备", resource, user)
    end

    return "未知原因"
end

print("[policy.lua] 已加载，access policy 规则就位")
```

## 13. Execution（运行方式）

```bash
cd code/01-c-server-access-policy
make
./access_server
```

需要系统已安装 Lua 5.4 的开发包（头文件 + 库），`Makefile` 用 `pkg-config lua5.4` 探测编译/链接参数；如果本机装的是其他版本（5.1/5.2/5.3）或者包名不同，按 `Makefile` 顶部的注释把 `LUA_PKG` 换成对应值即可，`server.c` 本身只用到 Lua 5.1 就已经稳定存在的 C API，不依赖某个具体小版本的新特性。

## 14. Step-by-step walkthrough（逐步执行走读）

完整输出：

```text
$ make
$ ./access_server
[policy.lua] 已加载，access policy 规则就位
[C Host] Access Policy Server 已启动，共有 4 个模拟请求待处理
---
[C Host] 收到请求 #1: user=alice resource=/api/reports
[C Host] ALLOW  user=alice  resource=/api/reports  -> 执行请求
---
[C Host] 收到请求 #2: user=bob resource=/api/admin
[C Host] DENY   user=bob    resource=/api/admin    -> 拒绝, 原因: 资源 '/api/admin' 需要 admin 权限，当前用户 'bob' 不具备
---
[C Host] 收到请求 #3: user=eve resource=/etc/passwd
[C Host] DENY   user=eve    resource=/etc/passwd   -> 拒绝, 原因: 用户 'eve' 在黑名单中
---
[C Host] 收到请求 #4: user=admin resource=/api/admin
[C Host] ALLOW  user=admin  resource=/api/admin    -> 执行请求
---
[C Host] 已处理完 4 个请求，Lua VM 已释放。
```

逐条对照：

1. `luaL_newstate` + `luaL_openlibs` + 加载 `policy.lua`：`print("[policy.lua] 已加载...")` 这一行证明脚本被成功加载并执行到了最后一行（如果脚本中途报错，这行不会出现，`main` 会在此之前就 `lua_close` 并退出）。
2. 请求 #1（**测试用例：应被允许**）：`alice` 访问 `/api/reports`，不在黑名单，`/api/reports` 也不在 `admin_only_resources` 里，`allow_request` 返回 `true`，`call_deny_reason` 不会被调用，直接打印 ALLOW。
3. 请求 #2（**测试用例：应被拒绝**）：`bob` 访问 `/api/admin`，命中 `admin_only_resources["/api/admin"]` 且 `bob` 不在 `admin_users` 里，`allow_request` 返回 `false`；随后 `call_deny_reason` 被调用，命中同一条规则，返回对应的说明文字。
4. 请求 #3：`eve` 在 `blacklist_users` 里，无论访问什么资源都直接 `false`，`deny_reason` 优先匹配黑名单规则。
5. 请求 #4：`admin` 访问 `/api/admin`，虽然命中 `admin_only_resources`，但 `admin` 同时在 `admin_users` 里，条件 `not admin_users[user]` 为假，整体判断不成立，最终返回 `true`。
6. 循环结束后 `lua_close(L)`，打印收尾日志，进程正常退出（退出码 0）。

## 15. Mental Model（心智模型）

```text
C Server
 |
 +-- Mechanism: accept / parse / execute / reject   (早就写好，从不因规则变化而改)
 |
 +-- Policy API: allow_request() / deny_reason()    (每次请求都调用一次)
         |
         v
        Lua
         |
         +-- Rules: blacklist_users / admin_only_resources / admin_users
         |
         +-- Decision: true/false + 说明文字
```

记忆点：C 侧的四个动作（accept/parse/execute/reject）永远不因为"谁能访问什么"这条规则变化而重新编译；唯一会变的是 `policy.lua` 里的三张表，改表不改二进制。

## 16. Why this is this Pattern（为什么这是 Policy 而不是别的模式）

这是 Policy，不是 Extension：C Server 在引入 Lua 之前，`execute_action` 的 ALLOW/DENY 两个分支就已经存在、已经能正常工作，Lua 没有让 Server 多学会一种新动作，只是把"选哪个分支"这一步从硬编码的 if/else 挪进了脚本。

这也不是 Rule Engine：`allow_request` 内部两条判断（黑名单、资源权限）没有被组织成一份"统一结构的规则列表"给 Host 遍历，Host 完全不知道也不关心 Lua 内部是怎么判断的，只拿到最终一个布尔值——决策逻辑对 C 是一个黑盒的单次调用，不是一组可以被 Host 遍历、增删的规则条目。

- **如果 Lua 消失，Host 还能做什么？** Host 依然能完整地 accept、parse、execute、reject——这些 Mechanism 完全不依赖 Lua。Host 会失去的，只是"运行时可调整的访问规则"这一能力：访问控制逻辑必须重新写回 C 代码（比如恢复成一段写死的 if/else），改一条规则重新需要编译、链接、发布整个二进制，运营/安全团队也不能自己动这份规则了。
- **如果把 Lua 换成 JSON/YAML/纯 C，这个 Pattern 还成立吗？** "机制与决策分离"这个 Pattern 本身依然成立，但载体的表达能力决定了能替代到什么程度。换成纯 C：完全退回到重新编译才能改规则的状态，只是仍然可以在架构上保持"决策逻辑单独一个函数"的组织方式。换成 JSON/YAML：这类声明式格式擅长表达"黑名单用户是一份列表""哪些资源需要什么权限"这类**静态**规则表，本示例里的 `blacklist_users`、`admin_only_resources` 这两张表完全可以用 JSON/YAML 表达，运行时用 C 或任何语言解析后查表判断；但如果决策逻辑变复杂（例如"根据一天中的时段动态调整权限""对同一用户的失败次数做限流"），JSON/YAML 本身没有计算能力，就必须靠解析它的宿主代码里额外写逻辑去处理，这部分逻辑又会变回不能在运行时修改的 C 代码——这正是 Lua 相对 JSON/YAML 的优势：Lua 既能表达数据（table），又能表达计算（函数），访问控制规则一旦超出"纯查表"的复杂度，Lua 依然可以在不改 C 代码的前提下继续演进，纯数据格式则不能。
