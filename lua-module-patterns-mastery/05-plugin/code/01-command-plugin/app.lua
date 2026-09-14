-- app.lua
--
-- Application：先 require framework（拿到 Framework 这个 Plugin
-- Manager），再逐个 require 三个插件模块（Load 每一个插件），然后
-- 通过 framework.register_command(...) 把它们注册进去（Register）。
-- 之后只通过 framework.execute_command(name, ...) 触发调用（Invoke /
-- Event），app.lua 本身从不直接调用 hello_plugin.run(...) 这种具体
-- 插件函数——那样会绕开 Framework，破坏 Dependency Inversion（那时
-- Application 就变成直接依赖某个具体插件，而不是依赖 Framework 提供
-- 的统一契约）。

local framework = require("framework")

local hello_plugin = require("plugins.hello_plugin")
local status_plugin = require("plugins.status_plugin")
local debug_plugin = require("plugins.debug_plugin")

print("== Register 三个插件 ==")
framework.register_command(hello_plugin.name, hello_plugin)
framework.register_command(status_plugin.name, status_plugin)
framework.register_command(debug_plugin.name, debug_plugin)
print("已注册命令: " .. table.concat(framework.list_commands(), ", "))

print("\n== Invoke：依次执行三个命令 ==")
framework.execute_command("hello", "Lua")
framework.execute_command("status")
framework.execute_command("status")
framework.execute_command("debug", 1, "two", true)

print("\n== Invoke 一个不存在的命令：Framework 必须优雅地返回错误，而不是抛异常/崩溃 ==")
local result, err = framework.execute_command("restart")
print(string.format("execute_command('restart') -> result=%s, err=%s", tostring(result), tostring(err)))

print("\n== Unload：卸载 debug 插件，之后再调用它必须优雅地失败 ==")
local unreg_ok = framework.unregister_command("debug")
print(string.format("unregister_command('debug') -> ok=%s", tostring(unreg_ok)))
print("卸载后已注册命令: " .. table.concat(framework.list_commands(), ", "))

local result2, err2 = framework.execute_command("debug", "still there?")
print(string.format("execute_command('debug') after unload -> result=%s, err=%s", tostring(result2), tostring(err2)))
