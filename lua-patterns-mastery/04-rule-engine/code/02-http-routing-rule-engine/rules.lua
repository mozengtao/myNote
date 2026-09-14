-- rules.lua
-- HTTP Routing Rule Engine
--
-- 这份文件不需要独立的 C Host：既是“规则表定义”，也是一份可以直接用
-- `lua rules.lua` 跑起来的自描述 driver（文件末尾的 `if not ...` 部分）。
-- 如果接入真实的 OpenResty/Nginx，规则表和 match_request/match_all 这两个
-- 函数会被搬进 access_by_lua_by_lua_file（或 content_by_lua_file），
-- driver 部分则替换成 Nginx 传进来的真实请求上下文，具体见本文件末尾的说明
-- 以及 02-example-http-routing-rule-engine.md 的第 13 节。
--
-- 每条规则统一长成：
--   {
--     name      = "<规则名>",
--     priority  = <number>,        -- 数字越大越先匹配
--     matcher   = function(req) ... end,   -- 路径前缀 + 方法匹配
--     condition = function(req) ... end,   -- 可选的附加条件（Header/查询参数）
--     backend   = "<转发目标>",
--   }

local routes = {
    {
        name = "admin-api",
        priority = 100,
        -- 更具体的路径要给更高的 priority，否则会被更宽泛的规则先截走
        matcher = function(req)
            return req.method == "GET" and req.path:match("^/api/admin/") ~= nil
        end,
        condition = function(req)
            return req.headers["X-Admin-Token"] ~= nil
        end,
        backend = "backend-admin"
    },
    {
        name = "api-read",
        priority = 50,
        matcher = function(req)
            return req.method == "GET" and req.path:match("^/api/") ~= nil
        end,
        condition = nil, -- 没有附加条件，matcher 命中即算命中
        backend = "backend-A"
    },
    {
        name = "api-write",
        priority = 50,
        matcher = function(req)
            return (req.method == "POST" or req.method == "PUT")
                and req.path:match("^/api/") ~= nil
        end,
        condition = function(req)
            -- 附加条件示例：要求请求带 Content-Type: application/json
            return req.headers["Content-Type"] == "application/json"
        end,
        backend = "backend-A"
    },
    {
        name = "admin-ui",
        priority = 40,
        matcher = function(req)
            return req.path:match("^/admin/") ~= nil
        end,
        condition = nil,
        backend = "backend-B"
    },
    {
        name = "static-assets",
        priority = 10,
        -- 附加条件示例：查询参数 ?debug=1 时改走调试后端
        matcher = function(req)
            return req.path:match("^/static/") ~= nil
        end,
        condition = function(req)
            return req.query and req.query.debug ~= "1"
        end,
        backend = "backend-static"
    },
    {
        name = "static-assets-debug",
        priority = 20, -- 比 static-assets 优先级更高，debug=1 时优先命中
        matcher = function(req)
            return req.path:match("^/static/") ~= nil
        end,
        condition = function(req)
            return req.query and req.query.debug == "1"
        end,
        backend = "backend-static-debug"
    },
    {
        name = "default-fallback",
        priority = 0, -- 最低优先级，兜底
        matcher = function(_)
            return true
        end,
        condition = nil,
        backend = "backend-default"
    }
}

--- 判断单条规则 route 是否命中请求 req：matcher 和 condition（如果存在）都要成立
local function rule_matches(route, req)
    if not route.matcher(req) then
        return false
    end
    if route.condition and not route.condition(req) then
        return false
    end
    return true
end

--[[
 match_first(req) —— First Match 语义
 路由场景通常用 First Match：按 priority 从高到低排序后，第一条命中的规则
 就决定最终转发目标，找到即返回，不再看后面的规则。
 返回命中的规则（table），若无命中返回 nil。
--]]
local function match_first(req)
    local sorted = {}
    for _, r in ipairs(routes) do
        table.insert(sorted, r)
    end
    table.sort(sorted, function(a, b) return a.priority > b.priority end)

    for _, route in ipairs(sorted) do
        if rule_matches(route, req) then
            return route
        end
    end
    return nil
end

--[[
 match_all(req) —— Multiple Match 语义
 如果需要“收集所有命中的规则，再按优先级合并”（例如同时要做路由转发 +
 附加一个日志规则 + 附加一个限流规则，三者互不冲突，都应该生效），
 就不能在第一条命中后提前退出，而是把所有命中的规则按 priority
 从高到低收集进一个数组返回，由调用方决定如何合并（取第一个当作转发目标，
 其余当作附加动作）。
--]]
local function match_all(req)
    local hits = {}
    for _, r in ipairs(routes) do
        if rule_matches(r, req) then
            table.insert(hits, r)
        end
    end
    table.sort(hits, function(a, b) return a.priority > b.priority end)
    return hits
end

-- 导出给其他模块 require 使用（例如接入真实 Nginx 时）
local M = {
    routes = routes,
    match_first = match_first,
    match_all = match_all,
}

--------------------------------------------------------------------------
-- Driver：可以直接用 `lua rules.lua` 跑一遍，演示匹配过程
--
-- 简化点：真实项目里通常会把 routes 表和 match_first/match_all 单独放进
-- 一个模块文件（例如 routes.lua），用 `local routes = require("routes")`
-- 引入到 Nginx 的 access_by_lua 阶段，driver 部分只在独立跑的时候存在。
-- 这里为了满足“一份文件里同时给出规则表和可运行 driver”的要求，两者写在
-- 同一个文件里；如果这个文件被 `require("rules")` 引入而不是直接执行，
-- run_demo() 仍然会被执行一次并打印演示输出，这是本文件的已知简化点。
--------------------------------------------------------------------------

local function print_request(req)
    io.write(string.format("Request: %s %s", req.method, req.path))
    if req.query and next(req.query) then
        io.write("?")
        local first = true
        for k, v in pairs(req.query) do
            if not first then io.write("&") end
            io.write(k .. "=" .. v)
            first = false
        end
    end
    io.write("\n")
end

local function run_demo()
    -- 构造几个模拟 Request 表
    local demo_requests = {
        {
            method = "GET",
            path = "/api/admin/users",
            headers = { ["X-Admin-Token"] = "secret123" },
            query = {},
        },
        {
            method = "GET",
            path = "/api/admin/users",
            headers = {}, -- 没带 admin token，会被 admin-api 拒绝，退回 api-read
            query = {},
        },
        {
            method = "POST",
            path = "/api/orders",
            headers = { ["Content-Type"] = "application/json" },
            query = {},
        },
        {
            method = "GET",
            path = "/admin/dashboard",
            headers = {},
            query = {},
        },
        {
            method = "GET",
            path = "/static/app.js",
            headers = {},
            query = {},
        },
        {
            method = "GET",
            path = "/static/app.js",
            headers = {},
            query = { debug = "1" },
        },
        {
            method = "GET",
            path = "/healthz",
            headers = {},
            query = {},
        },
    }

    print("=== HTTP Routing Rule Engine：First Match 演示 ===\n")
    for i, req in ipairs(demo_requests) do
        io.write(string.format("[%d] ", i))
        print_request(req)
        local route = match_first(req)
        if route then
            print(string.format("    -> 命中规则 %-20s priority=%-4d backend=%s\n",
                route.name, route.priority, route.backend))
        else
            print("    -> 无规则命中\n")
        end
    end

    print("=== 同一批请求用 Multiple Match 重新跑一遍（对比） ===\n")
    -- 只挑第 5/6 个请求演示：static-assets 和 static-assets-debug
    -- 在 First Match 下互斥，但如果规则设计成允许叠加（例如都命中时
    -- 都要生效），Multiple Match 可以把两条都收集出来。
    for i = 5, 6 do
        local req = demo_requests[i]
        io.write(string.format("[%d] ", i))
        print_request(req)
        local hits = match_all(req)
        io.write("    -> 命中 " .. #hits .. " 条规则: ")
        for _, r in ipairs(hits) do
            io.write(r.name .. "(" .. r.backend .. ") ")
        end
        io.write("\n\n")
    end
end

run_demo()

return M
