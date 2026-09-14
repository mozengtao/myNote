-- string_utils.lua
--
-- Table Module：一组无状态的字符串处理函数，供 Framework/Application 调用。
-- 只暴露 API（split/trim/join），不持有任何运行时状态。
-- 结构固定：local M = {} ... return M。

local M = {}

--- split(str, sep) -> table
-- 按分隔符 sep（默认逗号）把字符串切成一个数组。
-- 用 plain=true 的 string.find 而不是模式匹配，避免 sep 本身含有 Lua
-- 模式特殊字符（例如 "." "%"）时出现匹配错误。
function M.split(str, sep)
    sep = sep or ","
    local result = {}
    local start = 1
    local sep_start, sep_end = string.find(str, sep, start, true)
    while sep_start do
        table.insert(result, string.sub(str, start, sep_start - 1))
        start = sep_end + 1
        sep_start, sep_end = string.find(str, sep, start, true)
    end
    table.insert(result, string.sub(str, start))
    return result
end

--- trim(str) -> string
-- 去掉字符串首尾空白字符（空格/Tab/换行）。
function M.trim(str)
    return (str:gsub("^%s+", ""):gsub("%s+$", ""))
end

--- join(list, sep) -> string
-- 用分隔符 sep（默认逗号）把数组拼接成一个字符串。
function M.join(list, sep)
    sep = sep or ","
    return table.concat(list, sep)
end

return M
