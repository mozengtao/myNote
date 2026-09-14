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
