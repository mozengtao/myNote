# 第六部分 · 案例2：PostDissector 模式 — 官方 Wiki 示例群

不同于案例1/3是"单个完整插件"，这个案例分析的是**一种模式的四个递进示例**，它们分别来自官方 Wiki 的不同章节，共同拼出"PostDissector 能做什么、通常怎么组织"的完整画面。源码保存在 [code/postdissector/](code/postdissector/) 下：

```
01_trivial_postdissector.lua          来自 wiki.wireshark.org/lua/dissectors（postdissectors 章节）
02_infocolumn_swapper.lua             来自 wiki.wireshark.org/Lua/Examples（editing columns 章节）
03_tree_view_menu.lua                 来自 wiki.wireshark.org/Lua/Examples（View Packet Tree 章节）
04_fake_protocol_field_extractor.lua  来自 Peter Wu (@Lekensteyn) 的公开 gist，社区广泛转载引用
```

## 6.1 Plugin 背景

- **它解决什么问题**：普通 dissector 只能看到"自己这一层"的数据，而有些需求天生要求"看到整包所有协议都解析完之后的结果"——比如把 IP+TCP 的信息重新组合成一行摘要（案例01）、往 Info 列追加内容而不新增字段（案例02）、做一个调试用的"查看当前包所有已解析字段"的小工具（案例03）、基于已有字段做二次计算再展示成新列（案例04）。这些需求的共同点是：**它们都不属于任何具体协议，属于"整包结束之后"**。
- **为什么这样设计**：如果把这类逻辑塞进某个具体协议的 dissector 里，会导致"只有流量恰好经过这一层协议时才生效"，而且会造成"协议 A 的代码却要读协议 B 的字段"这种不合理的耦合。PostDissector 把"读取已有解析结果、做后处理"独立成一种排在最后执行的挂载点，天然解耦。

## 6.2 文件结构

四个示例都是单文件、无外部依赖，但内部结构呈现出一条清晰的"复杂度递进"路线：

```
01_trivial_postdissector.lua
  Field.new() x4  ->  Proto()  ->  ProtoField x3  ->  proto.fields=  ->  dissector()  ->  register_postdissector()
  (最小完整形态：读字段 -> 判断 -> 建新子树 -> register)

02_infocolumn_swapper.lua
  Proto()  ->  dissector()  ->  register_postdissector()
  (更简化：连 ProtoField 都没有，只改列文本)

03_tree_view_menu.lua
  gui_enabled() 判断  ->  闭包状态(output/tw)  ->  Proto()  ->  dissector()（用 all_field_infos()）
    ->  register_postdissector()  ->  菜单函数  ->  register_menu()
  (在 01 的基础上叠加了 GUI 层：菜单 + TextWindow)

04_fake_protocol_field_extractor.lua
  Field.new() x3  ->  Proto()  ->  ProtoField x3(float/float/string)  ->  proto.fields=
    ->  dissector()（数值计算 strlen/log）  ->  register_postdissector()
  (跟 01 结构相似，但强调"字段值可以参与数学运算再生成新字段"，
   并在注释里点出这类字段的常见用法：右键 "Apply as Column")
```

## 6.3 初始化流程

四份代码在"初始化"阶段的模式高度一致，这正是 PostDissector 的固定套路（对照 [10-development-workflow.md](10-development-workflow.md)）：

```
1. Field.new("已有协议.字段名")     创建只读提取器（如果需要读别的协议字段）
2. local myproto = Proto(name, desc)  声明一个"伪协议"身份——注意：这个 Proto 从来
                                       不会出现在任何 DissectorTable 里，它存在的
                                       唯一目的就是"给 register_postdissector 一个身份"
3. (可选) 声明 ProtoField，myproto.fields = {...}
4. function myproto.dissector(tvb, pinfo, tree) ... end
5. register_postdissector(myproto[, allfields])   <-- 全部四个示例的最后一步都是这一行
```

第03个示例额外多了一步：`register_menu("Lua/Tree View", menu_view_tree, MENU_TOOLS_UNSORTED)`，把"触发展示"的入口交给用户菜单，而不是自动展示。

## 6.4 Runtime：Packet 到来以后发生什么

这四个示例在"什么时候被调用"上完全一致——都是**所有普通 dissector 跑完之后，再跑一次自己的 dissector**。差异在"跑的时候做什么"：

```
01_trivial_postdissector.lua:
    读 tcp_src_f()/ip_src_f() 等 -> if tcp_src then 建子树 -> 写3个字段（源、目的、会话摘要）

02_infocolumn_swapper.lua:
    直接 pinfo.cols.info:append(" "..tostring(pinfo.dst).." -> "..tostring(pinfo.src))
    不建树、不读字段，是 PostDissector 能做到的"最小改动"

03_tree_view_menu.lua:
    { all_field_infos() } 拿到"这个包目前已经产生的全部字段"列表 -> 拼成一段文本
    -> updateWindow() 刷新（如果 TextWindow 已经打开）
    -> 用户点菜单才会第一次打开窗口，之后每个包都会自动刷新窗口内容

04_fake_protocol_field_extractor.lua:
    x = tostring(ip_src_field().value); n = #x + 5   对字符串长度做算术
    port = tcp_srcport_field() or udp_srcport_field(); n = math.log(port.value)
    都写进新建的字段里，供用户在 GUI 里右键 "Apply as Column"
```

## 6.5 使用了哪些基础设施

```
Proto                有（每个示例都建了一个"伪协议"）
ProtoField           01/04 有，02/03 没有（02 只改列，03 只展示文本不建字段）
DissectorTable       全部没有——这是 PostDissector 与普通 dissector 的关键区别：
                     它不需要挂到任何上游协议的路由表，注册方式是 register_postdissector
PostDissector        全部四个的核心机制
Field 提取器          01/04 用来读别的协议字段，03 用 all_field_infos() 读"全部"字段
GUI                  03 用了 register_menu + TextWindow + gui_enabled() 判断
Preferences          无
Expert Info          无
Conversation         无
Heuristic Dissector  无
Tap/Listener         无
```

## 6.6 哪些地方值得学习

1. **Proto 可以只是一个"挂载身份"，不必对应任何真实协议**——这打破了"每个 Proto 都必须解析某种真实字节格式"的直觉，PostDissector 场景下 Proto 更像是"一个命名空间 + 一个执行回调的授权证"。
2. **`register_postdissector` 是所有复杂度的共同终点**——不管前面用了多少种基础设施组合，最后一定是这一行代码，这让 PostDissector 模式极易识别：看到这行代码，就知道该按"读取模式"而不是"解析模式"去理解上面的代码。
3. **`gui_enabled()` 的防御性判断**（03示例）——同一份脚本要能在 tshark（无 GUI）下不崩溃，这是任何带 GUI 交互的 Lua 脚本都该做的检查，官方文档也在 GUI 一节反复强调。
4. **`pcall(tostring, finfo)` 防御性写法**（03示例第25-29行的 `getstring()` 函数）——官方注释直接说明"对随机 FieldInfo 调用 tostring() 可能出错"，用 `pcall` 兜底而不是让整个脚本崩溃，这是写通用调试工具时的必备习惯。
5. **明确指出 Tap 的局限性**（02示例开头注释）——"Taps are not guaranteed to be run at a point when they can set the column text"，这条注释本身就是一份关于"该用 PostDissector 还是 Tap"的选型指南（对照 [11-design-patterns.md](11-design-patterns.md) 和 [12-common-pitfalls.md](12-common-pitfalls.md)）。

## 6.7 一句话总结

PostDissector 模式的本质是"把 Proto 当身份证使用，而不是当协议解析器使用"——四个示例的复杂度差异很大，但骨架永远是"Field 提取器读值 -> 处理/展示 -> `register_postdissector`"，这是三个案例里结构最稳定、最容易照搬的一种范式。
