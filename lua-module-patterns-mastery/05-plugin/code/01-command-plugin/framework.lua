-- framework.lua
--
-- Plugin Module (Extension / Dependency Inversion)：这是 Command Plugin
-- 示例里的 Framework（Plugin Manager）。它只认识"契约"（Contract）——
-- 任何插件必须是一个 table，至少包含：
--   name        : string，命令名，用于日志/自描述
--   run(...)    : function，执行这个命令时的具体行为
--
-- Framework 的职责边界（严格对照 00-why-and-architecture.md 里的六步）：
--   define contract   -> validate_plugin
--   load plugin       -> 不在这里，由 app.lua 通过 require() 完成
--   validate plugin   -> register_command 内部调用 validate_plugin
--   register plugin   -> register_command 把 plugin 存进 registry
--   invoke plugin     -> execute_command 从 registry 取出 plugin，调用 plugin.run
--   unload plugin     -> unregister_command
--
-- 关键约束：这个文件永远不会出现
--     if name == "hello" then ... elseif name == "status" then ...
-- 这种按插件身份分支的代码。新增一个命令插件，只需要在 plugins/
-- 目录下新加一个文件，在 app.lua 里 require + register，
-- 不需要改动这个文件一行代码。

local Framework = {}

-- registry：Framework 唯一的内部 State —— 当前已注册的插件集合。
-- key = 命令名，value = 插件 table 本身。这是 Framework-level State，
-- 和任何单个插件内部维护的 State 是完全不同的两件事（后者见
-- plugins/status_plugin.lua 的 run_count）。
local registry = {}

--- validate_plugin(plugin) -> ok, err
-- 契约本身的落地：一个 table 要成为合法的 Command Plugin，必须满足
-- 这里检查的形状。这个函数只关心"形状对不对"，完全不关心
-- "这是谁"、"它内部怎么实现"——那是 Plugin 自己的事，Framework 管不到
-- 也不应该管。
local function validate_plugin(plugin)
    if type(plugin) ~= "table" then
        return false, "plugin must be a table"
    end
    if type(plugin.name) ~= "string" or plugin.name == "" then
        return false, "plugin.name must be a non-empty string"
    end
    if type(plugin.run) ~= "function" then
        return false, "plugin.run must be a function"
    end
    return true
end

--- Framework.register_command(name, plugin)
-- Register 步骤：Framework 只做三件事——校验契约、检查命名冲突、
-- 存进 registry。它从不检查 plugin.run 里面的代码具体做了什么。
function Framework.register_command(name, plugin)
    if type(name) ~= "string" or name == "" then
        error("register_command: name must be a non-empty string")
    end

    local ok, err = validate_plugin(plugin)
    if not ok then
        error("register_command: invalid plugin for '" .. name .. "': " .. err)
    end

    if registry[name] then
        error("register_command: command '" .. name .. "' is already registered")
    end

    registry[name] = plugin
    return true
end

--- Framework.unregister_command(name) -> ok, err
-- Unload 步骤：把插件从 registry 里移除。Framework 不负责、也无法
-- 知道插件内部是否还有资源需要清理——如果插件需要清理逻辑，应该由
-- 插件自己的 shutdown 钩子负责（本示例的插件足够简单，不需要）。
function Framework.unregister_command(name)
    if not registry[name] then
        return false, "unregister_command: command '" .. tostring(name) .. "' is not registered"
    end
    registry[name] = nil
    return true
end

--- Framework.execute_command(name, ...) -> result, err
-- Invoke 步骤：Framework 唯一知道如何跟插件打交道的方式，就是调用
-- 它已经校验过的 plugin.run(...)。Framework 不知道、也不需要知道
-- hello_plugin.run 内部具体做了什么，只负责"找到它、按契约调用它、
-- 把返回值原样交还调用者"。
function Framework.execute_command(name, ...)
    local plugin = registry[name]
    if not plugin then
        return nil, "execute_command: unknown command '" .. tostring(name) .. "'"
    end
    return plugin.run(...)
end

--- Framework.list_commands() -> array of names
-- 只读地暴露当前 Framework-level State 的一份快照（已排序，方便打印
-- 稳定输出），不直接暴露 registry 表本身——避免外部代码绕过
-- register_command/unregister_command 直接改写内部 State。
function Framework.list_commands()
    local names = {}
    for name in pairs(registry) do
        names[#names + 1] = name
    end
    table.sort(names)
    return names
end

return Framework
