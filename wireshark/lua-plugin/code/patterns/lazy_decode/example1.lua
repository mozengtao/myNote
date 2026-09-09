-- Pattern: Lazy Decode（register_postdissector 默认 allfields=false，含防御性 nil 处理）
-- 场景：一个"承载协议" lazycarrier 只有在 payload 超过阈值时才计算一个代价较高的
-- digest 字段（模拟：真实场景里可能是字符串拼接、正则、多次遍历）；一个 PostDissector
-- lazypost 想用 Field 提取器读这个 digest 字段，但既然它不总是被生成，
-- 读到的可能是 nil，必须显式处理，不能假设"这个字段肯定存在"。
--
-- 这是一个完整、可独立加载运行的插件（配合笔记 11-design-patterns.md 8.12节阅读）。
-- 跟例2的唯一差异是最后一行 `register_postdissector(p_lazypost)` 不传第二个参数，
-- 等价于 `allfields=false`（默认值）——"没有被引用的字段可以不生成"这条默认策略，
-- 加上我们自己在 lazycarrier 里主动做的"按阈值决定要不要算"，是同一个"按需生成"思路
-- 在两个不同层面（Wireshark 引擎 / 脚本自身逻辑）的体现。
--
-- 帧格式（LazyCarrier，发到 UDP 40024）：
--   offset 0    1字节  flags（本示例未使用，占位）
--   offset 1..  N字节  payload；只有 N > 4 时才会计算 digest 字段
--
-- 加载测试：
--   wireshark -X lua_script:example1.lua
-- payload 长度<=4，digest 不会被生成，PostDissector 应该展示"字段不存在"：
--   python3 -c "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(bytes([0]) + b'ab', ('127.0.0.1', 40024))"
-- payload 长度>4，digest 会被生成，PostDissector 应该展示具体的 digest 值：
--   python3 -c "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(bytes([0]) + b'hello world', ('127.0.0.1', 40024))"

local p_carrier = Proto("lazycarrier1", "Lazy Decode Example 1: Carrier Protocol")
p_carrier.prefs.port = Pref.uint("UDP Port", 40024, "UDP port this dissector listens on")

local f_flags  = ProtoField.uint8("lazycarrier1.flags", "Flags", base.HEX)
local f_digest = ProtoField.uint32("lazycarrier1.digest", "Payload Digest (expensive)", base.HEX)
p_carrier.fields = { f_flags, f_digest }

local DIGEST_THRESHOLD = 4 -- 只有 payload 超过这个长度才值得付出"昂贵计算"的代价

function p_carrier.dissector(buffer, pinfo, tree)
    if buffer:len() < 1 then return 0 end

    pinfo.cols.protocol = p_carrier.name
    local subtree = tree:add(p_carrier, buffer())
    subtree:add(f_flags, buffer(0, 1))

    local payload = buffer(1)
    if payload:len() > DIGEST_THRESHOLD then
        -- 模拟"代价较高"的计算：真实场景可能是字符串拼接、正则匹配、多次遍历；
        -- 这里用一个逐字节累加的循环代表这种开销。
        local digest = 0
        for i = 0, payload:len() - 1 do
            digest = (digest * 31 + payload(i, 1):uint()) % 0x100000000
        end
        subtree:add(f_digest, digest)
    end
    -- payload 不超过阈值时，f_digest 这个字段在这个包里完全没有被 add 进 Tree——
    -- 对下面的 Field.new("lazycarrier1.digest") 提取器来说，这个包读到的就是 nil。

    return buffer:len()
end

local udp_port = DissectorTable.get("udp.port")
udp_port:add(p_carrier.prefs.port, p_carrier)

----------------------------------------------------------------
-- PostDissector：用 Field 提取器读 lazycarrier1.digest，必须做 nil 防御。
----------------------------------------------------------------

local digest_f = Field.new("lazycarrier1.digest")

local p_lazypost = Proto("lazypost1", "Lazy Decode Example 1: PostDissector (allfields=false)")
local f_summary = ProtoField.string("lazypost1.summary", "Digest Summary")
p_lazypost.fields = { f_summary }

function p_lazypost.dissector(buffer, pinfo, tree)
    local subtree = tree:add(p_lazypost, buffer())

    local digest_finfo = digest_f()
    if digest_finfo then
        subtree:add(f_summary, string.format("digest=0x%08x", digest_finfo.value)):set_generated()
    else
        -- 防御性处理：字段因为"懒解析"没被生成，不能假设它总是存在，
        -- 直接当成"这个包没有 digest"处理，而不是让 Lua 抛异常。
        subtree:add(f_summary, "(digest field not present in this packet)"):set_generated()
    end
end

register_postdissector(p_lazypost) -- 不传第二个参数，等价于 allfields=false（默认值）
