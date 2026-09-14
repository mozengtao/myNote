-- app.lua
--
-- Framework/Application：require("config") 拿到配置读取 API，
-- 只通过 get()/is_enabled() 访问配置，不直接改 config.defaults。

local config = require("config")

print("== 读取已知配置项 ==")
print("  app_name: " .. config.get("app_name"))
print("  timeout_ms: " .. config.get("timeout_ms"))

print("\n== 读取未知配置项（带 fallback 默认值）==")
print("  retry_count (fallback=3): " .. config.get("retry_count", 3))

print("\n== Feature Flag 检查 ==")
local flags_to_check = { "dark_mode", "experimental_cache", "unknown_flag" }
for _, flag in ipairs(flags_to_check) do
    print(string.format("  %s -> %s", flag, tostring(config.is_enabled(flag))))
end
