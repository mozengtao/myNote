-- device.lua
--
-- Class / Constructor Module：Device 是类型（Class Table），只装
-- start/stop/status 三个方法，一份，随 require 只创建一次。每次
-- Device.new(id) 产生一个独立 Object，拥有自己的 id / running 状态。

local Device = {}
Device.__index = Device

function Device.new(id)
    local self = setmetatable({}, Device)
    self.id = id
    self.running = false   -- Object State：只属于这一个 Device 实例
    return self
end

function Device:start()
    if self.running then
        return false, "already running"
    end
    self.running = true
    return true
end

function Device:stop()
    if not self.running then
        return false, "already stopped"
    end
    self.running = false
    return true
end

--- device:status() -> string
-- 只读当前实例自己的 running 字段，不会读到其他 Device 的状态。
function Device:status()
    return string.format("Device[%s] = %s", self.id, self.running and "running" or "stopped")
end

return Device
