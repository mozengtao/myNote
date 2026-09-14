-- app.lua
--
-- Application：全程只 require("factory")。
-- 搜索这个文件找不到任何一个具体数据库驱动模块的路径字符串——
-- Application 不知道、也不关心底层到底是哪一种数据库,
-- 只知道"我要一个 db client"。

local factory = require("factory")

print("== 通过 Factory 创建 sqlite client ==")
local sqlite_db, err1 = factory.create("sqlite", { path = "app.db" })
if sqlite_db then
    sqlite_db:insert({ name = "alice" })
    sqlite_db:insert({ name = "bob" })
    sqlite_db:query()
else
    print("  创建失败: " .. err1)
end

print("\n== 通过 Factory 创建 postgres client ==")
local pg_db, err2 = factory.create("postgres", { dsn = "postgres://db.internal/app" })
if pg_db then
    pg_db:insert({ name = "carol" })
    pg_db:query()
else
    print("  创建失败: " .. err2)
end

print("\n== 通过 Factory 创建 memory client（测试场景）==")
local mem_db, err3 = factory.create("memory", {})
if mem_db then
    mem_db:insert({ name = "test-user-1" })
    mem_db:insert({ name = "test-user-2" })
    mem_db:insert({ name = "test-user-3" })
    mem_db:query()
else
    print("  创建失败: " .. err3)
end

print("\n== 请求一个未知的 db 驱动 ==")
local bad_db, bad_err = factory.create("mysql", {})
if not bad_db then
    print("  预期的失败: " .. bad_err)
else
    print("  意外：mysql 竟然创建成功了")
end
