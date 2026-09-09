-- SOURCE: https://gist.github.com/Lekensteyn/288ea8c2f8efdf87819f8489a75b5475
-- 作者: Peter Wu (@Lekensteyn)。这是社区被广泛引用/转载的 postdissector 教学示例，未做修改。
-- 用于本笔记案例2分析引用。
--
-- 特点：用 Field 提取器读取 tcp/udp 端口 + ip.src，做数值计算(strlen, log)，
-- 生成新字段，并展示"把计算结果字段 Apply as Column"这一常见real-world用法。

-- Wireshark post-dissector written in Lua
--
-- "Turns out #wireshark allows adding any field value as a column.
--  How about a column with a function of a value like strlen(x)+5 or log(x)?😁"
-- https://twitter.com/Wirefloss/status/790677617955344384
--
-- You were asking for what? Ok, here you go!  An example of a Lua
-- post-dissector which adds a field to the Wireshark protocol tree which can
-- then be displayed as custom column.  Have fun! ~ @Lekensteyn
--
-- Instructions:
-- 1. Install Lua dissector (copy into the plugins directory)
-- 2. Open the "Fake protocol" tree, right-click on the field and select
--    "Apply as Column".
--
-- Other references if you want to play:
-- https://wiki.wireshark.org/Lua/Dissectors#postdissectors
-- https://www.wireshark.org/docs/wsdg_html_chunked/wsluarm.html
-- https://www.wireshark.org/docs/wsdg_html_chunked/wsluarm_modules.html

-- Existing fields to use as input
local ip_src_field =        Field.new("ip.src")
local tcp_srcport_field =   Field.new("tcp.srcport")
local udp_srcport_field =   Field.new("udp.srcport")

-- our fake protocol
local fake_proto = Proto("fake", "Fake protocol")
-- our fake fields
local number_field = ProtoField.float("fake.number", "Number")
local number2_field = ProtoField.float("fake.number2", "Number2")
local string_field = ProtoField.string("fake.string", "String")
-- register fields to the protocol
fake_proto.fields = {
    number_field,
    number2_field,
    string_field,
}

function fake_proto.dissector(tvb, pinfo, tree)
    -- Add a new protocol tree for out fields
    local subtree = tree:add(fake_proto)

    -- Calculate "strlen(x) + 5" where "x" is the first ip.src field value
    -- Note: for technical reasons, tostring is needed to convert the internal
    -- Address type to a Lua string.
    local x = tostring(ip_src_field().value)
    local n = #x + 5

    -- Add the result to the tree (as number and as string)
    subtree:add(number_field, n)
    subtree:add(string_field, tostring(n))

    -- Example for "log(x)"
    -- Hack: support both tcp and udp ports
    local port = tcp_srcport_field() or udp_srcport_field()
    x = port.value
    n = math.log(x)
    subtree:add(number2_field, n)
end

-- Ensure that our dissector is invoked after dissection of a packet.
register_postdissector(fake_proto)
