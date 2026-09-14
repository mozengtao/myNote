-- config.lua
--
-- Table Module：配置读取的 Namespace。
-- 内部只保存"静态默认值"（defaults / feature_flags），不追踪
-- "谁在什么时候改过某个配置项"——一旦需要追踪修改历史，
-- 就已经超出 Table Module 的边界，会被推向 Pattern 2/3。

local M = {}

--- 默认配置：只读的静态数据，随模块加载一次性确定
M.defaults = {
    app_name = "myapp",
    max_connections = 100,
    timeout_ms = 3000,
    log_level = "info",
}

--- 默认 feature flag：只读静态数据
M.feature_flags = {
    dark_mode = true,
    experimental_cache = false,
    verbose_logging = false,
}

--- get(key, fallback) -> any
-- 从 defaults 中查找 key，找不到时返回调用者传入的 fallback。
function M.get(key, fallback)
    local value = M.defaults[key]
    if value == nil then
        return fallback
    end
    return value
end

--- is_enabled(flag) -> boolean
-- 查询某个 feature flag 是否开启，未知 flag 视为未开启。
function M.is_enabled(flag)
    return M.feature_flags[flag] == true
end

return M
