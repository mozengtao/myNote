-- Pattern: Conversation Cache（把跨包的状态挂在"会话"上，而不是用全局变量）
-- 教学示例，非完整插件；配合笔记 11-design-patterns.md 阅读。
--
-- 典型场景：第一个包里协商了"后续用什么方式解码"（比如某个握手包里声明了编码格式），
-- 后面同一个 TCP/UDP 会话的所有包都要沿用这个状态。
-- 关键 API：pinfo.conversation（4.5+ 用 Conversation.find_from_pinfo(pinfo)）。
--
-- 注意：Lua 里没有官方的"conversation_add_proto_data"直接绑定，
-- 常见做法是用一张以 conversation 对象（或其 tostring）为 key 的 Lua table 做缓存，
-- 并且要小心它在 "重新加载 capture / 重新解析" 时的生命周期（见 12-common-pitfalls.md）。

local p_conv = Proto("convexample", "Conversation Cache Example")
local f_mode = ProtoField.string("convexample.mode", "Negotiated Mode")
p_conv.fields = { f_mode }

-- 缓存表：key 是 tostring(conversation)，value 是我们自己定义的状态
-- 这不是 wmem，只是 Lua 侧的一个进程内表；文件重新打开时需要在 init() 里清空。
local conv_state = {}

function p_conv.dissector(buffer, pinfo, tree)
    local subtree = tree:add(p_conv, buffer())

    local conv = pinfo.conversation  -- 4.5 以前的写法；新代码建议用 Conversation.find_from_pinfo(pinfo)
    local key = tostring(conv)

    if buffer(0, 1):uint() == 0xFF then
        -- 这是"协商包"：把模式记下来，供后续包使用
        conv_state[key] = { mode = "binary" }
    end

    local state = conv_state[key]
    if state then
        subtree:add(f_mode, state.mode):set_generated()
    end
end

-- 打开新文件 / 重新解析整个 capture 时会调用一次，必须清掉旧状态，
-- 否则上一个文件残留的 conversation 状态会"串"到新文件里（常见踩坑）。
function p_conv.init()
    conv_state = {}
end
