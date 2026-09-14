-- app.lua
--
-- Framework/Application：通过 require("string_utils") 拿到 Module 的引用，
-- 只调用它暴露的 API，不知道、也不关心 split/trim/join 内部怎么实现。

local string_utils = require("string_utils")

local csv_line = "  alice , bob ,charlie  "
print(string.format("原始字符串: %q", csv_line))

print("\n== split 后的原始片段 ==")
local raw_parts = string_utils.split(csv_line, ",")
for i, p in ipairs(raw_parts) do
    print(string.format("  [%d] %q", i, p))
end

print("\n== 逐个 trim 后 ==")
local trimmed_parts = {}
for i, p in ipairs(raw_parts) do
    trimmed_parts[i] = string_utils.trim(p)
    print(string.format("  [%d] %q", i, trimmed_parts[i]))
end

print("\n== join 结果 ==")
local joined = string_utils.join(trimmed_parts, " | ")
print("  " .. joined)
