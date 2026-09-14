-- app.lua
--
-- Application：为两个不同的用户各创建一个 Session 实例；一个用正确
-- 密码登录，另一个用错误密码登录。打印两者的 is_authenticated()，
-- 证明认证结果只取决于各自实例上的 authenticated 字段，互不干扰。

local Session = require("session")

print("== 创建两个独立的 Session 实例 ==")
local session_alice = Session.new("alice")
local session_bob = Session.new("bob")

print(session_alice:describe())
print(session_bob:describe())

print("\n== alice 用正确密码登录，bob 用错误密码登录 ==")
local ok_a, err_a = session_alice:login("correct-password")
local ok_b, err_b = session_bob:login("wrong-password")

print(string.format("session_alice:login() -> ok=%s", tostring(ok_a)))
print(string.format("session_bob:login()   -> ok=%s, err=%s", tostring(ok_b), tostring(err_b)))

print("\n== 分别检查 is_authenticated()，两者结果必须不同 ==")
print(string.format("session_alice:is_authenticated() -> %s", tostring(session_alice:is_authenticated())))
print(string.format("session_bob:is_authenticated()   -> %s", tostring(session_bob:is_authenticated())))

print(session_alice:describe())
print(session_bob:describe())

print("\n== alice 登出后，bob 的认证状态不应受到任何影响 ==")
session_alice:logout()
print(string.format("session_alice:is_authenticated() -> %s", tostring(session_alice:is_authenticated())))
print(string.format("session_bob:is_authenticated()   -> %s (bob 从未被 alice 的操作影响)", tostring(session_bob:is_authenticated())))

print("\n== 两个实例共享同一份方法（同一个 Class Table） ==")
print(string.format(
    "session_alice.login == session_bob.login -> %s",
    tostring(session_alice.login == session_bob.login)
))
