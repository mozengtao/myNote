-- SOURCE: https://wiki.wireshark.org/Lua/Examples  (章节: editing columns)
-- 官方 Wiki 示例，未做修改。用于本笔记案例2分析引用。
--
-- 特点：postdissector 最小可用形态——不建树、不加字段，只修改 Info 列文本。
-- 注意官方注释解释了为什么不能用 Tap 来做这件事：Tap 不保证在能设置列文本的时机运行。

-- Append "<dst> -> <src>" to the Info column with a post-dissector.
-- (Taps are not guaranteed to be run at a point when they can set the
-- column text, so they can't be used for this.)

-- create a new protocol so we can register a post-dissector
local myproto = Proto("swapper","Dummy proto to edit info column")

-- the dissector function callback
function myproto.dissector(tvb,pinfo,tree)
    pinfo.cols.info:append(" " .. tostring(pinfo.dst).." -> "..tostring(pinfo.src))
end
-- register our new dummy protocol for post-dissection
register_postdissector(myproto)
