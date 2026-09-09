# 第九部分：常见踩坑

每一条都按"现象 -> 根因 -> 定位方法 -> 修复方式"展开，并尽量指向前面章节/案例里能验证结论的具体代码。

## 9.1 ProtoField 生命周期错误

- **现象**：脚本加载时报错，或者字段在 Filter 里搜不到、Tree 里显示乱码/默认值。
- **根因**：在 `dissector()` 函数体内调用 `ProtoField.new(...)`（每包都创建一个"新"字段，但从未注册进任何 `proto.fields`），或者忘记把创建好的 ProtoField 放进 `proto.fields = {...}`。
- **定位方法**：搜索脚本里 `ProtoField.` 出现的位置，确认它们**全部**在顶层代码，且都出现在某个 `proto.fields = {...}` 语句的右侧列表里（对照 [04-infrastructure.md](04-infrastructure.md) 4.11节、[02-architecture.md](02-architecture.md) 2.5节的依赖图）。
- **修复方式**：把所有 `ProtoField.xxx(...)` 移到文件顶层，紧跟着写 `proto.fields = { pf1, pf2, ... }`。

## 9.2 Offset 更新错误

- **现象**：从某个字段开始，后续所有字段的值都是错的（读到了别的字段的字节），或者报 Tvb 越界异常。
- **根因**：手写的 `offset = offset + n` 在某个分支里漏加、加错，或者条件分支消费的字节数不一致但用了同一个"下一步 offset"。
- **定位方法**：把每一处 `tvbuf:range(offset, len)` 和随后的 `offset = offset + len` 用表格摆出来对齐检查；或者用 [08-design-patterns.md](11-design-patterns.md) 8.5 的 Offset Walker 重写一遍，让工具帮你做累加。
- **修复方式**：采用 [code/patterns/offset_walker/example1.lua](code/patterns/offset_walker/example1.lua) 的 Cursor 封装，把"读N字节并前进"合并成一步，消除手写累加的机会。

## 9.3 Tvb 越界

- **现象**：Lua 运行时错误，类似 "Range is out of bounds"，脚本在解析某些（通常是畸形/截断的）包时直接崩溃或让 Wireshark 显示解析错误。
- **根因**：在没有检查 `buffer:len()`/`reported_length_remaining()` 的情况下，直接对声明长度而非实际长度做切片（比如 TLV 的 `length` 字段是攻击者/畸形数据伪造的超大值）。
- **定位方法**：找到崩溃时的包，用 tshark 加 `-X lua_script:yourscript.lua` 单独跑，配合案例1式的 `dprint()` 调试打印（见 [06-case-study-dns-dissector.md](06-case-study-dns-dissector.md) 6.7节）定位到具体哪一行 `tvb(offset,len)` 越界。
- **修复方式**：任何"用协议里声明的长度去切片"之前，先判断 `offset + declared_len <= buffer:len()`，参考 [code/patterns/tlv_parser/example1.lua](code/patterns/tlv_parser/example1.lua) 和 [code/patterns/length_prefixed/example1.lua](code/patterns/length_prefixed/example1.lua) 里的边界检查写法；同时案例1对"长度不足"的第一反应永远是 `add_proto_expert_info` 后 `return`，而不是继续往下解析（见 [06-case-study-dns-dissector.md](06-case-study-dns-dissector.md) 6.4节步骤3）。

## 9.4 Conversation Cache 误用

- **现象**：重新打开一个新的 capture 文件后，Tree 里出现了"不属于这个文件"的历史状态；或者反复打开/关闭同一个文件，缓存越攒越大。
- **根因**：用一张 Lua table 做跨包缓存，但从来没有在 `proto.init()` 里清空它——`init()` 在"capture 打开"和"capture 关闭"时都会调用（见 [03-lifecycle.md](03-lifecycle.md) 3.2/3.4节），如果不清空，旧数据会一直累积、跨文件复用。
- **定位方法**：搜索脚本里所有"跨包持有状态"的全局/`local`（但在文件顶层声明的）表，检查是否存在对应的 `proto.init()` 重置逻辑。
- **修复方式**：参考 [code/patterns/conversation_cache.lua](code/patterns/conversation_cache.lua) 里 `function p_conv.init() conv_state = {} end` 的写法——任何跨包缓存表都要有对应的 `init()` 清空。

## 9.5 TCP Reassembly（重组）相关问题

- **现象**：协议跑在 TCP 上，包被 TCP 分段后，dissector 只解析出半个 PDU，或者对同一个逻辑消息重复解析了好几次。
- **根因**：没有使用 Wireshark 提供的 TCP 重组基础设施（`dissect_tcp_pdus()`，或返回 `DESEGMENT_ONE_MORE_SEGMENT` 之类的约定），而是假设"一个 TCP 包等于一个协议消息"。
- **定位方法**：观察畸形出现的场景是否总是伴随"这个包比预期的短""下一个包看起来是上一个消息的续篇"；用 Wireshark 的 "Follow TCP Stream" 对照原始字节流确认消息边界确实跨越了多个 TCP 段。
- **修复方式**：长度前缀类协议优先使用官方的 `dissect_tcp_pdus(tvb, tree, min_header_size, get_len_func, dissect_func, [desegment])`（见 [04-infrastructure.md](04-infrastructure.md) 里对 Length-Prefixed Parser 一节的说明），把"判断消息是否到齐"的职责交还给框架，而不是自己在 dissector 里手动拼包。

## 9.6 Preferences 更新时机问题

- **现象**：用户在 Preferences 对话框里改了端口号，点 OK 之后，抓包窗口显示旧端口的流量仍然被正确解析、新端口却没生效（或反过来，两个端口同时生效）。
- **根因**：只用局部变量记住了新的端口值，却没有在 `prefs_changed()` 里同步调整 `DissectorTable`——旧端口的注册没删，新端口的注册没加。
- **定位方法**：检查是否实现了 `function proto.prefs_changed()`；如果实现了，检查函数体内是否有 `DissectorTable.get(...):remove(old, proto)` 和 `:add(new, proto)` 成对出现。
- **修复方式**：完整照抄案例1的写法（[code/dns_dissector.lua](code/dns_dissector.lua) 第292-315行）：先比较新旧端口是否变化，变化了才 remove 旧的、add 新的，避免每次哪怕端口没变也重复注册。**这也是 [09-feature-matrix.md](09-feature-matrix.md) 里指出案例3的一个改进空间**——它定义了端口 Preference 却没有实现 `prefs_changed()`。

## 9.7 Register 顺序错误

- **现象**：脚本加载时某一行报错"table not found"/"attempt to index a nil value"，通常出现在多文件插件里。
- **根因**：违反了第三部分 3.2 讲的 ASCIIbetical 加载顺序规则——某个文件的顶层代码试图 `DissectorTable.get("xxx")` 一张还没被别的文件创建出来的表。
- **定位方法**：确认报错文件名和"应该先加载的文件名"的字母顺序关系；检查"建表"的代码是不是被误放在了 `init()` 里（`init()` 延后执行，这本身没问题，但如果你误以为它跟顶层代码同时执行，就会在别处提早引用导致报错）。
- **修复方式**：遵循案例3的模式——"建表"放在文件名靠前的入口文件的**顶层代码**里；"往表里加自己"放在各个子协议文件的 `init()` 回调里，这样彻底摆脱对加载顺序的依赖（见 [08-case-study-rls-dissector.md](08-case-study-rls-dissector.md) 6.3节的分析）。必要时给文件加数字前缀强制排序。

## 9.8 Lua 文件加载顺序（跨文件依赖）

- **现象**：单个文件内一切正常，但把多个 `.lua` 放进 plugins 目录一起加载后行为异常，或者在不同操作系统/不同安装方式下（文件系统排序规则不同）表现不一致。
- **根因**：依赖了"文件系统遍历顺序恰好符合预期"这种脆弱假设，而不是显式的、跟文件名无关的机制。
- **定位方法**：查阅 [03-lifecycle.md](03-lifecycle.md) 3.2节引用的官方规则原文，确认自己的代码是否符合"先建表的文件必须 ASCIIbetically 排在使用表的文件之前（如果两者都在顶层代码里发生）"。
- **修复方式**：能用 `init()` 延后执行的注册逻辑，尽量都放进 `init()`（因为所有 `init()` 保证在所有文件的顶层代码执行完之后才被调用，天然免疫加载顺序问题）；确实需要在顶层代码就完成的依赖，用文件命名的数字前缀强制约束顺序。

## 9.9 PostDissector 使用错误

- **现象**：PostDissector 里读某个 Field 提取器，取到 `nil` 或者取到的是"上一个包"的值；或者反过来，脚本在 tshark 命令行下报错说 `TextWindow`/`register_menu` 未定义。
- **根因**：（1）读取的字段在当前包里没有被任何 dissector 写入过（用户没展开对应 Tree 分支，或没有传 `register_postdissector(proto, true)` 强制生成所有字段——见 [11-design-patterns.md](11-design-patterns.md) 8.12节 Lazy Decode）；（2）GUI 相关调用没有用 `gui_enabled()` 判断保护，在无界面环境下必然崩溃。
- **定位方法**：先确认字段确实"应该存在"于当前包（在 Wireshark GUI 里手动展开对应 Tree 验证）；检查脚本里所有 `TextWindow`/`register_menu`/`new_dialog` 调用是否被 `if gui_enabled() then ... end` 包裹。
- **修复方式**：需要稳定读到字段就传 `register_postdissector(proto, true)`（注意会拖慢速度，权衡取舍）；所有 GUI 相关代码模仿案例2第03个示例（[code/postdissector/03_tree_view_menu.lua](code/postdissector/03_tree_view_menu.lua)）开头的 `if not gui_enabled() then return end`。

## 9.10 Heuristic 返回值问题

- **现象**：明明协议不匹配，启发式函数却"认领"了包（导致别的协议的启发式候选没机会尝试，或者错误的 Tree 展示出现在不该出现的包上）。
- **根因**：某个判断分支里"忘记 return false"，Lua 函数默认返回 `nil`，虽然在框架层面 `nil` 通常被当作 falsy 处理，但这依赖的是一种"没有显式声明的隐式行为"，一旦哪个分支不小心 `return true` 或者忘了 return 直接走到函数末尾却做了 Tree 修改，就会造成"半吞半吐"的错误状态。
- **定位方法**：逐一检查启发式函数的每一个 `if ... then return false end` 分支是否覆盖了所有"不匹配"的情况；确认成功路径上，真正开始修改 Tree 之前的所有检查都已经通过。
- **修复方式**：严格照抄案例1 `heur_dissect_dns()` 的写法——每一处"发现不对"就立刻 `return false`，只有走到函数最后、所有检查都通过后才 `return true`，中间不要有能被跳过 return 语句的执行路径（[code/dns_dissector.lua](code/dns_dissector.lua) 第529-610行）。

## 9.11 Subdissector 调用方式错误

- **现象**：转发给下游协议后，下游解析出的字段/Tree 完全是乱的，或者脚本在某些 Wireshark 版本上报"attempt to call a nil value"。
- **根因**：（1）传给 `:call()` 的 Tvb 范围切错了（比如把整个 buffer 传下去，而不是"剩余未消费的部分"）；（2）`Dissector.get(name)` 在目标协议不存在的环境里返回错误而不是优雅地返回 `nil`，没有用 `pcall` 保护就直接崩溃。
- **定位方法**：核对转发调用里的 Tvb 切片参数是否正好对应"协议规范里定义的下一层载荷范围"；确认是否在低版本 Wireshark（缺少某些新 dissector）下测试过。
- **修复方式**：完全照抄案例3两个版本文件的写法——`local function get_dissector() dissector = Dissector.get(name) end; if pcall(get_dissector) then dissector:call(...) else <降级处理> end`（见 [08-case-study-rls-dissector.md](08-case-study-rls-dissector.md) 6.4节，行号见 [code/rls/rls_v3_2.lua](code/rls/rls_v3_2.lua) 第103-114行）。

## 9.12 性能问题

- **现象**：打开一个较大的 pcap 文件后，加载/滚动/应用 Filter 明显变慢，尤其是在启用了某个 Lua 插件之后。
- **根因**：常见来源——（1）`register_postdissector(proto, true)` 强制生成所有字段；（2）在 `dissector()` 里做了大量字符串拼接/正则/表创建，而这些操作在解释执行的 Lua 里比 C 慢得多；（3）Heuristic 函数没有"尽量便宜的检查排在前面"，导致对每一个不相关的包都做了较重的计算才被拒绝。
- **定位方法**：用 tshark 对同一个文件分别测"启用/禁用该插件"的耗时（`time tshark -r big.pcap -X lua_script:...`）做对比；官方文档也建议用 tshark 做性能画像。
- **修复方式**：`allfields` 非必要不开启；把 Heuristic 里"最便宜、最容易排除"的检查放在最前面（案例1的顺序：先查长度，再查固定取值范围的位域，最后才查"数量是否合理"，见 [06-case-study-dns-dissector.md](06-case-study-dns-dissector.md) 6.4节）；避免在每包都执行的路径上做不必要的表/字符串分配。

## 9.13 内存问题

- **现象**：长时间抓包或反复重新加载脚本后，Wireshark 占用内存持续增长。
- **根因**：跨包缓存表（9.4提到的 Conversation Cache）没有清理机制，或者往表里塞的是 `Tvb`/`TvbRange` 对象本身（它们生命周期只有一包，长期持有语义上就是错误的，具体实现上也不被支持）。
- **定位方法**：检查所有"跨包持有"的数据结构，确认存的是 `ByteArray`/普通 Lua 值/自定义 table，而不是 `Tvb`/`TvbRange`/`TreeItem`；确认存在对应的清空时机（`init()`，或者手动限制表的最大条目数）。
- **修复方式**：需要跨包保存字节内容时用 `tvbrange:bytes()` 转成 `ByteArray` 再存（见 [04-infrastructure.md](04-infrastructure.md) 4.16节）；Conversation Cache 严格遵守 9.4 的清理规则。

## 9.14 一个通用的定位思路

```
遇到任何"行为不符合预期"的情况，按这个顺序排查：
  1. 是不是生命周期搞错了？（这个对象我以为它跨包存在，其实每包重建；
     或者反过来，我以为每包重建，其实是共享的）
     -> 回看 05-runtime-mental-model.md
  2. 是不是顺序搞错了？（这行代码依赖另一行代码先执行，但没有）
     -> 回看 02-architecture.md 2.5节依赖图 + 03-lifecycle.md 3.2节加载规则
  3. 是不是边界没检查？（offset、length、Dissector 是否存在）
     -> 回看 11-design-patterns.md 里对应模式的边界检查写法
  4. 加打印调试：临时在关键路径插入 print(...) 或案例1那种 dprint()，
     用 tshark -X lua_script:script.lua -r file.pcap 跑，看输出顺序
  5. 对照三个案例里"同类问题"的正确写法，逐行比对差异
```

下一部分（[13-final-mental-model.md](13-final-mental-model.md)）把前面九个部分收束成一份"看到任何 Lua Plugin 该怎么读"的最终心智模型。
