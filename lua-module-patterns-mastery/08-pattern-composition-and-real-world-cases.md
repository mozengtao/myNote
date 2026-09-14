# 08 · Pattern 组合 Mental Model + 3 个真实工程案例

> 前置阅读：五个 Pattern 的 `00-why-and-architecture.md`、[06](06-require-and-state-mental-model.md)、[07](07-dependency-direction.md)。本文要纠正一个常见误解——"一个项目应该统一用一种 Pattern"——真实项目里，5 个 Pattern 几乎总是同时出现，只是分别用在不同的文件上。

## 1. 组合 Mental Model：Pattern 是分层的，不是互斥的

```text
Plugin
  |
  +-- 内部用 Factory 决定这次要创建哪一种"连接/客户端"对象
        |
        +-- Factory 创建出来的是一个 Class / Constructor 对象
              |
              +-- 这个对象的方法内部，可能调用一个 Private + Public 的辅助模块
                    （比如一个只暴露 encode()/decode() 的协议编码模块）
```

把这四层落到一次真实调用上：

```text
Framework.dispatch(packet)
    |
    v
Plugin.process(packet)                        <-- Pattern 5：Plugin 的契约方法
    |
    v
local conn = factory.create(packet.transport)  <-- Pattern 4：Factory 隐藏了到底 new 的是 TcpConnection 还是 MockConnection
    |
    v
conn:send(protocol.encode(packet.payload))     <-- Pattern 3：conn 是一个独立 Object，有自己的 buffer/state
    |                                               Pattern 2：protocol.encode() 是一个 Private+Public 模块的公开入口，
    |                                                            内部的 encode_header()/encode_payload() 对 Plugin 不可见
    v
返回结果
```

**关键结论**：Pattern 之间不是"升级替代"关系（这在 [00-mental-model-overview.md](00-mental-model-overview.md) 第 2 节已经强调过），而是**架构里不同粒度的职责，各自选最合适的形状**——`Plugin.process()` 这一层职责是"响应 Framework 的一次事件"，适合 Plugin 形状；它内部要"创建一个连接对象"，适合 Factory 形状；连接对象要"维护自己的 buffer"，适合 Class 形状；协议编码要"隐藏 wire format 细节"，适合 Private+Public 形状。同一个调用链上，四种形状各自负责自己最擅长的那一层。

## 2. Case 1：HTTP Client Library

### 目录结构

```text
http/
+-- constants.lua        Pattern 1: Table Module
+-- parser.lua           Pattern 2: Private + Public
+-- client.lua            Pattern 3: Class / Constructor
+-- factory.lua           Pattern 4: Factory
+-- transport/
    +-- tcp.lua           Pattern 5: Plugin
    +-- mock.lua          Pattern 5: Plugin
```

### 每个文件为什么用这个 Pattern

- **`constants.lua`（Table Module）**：装 HTTP 方法名（`GET`/`POST`）、状态码、默认 header 名字。这些是纯粹的只读常量 + 无状态工具函数，没有任何理由引入 Object 或 Contract，Table Module 是唯一合理的形状（对应 [01-table-module/00-why-and-architecture.md](01-table-module/00-why-and-architecture.md)）。
- **`parser.lua`（Private + Public）**：对外只暴露 `parse_response(raw)`；内部的 `parse_status_line()`、`parse_headers()`、`parse_body()` 全部是 `local function`。HTTP 响应格式的解析细节会随 HTTP/1.1 到 HTTP/2 演进，但只要 `parse_response()` 的输入输出契约不变，内部怎么解析可以随时重写（对应 [02-private-public/00-why-and-architecture.md](02-private-public/00-why-and-architecture.md)）。
- **`client.lua`（Class / Constructor）**：`HttpClient.new(base_url)` 创建的每一个 client 有自己的 `base_url`、`default_headers`、`keep_alive` 连接池等状态；一个进程里可能同时对接多个后端服务，必须是可以创建多个、互不干扰的 Object（对应 [03-class-constructor/00-why-and-architecture.md](03-class-constructor/00-why-and-architecture.md)）。
- **`factory.lua`（Factory）**：`factory.create(transport_kind, config)` 决定这个 `HttpClient` 底层到底用哪种 transport——生产环境用 `tcp`，单元测试用 `mock`，调用方（业务代码）完全不需要知道、也不应该知道这个选择是怎么发生的（对应 [04-factory/00-why-and-architecture.md](04-factory/00-why-and-architecture.md)）。
- **`transport/tcp.lua` / `transport/mock.lua`（Plugin）**：两者都实现同一个 transport 契约（`:send(bytes)`/`:close()`），`factory.lua` 通过契约调用它们，不关心具体是哪一个；新增一种 transport（比如 `transport/unix_socket.lua`）不需要修改 `client.lua`/`factory.lua` 的选择逻辑之外的任何东西（严格来说这里 Factory 内部仍有一份"名字 -> 模块"的映射表，属于第 07 篇里"方案 B"的收敛点；如果要做到真正的方案 C（Framework 完全不认识具体名字），需要把这份映射表也换成运行时 Discovery/Register，这正是 Plugin Pattern 相对 Factory 多做的那一步）。

### 调用流程

```text
Application
    |
    | require("http.factory")
    v
factory.create("tcp", config)
    |
    v
client.lua: HttpClient.new(base_url, transport)     <-- Class 对象诞生，持有这个 transport 实例
    |
    | client:get("/users")
    v
transport/tcp.lua: conn:send(request_bytes)          <-- Plugin 契约方法
    |
    v
parser.lua: parse_response(raw_bytes)                 <-- Private+Public，只暴露 parse_response
    |
    v
constants.lua: STATUS_CODES[200]                       <-- Table Module，纯查表
    |
    v
返回给 Application 一个结构化的 response 对象
```

## 3. Case 2：Protocol Framework

### 目录结构

```text
protocol/
+-- constants.lua        Pattern 1: Table Module      (协议号、字段长度常量)
+-- packet.lua           Pattern 3: Class / Constructor  (每个 Packet 是独立对象，有自己的 header/payload)
+-- parser.lua           Pattern 2: Private + Public   (对外只暴露 encode()/decode())
+-- factory.lua          Pattern 4: Factory            (根据协议号创建对应的 Packet 子类/配置)
+-- plugins/
    +-- http.lua         Pattern 5: Plugin
    +-- dns.lua          Pattern 5: Plugin
    +-- mqtt.lua         Pattern 5: Plugin
```

### 架构图

```text
                     Application
                          |
                          | dispatch(raw_bytes)
                          v
                  protocol/dispatcher (Framework)
                          |
                          | 只依赖 Plugin Interface（can_handle / process）
                          v
                    Plugin Interface
                          ^
                          | 实现
              +-----------+-----------+
              |           |           |
        plugins/http  plugins/dns  plugins/mqtt
              |           |           |
              v           v           v
        使用 packet.lua 构造/解析各自的 Packet 对象（Pattern 3）
              |           |           |
              v           v           v
        使用 parser.lua 的 encode()/decode()（Pattern 2）
              |           |           |
              v           v           v
        使用 constants.lua 的协议号/字段常量（Pattern 1）
```

### 调用流程

```text
Application
    |
    | dispatcher.dispatch(raw_bytes)
    v
dispatcher（Framework）依次询问已注册的 plugin: plugin.can_handle(raw_bytes)
    |
    v
plugins/http.lua.can_handle(raw_bytes) -> true
    |
    v
plugins/http.lua.process(raw_bytes)
    |
    +-- factory.create("http") -> 返回一个配置好的 packet 构造参数（Pattern 4）
    |
    +-- packet.lua: Packet.new(header, payload)（Pattern 3，独立对象，持有自己的字段）
    |
    +-- parser.lua: parser.decode(raw_bytes)（Pattern 2，内部字段偏移量对 plugin 不可见）
    |
    v
返回结构化的 Packet 对象给 Application
```

这个案例里 Factory 和 Plugin 是**平级协作**的关系：Plugin 负责"我能不能处理这种协议、处理完之后要做什么"，Factory 负责"这次处理具体需要哪种配置/子类型"——两者服务的是同一次 `dispatch` 调用里的不同决策点。

## 4. Case 3：Wireshark-like Framework

### 架构图

```text
framework
    |
    v
dispatcher                                  <-- Framework，只认识 Plugin Interface
    |
    v
plugin registry                              <-- Framework State：当前注册了哪些 dissector
    |
 +--+------------+------------+
 v               v             v
HTTP dissector  DNS dissector  DOCSIS dissector    <-- 每个都是 Pattern 5 Plugin
    |               |             |
    v               v             v
（各自内部可能再用 Class 表示解析出的字段结构，
  再用 Private+Public 隐藏具体的位域/字节序解析细节）
```

### 与真实 Lua Plugin Architecture 的联系

这正是本系列 [05-plugin/03-example-dissector-plugin.md](05-plugin/03-example-dissector-plugin.md) 示例模拟的场景，也正是真实 Wireshark Lua Dissector API（`Proto`、`DissectorTable`）在做的事情——Framework（Wireshark 核心）完全不知道"HTTP"、"DNS"、"DOCSIS"这些名字，它只知道"注册进来的东西必须能回答『这个包你要不要』、『你要的话怎么解析』"。三个 dissector 谁先谁后加载、加载几个，对 Framework 源码零影响，这就是 [07-dependency-direction.md](07-dependency-direction.md) 里"方案 C"在真实工业级 Lua 项目里的样子——这也是为什么本系列在选择 15 个示例场景时，特意把"Wireshark-like Dissector"作为 Plugin Pattern 的第三个示例：它比 Command Plugin / Protocol Handler Plugin 更贴近"真实存在、大规模使用"的 Lua Plugin 系统。

## 5. 三个案例的共同规律

```text
Table Module        总是最底层，被所有其它 Pattern 引用，自己不引用任何人
Private + Public     通常在"实现细节需要独立于契约演进"的地方出现（parser/encoder）
Class / Constructor  通常在"需要表示一个有身份、有状态的实体"的地方出现（连接、Packet、Session）
Factory              通常在"创建某个 Class 实例，但具体创建哪一种要运行时才知道"的地方出现
Plugin                通常在最外层，是 Framework 与外部扩展点之间的边界
```

依赖方向永远是**从外层指向内层**（Plugin 可以用 Factory，Factory 可以用 Class，Class 可以用 Private+Public，所有人都可以用 Table Module），反过来则几乎不会发生——`constants.lua` 不应该、也没有理由反向 `require` 一个 Plugin 模块。这条"依赖只能从外层指向内层"的规律，也是判断一个真实项目里 Module 分层是否健康的直接标准。

下一篇 [09-cross-pattern-comparison.md](09-cross-pattern-comparison.md) 给出五个 Pattern 的最终横向对比表，并汇总五个 Anti-pattern。
