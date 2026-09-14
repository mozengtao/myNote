-- framework.lua
--
-- Dissector Plugin 的 Framework：不像 01/02 那样用"名字直接查表"来
-- 分发,这里 Framework 事先并不知道一个 packet_context 该交给谁——
-- 它遍历所有已注册的插件,依次调用每个插件自己的 can_handle(ctx),
-- 第一个返回 true 的插件获得 dissect(ctx) 的调用权。这是"插件数量、
-- 身份、顺序对 Framework 完全不可见"的另一种落地方式：Framework 从
-- 不知道、也不需要知道到底有几个 dissector、它们叫什么名字。
--
-- Contract：一个合法的 Dissector Plugin 必须是一个 table，至少包含：
--   name              : string，插件名（日志/自描述用）
--   can_handle(ctx)   : function，返回 true/false，表示"我能不能
--                        认领这个 packet_context"
--   dissect(ctx)      : function，真正的解析行为，只有在
--                        can_handle(ctx) 返回 true 之后才会被调用

local Framework = {}

-- registry：这里用数组而不是像 01/02 那样用按名字建的 hash 表,
-- 因为"谁能处理"这件事本身需要在运行时按注册顺序逐个询问插件,
-- 不是靠一个固定 key 直接查出来的。这是 Framework-level State。
local registry = {}

local function validate_plugin(plugin)
    if type(plugin) ~= "table" then
        return false, "plugin must be a table"
    end
    if type(plugin.name) ~= "string" or plugin.name == "" then
        return false, "plugin.name must be a non-empty string"
    end
    if type(plugin.can_handle) ~= "function" then
        return false, "plugin.can_handle must be a function"
    end
    if type(plugin.dissect) ~= "function" then
        return false, "plugin.dissect must be a function"
    end
    return true
end

--- Framework.register_dissector(plugin)
-- Register 步骤：校验契约 + 检查重名 + 追加进 registry 数组末尾
-- （数组顺序 = 询问 can_handle 的顺序）。
function Framework.register_dissector(plugin)
    local ok, err = validate_plugin(plugin)
    if not ok then
        error("register_dissector: invalid plugin: " .. err)
    end

    for _, existing in ipairs(registry) do
        if existing.name == plugin.name then
            error("register_dissector: dissector '" .. plugin.name .. "' already registered")
        end
    end

    registry[#registry + 1] = plugin
    return true
end

--- Framework.unregister_dissector(name) -> ok, err
function Framework.unregister_dissector(name)
    for i, plugin in ipairs(registry) do
        if plugin.name == name then
            table.remove(registry, i)
            return true
        end
    end
    return false, "unregister_dissector: dissector '" .. tostring(name) .. "' not registered"
end

--- Framework.dispatch(packet_context) -> name, result  或  nil, err
-- Invoke / Event 步骤：依次询问每个已注册插件的 can_handle(ctx)，
-- 第一个回答 true 的插件拿到 dissect(ctx) 的调用权，其结果连同该
-- 插件的 name 一起返回。如果没有任何插件认领，优雅地返回
-- (nil, err)，而不是抛异常或崩溃——对应 00-why-and-architecture.md
-- Failure 一节里"没有插件认领"这个分支。
function Framework.dispatch(packet_context)
    for _, plugin in ipairs(registry) do
        if plugin.can_handle(packet_context) then
            local result = plugin.dissect(packet_context)
            return plugin.name, result
        end
    end
    return nil, "dispatch: no dissector claimed this packet_context"
end

function Framework.list_dissectors()
    local names = {}
    for _, plugin in ipairs(registry) do
        names[#names + 1] = plugin.name
    end
    return names
end

return Framework
