-- math_ext_demo.lua
--
-- 调用 math_ext.c 里注册好的整张 math_ext Table，
-- 覆盖整数参数（is_prime / gcd）和浮点参数（clamp / lerp / round）
-- 两类场景，最后故意传错参数类型触发 luaL_check* 报错，
-- 用 pcall 捕获，证明"C 侧校验失败 = 一次 Lua Error，
-- 不会让整个宿主程序崩溃"。

print("== math_ext.clamp(v, lo, hi) ==")
print("clamp(5, 0, 10)   =", math_ext.clamp(5, 0, 10))
print("clamp(-3, 0, 10)  =", math_ext.clamp(-3, 0, 10))
print("clamp(99, 0, 10)  =", math_ext.clamp(99, 0, 10))

print()
print("== math_ext.lerp(a, b, t) ==")
print("lerp(0, 100, 0.5) =", math_ext.lerp(0, 100, 0.5))
print("lerp(10, 20, 0)   =", math_ext.lerp(10, 20, 0))
print("lerp(10, 20, 1)   =", math_ext.lerp(10, 20, 1))

print()
print("== math_ext.is_prime(n) ==")
for _, n in ipairs({1, 2, 17, 18, 97}) do
    print("is_prime(" .. n .. ") =", math_ext.is_prime(n))
end

print()
print("== math_ext.gcd(a, b) ==")
print("gcd(48, 18)  =", math_ext.gcd(48, 18))
print("gcd(17, 5)   =", math_ext.gcd(17, 5))

print()
print("== math_ext.round(v, ndigits) ==")
print("round(3.14159, 2) =", math_ext.round(3.14159, 2))
print("round(3.14159)    =", math_ext.round(3.14159))  -- ndigits 省略，luaL_optinteger 缺省为 0

print()
print("== Failure：故意传错参数类型，用 pcall 捕获 luaL_check* 报错 ==")
-- clamp 的第一个参数按约定应该是 number，这里故意传一个字符串。
-- luaL_checknumber 内部检测到类型不对，会直接 luaL_error，
-- 相当于在 C 里 "抛出" 一个 Lua Error；如果这里不用 pcall 包一层，
-- 这个 Error 会一路传播到 main() 的 luaL_dofile，导致整个宿主程序
-- 以非 0 状态退出。
local ok, err = pcall(math_ext.clamp, "not-a-number", 0, 10)
print("pcall 结果 ok =", ok)
print("pcall 捕获的错误信息 =", err)
