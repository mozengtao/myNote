-- SOURCE: https://wiki.wireshark.org/Lua/Examples  (章节: View Packet Tree of Fields/FieldInfo)
-- 官方 Wiki 示例，未做修改。用于本笔记案例2分析引用。
--
-- 特点：postdissector + all_field_infos() 遍历"当前包已经解析出的全部字段" + GUI 菜单(register_menu)
-- + TextWindow，展示了 PostDissector 如何跟 GUI 基础设施组合，做成一个调试/检视工具。

-- This example iterates through the field tree of the packets, and prints out the tree field information in a text window.
-- It shows the current tree for the selected packet, but this does not mean it always shows the full tree,
-- because wireshark performs multiple dissection passes of a packet, with the initial pass only being high-level and not
-- dissecting fully (for performance reasons).  You can see this behavior better by changing line 30 of this example
-- to this, so it concatenates output instead of clearing it every time:
-- output = output .. "\nTree fields for packet #".. pinfo.number .. ":\n"

-- this only works in wireshark
if not gui_enabled() then return end

local output = ""    -- for the output we'll show in the text window
local tw = nil       -- the text window
-- function to refresh the text window
local function updateWindow()
    if tw then tw:set(output) end
end

-- calling tostring() on random FieldInfo's can cause an error, so this func handles it
local function getstring(finfo)
    local ok, val = pcall(tostring, finfo)
    if not ok then val = "(unknown)" end
    return val
end

-- create a new protocol so we can register a post-dissector
local myproto = Proto("tree_view","Dummy proto to view tree")

-- the dissector function callback
function myproto.dissector(tvb,pinfo,tree)
    output = output.. "\nTree fields for packet #".. pinfo.number .. ":\n"
    -- get a table of all FieldInfo objects
    local fields = { all_field_infos() }
    for ix, finfo in ipairs(fields) do
        output = output .. "\t[" .. ix .. "] " .. finfo.name .. " = " .. getstring(finfo) .. "\n"
    end
    updateWindow()
end
-- register our new dummy protocol for post-dissection
register_postdissector(myproto)


-- now we create the menu function for this, which creates a text window to display this stuff
local function menu_view_tree()
    tw = TextWindow.new("Tree View")
    tw:set_atclose(function() tw = nil end)
    updateWindow()
end

-- add this to the Tools->Lua submenu
register_menu("Lua/Tree View", menu_view_tree, MENU_TOOLS_UNSORTED)
