# 第六部分 · 案例3：生产级真实插件 — `RLS-wireshark-dissector`

源码保存在 [code/rls/](code/rls/)，完整对应开源仓库 [nextmn/RLS-wireshark-dissector](https://github.com/nextmn/RLS-wireshark-dissector)（License: CC0-1.0，作者 Louis Royer 与 NextMN 社区）：

```
code/rls/rls_dispatcher.lua   <- 仓库根目录 rls.lua        （入口/分发器）
code/rls/rls_v3_1.lua         <- protocol/rls-3-1.lua      （RLS 3.1.x 版本实现）
code/rls/rls_v3_2.lua         <- protocol/rls-3-2.lua      （RLS 3.2.x 版本实现）
```

这是三个案例里唯一"不是为了教学而写、而是为了真实生产使用而写"的插件：它给 [UERANSIM](https://github.com/aligungr/UERANSIM)（一个开源 5G 核心网/UE 模拟器）产生的 Radio Link Simulation (RLS) 协议流量做解析，目前仍在维护、已被打包进 Debian 软件源。

## 6.1 Plugin 背景

- **它解决什么问题**：UERANSIM 模拟 5G 基站(gNB)和终端(UE)之间的"空口链路"，实际走的是普通 UDP，报文格式是 UERANSIM 自定义的 RLS 协议，且这个协议**随 UERANSIM 版本演进会改变格式**（3.1.x 和 3.2.x 的报文结构不同）。没有这个插件，用户在 Wireshark 里只能看到一堆无法解读的 UDP payload。
- **为什么这样设计**：因为协议本身"按版本分裂"，插件设计者没有把所有版本塞进一个巨大的 if/elseif，而是**为每个版本单独建一个 Proto，用一张自定义 DissectorTable 做版本路由**——这是一个典型的"协议演进"场景下的架构决策，也是本笔记选它作案例3的核心原因：它展示了"当协议本身会变化时，插件架构该怎么变化"。

## 6.2 文件结构

```
rls.lua                (入口/分发器，仓库根目录)
protocol/
  rls-3-1.lua          (3.1.x 版本实现，自行注册进 "rls" 表)
  rls-3-2.lua          (3.2.x 版本实现，自行注册进 "rls" 表)
pcap/                  (测试用抓包样本，不影响运行时)
```

三个 `.lua` 文件是**互相独立、无 `require` 关系**的普通插件文件——它们靠"共享同一张 DissectorTable"来协作，而不是靠模块引用。这正是 [03-lifecycle.md](03-lifecycle.md) 3.2节讲的"Lua 插件多文件协作模式"的真实体现：安装时把整个仓库 clone 进 `~/.local/lib/wireshark/plugins/`，Wireshark 加载器会按 ASCIIbetical 顺序发现并加载所有 `.lua` 文件（`protocol/` 子目录也会被扫描到）。

## 6.3 初始化流程

### 入口文件 `rls.lua`（[code/rls/rls_dispatcher.lua](code/rls/rls_dispatcher.lua)）顶层代码：

```
第22行  local rlsProtocol = Proto("RLS", "UERANSIM Radio Link Simulation (RLS) Protocol")
第23-28行  set_plugin_info({version=..., author=..., repository=..., description=...})
第31行  DissectorTable.new("rls", "RLS version", ftypes.UINT32, base.HEX, rlsProtocol)
           -- 关键一步：自建一张路由表，key 是版本号(UINT32)，此表由 rlsProtocol"拥有"
第34行  rlsProtocol.prefs.udp_port = Pref.uint("RLS UDP port", 4997, "UDP port for RLS")
第38行  fields.Version = ProtoField.string("rls.version", "Version")
第40行  function rlsProtocol.dissector(buffer, pinfo, tree) ... end   -- 声明分发逻辑
第69行  function rlsProtocol.init() ... end                            -- 声明端口注册逻辑
```

### 子协议文件 `protocol/rls-3-1.lua` / `rls-3-2.lua`（各自独立）：

```
各自 local rlsProtocol31/32 = Proto("RLS-3.1"/"RLS-3.2", ...)   -- 独立的身份，不是共享同一个 Proto
各自声明自己版本专属的 ProtoField（字段集合不完全相同，比如 3.1 有 Mcc/Mnc/Nci，3.2 没有）
各自 function rlsProtocolXX.dissector(buffer, pinfo, tree) ... end
各自 function rlsProtocolXX.init()
       local rls = DissectorTable.get("rls")     -- 找到 rls.lua 建好的那张表
       rls:add(0x0301 / 0x0302, rlsProtocolXX)    -- 把"自己"注册进去，key=版本号
       DissectorTable.get("udp.port"):add_for_decode_as(rlsProtocolXX)
     end
```

这里体现出前面 [03-lifecycle.md](03-lifecycle.md) 强调的关键时机区别：`rls.lua` 的 `DissectorTable.new("rls",...)` 是**顶层代码**，在文件加载阶段就执行；而 `rls-3-1.lua`/`rls-3-2.lua` 的 `DissectorTable.get("rls"):add(...)` 写在各自的 **`init()` 回调**里，只在"capture 文件打开"时才真正执行——这意味着不管三个文件被加载的先后顺序如何，只要 `rls.lua` 在任何一次 `init()` 触发之前完成加载（而它必然会，因为所有文件的顶层代码都在任何 `init()` 之前跑完），这套注册就一定不会出错。**这是"顶层代码建表、init() 里填表"这一多文件协作范式的教科书级正确用法。**

## 6.4 Runtime：Packet 到来以后发生什么

```
UDP 端口 4997（或用户 Decode As 指定的端口）命中
        |
        v
rlsProtocol.dissector(buffer, pinfo, tree)     <- rls.lua 的分发逻辑
    |
    +-- buffer(0,1):uint() ~= 0x03 ?  是就 return（第一个字节固定是 0x03，快速判断"是不是我"）
    +-- version = buffer(1,2):uint()          读版本号(2字节)
    +-- subprotocol = DissectorTable.get("rls"):get_dissector(version)
    +-- 查不到？ 按 version 大于/小于已知范围，回退到 latestVersion/oldestVersion 再查一次
    +-- 还查不到？ 展示"Unsupported version"，直接 return 4（只消费4字节，放弃继续解析）
    +-- 查到了 -> subprotocol:call(buffer():tvb(), pinfo, tree)   转发全部字节给具体版本实现
        |
        v
rlsProtocol32.dissector(buffer, pinfo, tree)   <- 以 3.2.x 为例
    |
    +-- 二次校验版本号（防御性重复检查，因为这个函数也可能被直接调用）
    +-- 建自己的子树，解析 Version/MsgType/Sti 等公共头部字段
    +-- msgType = buffer(4,1):uint()  按消息类型分派（Table-Driven + Dispatcher 模式叠加）：
          msgType==4 Heartbeat        -> 解析位置坐标
          msgType==5 Heartbeat ACK    -> 解析信号强度
          msgType==6 PDU Transmission -> 按 pduType 再分派一层：
                pduType==1 RRC  -> pcall(Dissector.get(nrRrcDissectors[rrcMsgType]))
                                    成功则 dissector:call(...) 转发给 nr-rrc.* 的 Subdissector
                                    失败（老版本Wireshark没这个dissector）则降级展示文字提示
                pduType==2 Data -> Dissector.get("ip"):call(...) 转发给内建 ip dissector
          msgType==7 PDU Transmission ACK -> 遍历一个 ack 列表，动态个数循环 add 字段
```

这是一条**三层的 Subdissector Chain**：`RLS`（版本分发） -> `RLS-3.2`（消息类型分发） -> `nr-rrc.* / ip`（真正的载荷协议）。每一层只关心"自己这一层该看的一个字段"，然后把"剩下的字节"原样转交给下一层——完全体现第四部分 4.24 讲的 Subdissector 设计初衷。

## 6.5 使用了哪些基础设施

```
Proto                有，且是"多个 Proto 协作"（RLS / RLS-3.1 / RLS-3.2 三个身份）
ProtoField           有，且按版本各自维护自己的字段集合（3.1/3.2 字段不完全相同）
DissectorTable       核心机制：自建 "rls" 表做版本路由 + 复用 "udp.port" 表接入 UDP
Decode As            有：add_for_decode_as(...)，允许用户手动指定端口
Preferences          有：Pref.uint 配置 UDP 端口
Subdissector         有：三层链条，还跨到了内建的 ip dissector 和 nr-rrc.* 系列
init()               核心机制：所有"把自己注册进共享表"的动作都发生在这里
set_plugin_info      有：声明版本号/作者/仓库地址等元信息（帮助用户在 Wireshark 里识别插件版本）
Expert Info          无
Conversation         无
Heuristic Dissector  无（版本号是显式读出来的，不需要"猜"）
Tap/Listener         无
PostDissector        无
GUI                  无
```

## 6.6 哪些地方值得学习

1. **协议按版本演进时，用"多个 Proto + 自定义 DissectorTable"而不是"一个巨型 if 版本号"**——这让新增一个版本（比如未来的 RLS 3.3.x）只需要新增一个文件，完全不用改动 `rls.lua` 和已有版本的文件，是[11-design-patterns.md](11-design-patterns.md) 里 Registry Pattern 的真实生产案例。
2. **版本回退逻辑写得很"宽容"**：查不到精确版本号时，不是直接报错，而是按"高于最新就退到最新、低于最旧就退到最旧"来兜底解析——这是真实协议演进场景下常见的"向前/向后兼容"处理策略，教学示例通常不会展示这种细节。
3. **`pcall` 包住可能不存在的 Subdissector 查找**：`nr-rrc.*` 系列 dissector 只在较新版本 Wireshark 里存在，用 `pcall(get_dissector)` 探测式调用，探测失败就降级展示文字而不是让整个解析崩溃——这是"面向不同用户环境做防御性编程"的好例子。
4. **`init()` 里同时做两件事**（`rls:add(...)` 和 `udp_port:add_for_decode_as(...)`）——把"参与版本路由"和"参与 Decode As 候选列表"这两个不同的注册目标放在同一个生命周期钩子里，简洁但不含糊。
5. **`set_plugin_info(...)`**——很多教学示例不会写这个，但生产插件几乎都会写，方便用户在 `Help > About Wireshark > Plugins` 里核实版本、找到仓库地址、排查兼容性问题。

## 6.7 一句话总结

这是三个案例里**基础设施用得"最少"、但架构决策"最讲究"**的一个——它没有用 Expert Info、Conversation、Heuristic，却靠"自定义 DissectorTable 做版本路由 + 每个版本独立 init() 自注册"这一个决策，解决了"协议本身会随外部项目演进而改变格式"这个真实工程问题，是理解 Registry/Dispatcher 模式和多文件协作机制的最佳范本。
