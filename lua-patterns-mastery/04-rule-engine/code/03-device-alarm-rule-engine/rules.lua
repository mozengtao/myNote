-- rules.lua
-- Device Alarm Rule Engine：与 firewall 示例的区别在于两点
--   1. Action 不是一个纯字符串，而是一个结构化 table { level, message }
--   2. Host 侧采用 Multiple Match 语义：本文件对外暴露一个 evaluate(metrics)
--      函数，一次性返回“这一轮所有命中规则的结果”组成的数组，而不是只返回
--      第一条命中规则的结果。
--
-- 设备指标字段：temperature（摄氏度）、cpu（百分比）、memory（百分比）、
-- link_state（"up"/"down"）。

rules = {
    -- IF temperature > 80 THEN ALARM
    {
        name = "temperature-critical",
        match = function(metrics)
            return metrics.temperature > 80
        end,
        action = function(metrics)
            return {
                level = "ALARM",
                message = string.format(
                    "设备温度过高: %.1f C（阈值 80 C）", metrics.temperature)
            }
        end
    },

    -- IF 60 < temperature <= 80 THEN WARNING
    {
        name = "temperature-warning",
        match = function(metrics)
            return metrics.temperature > 60 and metrics.temperature <= 80
        end,
        action = function(metrics)
            return {
                level = "WARNING",
                message = string.format(
                    "设备温度偏高: %.1f C（关注阈值 60 C）", metrics.temperature)
            }
        end
    },

    -- IF cpu > 90 THEN ALARM
    {
        name = "cpu-critical",
        match = function(metrics)
            return metrics.cpu > 90
        end,
        action = function(metrics)
            return {
                level = "ALARM",
                message = string.format(
                    "CPU 占用过高: %.1f%%（阈值 90%%）", metrics.cpu)
            }
        end
    },

    -- IF 75 < cpu <= 90 THEN WARNING
    {
        name = "cpu-warning",
        match = function(metrics)
            return metrics.cpu > 75 and metrics.cpu <= 90
        end,
        action = function(metrics)
            return {
                level = "WARNING",
                message = string.format(
                    "CPU 占用偏高: %.1f%%（关注阈值 75%%）", metrics.cpu)
            }
        end
    },

    -- IF memory > 90 THEN WARNING
    {
        name = "memory-warning",
        match = function(metrics)
            return metrics.memory > 90
        end,
        action = function(metrics)
            return {
                level = "WARNING",
                message = string.format(
                    "内存占用偏高: %.1f%%（关注阈值 90%%）", metrics.memory)
            }
        end
    },

    -- IF link_state == "down" THEN ALARM
    {
        name = "link-down",
        match = function(metrics)
            return metrics.link_state == "down"
        end,
        action = function(metrics)
            return {
                level = "ALARM",
                message = "链路已断开（link_state = down）"
            }
        end
    }
}

--[[
 evaluate(metrics) 是 Host 调用的统一入口。
 metrics 是一个 Lua table，字段来自 C 侧的一轮采集：
   { temperature = <number>, cpu = <number>, memory = <number>,
     link_state = "up"/"down" }

 返回值：一个数组（从下标 1 开始的 table），每个元素是
   { rule = "<规则名>", level = "ALARM"/"WARNING", message = "<描述>" }
 本轮命中了几条规则，数组就有几个元素；一条都没命中则返回空数组 {}。
 这就是 Multiple Match：与 firewall 示例的 First Match 不同，这里不会在
 找到第一条命中规则后就停止，而是把 rules 表完整遍历一遍。
--]]
function evaluate(metrics)
    local results = {}
    for _, rule in ipairs(rules) do
        if rule.match(metrics) then
            local result = rule.action(metrics)
            result.rule = rule.name
            table.insert(results, result)
        end
    end
    return results
end
