-- framework.lua
--
-- Protocol Handler Plugin 的 Framework：维护一张"协议名 -> 插件"的
-- 注册表，dispatch(packet) 只读 packet.protocol 这一个字段，据此从
-- registry 里查出对应的插件并调用它——从不硬编码任何具体协议的
-- 名字或行为。
--
-- Contract：一个合法的 Handler Plugin 必须是一个 table，至少包含：
--   name           : string，协议名（用于日志/自描述）
--   handle(packet) : function，处理一个属于这个协议的 packet
--
-- 关键约束：这个文件永远不会出现
--     if packet.protocol == "http" then ... elseif packet.protocol == "dns" then ...
-- 这种按协议名分支的代码。新增一个协议 handler，只需要新加一个
-- plugins/*.lua 文件并在 app.lua 里注册，不需要改动这个文件。

local Framework = {}

-- registry：Framework-level State，key = 协议名字符串，value = plugin table。
local registry = {}

local function validate_plugin(plugin)
    if type(plugin) ~= "table" then
        return false, "plugin must be a table"
    end
    if type(plugin.name) ~= "string" or plugin.name == "" then
        return false, "plugin.name must be a non-empty string"
    end
    if type(plugin.handle) ~= "function" then
        return false, "plugin.handle must be a function"
    end
    return true
end

--- Framework.register_handler(protocol, plugin)
-- Register 步骤：校验契约 + 检查协议名冲突 + 存进 registry。
function Framework.register_handler(protocol, plugin)
    if type(protocol) ~= "string" or protocol == "" then
        error("register_handler: protocol must be a non-empty string")
    end

    local ok, err = validate_plugin(plugin)
    if not ok then
        error("register_handler: invalid plugin for protocol '" .. protocol .. "': " .. err)
    end

    if registry[protocol] then
        error("register_handler: protocol '" .. protocol .. "' already has a handler")
    end

    registry[protocol] = plugin
    return true
end

--- Framework.unregister_handler(protocol) -> ok, err
function Framework.unregister_handler(protocol)
    if not registry[protocol] then
        return false, "unregister_handler: no handler registered for '" .. tostring(protocol) .. "'"
    end
    registry[protocol] = nil
    return true
end

--- Framework.dispatch(packet) -> result, err
-- Invoke / Event 步骤：每一次 dispatch 调用都是 Framework 把"一个
-- 新事件"（一个 packet）转交给对应插件的 handle 函数去处理。
-- Framework 只负责"按 packet.protocol 查表 + 调用"，从不参与
-- packet 内部字段该怎么解读——那是插件的 Policy。
function Framework.dispatch(packet)
    if type(packet) ~= "table" or type(packet.protocol) ~= "string" then
        return nil, "dispatch: packet must be a table with a string 'protocol' field"
    end

    local plugin = registry[packet.protocol]
    if not plugin then
        return nil, "dispatch: no handler registered for protocol '" .. packet.protocol .. "'"
    end

    return plugin.handle(packet)
end

function Framework.list_protocols()
    local names = {}
    for name in pairs(registry) do
        names[#names + 1] = name
    end
    table.sort(names)
    return names
end

return Framework
