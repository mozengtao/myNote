# 第四部分：Wireshark 已经提供了哪些基础设施

## 4.1 心态转变

写 Lua Plugin 最大的误区是把它当成"从零写一个协议解析库"。实际上 Wireshark 的 C 核心（`epan`）已经把几乎所有跟"协议无关"的通用能力做好了，Lua 层只是把这些能力包了一层脚本接口暴露出来。你要写的只是"协议特有的那一小块逻辑"。

本部分逐项过一遍这些基础设施。每一项固定回答六个问题：**为什么存在 / 解决什么问题 / 什么时候用 / 生命周期 / 最常见用途 / 容易踩的坑**。

---

## 4.2 Protocol Registry（协议注册表）

- **为什么存在**：Wireshark 需要一个全局唯一的地方，知道"这个进程里一共注册了哪些协议"，用来支撑 Filter 自动补全、`Analyze > Enabled Protocols`、Preferences 分类页等 UI。
- **解决什么问题**：避免每个协议自己维护身份信息、避免命名冲突（两个协议不能用同一个 abbreviation）。
- **什么时候用**：你不会直接操作它——调用 `Proto("name", "desc")` 的那一刻，就是在往这张表插入一条记录。
- **生命周期**：进程/lua_State 级别，跟随 Reload 而重建。
- **最常见用途**：几乎所有插件的第一行代码。
- **容易踩坑**：abbreviation 重名（跟内建协议或别的插件冲突）会导致注册失败或互相覆盖；这个坑往往在插件多、加载顺序变化时才暴露。

## 4.3 Dissector Registry（Dissector 注册表）

- **为什么存在**：区分"协议的身份"（Proto）和"协议的处理函数"（Dissector 对象）。Wireshark 内部允许通过名字查到任何已注册的 Dissector，即使你不知道它是 Lua 写的还是 C 写的。
- **解决什么问题**：让子协议可以"调用别人已经写好的解析逻辑"，而不必知道对方是用什么语言实现的。
- **什么时候用**：`Dissector.get("ip")`、`Dissector.get("nr-rrc.dl.ccch")` 这种"我要把剩下的 bytes 转交给某个已知协议处理"的场景。
- **生命周期**：跟随 Protocol Registry。
- **最常见用途**：Subdissector Chain（见案例3 `rls_v3_2.lua` 里 `Dissector.get("ip"):call(...)`）。
- **容易踩坑**：`Dissector.get(name)` 在对应的协议还没被注册/编译进当前 Wireshark 版本时会抛错——案例3特别用 `pcall` 包住这个调用，因为老版本 Wireshark 可能没有 `nr-rrc.*` 系列 dissector。

## 4.4 DissectorTable（Dissector 路由表）

- **为什么存在**：某个字段的值（端口号、类型码、协议号）决定"接下来该调用谁"，这是一个通用模式，值得抽象成一张"key -> Proto/Dissector"的表，而不是每个协议自己写一套 if/elseif。
- **解决什么问题**：解耦"上游协议"和"下游协议"——TCP 不需要认识 HTTP，只需要维护一张 `tcp.port` 表，HTTP 自己把 80 端口挂进去。
- **什么时候用**：你的协议跑在某个已知的传输协议之上（UDP/TCP 端口），或者你想让别人能把子协议挂到你身上（`DissectorTable.new(...)`）。
- **生命周期**：由创建它的协议持有，通常是 Proto 级别（跟随注册表），内容（add/remove）可以在 `init()` 或 `prefs_changed()` 里动态调整。
- **最常见用途**：`DissectorTable.get("udp.port"):add(65333, dns)`（案例1）；`DissectorTable.new("rls", ...)` 自建路由表（案例3）。
- **容易踩坑**：改端口 Preference 时忘记先 `remove` 旧端口再 `add` 新端口，会导致旧端口残留指向你的协议；`get_dissector(key)` 查不到时返回 `nil`，必须判空（案例3的 fallback 逻辑就是在处理这个）。

## 4.5 Decode As

- **为什么存在**：`DissectorTable` 的"默认路由"是静态配置好的，但用户经常需要临时改写——比如一段流量走在非标准端口上。Decode As 提供了运行时改写路由表的 UI。
- **解决什么问题**：不用改代码/改 Preferences，右键一个包就能临时/永久指定它该用哪个 Proto 解析。
- **什么时候用**：`dissectortable:add_for_decode_as(proto)`，把自己加入某张表的"候选列表"，这样用户在 Decode As 对话框里才能看到你。
- **生命周期**：候选资格是注册时确定的（跟随 Proto 生命周期）；具体某个包"被 Decode As 成什么"是运行时用户交互产生的临时状态。
- **最常见用途**：案例1的"Voila, you're now decoding DNS packets using ... Decode As"；案例3 `init()` 里的 `udp_port:add_for_decode_as(rlsProtocol)`。
- **容易踩坑**：只 `add(port, proto)` 而不 `add_for_decode_as(proto)`，用户在没有命中默认端口的场景下，Decode As 列表里根本找不到你的协议。

## 4.6 Tvb（Packet Buffer / 原始字节）

- **为什么存在**：需要一个"这个包(或包的某一层)的原始字节"的统一表示，支持跨越 TCP 重组、隧道解封装等复杂场景，而不是简单的字节数组。
- **解决什么问题**：让"字节从哪来"（可能是原始帧，也可能是重组后的、解密后的、隧道解封装后的）这件复杂事情对 dissector 透明。
- **什么时候用**：`dissector(tvbuf, pktinfo, root)` 的第一个参数，永远是这次调用范围内的字节视图。
- **生命周期**：**每次 dissector 调用新建**，调用结束后失效——不能跨包缓存 Tvb 对象本身（要缓存的话必须 `:bytes()` 转成 `ByteArray` 再存）。
- **最常见用途**：`tvb:len()`、`tvb:reported_length_remaining()`、`tvb:range(offset,len)`。
- **容易踩坑**：`tvb:len()`（已捕获长度）和 `tvb:reported_length_remaining()`（协议声明的总长度，可能因 snaplen 被截断）混用，导致对"包是否完整"的判断错误——案例1的注释专门解释了为什么它选择用后者。

## 4.7 TvbRange（Tvb 的切片视图）

- **为什么存在**：绝大多数操作只关心 Tvb 里的某一段字节，需要一个"偏移量+长度"的轻量视图，而不必每次都复制字节。
- **解决什么问题**：`tvb(offset,len)` 语法糖，把"取一段字节并按某种类型解释"这件事标准化（`:uint()`、`:le_uint()`、`:string()`、`:bitfield()`……）。
- **什么时候用**：几乎每一行 `tree:add(field, tvbuf:range(pos,len))` 背后都有一个隐式或显式创建的 TvbRange。
- **生命周期**：跟随创建它的 Tvb，同样是单包级别。
- **最常见用途**：字段取值、`bytes()` 转 `ByteArray`、`bitfield(offset,len)` 取比特域（案例1 heuristic 函数里检查 DNS opcode/rcode 就是这么做的）。
- **容易踩坑**：越界访问（`offset+len` 超过实际长度）会抛 Lua 运行时错误而不是返回空——写 TLV/长度前缀类解析器时必须先检查剩余长度再切片（见 [11-design-patterns.md](11-design-patterns.md) 的 TLV Parser 和 [12-common-pitfalls.md](12-common-pitfalls.md)）。

## 4.8 Pinfo（Packet Info）

- **为什么存在**：一个包在被逐层解析的过程中，需要一个"贯穿整个解析链的上下文对象"，携带地址、端口、时间戳、列文本、conversation 等元信息，并允许下游协议往上游协议设置的信息上继续追加。
- **解决什么问题**：让"Packet List 上一行该显示什么"这种全局性的展示逻辑，可以被解析链上任何一层协议共同参与决定（每层都能往 Info 列 append 一点内容）。
- **什么时候用**：`dissector(tvb, pinfo, tree)` 第二个参数；读地址/端口用 `pinfo.src`/`pinfo.dst`/`pinfo.src_port`；设置列文本用 `pinfo.cols.*`；参与 conversation 用 `pinfo.conversation`。
- **生命周期**：单包级别，但同一个 Pinfo 对象会依次流经这一层所有相关的 dissector（本层 dissector、postdissector、tap 等）。
- **最常见用途**：`pinfo.cols.protocol:set(...)`、`pinfo.cols.info:append(...)`、`pinfo.visited`（判断是不是第二次解析同一个包，用于避免重复统计）。
- **容易踩坑**：在允许多次解析同一个包的场景（比如打开了某些需要多轮 dissect 的 Display Filter）忘记检查 `pinfo.visited`，导致 Tap/统计类逻辑重复计数。

## 4.9 Columns（列）

- **为什么存在**：Packet List 的每一列（No./Time/Source/Destination/Protocol/Length/Info）本质上是不同协议在解析过程中协作写出来的文本，需要一个统一的写入接口。
- **解决什么问题**：允许多层协议依次往同一列追加信息，而不是只有最后一层能写。
- **什么时候用**：`pinfo.cols.info:set(...)` / `:append(...)` / `:prepend(...)`；`pinfo.cols.protocol:set(...)`。
- **生命周期**：单包级别，随 Pinfo。
- **最常见用途**：案例1里先 `set("(".. transid ..")")` 再 `prepend("Response " )`；案例2的 swapper 例子专门演示只改列不建树。
- **容易踩坑**：官方注释明确指出——**Tap 不保证在能安全设置列文本的时机运行**，如果只是想改列文本，应该用普通 dissector 或 postdissector，而不是 Tap。

## 4.10 Tree（Protocol Tree / TreeItem）

- **为什么存在**：用户看到的 Packet Details 面板，是一棵嵌套树；这棵树需要在解析过程中被逐层构建出来，同时还要支持"选中一个节点，右下角显示对应的原始字节范围"（Tree 节点和 Tvb 范围是绑定的）。
- **解决什么问题**：把"解析出的字段"和"这段字节在原始 buffer 里的位置"统一到一个可交互的数据结构里。
- **什么时候用**：`dissector` 的第三个参数（`root`），几乎每次调用 `root:add(...)` 或某个子树的 `:add(...)`。
- **生命周期**：单包级别，随这次解析调用新建、结束后固化成 GUI 可展示的数据（在 tshark 里则用于文本/JSON 输出）。
- **最常见用途**：`tree:add(proto_or_field, tvbrange, [value])`，返回新子节点，可以继续往下挂。
- **容易踩坑**：`:add()` 的返回值才是"新加的这个节点"，如果想继续往它下面加子节点，必须用返回值，而不是复用外层的 `tree` 变量（案例1里 `flag_tree`、`query_flag_tree` 的分层写法就是在演示这一点）。

## 4.11 ProtoField（协议字段元数据）

- **为什么存在**：一个字段要同时具备"怎么从 bytes 转成人类可读值"、"在 Filter 里叫什么名字"、"在 Tree 里显示成什么样"三种能力，需要一个统一的元数据对象。
- **解决什么问题**：避免每次展示字段都手写格式化逻辑；让字段自动获得 Filter 能力。
- **什么时候用**：声明阶段（顶层代码）创建，`proto.fields = {...}` 赋值后才算真正注册；运行时只在 `tree:add(field, ...)` 里被引用，不再重新创建。
- **生命周期**：跟随 Proto，整个 capture 期间共用同一批 ProtoField 对象（详见 [05-runtime-mental-model.md](05-runtime-mental-model.md)）。
- **最常见用途**：位域字段（`ftypes.BOOLEAN`/`ftypes.UINT16` + `mask` 参数，见案例1的 flags 解析）、值映射表（`base.DEC` + 一张 `{[数字]=文字}` 表）。
- **容易踩坑**：在 `dissector()` 函数体内 `ProtoField.new(...)`——这是最常见的新手错误之一，会导致每包都创建新字段、注册失败或崩溃；ProtoField 必须在顶层代码创建。

## 4.12 Expert Info（专家信息）

- **为什么存在**：解析过程中发现"协议异常/值得关注但又不是致命错误"的情况很常见（畸形包、被截断、使用了已废弃字段），需要一种独立于普通字段、但同样可过滤、可在专门面板汇总的机制。
- **解决什么问题**：让"这个包有问题"这件事，从"淹没在 Tree 里的一行文字"变成"可以用 `expert.severity == Error` 过滤、可以在 Expert Info 面板批量查看"的一等公民。
- **什么时候用**：`ProtoExpert.new(abbr, text, group, severity)` 声明阶段创建，`proto.experts = {...}` 注册；运行时用 `item:add_proto_expert_info(ef)` 或 `tree:add_tvb_expert_info(ef, tvbrange)` 触发。
- **生命周期**：跟随 Proto（声明部分），触发是单包级别的。
- **最常见用途**：畸形包（`expert.group.MALFORMED` + `expert.severity.ERROR`，案例1的 `ef_too_short`）、信息性提示（`expert.severity.CHAT`/`NOTE`，案例1的 `ef_query`/`ef_response`）。
- **容易踩坑**：官方文档特别提到旧写法 `tree:add_expert_info(PI_MALFORMED, PI_ERROR, "...")` 仍能用，但不会被正确注册到协议的过滤体系里；新代码应该统一用 `ProtoExpert` + `add_proto_expert_info`/`add_tvb_expert_info`（案例1的注释直接对比了新旧两种写法）。

## 4.13 Conversation（会话）

- **为什么存在**：很多协议行为依赖"这是同一对地址:端口之间的第几个包"，而不是单个包本身——需要一个跨包的会话身份。
- **解决什么问题**：让协议可以"记住"某个会话第一次出现时协商/探测出的信息（比如启发式判断出这是 DNS），后续包直接命中，不用每次重新做启发式判断。
- **什么时候用**：启发式 dissector 成功识别出协议后，通常会把 `pinfo.conversation`（或新版本 `Conversation.find_from_pinfo(pinfo).dissector`）设置成自己，让同一会话后续的包跳过启发式、直接调用。
- **生命周期**：一个 conversation 对象的生命周期约等于"这对地址端口在这个 capture 里活跃的时间段"，随新的 capture 文件重新开始。
- **最常见用途**：案例1展示了三种历史上先后引入的写法（`pktinfo.conversation = dns` / `pktinfo.conversation.dissector = dns` / `Conversation.find_from_pinfo(pktinfo).dissector = dns`），本质是同一件事的三代 API。
- **容易踩坑**：conversation "粘性"绑定的是 dissector，不是数据；如果协议在会话中途切换了行为（比如从明文变成加密），必须有额外的状态判断，不能假设"一旦绑定就永远用同一种解析方式"。

## 4.14 Conversation Cache（会话状态缓存）

- **为什么存在**：Conversation 对象本身只是一个"身份"，真正要缓存的"业务状态"（协商出的编码方式、已看到的序列号等）需要额外的存储机制。
- **解决什么问题**：避免用全局变量存储跨包状态（全局变量在多个 capture 文件之间、甚至同一文件重新解析时会"串"数据）。
- **什么时候用**：需要记住"上一个包告诉我们的信息，这个包要用到"的任何场景。
- **生命周期**：理论上应该跟随 conversation（甚至跟随 capture 文件）；但纯 Lua side 没有现成的、跟 C 层 `conversation_add_proto_data` 完全对齐的官方 API（社区有 `wslua_conversation` 这样的 C 扩展模块弥补），所以常见做法是**自己维护一张 Lua table，并在 `proto.init()` 里清空**。
- **最常见用途**：见 [code/patterns/conversation_cache.lua](code/patterns/conversation_cache.lua)。
- **容易踩坑**：忘记在 `init()` 清空缓存表，导致"重新打开另一个文件"时旧状态残留；用 tostring(conversation) 做 key 时要确认这个字符串在同一会话内是稳定的。

## 4.15 Preferences（Pref）

- **为什么存在**：协议经常需要用户可配置的参数（端口号、是否启用某个可选解析、调试级别），这些参数需要能持久化到 Wireshark 的配置文件，并出现在标准的 Preferences 对话框里。
- **解决什么问题**：统一配置项的存储、UI 展示、变更通知，不需要每个协议自己发明配置系统。
- **什么时候用**：`proto.prefs.xxx = Pref.uint/bool/string/enum(...)` 声明阶段创建；运行时读 `proto.prefs.xxx` 当前值；变更时实现 `proto.prefs_changed()`。
- **生命周期**：声明跟随 Proto；取值贯穿整个运行期，直到用户改变或重新加载。
- **最常见用途**：端口号配置（案例1、案例3都有）、开关某个可选特性（案例1的 `heur` 开关）、调试级别（案例1的 `debug` 枚举）。
- **容易踩坑**：`prefs_changed()` 里忘记同步更新 `DissectorTable` 的注册状态（只改了本地变量，没有 remove 旧端口/add 新端口），导致 Preferences 界面显示的端口和实际生效的端口不一致（案例1的写法是反面教材的"正确示范"，见 [06-case-study-dns-dissector.md](06-case-study-dns-dissector.md)）。

## 4.16 ByteArray

- **为什么存在**：有些场景需要脱离 Tvb 语境、单纯操作一段字节（比如需要长期缓存、或者自己拼装出一段新的字节序列）。
- **解决什么问题**：提供不依赖"这段字节属于哪个包"的、可独立存在的字节容器。
- **什么时候用**：`tvbrange:bytes()` 从 TvbRange 转换而来；需要跨包保存字节内容时（Tvb/TvbRange 不能跨包存，ByteArray 可以）。
- **生命周期**：一旦创建，独立于任何 Tvb 存在，可以被长期持有（比如放进 Conversation Cache）。
- **最常见用途**：案例1的 `getQueryName()` 里用 `barray:get_index(pos)` 逐字节读取 DNS 标签长度、`barray:raw(pos,len)` 取子串。
- **容易踩坑**：`ByteArray` 是 0-based 索引（`get_index(0)` 是第一个字节），跟 Lua 字符串/table 常见的 1-based 习惯不一致，案例1的注释里专门提醒了这一点。

## 4.17 Address

- **为什么存在**：源/目的地址需要统一表示 IPv4/IPv6/以太网 MAC 等多种地址类型，并支持比较、格式化。
- **解决什么问题**：让协议代码不用关心地址是哪种具体类型，就能做"是否相等"、"转字符串"等操作。
- **什么时候用**：`pinfo.src` / `pinfo.dst`，或者从 Field 提取器读到的地址类值。
- **生命周期**：单包级别，随 Pinfo。
- **最常见用途**：Conversation key 的组成部分之一；日志/调试输出（`tostring(address)`）。
- **容易踩坑**：地址对象转字符串前后要用 `tostring()`，直接拼接字符串（`..`）在某些版本/类型上不会自动触发转换。

## 4.18 NSTime

- **为什么存在**：时间戳需要纳秒精度、且需要能相互做差值运算（用于 delta time、RTT 等统计）。
- **解决什么问题**：避免用浮点数表示时间戳导致的精度问题。
- **什么时候用**：`pinfo.abs_ts`、`pinfo.rel_ts`、`pinfo.delta_ts` 等只读属性；某些字段本身就是 `ftypes.ABSOLUTE_TIME`/`ftypes.RELATIVE_TIME` 类型时也会用到。
- **生命周期**：单包级别。
- **最常见用途**：统计类插件计算包间时间差、超时判断。
- **容易踩坑**：跟 Lua 原生 `os.time()` 精度/时区语义不同，不要混用。

## 4.19 Listener / Tap

- **为什么存在**：很多需求（画流量图、算吞吐量、统计错误率）不需要修改 Tree，只需要"看一眼每个包的某些字段然后做汇总"，这跟"解析并展示单个包"是两类完全不同的关注点，应该用不同的机制承载。
- **解决什么问题**：把"统计/汇总"从"解析/展示"中解耦；Tap 可以基于**已经存在的 Filter 表达式**只订阅感兴趣的一部分包（`Listener.new("frame", "ip.addr == 10.0.0.0/8")`），而不用自己重新判断。
- **什么时候用**：`Listener.new(tap_name, [filter])` 创建；实现 `tap.packet(pinfo, tvb, tapinfo)` 每包回调、`tap.draw()` 汇总输出回调、`tap.reset()` 重置回调。
- **生命周期**：跟随它监听的对象（一次统计会话/一次 "Statistics" 窗口打开期间），`reset()`在开始新一轮统计前调用。
- **最常见用途**：Wireshark 官方测试脚本 `test/lua/listener.lua` 里的 `max_gap.lua` 示例——统计两个 IP 之间连续包的最大时间间隔。
- **容易踩坑**：前面 Columns 小节提过的——**Tap 不保证在能安全设置列文本的时机运行**，不要在 tap.packet 里做"修改这个包展示内容"的事，那是 dissector/postdissector 的职责。

## 4.20 Field 提取器（Field / FieldInfo）

- **为什么存在**：有时候你想要的不是"自己解析出一个新字段"，而是"读别人已经解析好的字段的值"（尤其是 PostDissector 场景），需要一种只读地"回看"已产生解析结果的机制。
- **解决什么问题**：避免重复解析——`ip.src`/`tcp.srcport` 已经被 IP/TCP 层解析过了，没必要自己再手写一遍。
- **什么时候用**：`Field.new("proto.field")` 在顶层代码创建提取器对象（只创建一次）；`local fieldinfo = my_field()` 在 dissector 函数体内取"当前包"的值（每包调用）。
- **生命周期**：提取器对象（`Field`）是整个 capture 共用的；取到的值（`FieldInfo`）是单包级别的，且只有在"这个字段已经被写入过 Tree"之后才有值。
- **最常见用途**：PostDissector 的核心手法（见案例2全部四个示例）；案例1也用它读自己刚写的字段（`questions_field()()` 读 `mydns.num_questions`）。
- **容易踏坑**：`Field.new(...)` 绝对不能写在 `dissector()` 函数体内部，也不能写在对应 `proto.fields=...` 赋值之前——这是官方文档和案例1注释都反复强调的顺序约束（呼应第二部分 2.5 的依赖图）。

## 4.21 GUI（菜单 / 对话框 / TextWindow）

- **为什么存在**：有些 Lua 脚本不是为了解析协议，而是为了给用户提供交互式工具（比如"导出某个字段的统计"、"批量检查某个条件"），需要挂到 Wireshark 菜单里、弹出自定义窗口。
- **解决什么问题**：不用自己写一套 UI 框架，直接借用 Wireshark 已有的窗口系统。
- **什么时候用**：`register_menu(path, func, [menu_group])` 挂菜单项；`TextWindow.new(title)` 弹出纯文本窗口；`new_dialog(title, func, ...)` 弹出带输入框的对话框。
- **生命周期**：菜单注册是启动时一次性的；窗口对象的生命周期由用户操作（打开/关闭）决定。
- **最常见用途**：案例2的 `tree_view_menu.lua`——postdissector 收集数据，菜单负责展示。
- **容易踩坑**：`register_menu`/`TextWindow`/`new_dialog` 只在有 GUI 的 Wireshark 里有意义，tshark（无界面）环境下要用 `gui_enabled()` 判断并跳过，否则脚本在 tshark 下会报错（案例2的 `tree_view_menu.lua` 开头就有 `if not gui_enabled() then return end`）。

## 4.22 PostDissector

- **为什么存在**：有些逻辑必须在"所有协议都解析完"之后才能做——比如想同时用到 IP 层和 TCP 层的字段做汇总展示，如果自己是一个中间层的协议，是看不到后面协议解析结果的。
- **解决什么问题**：提供一个"排在最后执行"的钩子位置，此时整棵 Tree 已经基本成型，可以安全地用 Field 提取器读任何已经出现过的字段。
- **什么时候用**：`register_postdissector(proto, [allfields])`——`allfields=true` 时会强制生成所有字段（哪怕用户没在 Tree 上展开），代价是性能开销明显增大。
- **生命周期**：注册是一次性的（顶层代码）；`dissector` 函数体每包执行一次，且保证在所有普通 dissector 之后。
- **最常见用途**：案例2的全部内容（改列文本、新增汇总字段、调试用的字段浏览器）。
- **容易踩坑**：`allfields` 默认是 `false`，如果你的 Field 提取器读的字段"用户没有展开对应的 Tree 分支"，可能读不到值——这也是为什么很多调试类 postdissector 会显式传 `true`，但要清楚这会拖慢整体解析速度。

## 4.23 Heuristic Dissector（启发式 Dissector）

- **为什么存在**：有些协议没有固定端口/固定标识，只能通过"看内容像不像"来猜测——需要一种"排队试一遍，谁先认领就归谁"的机制。
- **解决什么问题**：让多个协议可以对同一类载荷（比如所有 UDP payload）分别提交"候选识别函数"，由框架统一调度、避免手写"先试这个再试那个"的链条。
- **什么时候用**：`proto:register_heuristic(listname, func)`，函数必须返回 `true`（认领，且已完成解析）或 `false`（放弃，让框架尝试下一个候选）。
- **生命周期**：注册一次性；函数每次"轮到自己被尝试"时调用一次（可能不是每包都被调用——一旦启发式命中并绑定了 conversation，后续包会走 4.13 的直连路径，不再进入启发式候选队列）。
- **最常见用途**：案例1的 `heur_dissect_dns`——先做尽可能便宜的合法性检查（长度、opcode/rcode 取值范围），最后才调用真正的 dissector。
- **容易踩坑**：**返回值语义反直觉的常见错误**——中途识别失败一定要显式 `return false`，绝不能"忘记 return"（Lua 里默认返回 `nil`，框架会把它当 falsy 处理，行为上凑巧等价于 false，但依赖这种"凑巧"是不好的习惯，官方案例每个分支都写了明确的 return）；另外，写一个"看起来什么都能匹配"的启发式函数会挡住其它协议的候选机会，案例1的注释专门吐槽了这一点："in practice, you'd never set a dissector like this to be heuristic ... but I'm doing it to show how it would be done"。

## 4.24 Subdissector（子解析器）/ Subdissector Chain

- **为什么存在**：协议是分层的，没有任何一个 dissector 应该负责解析"自己以外"的内容——需要一种标准方式把"剩下的字节"移交给下一层。
- **解决什么问题**：让 TCP 不需要认识 HTTP/TLS，让你的 Proto 也不需要认识它内部可能封装的任何协议。
- **什么时候用**：拿到一个已知的 `Dissector` 对象后调用 `dissector:call(tvb, pinfo, tree)`；或者通过 `DissectorTable:try_heuristics(...)`间接触发。
- **生命周期**：单次调用级别，跟随当前包的解析过程。
- **最常见用途**：案例3是最典型的例子——`rls.lua` 把整包转发给具体版本的实现，`rls_v3_2.lua` 又把 RRC/Data 载荷转发给 `nr-rrc.*` 或 `ip` dissector，形成一条 **RLS -> RLS-3.2 -> nr-rrc/ip** 的三层 Subdissector Chain。
- **容易踩坑**：转发时 Tvb 范围切错（应该只把"剩余payload"传下去，不能把整包再传一遍，否则下游会从错误的 offset 开始解析）；调用 `:call()` 前不判空（`Dissector.get()` 找不到时返回错误而不是 `nil`，需要 `pcall` 兜底，见案例3 `rls_v3_1.lua`/`rls_v3_2.lua` 的 `pcall(get_dissector)` 写法）。

---

## 4.25 速查表（按使用频率/重要程度排序）

```
必用（几乎每个插件都会用到）：
  - Proto / ProtoField / Tvb / TvbRange / Pinfo / Tree

常用（超过"最小可用"规模的插件基本都会遇到）：
  - DissectorTable / Preferences / Columns / ByteArray

按需（协议特性决定要不要用）：
  - Expert Info（想要"畸形包高亮"）
  - Conversation / Conversation Cache（协议有跨包状态）
  - Heuristic Dissector（协议没有固定端口/标识）
  - Subdissector（协议内部还封装了别的协议）
  - Decode As（想支持非默认端口场景）

可选增强（跟"解析单个包"关系不大）：
  - Listener/Tap（做统计/画图）
  - PostDissector（需要在所有协议解析完之后做汇总）
  - GUI（菜单/对话框/TextWindow）
  - Field 提取器（PostDissector 场景的核心工具，也可用于普通 dissector 内部）

底层工具（其它对象内部依赖，通常不直接大量手写）：
  - Address / NSTime
```

下一部分（[05-runtime-mental-model.md](05-runtime-mental-model.md)）把这些对象按照"一次 Packet 解析"的时间顺序重新串一遍，并明确标注每一个的"只读/可写/缓存/共享/新建"属性。
