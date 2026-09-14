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
