-- status_plugin.lua
--
-- 演示 Plugin-level State：run_count 是这个插件自己私有的数据，每次
-- 被 Framework.execute_command 调用一次就自增一次。这个变量对
-- Framework 的 registry、对 hello_plugin/debug_plugin 都完全不可见——
-- State 归这个插件自己所有，是"每个 Plugin 独立持有，通常不与其他
-- 插件或 Framework 共享"的具体例子。

local M = {
    name = "status",
}

local run_count = 0

function M.run()
    run_count = run_count + 1
    local msg = string.format("status_plugin has been invoked %d time(s)", run_count)
    print("[status_plugin] " .. msg)
    return run_count
end

return M
