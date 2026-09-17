-- register_demo.lua
-- 演示：把一个 Lua Function 交给 C 长期持有（register_handler），
-- 然后在同一次脚本运行期内，再让 C 把它取回来调用一次
-- （fire_handler）。第三次调用发生在这个脚本执行完之后，
-- 由 main() 直接从 C 侧触发，见 register_demo.c 的注释。

register_handler(function(event)
    print("[lua handler] got event:", event)
end)

print("[lua] 脚本内第一次调用 fire_handler")
fire_handler("hello from lua, same script run")
