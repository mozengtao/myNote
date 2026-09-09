-- Pattern: Registry Pattern（示例1，入口文件：只做分发，不知道有多少子协议）
-- 场景：模拟 code/rls/ 三个真实文件的结构——一个"入口"创建共享的 DissectorTable，
-- 若干"子协议文件"各自在自己的 init() 里把自己 add 进这张表，入口完全不需要
-- 知道子协议有多少个/是谁。
--
-- 这 3 个文件（entry.lua / v1.lua / v2.lua）必须同时加载才是一个完整插件
-- （配合笔记 11-design-patterns.md 8.10节阅读）。
--
-- 加载方式（3个文件都要加载，顺序不影响结果，因为注册逻辑在 init() 里，
-- Wireshark 会在所有脚本的顶层代码跑完之后才统一调用各 Proto 的 init()）：
--   wireshark -X lua_script:entry.lua -X lua_script:v1.lua -X lua_script:v2.lua
-- 或者把三个文件一起放进 ~/.local/lib/wireshark/plugins/ 之类的个人插件目录。
--
-- 帧格式：offset 0 是1字节 version，紧跟着的字节转发给对应版本的子协议解析。
--
-- 测试 version=1：
--   python3 -c "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(bytes([1, 0xAA, 0xBB]), ('127.0.0.1', 40020))"
-- 测 version=9（未注册，走 Expert Info 分支）：
--   python3 -c "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(bytes([9, 0x01]), ('127.0.0.1', 40020))"

local entryProto = Proto("patternreg", "Registry Pattern Example 1: Entry")
entryProto.prefs.port = Pref.uint("UDP Port", 40020, "UDP port this dissector listens on")

-- 创建一张共享的 DissectorTable：这就是"注册表"本身，v1.lua/v2.lua 会各自把自己 add 进来。
-- 入口文件从此以后完全不需要知道注册表里到底有几个版本。
DissectorTable.new("patternreg.version", "PatternReg version", ftypes.UINT8, base.DEC, entryProto)

local f_version = ProtoField.uint8("patternreg.version", "Version", base.DEC)
entryProto.fields = { f_version }

function entryProto.dissector(buffer, pinfo, tree)
    if buffer:len() < 1 then return 0 end

    pinfo.cols.protocol = entryProto.name
    local subtree = tree:add(entryProto, buffer(0, 1))
    local version = buffer(0, 1):uint()
    subtree:add(f_version, buffer(0, 1))

    local sub = DissectorTable.get("patternreg.version"):get_dissector(version)
    if sub then
        sub:call(buffer(1):tvb(), pinfo, tree)
    else
        subtree:add_proto_expert_info(nil, "no sub-protocol registered for version " .. version)
        pinfo.cols.info:set("PatternReg: unknown version " .. version)
    end

    return buffer:len()
end

-- 把"挂到 udp.port"这一步放在 init() 里，跟 code/rls/rls_dispatcher.lua 的写法一致，
-- 遵守 03-lifecycle.md 里"注册相关的副作用应该发生在 init()，不是顶层代码"的规则。
function entryProto.init()
    local udp_port = DissectorTable.get("udp.port")
    udp_port:add(entryProto.prefs.port, entryProto)
end
