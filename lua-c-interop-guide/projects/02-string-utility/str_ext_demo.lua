-- str_ext_demo.lua
--
-- 调用 str_ext.c 里注册好的整张 str_ext Table，
-- 重点验证 lua_tolstring 对"带内嵌 \0 的字符串"依然正确处理
-- （不会被当成 C 风格字符串截断），以及 split 返回的 Table
-- 内容是否正确。

print("== str_ext.reverse(s) ==")
print("reverse('hello')      =", str_ext.reverse("hello"))
print("reverse('Lua/C API')  =", str_ext.reverse("Lua/C API"))

print()
print("== str_ext.trim(s) ==")
print("trim('  hi there  ')  = '" .. str_ext.trim("  hi there  ") .. "'")
print("trim('\\t\\nboth\\r\\n') = '" .. str_ext.trim("\t\nboth\r\n") .. "'")

print()
print("== str_ext.to_upper(s) ==")
print("to_upper('Lua meets C') =", str_ext.to_upper("Lua meets C"))

print()
print("== str_ext.split(s, sep) 返回 Table ==")
local parts = str_ext.split("a,bb,ccc,dddd", ",")
print("split('a,bb,ccc,dddd', ',') 共", #parts, "段：")
for i, p in ipairs(parts) do
    print(string.format("  parts[%d] = %q", i, p))
end
assert(#parts == 4)
assert(parts[1] == "a")
assert(parts[2] == "bb")
assert(parts[3] == "ccc")
assert(parts[4] == "dddd")
print("split 结果校验通过")

print()
print("== 关键场景：字符串内嵌 '\\0'，验证 lua_tolstring 不会截断 ==")
-- Lua string 本身就是长度显式的字节数组，可以合法内嵌 '\0'。
-- string.char(104,105,0,108,117,97) 构造出 "hi\0lua"，总长度 6。
local with_nul = string.char(104, 105, 0, 108, 117, 97)
print("原始长度 #with_nul =", #with_nul)

local reversed = str_ext.reverse(with_nul)
print("reverse 后长度      =", #reversed)
-- 如果 C 侧误用了 lua_tostring/strlen 之类只认 '\0' 结尾的写法，
-- 这里长度会变成 2（"hi"），字节内容也会不完整；
-- 用 lua_tolstring 正确处理后，长度应该依然是 6，
-- 且内容是原字符串的完整字节反转： "aul\0ih"
assert(#reversed == 6)
assert(reversed == string.char(97, 117, 108, 0, 105, 104))
print("reverse 对内嵌 \\0 的处理校验通过（长度与字节内容都完整保留）")

print()
print("== Failure：如果误用 luaL_checkstring/lua_tostring 却忽略长度 ==")
print("（本项目所有函数内部都用 lua_tolstring 显式取长度，因此不会触发这个坑；")
print(" 完整的失败示范代码见 README.md 的 Failure 小节。）")
