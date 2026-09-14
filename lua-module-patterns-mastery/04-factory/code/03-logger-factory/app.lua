-- app.lua
--
-- Application：全程只 require("factory")。
-- 搜索这个文件找不到任何一个具体 logger 实现模块的路径字符串。

local factory = require("factory")

local LOG_PATH = "/tmp/lua-module-patterns-logger-factory-example.log"

print("== console logger ==")
local console_logger = factory.create("console", {})
console_logger:info("service started")
console_logger:error("connection refused")
console_logger:debug("retry count = 3")

print("\n== null logger（应当没有任何输出）==")
local null_logger = factory.create("null", {})
null_logger:info("this should never be printed")
null_logger:error("this should never be printed either")
null_logger:debug("nor this")
print("  (以上两行之间没有任何 [INFO]/[ERROR]/[DEBUG] 输出，证明 null logger 确实什么都没做)")

print("\n== file logger（真实写入磁盘）==")
local file_logger = factory.create("file", { path = LOG_PATH })
file_logger:info("service started")
file_logger:error("connection refused")
file_logger:debug("retry count = 3")
print("  已通过 file logger 写入: " .. LOG_PATH)

print("\n== 读回磁盘文件内容，证明确实写盘了 ==")
local reader = io.open(LOG_PATH, "r")
if not reader then
    print("  读取失败：文件不存在")
else
    print("  --- " .. LOG_PATH .. " 内容开始 ---")
    for line in reader:lines() do
        print("  " .. line)
    end
    print("  --- 内容结束 ---")
    reader:close()
end

print("\n== 请求一个未知的 logger kind ==")
local bad_logger, bad_err = factory.create("syslog", {})
if not bad_logger then
    print("  预期的失败: " .. bad_err)
else
    print("  意外：syslog 竟然创建成功了")
end
