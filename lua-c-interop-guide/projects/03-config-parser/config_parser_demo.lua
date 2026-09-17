-- config_parser_demo.lua
--
-- 演示一个"真实应用"会怎样用嵌套 Table + 数组部分来配置 C 侧的
-- app_config_t：先给一份合法配置证明能被正确解析，
-- 再给三份写错的配置，用 pcall 证明校验能在恰当的地方拦下来，
-- 并报出清晰的字段路径（如 "server.port must be a number"）。

print("== 合法配置：应该被成功解析并打印 ==")
load_config({
    server = { host = "127.0.0.1", port = 8080 },
    limits = { max_connections = 100, timeout_ms = 5000 },
    features = { "logging", "metrics" },
})

print("\n== 错误示例 1：server.port 类型错误（字符串而不是数字） ==")
local ok1, err1 = pcall(load_config, {
    server = { host = "127.0.0.1", port = "8080" },
    limits = { max_connections = 100, timeout_ms = 5000 },
    features = { "logging" },
})
print("  ok1 =", ok1)
print("  err1 =", err1)

print("\n== 错误示例 2：server.port 超出合法范围 ==")
local ok2, err2 = pcall(load_config, {
    server = { host = "127.0.0.1", port = 99999 },
    limits = { max_connections = 100, timeout_ms = 5000 },
    features = {},
})
print("  ok2 =", ok2)
print("  err2 =", err2)

print("\n== 错误示例 3：缺失必填字段 limits.timeout_ms ==")
local ok3, err3 = pcall(load_config, {
    server = { host = "127.0.0.1", port = 8080 },
    limits = { max_connections = 100 },
    features = { "logging" },
})
print("  ok3 =", ok3)
print("  err3 =", err3)

print("\n[lua] 脚本执行完毕，程序没有崩溃")
