-- Pattern: Lazy Decode（对比 allfields=true：强制生成 + 性能代价）
-- 场景：跟例1同样的 lazycarrier/digest 设定，但 PostDissector 这次调用
-- `register_postdissector(proto, true)`，显式要求"哪怕字段没被引用/没被展开，
-- 也强制生成"。这是 12-common-pitfalls.md 9.9节给出的标准修复方式——
-- 需要稳定读到某个字段就传 true，代价是明显更大的性能开销（见 9.12节）。
--
-- 注意：这里的 digest 字段本身是否被生成，仍然取决于 lazycarrier2 自己的业务逻辑
-- （payload 是否超过阈值）——allfields=true 解决的是另一类"懒"：Wireshark 引擎层面
-- "这个字段没被 Filter/Column/Field 提取器引用，可能不会为每个包都生成对应 Tree 节点"
-- 的默认优化。两种"懒"经常一起出现，但改的是不同层面的开关，实际项目里都要考虑到。
--
-- 这是一个完整、可独立加载运行的插件（配合笔记 11-design-patterns.md 8.12节阅读）。
--
-- 帧格式（LazyCarrier，发到 UDP 40025）：跟例1完全一样。
--   offset 0    1字节  flags（占位，未使用）
--   offset 1..  N字节  payload；只有 N > 4 时才会计算 digest 字段
--
-- 加载测试：
--   wireshark -X lua_script:example2.lua
--   python3 -c "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); \
--       s.sendto(bytes([0]) + b'hello world', ('127.0.0.1', 40025))"
-- 用 tshark 对比两种写法在大文件上的耗时差异（allfields=true 通常明显更慢）：
--   time tshark -r big.pcap -X lua_script:example1.lua > /dev/null
--   time tshark -r big.pcap -X lua_script:example2.lua > /dev/null

local p_carrier = Proto("lazycarrier2", "Lazy Decode Example 2: Carrier Protocol")
p_carrier.prefs.port = Pref.uint("UDP Port", 40025, "UDP port this dissector listens on")

local f_flags  = ProtoField.uint8("lazycarrier2.flags", "Flags", base.HEX)
local f_digest = ProtoField.uint32("lazycarrier2.digest", "Payload Digest (expensive)", base.HEX)
p_carrier.fields = { f_flags, f_digest }

local DIGEST_THRESHOLD = 4

function p_carrier.dissector(buffer, pinfo, tree)
    if buffer:len() < 1 then return 0 end

    pinfo.cols.protocol = p_carrier.name
    local subtree = tree:add(p_carrier, buffer())
    subtree:add(f_flags, buffer(0, 1))

    local payload = buffer(1)
    if payload:len() > DIGEST_THRESHOLD then
        local digest = 0
        for i = 0, payload:len() - 1 do
            digest = (digest * 31 + payload(i, 1):uint()) % 0x100000000
        end
        subtree:add(f_digest, digest)
    end

    return buffer:len()
end

local udp_port = DissectorTable.get("udp.port")
udp_port:add(p_carrier.prefs.port, p_carrier)

----------------------------------------------------------------
-- PostDissector：跟例1几乎一样，唯一差异是 register_postdissector 的第二个参数。
----------------------------------------------------------------

local digest_f = Field.new("lazycarrier2.digest")

local p_lazypost = Proto("lazypost2", "Lazy Decode Example 2: PostDissector (allfields=true)")
local f_summary = ProtoField.string("lazypost2.summary", "Digest Summary")
p_lazypost.fields = { f_summary }

function p_lazypost.dissector(buffer, pinfo, tree)
    local subtree = tree:add(p_lazypost, buffer())

    -- 即使传了 allfields=true，防御性 nil 检查依然不能省：digest 字段本身
    -- 是否存在取决于 lazycarrier2 的业务逻辑（payload 长度是否超过阈值），
    -- allfields 只保证"该生成的字段不会因为没被引用而被跳过"，不能凭空造出数据里
    -- 原本就不存在的字段。
    local digest_finfo = digest_f()
    if digest_finfo then
        subtree:add(f_summary, string.format("digest=0x%08x", digest_finfo.value)):set_generated()
    else
        subtree:add(f_summary, "(digest field not present in this packet)"):set_generated()
    end
end

-- 第二个参数 true：强制生成 lazypost2 自身以及它依赖读取的字段，
-- 用于"不管用户在 GUI 里选没选中这个包，都要能稳定用 Field 提取器读到值"的场景
-- （比如批量统计/导出）。代价是性能明显下降，非必要不要在生产插件里默认开启。
register_postdissector(p_lazypost, true)
