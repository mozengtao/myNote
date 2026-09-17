-- callback_demo.lua
-- 被 C 侧 luaL_dofile 加载后，只是把三个全局 Lua Function 定义出来，
-- 挂在全局 Table 里。真正"调用"它们的动作全部发生在 C 里
-- （callback_demo.c 用 lua_getglobal 把它们从全局 Table 取到 Stack 上，
-- 再用 lua_call / lua_pcall 触发执行）。

-- 一个正常返回的 Lua Function：给 C 侧 lua_call 演示的场景使用
function add(a, b)
    return a + b
end

-- 一个会主动报错的 Lua Function：
-- 用来演示 lua_call 遇到错误时会怎样（穿透式 longjmp）
-- 以及 lua_pcall 如何安全捕获同样的错误
function boom(a, b)
    if b == 0 then
        error("boom: b 不能是 0")
    end
    return a / b
end

-- 一个会抛出「非字符串」错误对象的 Lua Function：
-- error() 的第一个参数可以是任意 Lua Value，不一定是字符串，
-- 用来提醒：lua_pcall 之后 Stack 顶部的错误对象不能想当然地当成字符串处理
function boom_with_table()
    error({ code = 42, message = "结构化错误" })
end
