# 02 · Example 2：Device

> 前置阅读：[00-why-and-architecture.md](00-why-and-architecture.md)。本例用最小的字段集合（只有一个 `running` 布尔值）突出"多个实例状态发散"这一件事，去掉了 Example 1 里 `buffer` 之类的额外细节干扰。

## 1. Problem / Scenario

一个系统里要同时管理多台设备：有的已经启动、有的还没启动，操作某一台设备（启动/停止）绝不能影响其他设备。需要一种类型（`Device`），可以创建任意多个实例，每个实例自己记录"是否在运行"。

## 2. Architecture

```text
Application (app.lua)
     |
     |  require("device") -- 只发生一次
     v
Class Module (Device)
     |
     |  Device.new(id) -- 调用三次，分别产生 dev1、dev2、dev3
     v
Object (dev1)     Object (dev2)     Object (dev3)
  running=?          running=?         running=?
  (各自独立)         (各自独立)          (各自独立)
```

```text
             Device
                |
           __index
                |
      +---------+---------+
      v         v         v
    dev1      dev2      dev3
      |         |         |
   running   running   running
   (独立)     (独立)     (独立)
```

## 3. Framework / Caller

`app.lua` 创建 3 个 `Device` 实例，只启动其中两个（`dev1`、`dev3`），第三个（`dev2`）保持不动；随后再停止 `dev1`。全程只调用实例方法（`:start()`/`:stop()`/`:status()`），不直接触碰 `Device` 这个 Class Table。

## 4. Module

`device.lua` 只装三个方法：`start`（把 `running` 置为 `true`，如果已经在运行则失败）、`stop`（反向操作）、`status`（只读，拼出一行描述字符串）。构造函数 `Device.new(id)` 是唯一写 `self.running` 初始值的地方。

## 5. Complete Lua Code

`code/02-device/device.lua`：

```lua
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
```

`code/02-device/app.lua`：

```lua
-- app.lua
--
-- Application：创建 3 个 Device 实例，只启动其中两个、停止其中一个，
-- 逐一打印 status()，证明每个实例的 running 状态完全由自己的
-- Instance Table 决定，不受其他实例、也不受 Device 这个 Class Table
-- 本身状态的影响（因为 Class Table 从来没有存过 running 字段）。

local Device = require("device")

print("== 创建 3 个独立的 Device 实例 ==")
local dev1 = Device.new("dev-1")
local dev2 = Device.new("dev-2")
local dev3 = Device.new("dev-3")

print(dev1:status())
print(dev2:status())
print(dev3:status())

print("\n== 启动 dev1 和 dev3，dev2 保持不动 ==")
dev1:start()
dev3:start()

print(dev1:status())
print(dev2:status())
print(dev3:status())

print("\n== 再停止 dev1，dev3 必须仍然是 running ==")
dev1:stop()

print(dev1:status())
print(dev2:status())
print(dev3:status())

print("\n== 重复启动同一个 Device 会失败（体现 state 只属于这一个实例） ==")
local ok, err = dev3:start()
print(string.format("dev3:start() -> ok=%s, err=%s", tostring(ok), tostring(err)))

print("\n== 三个实例共享同一份方法，但各自的 running 字段互不相同 ==")
print(string.format(
    "dev1.start == dev2.start == dev3.start -> %s (方法只有一份,来自同一个 Class Table)",
    tostring(dev1.start == dev2.start and dev2.start == dev3.start)
))
print(string.format(
    "最终各自的 running: dev1=%s, dev2=%s, dev3=%s (三份独立的 Instance State,互不相同)",
    tostring(dev1.running), tostring(dev2.running), tostring(dev3.running)
))
```

## 6. Execution Flow

用 `$HOME/.local/bin/lua5.4 app.lua` 实际跑出的输出（已验证，无报错）：

```text
== 创建 3 个独立的 Device 实例 ==
Device[dev-1] = stopped
Device[dev-2] = stopped
Device[dev-3] = stopped

== 启动 dev1 和 dev3，dev2 保持不动 ==
Device[dev-1] = running
Device[dev-2] = stopped
Device[dev-3] = running

== 再停止 dev1，dev3 必须仍然是 running ==
Device[dev-1] = stopped
Device[dev-2] = stopped
Device[dev-3] = running

== 重复启动同一个 Device 会失败（体现 state 只属于这一个实例） ==
dev3:start() -> ok=false, err=already running

== 三个实例共享同一份方法，但各自的 running 字段互不相同 ==
dev1.start == dev2.start == dev3.start -> true (方法只有一份,来自同一个 Class Table)
最终各自的 running: dev1=false, dev2=false, dev3=true (三份独立的 Instance State,互不相同)
```

逐段对应结论：

- 三个实例刚创建时全部 `stopped`——`Device.new` 里 `self.running = false` 对每个实例各写了一次，不是共享同一个 `false`。
- 只启动 `dev1`、`dev3` 之后，`dev2` 仍然 `stopped`——`dev1:start()` 只修改了 `dev1` 自己的表。
- 停止 `dev1` 之后，`dev3` 依然 `running`——两者的 `running` 字段是两张不同表上的两个不同 key，互不影响。
- 对已经在运行的 `dev3` 再调用 `:start()` 会返回 `false, "already running"`——这个判断读的是 `dev3` 自己的 `running`，与 `dev1`/`dev2` 无关。
- 最后一行验证方法确实只有一份（`dev1.start == dev2.start == dev3.start` 为 `true`），而 `running` 字段的最终值三者互不相同。

## 7. State

| 字段 | 归属 | 说明 |
|---|---|---|
| `Device.new/start/stop/status` | Class Table（`Device`），一份 | 三个实例共用，创建更多 Device 不会增加方法数量 |
| `dev1.id` / `dev1.running` | `dev1` 的 Instance Table，独立一份 | 只属于 `dev1` |
| `dev2.id` / `dev2.running` | `dev2` 的 Instance Table，独立一份 | 只属于 `dev2` |
| `dev3.id` / `dev3.running` | `dev3` 的 Instance Table，独立一份 | 只属于 `dev3` |

`Device` 表上除了方法和 `__index` 之外没有任何字段——如果不小心写成 `Device.running = false`（而不是 `self.running = false`），就会变成本 Pattern 的 Anti-pattern（见 [00-why-and-architecture.md](00-why-and-architecture.md) 的 Anti-pattern 一节），三台设备会共享同一个 `running`，`dev1:start()` 会让 `dev2:status()` 也显示 `running`。

## 8. Why this Pattern fits

"设备是否在运行"是典型的、必须按实例隔离的状态：现实中不会有"启动了任意一台设备，所有设备都变成运行状态"这种逻辑，Class / Constructor Module 用一张 Instance Table 对应一台物理/逻辑设备，恰好匹配这种"一对一"的直觉。方法（`start`/`stop`/`status`）对所有设备都是同一套判断逻辑，天然适合放在 Class Table 上共享一份。

## 9. What would go wrong with another Pattern

- 如果用 **Table Module**：`device.lua` 直接写 `M.running = false`，那么只能表示"系统里唯一一台设备"，`M.start()` 会覆盖所有"设备"共享的同一个开关，无法区分 `dev1`/`dev2`/`dev3`。
- 如果尝试用一个数组模拟多台设备（比如 `M.devices[id].running`），本质上是在 Module 内部手写一份"简化版的 Instance Table 机制"，还要自己维护 `id` 到数据的映射，比直接用 `Device.new(id)` 让每个实例拿到独立表更繁琐，也更容易在多处忘记按 `id` 索引。
- Class / Constructor Module 把"每个对象一份独立状态"这件事交给 Lua 的 `setmetatable`/`__index` 机制直接处理，不需要额外的映射结构。

## 10. Mental Model

**一句话记忆**：`Device` 只定义"启动/停止/查询状态"这套规则，规则本身不认识任何一台具体设备；`Device.new(id)` 每调用一次，就多一台"活的"设备，它的 `running` 只由自己决定。
