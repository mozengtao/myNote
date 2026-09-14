-- config.lua
--
-- 这是一份"可计算"的配置文件，用来对比传统 key=value 静态配置：
--   1. Host（main.c）在加载它之前，通过全局变量/全局函数注入了两样东西：
--        ENV                  -- 当前运行环境（"development" / "production"）
--        os_default_workers   -- 一个 C 函数，返回"建议的 worker 数量"
--   2. config.lua 内部利用这两样东西做条件判断和函数调用，
--      而不是把计算结果写死成常量，体现"配置具备计算能力"。
--
-- 简化点：真实项目里 ENV 通常来自命令行参数或 os.getenv("APP_ENV")，
-- 这里为了让示例自包含、边界清晰，改成由 Host 在 luaL_loadfile 之前
-- 直接注入一个只读全局变量。

server = {
    -- 条件分支：生产环境固定用 80 端口，其余环境用 8080，
    -- 这一行在传统 key=value 配置里是不可能做到的。
    port = (ENV == "production") and 80 or 8080,

    -- 函数调用：worker 数量不是写死的常量，而是由 Host 提供的
    -- C 函数在"加载配置的这一刻"动态计算出来的。
    -- `os_default_workers and os_default_workers() or 4` 是 Lua 里
    -- 常见的"安全调用"写法：如果 Host 没有注入这个函数，就退回默认值 4。
    workers = os_default_workers and os_default_workers() or 4,

    -- 纯数据字段，和传统配置没有区别。
    timeout = 30,

    -- 布尔表达式：debug 开关由 ENV 推导得出，
    -- 不需要在配置里再手动维护一个容易和 ENV 字段"忘记同步"的开关。
    debug = (ENV ~= "production"),
}
