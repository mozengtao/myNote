-- app.lua
--
-- Framework/Application：require("http") 拿到常量 + 工具函数的集合，
-- 直接当作一个只读的"HTTP 协议知识包"来用。

local http = require("http")

print("== HTTP 方法常量 ==")
local method_order = { "GET", "POST", "PUT", "DELETE", "PATCH" }
for _, name in ipairs(method_order) do
    print(string.format("  %s -> %s", name, http.METHODS[name]))
end

print("\n== 构建请求 Header ==")
local headers = {
    http.build_header("Content-Type", "application/json"),
    http.build_header("Authorization", "Bearer token123"),
}
for _, h in ipairs(headers) do
    print("  " .. h)
end

print("\n== 状态码检查 ==")
local codes_to_check = { 200, 201, 301, 404, 500 }
for _, code in ipairs(codes_to_check) do
    print(string.format("  %d %s -> is_success=%s",
        code, http.status_text(code), tostring(http.is_success(code))))
end
