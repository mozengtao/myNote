-- Pattern: Dispatcher（字符串键分派）
-- 场景：Dispatcher 不一定要按数字分派，一个"迷你文本命令协议"按命令关键字
-- （GET / PUT / DEL）分派到不同处理函数，证明分派表的 key 可以是任意可比较的值。
--
-- 这是一个完整、可独立加载运行的插件（配合笔记 11-design-patterns.md 8.1节阅读）。
--
-- 报文格式（UDP payload，纯文本，用空格分隔，不含结尾换行）：
--   "GET <key>"          查询
--   "PUT <key> <value>"  写入
--   "DEL <key>"          删除
--
-- 加载测试：
--   wireshark -X lua_script:example2.lua
-- 发一条 PUT 命令到 UDP 40002：
--   python3 -c "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(b'PUT counter 42', ('127.0.0.1', 40002))"
-- 换成 b'GET counter' 测 GET 分支，b'XYZ counter' 测"未知命令"分支。

local p_dispcmd = Proto("dispcmd", "Dispatcher Pattern Example 2: Text Command Protocol")
p_dispcmd.prefs.port = Pref.uint("UDP Port", 40002, "UDP port this dissector listens on")

local f_command = ProtoField.string("dispcmd.command", "Command")
local f_key     = ProtoField.string("dispcmd.key", "Key")
local f_value   = ProtoField.string("dispcmd.value", "Value")
p_dispcmd.fields = { f_command, f_key, f_value }

-- 按命令关键字分派：key 是字符串而不是数字，Dispatcher 模式本身跟 key 的类型无关，
-- 只要能用 handlers[key] 查到对应处理函数就行。
local handlers = {}

handlers["GET"] = function(args, pinfo, subtree)
    subtree:add(f_key, args[1] or "")
    pinfo.cols.info:set("GET key=" .. (args[1] or "?"))
end

handlers["PUT"] = function(args, pinfo, subtree)
    subtree:add(f_key, args[1] or "")
    subtree:add(f_value, args[2] or "")
    pinfo.cols.info:set(string.format("PUT key=%s value=%s", args[1] or "?", args[2] or "?"))
end

handlers["DEL"] = function(args, pinfo, subtree)
    subtree:add(f_key, args[1] or "")
    pinfo.cols.info:set("DEL key=" .. (args[1] or "?"))
end

function p_dispcmd.dissector(buffer, pinfo, tree)
    pinfo.cols.protocol = p_dispcmd.name
    local subtree = tree:add(p_dispcmd, buffer())

    local text = buffer():string()
    -- 按空格拆分成 tokens：tokens[1] 是命令关键字，其余是参数
    local tokens = {}
    for tok in text:gmatch("%S+") do
        tokens[#tokens + 1] = tok
    end

    local command = tokens[1]
    subtree:add(f_command, buffer(0, command and #command or 0))

    local handler = command and handlers[command]
    if handler then
        local args = { tokens[2], tokens[3] }
        handler(args, pinfo, subtree)
    else
        subtree:add_proto_expert_info(nil, "unknown command " .. tostring(command))
        pinfo.cols.info:set("Unknown command " .. tostring(command))
    end

    return buffer:len()
end

local udp_port = DissectorTable.get("udp.port")
udp_port:add(p_dispcmd.prefs.port, p_dispcmd)
