-- SOURCE: https://wiki.wireshark.org/lua/dissectors  (章节: postdissectors)
-- 官方 Wiki 上的 "trivial postdissector" 示例，未做修改。
-- 用于本笔记案例2《PostDissector 模式》分析引用。
--
-- 特点：只用 Field 提取器读取别的协议(ip/tcp)已经解析好的字段，
-- 自己不解析任何 bytes，只是把已有信息重新组织展示成一个新的伪协议子树。

-- trivial postdissector example
-- declare some Fields to be read
ip_src_f = Field.new("ip.src")
ip_dst_f = Field.new("ip.dst")
tcp_src_f = Field.new("tcp.srcport")
tcp_dst_f = Field.new("tcp.dstport")
-- declare our (pseudo) protocol
trivial_proto = Proto("trivial","Trivial Postdissector")
-- create the fields for our "protocol"
src_F = ProtoField.string("trivial.src","Source")
dst_F = ProtoField.string("trivial.dst","Destination")
conv_F = ProtoField.string("trivial.conv","Conversation","A Conversation")
-- add the field to the protocol
trivial_proto.fields = {src_F, dst_F, conv_F}
-- create a function to "postdissect" each frame
function trivial_proto.dissector(buffer,pinfo,tree)
    -- obtain the current values the protocol fields
    local tcp_src = tcp_src_f()
    local tcp_dst = tcp_dst_f()
    local ip_src = ip_src_f()
    local ip_dst = ip_dst_f()
    if tcp_src then
       local subtree = tree:add(trivial_proto,"Trivial Protocol Data")
       local src = tostring(ip_src) .. ":" .. tostring(tcp_src)
       local dst = tostring(ip_dst) .. ":" .. tostring(tcp_dst)
       local conv = src  .. "->" .. dst
       subtree:add(src_F,src)
       subtree:add(dst_F,dst)
       subtree:add(conv_F,conv)
    end
end
-- register our protocol as a postdissector
register_postdissector(trivial_proto)
