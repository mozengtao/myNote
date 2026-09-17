-- types_demo.lua
-- describe_value(v) 是 C Function：C 侧用 lua_type 判断 v 的类型，
-- 再用对应的 lua_to* 把值读出来打印（Lua -> C 方向）。
-- make_sample() 也是 C Function：C 侧用 lua_push* 把多种类型的值
-- Push 到 Stack 上，作为返回值交给 Lua（C -> Lua 方向）。

print("== 分别用 describe_value 描述几种基本类型 ==")
describe_value(nil)
describe_value(true)
describe_value(false)
describe_value(42)
describe_value(3.14)
describe_value("hello")
describe_value("")

print()
print("== make_sample() 一次性从 C 拿回多种类型 ==")
local n, b, i, f, s = make_sample()
print("n(nil)      =", n)
print("b(boolean)  =", b)
print("i(integer)  =", i)
print("f(number)   =", f)
print("s(string)   =", s)

print()
print("== 把 make_sample() 的返回值再传回 describe_value，走一个完整闭环 ==")
describe_value(n)
describe_value(b)
describe_value(i)
describe_value(f)
describe_value(s)

print()
print("== Failure 演示 1/2：lua_to* 静默转换失败（demo_pitfalls） ==")
demo_pitfalls()

print()
print("== Failure 演示 3：luaL_checknumber 遇到无法转换的参数会直接报错 ==")
local ok, err = pcall(checknumber_strict, "abc")
print("pcall(checknumber_strict, \"abc\") ->", ok, err)

local ok2, result2 = pcall(checknumber_strict, "3.5")
print("pcall(checknumber_strict, \"3.5\") ->", ok2, result2)
