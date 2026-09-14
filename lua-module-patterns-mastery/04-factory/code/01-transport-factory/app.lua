-- app.lua
--
-- Application：全程只 require("factory")。
-- 这个文件里搜索不到任何一个具体 transport 实现模块的路径字符串——
-- 这正是 Factory 存在的意义: Application 只认识"创建一个 kind"这件事,
-- 不认识具体是哪个模块。

local factory = require("factory")

print("== 通过 Factory 创建 tcp 客户端 ==")
local tcp_client, err = factory.create("tcp", { host = "10.0.0.5", port = 9000 })
if not tcp_client then
    print("  创建失败: " .. err)
else
    tcp_client:send("hello-over-tcp")
    tcp_client:send("second-message")
    tcp_client:close()
end

print("\n== 通过 Factory 创建 mock 客户端（测试场景）==")
local mock_client, mock_err = factory.create("mock", {})
if not mock_client then
    print("  创建失败: " .. mock_err)
else
    mock_client:send("test-payload-1")
    mock_client:send("test-payload-2")
    mock_client:close()
end

print("\n== 请求一个未知的 transport kind ==")
local bad_client, bad_err = factory.create("websocket", {})
if not bad_client then
    print("  预期的失败: " .. bad_err)
else
    print("  意外：websocket 竟然创建成功了")
end
