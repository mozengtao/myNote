# 第八部分：常见设计模式

本部分的每个模式都配有一个独立的教学示例文件（不是从三个案例里抠出来的片段，而是为了突出"模式本身"单独写的最小可运行 Lua 代码），保存在 [code/patterns/](code/patterns/)。同时会指出它在三个案例里的真实出现位置。

## 8.1 Dispatcher Pattern（分派模式）

- **什么时候用**：某个字段的值决定"接下来该怎么解析剩下的字节"，且分支数量不小（3个以上），用 if/elseif 链会越写越长。
- **典型代码结构**：用一张 `{ [值] = 处理函数 }` 的表代替 if/elseif，主流程只负责"查表 + 调用"。
- **优点**：新增一个分支只需要往表里加一行，不用改动主流程；处理函数天然是独立可测试的单元。
- **缺点**：如果分支之间有共享的前置/后置逻辑，容易在每个处理函数里重复写，需要额外抽出公共部分。
- **示例1（数值型分派）**：传感器上报帧按 `msgType`（1/2/3）分派到 Temperature/Humidity/Battery 三个处理函数，见 [code/patterns/dispatcher/example1.lua](code/patterns/dispatcher/example1.lua)。
- **示例2（字符串键分派）**：迷你文本命令协议按命令关键字（GET/PUT/DEL）分派，证明分派表的 key 不一定是数字，见 [code/patterns/dispatcher/example2.lua](code/patterns/dispatcher/example2.lua)。
- **真实出现位置**：案例3 `rls_v3_2.lua` 按 `msgType` 分派到 Heartbeat/PDU Transmission/ACK 四个分支（虽然那里用的是 if/elseif 写法，但语义就是 Dispatcher；如果消息类型继续增多，重构成表驱动是自然的下一步）；案例1 `dns.dissector()` 里 `isResponse()` 之后走不同 flag 解析分支，也是简化版的二元 Dispatcher。

## 8.2 Parser Pattern（独立解析函数）

- **什么时候用**：一段字节的解析逻辑复杂到不适合写在主 `dissector()` 函数体里（比如涉及循环、递归、需要返回多个值），应该拆成独立函数。
- **典型代码结构**：`local function parseXxx(tvbr) ... return field1, field2, consumed_len end`，主 dissector 只负责调用并把结果写进 Tree。
- **优点**：可单独测试（甚至脱离 Wireshark 环境用纯 Lua 测试逻辑）；主 dissector 保持"骨架清晰"。
- **缺点**：如果 Parser 函数需要访问很多上下文（pinfo、多个中间变量），参数列表会变长，这时可以考虑改用 8.5 的 Offset Walker 把上下文打包传递。
- **代码**：案例1的 `getQueryName()` 就是完整示例，见 [code/dns_dissector.lua](code/dns_dissector.lua) 第638-678行。
- **示例1（多值返回）**：`parseHeader()` 独立解析帧头，一次返回 version/flags/length/consumed 四个值，主 dissector 拿到值后再决定要不要继续读 payload，见 [code/patterns/parser_function/example1.lua](code/patterns/parser_function/example1.lua)。
- **示例2（可复用的变长字段解析）**：`parsePascalString()` 独立解析"1字节长度+内容"的 Pascal 字符串，在同一个协议里解析 name/title 两处复用同一份逻辑，见 [code/patterns/parser_function/example2.lua](code/patterns/parser_function/example2.lua)。
- **真实出现位置**：案例1（DNS 标签名解析）；案例3两个版本文件里"解析 RRC/Data PDU"的那一段虽然没有拆成独立函数，但逻辑上承担的正是 Parser 的角色。

## 8.3 Table-Driven Parsing（表驱动解析）

- **什么时候用**：协议头部是"一串固定长度字段的顺序排列"，字段之间没有复杂的条件依赖。
- **典型代码结构**：一张 `{ {field=f, len=n}, ... }` 的描述表，配一个通用循环去消费。
- **优点**：新增/调整字段只改数据，不改逻辑；一眼就能看出整个协议头部的布局。
- **缺点**：字段长度依赖前面某个字段的值（比如"先读一个 length 字段，再决定后面读多长"）时，纯数据表达不了这种依赖，需要退回手写循环或用 8.6 的 Length-Prefixed Parser。
- **示例1（固定顺序字段描述表）**：`SensorFrameHeader` 用一张 `{field, len}` 的表描述4个固定长度字段，见 [code/patterns/table_driven/example1.lua](code/patterns/table_driven/example1.lua)。
- **示例2（表项自带 decode 函数）**：`SensorReading` 的表项多了 `read`/`format` 两个函数，负责把原始字节换算成带单位的可读文本（温度、电压），见 [code/patterns/table_driven/example2.lua](code/patterns/table_driven/example2.lua)。
- **真实出现位置**：案例1里 `dns.fields` 本身可以看成一张"字段描述表"，只是官方注释也承认"在真实脚本里，我不会这么手写，而是用程序化的方式生成这张表，以避免漏掉某个字段"——这句注释本身就是在推荐 Table-Driven 思路。

## 8.4 Subdissector Chain（子解析器链）

- **什么时候用**：协议内部封装了另一个已知协议（隧道、复用、载荷转发）。
- **典型代码结构**：`local d = Dissector.get(name); d:call(buffer(offset,len):tvb(), pinfo, tree)`，把"剩下的字节"连同 `pinfo`/`tree` 一起转发。
- **优点**：不用重新实现下游协议；`pinfo`/`tree` 一起传递，保证列文本、Tree 层级都正确嵌套。
- **缺点**：`Dissector.get(name)` 在目标协议不存在的环境里会报错，必须用 `pcall` 兜底；截取的 Tvb 范围一旦算错，下游会从错误的 offset 开始解析出一堆垃圾字段。
- **示例1（转发给内置协议）**：`MiniTunnel` 隧道协议把内层字节转发给 Wireshark 内置的 "ip" dissector，`Dissector.get` 用 `pcall` 兜底，见 [code/patterns/subdissector_chain/example1.lua](code/patterns/subdissector_chain/example1.lua)。
- **示例2（多目标转发 + 回退兜底）**：`MiniMux` 按1字节 protoId 在 "ip"/"http" 之间转发，未知类型或目标协议不存在时统一回退到永远存在的 "data"，见 [code/patterns/subdissector_chain/example2.lua](code/patterns/subdissector_chain/example2.lua)。
- **真实出现位置**：案例3是最完整的范例——`RLS -> RLS-3.2 -> nr-rrc.*/ip` 三层链条，且用 `pcall` 处理了下游协议可能不存在的情况（见 [08-case-study-rls-dissector.md](08-case-study-rls-dissector.md) 6.4节）。

## 8.5 Offset Walker（偏移量游标）

- **什么时候用**：字段数量较多、且存在若干条件分支（不同分支消费不同字节数）时，手写 `pos = pos + n` 容易在某个分支里漏加或加错。
- **典型代码结构**：把"当前偏移量"包成一个对象（`Cursor`），提供 `read(n)` 方法，内部自动前进并做越界检查。
- **优点**：offset 算术集中到一处，出错概率大幅降低；越界检查一次写好，处处受益。
- **缺点**：多引入一层抽象，对非常简单的协议（字段少、无分支）是过度设计。
- **示例1（基础游标）**：`Cursor:read(n)` 自动前进并做越界检查，见 [code/patterns/offset_walker/example1.lua](code/patterns/offset_walker/example1.lua)。
- **示例2（peek/skip 前瞻游标）**：`Cursor:peek(n)` 非消费地看一眼接下来的字节，用来判断"要不要解析一个可选字段"，确认后再用 `Cursor:skip(n)`/`read(n)` 正式消费，见 [code/patterns/offset_walker/example2.lua](code/patterns/offset_walker/example2.lua)。
- **真实出现位置**：案例1的 `pos = pos + name_len` / `pos = pos + 4` 手写累加是这个模式"没有被抽出来"的真实反例——正因为 DNS 记录解析涉及循环和不定长字段，用 Offset Walker 封装会更安全（这也是第九部分踩坑分析里"Offset 更新错误"这一条的直接动机）。

## 8.6 Length-Prefixed Parser（长度前缀解析）

- **什么时候用**：字段格式是"先读N字节表示长度，再据此读取该长度的内容"（变长字符串、变长二进制块）。
- **典型代码结构**：先读长度字段，检查剩余字节是否够，再切出对应长度的内容。
- **优点**：能提前判断"数据是否到齐"，是 TCP 重组（`dissect_tcp_pdus`）判断"还差多少字节"的理论基础。
- **缺点**：长度字段本身的字节序/宽度必须跟协议规范精确对齐，错一位就会导致后续全部错位。
- **示例1（单个长度前缀字段）**：4字节长度 + N字节 payload，见 [code/patterns/length_prefixed/example1.lua](code/patterns/length_prefixed/example1.lua)。
- **示例2（重复的长度前缀数组）**：`count` + N个 `[len+string]`，把 Length-Prefixed 用在"变长数组"场景，见 [code/patterns/length_prefixed/example2.lua](code/patterns/length_prefixed/example2.lua)。
- **真实出现位置**：案例3 `rls_v3_1.lua` 里 `gnbNameLength`/`linkIpLength` 的解析（先读4字节长度，再读对应长度的字符串），以及 `pduLength` 字段控制后续 payload 的解析范围。

## 8.7 TLV Parser（Type-Length-Value 通用解析）

- **什么时候用**：协议由"重复出现的 [Type][Length][Value]"结构组成（选项列表、扩展字段列表），且事先不知道会出现多少个。
- **典型代码结构**：`while offset < total do 读type/length -> 校验 -> 处理value -> offset+=2+length end`。
- **优点**：一套循环处理任意数量的 TLV 项；新增一种 Type 只需要在"处理 value"那一步加一个分支。
- **缺点**：必须在每次迭代里做严格的边界检查（`offset+2+length <= total`），否则畸形长度会导致 Tvb 越界异常。
- **示例1（扁平 TLV）**：通用的 `while` 循环处理任意数量的 `[Type][Length][Value]` 项，见 [code/patterns/tlv_parser/example1.lua](code/patterns/tlv_parser/example1.lua)。
- **示例2（嵌套 TLV）**：某个 Type（Container）的 Value 本身又是一段 TLV 序列，`parseTlvSequence()` 遇到 Container 时递归调用自己（带深度上限），见 [code/patterns/tlv_parser/example2.lua](code/patterns/tlv_parser/example2.lua)。
- **真实出现位置**：三个案例都没有典型的 TLV 结构（DNS 用的是"标签+长度"的变体，RLS 是固定分支不是通用 TLV），这是一个"案例里没有但真实世界很常见"的模式，因此单独给了完整可运行示例，供类比迁移到 TLS extensions、DHCP options、SNMP 等协议。

## 8.8 Recursive Parser（递归解析）

- **什么时候用**：数据结构本身是自相似的（域名标签序列、嵌套的 TLV、树形结构）。
- **典型代码结构**：函数内部用循环或函数自调用，直到遇到终止条件（长度为0的标签、深度上限）。
- **优点**：跟数据结构的自然形状一一对应，代码即文档。
- **缺点**：必须有明确的终止条件和深度上限，否则畸形包可能导致死循环或栈溢出。
- **代码**：见案例1 `getQueryName()`（[code/dns_dissector.lua](code/dns_dissector.lua) 第638-678行），用 `repeat...until label_len==0` 实现，本质上是"循环模拟递归"的写法（每次读一个标签长度，长度为0则终止）。
- **示例1（循环模拟递归：链式自相似）**：`parseLabelSequence()` 重新实现类 DNS 的标签序列解析，`repeat...until label_len==0`，见 [code/patterns/recursive_parser/example1.lua](code/patterns/recursive_parser/example1.lua)。
- **示例2（真正的自调用递归：树形自相似）**：`Group` 可以嵌套 `SubGroup`，`parseGroup()` 遇到 `SubGroup` 时真正调用自己（带深度上限保护），链式的循环写法在这里表达不出树形结构，见 [code/patterns/recursive_parser/example2.lua](code/patterns/recursive_parser/example2.lua)。
- **真实出现位置**：案例1的 DNS 域名标签解析是唯一真正意义上的递归/自相似结构解析。

## 8.9 Factory Pattern（工厂模式）

- **什么时候用**：同一类"东西"（比如某种 ProtoField 组合，或某种子协议对象）需要按统一规则批量创建，避免手写重复代码。
- **典型代码结构**：`local function makeVersionedProto(version, name) local p = Proto(name, ...); ...; return p end`。
- **优点**：减少重复；统一约束多个同类对象的创建规则（比如统一的命名前缀、统一的字段基础集合）。
- **缺点**：如果每个实例的差异远大于共性，硬套工厂会让代码更难读。
- **示例1（批量生成版本化 Proto）**：`makeVersionedProto(version, name, port)` 一次生成两个字段命名规则相同的版本化协议，见 [code/patterns/factory/example1.lua](code/patterns/factory/example1.lua)。
- **示例2（批量生成同模式的 ProtoField）**：`makeFieldSet(prefix, count)` 按统一命名规则批量生成 N 个 ProtoField，用于解析连续出现的多个传感器读数，见 [code/patterns/factory/example2.lua](code/patterns/factory/example2.lua)。
- **真实出现位置**：三个案例都没有显式使用（各自的 Proto 数量不多，手写也不复杂），但案例3的 `rls_v3_1.lua`/`rls_v3_2.lua` 之间高度相似的结构（同样的 `msgTypeNames`/`pduTypeNames`/`rrcMsgTypeNames` 表 + 同样的字段命名规则），如果未来版本继续增多，用一个工厂函数生成"公共字段 + 公共表"会是合理的下一步重构方向。

## 8.10 Registry Pattern（注册表模式）

- **什么时候用**：需要支持"可扩展的子协议集合"，且不希望"注册中心"和"子协议实现"耦合在一个文件里。
- **典型代码结构**：一个文件创建 `DissectorTable.new(...)`；若干独立文件各自在自己的 `init()` 里把自己 `add` 进这张表。
- **优点**：对扩展开放、对修改关闭——新增子协议只需要新文件，不改动已有代码。
- **缺点**：依赖 [03-lifecycle.md](03-lifecycle.md) 讲的加载时机规则，如果错误地把"add 自己"写在顶层代码而不是 `init()` 里，就可能因为文件加载顺序问题而失败。
- **示例1（DissectorTable 注册表，多文件协作）**：`entry.lua` 创建共享的 `DissectorTable`，`v1.lua`/`v2.lua` 各自在自己的 `init()` 里把自己 `add` 进去，模拟 `code/rls/` 的真实结构，见 [code/patterns/registry/entry.lua](code/patterns/registry/entry.lua)、[v1.lua](code/patterns/registry/v1.lua)、[v2.lua](code/patterns/registry/v2.lua)（三个文件需要同时加载）。
- **示例2（不依赖 DissectorTable 的轻量注册表）**：单文件内用一张普通 Lua table 做注册表，`Registry.register(key, name, handlerFn)` 供"模块"登记自己，入口 dissector 只做查表转发，见 [code/patterns/registry/example2.lua](code/patterns/registry/example2.lua)。
- **真实出现位置**：案例3是这个模式在真实项目里的完整体现（见 [08-case-study-rls-dissector.md](08-case-study-rls-dissector.md)），也是三个案例里唯一使用这个模式的。

## 8.11 Expert Pattern（专家信息模式）

- **什么时候用**：解析过程中遇到"值得标记但不阻断解析"的情况——畸形、废弃字段、有安全含义的取值、纯粹信息性的观察。
- **典型代码结构**：预先按"错误类型"分类声明多个 `ProtoExpert`（不要偷懒只声明一个通用的"出错了"），在对应分支调用 `item:add_proto_expert_info(ef, [自定义文本])`。
- **优点**：用户可以用 `expert.severity >= Warn` 之类的 Filter 快速筛出所有问题包，比人工翻 Tree 高效得多。
- **缺点**：如果 severity 分级不合理（什么都标 Error），Expert Info 面板会失去信号意义。
- **示例1（单一类别 ProtoExpert）**：`SecureMsg` 声明长度字段超过实际字节时触发一个 `ERROR` 级别的 `ProtoExpert`，见 [code/patterns/expert_info/example1.lua](code/patterns/expert_info/example1.lua)。
- **示例2（多类别 ProtoExpert 分级）**：`MiniLogin` 按"过期版本(Warn)/认证失败(Note)/非法状态(Error)"三种性质分别声明 `ProtoExpert`，模仿案例1按类别分声明的做法，见 [code/patterns/expert_info/example2.lua](code/patterns/expert_info/example2.lua)。
- **真实出现位置**：案例1是完整范例，5 个 `ProtoExpert` 按"请求/响应/信息性彩蛋/长度错误/查询畸形"分类声明，是三个案例里对这个模式运用最充分的。

## 8.12 Lazy Decode（懒解析）

- **什么时候用**：某个字段的解析代价较高（比如需要额外的字符串拼接、复杂计算），但用户可能根本不会展开对应的 Tree 节点去看它。
- **典型代码结构**：官方 API 对这个模式的支持主要体现在 `register_postdissector(proto, allfields)` 的 `allfields` 参数——默认 `false`，意味着"没有被引用/没有被展开的字段可以不生成"；只有显式传 `true` 才强制全部生成。
- **优点**：大幅降低不需要的字段带来的性能开销。
- **缺点**：如果你的 PostDissector 依赖某个字段"总是存在"，但该字段因为 Lazy 而没被生成，会读到 `nil`，需要显式处理。
- **示例1（默认 allfields=false + 防御性 nil 处理）**：`lazycarrier1` 只有 payload 超过阈值才计算代价较高的 digest 字段，`lazypost1` 用 `Field.new()` 读它时必须处理"字段不存在"的情况，见 [code/patterns/lazy_decode/example1.lua](code/patterns/lazy_decode/example1.lua)。
- **示例2（对比 allfields=true 的强制生成 + 性能取舍）**：同样的 carrier/PostDissector 结构，`register_postdissector(proto, true)` 强制生成字段，注释里说明这跟"字段本身是否存在"是两个不同层面的问题，见 [code/patterns/lazy_decode/example2.lua](code/patterns/lazy_decode/example2.lua)。
- **真实出现位置**：案例2的三个使用 `register_postdissector` 的示例都**没有**传 `allfields=true`（只有官方 Wiki 里另一个未收录进本笔记 case study 的例子传了 `true`），这本身就是"默认懒解析"策略的体现；[12-common-pitfalls.md](12-common-pitfalls.md) 会展开讲这里最容易踩的坑。

---

## 8.13 一张图串起来：这些模式分别解决"解析流程"的哪一环

```mermaid
flowchart LR
    subgraph Structure["协议结构决定用什么解析模式"]
        TD["Table-Driven<br/>固定顺序字段"]
        LP["Length-Prefixed<br/>先读长度再读内容"]
        TLV["TLV Parser<br/>重复的类型-长度-值"]
        REC["Recursive Parser<br/>自相似结构"]
    end

    subgraph Flow["流程组织决定怎么分派"]
        DISP["Dispatcher<br/>按字段值分支"]
        PARSE["Parser Pattern<br/>拆出独立解析函数"]
        WALK["Offset Walker<br/>集中管理游标"]
    end

    subgraph Ecosystem["生态/架构层面的模式"]
        REG["Registry<br/>可扩展子协议登记表"]
        CHAIN["Subdissector Chain<br/>转发给别的协议"]
        FAC["Factory<br/>批量创建同类对象"]
    end

    subgraph Quality["质量/性能相关"]
        EXP["Expert Pattern<br/>标记异常"]
        LAZY["Lazy Decode<br/>按需生成字段"]
    end

    Structure --> Flow
    Flow --> Ecosystem
    Ecosystem --> Quality
```

下一部分（[12-common-pitfalls.md](12-common-pitfalls.md)）反过来看：当这些模式**没有被正确使用**时，会具体表现成哪些常见 Bug。
