# 第六部分 · 案例1：官方教学级 dissector — `test/lua/dissector.lua`

完整源码保存在 [code/dns_dissector.lua](code/dns_dissector.lua)（678 行，原文件加了5行来源说明注释，行号从第6行起对应官方仓库的第1行）。原始位置：[wireshark/wireshark/test/lua/dissector.lua](https://github.com/wireshark/wireshark/blob/master/test/lua/dissector.lua)，作者 Hadriel Kaplan，脚本内部自称 `dns_dissector.lua`。

## 6.1 Plugin 背景

- **它解决什么问题**：官方注释写得非常直白——这不是给用户用的真实 DNS 解析器（Wireshark 内建 DNS dissector 已经很完善），而是**两个目的合一的参考实现**：(1) 给写 Lua dissector 的人当教程；(2) 给 Wireshark 自己的测试套件当"覆盖尽可能多 API 调用方式"的回归测试脚本。这也是它被用作案例1的理由——它是官方认证的"教学范式"，几乎把第四部分列出的基础设施用了个遍。
- **为什么这样设计**：正因为要兼顾"教程"和"测试覆盖率"，代码里能看到很多"同一件事演示两种写法"的注释（比如 `ProtoField.new(...)` vs `ProtoField.uint16(...)`），这在真实生产插件里是不必要的冗余，但对学习者是很好的对照素材。

## 6.2 文件结构

单文件脚本，按顶层代码的先后顺序划分为几个逻辑段（行号对应 [code/dns_dissector.lua](code/dns_dissector.lua)）：

```
1-51     文件头注释：背景、使用说明
53-124   调试基础设施：debug_level 表、命令行参数覆盖、dprint/dprint2
126-137  版本检查（防止在太老的 Wireshark 上跑，因为用到了 1.11.3 才有的 API）
139-196  Proto 声明 + 一大批 ProtoField 声明（含位域字段）
198-206  dns.fields = {...}   真正把字段注册进协议
208-233  ProtoExpert 声明 + dns.experts = {...} 注册
235-261  Field 提取器声明 + isResponse() 辅助函数
263-311  Preferences 声明 + dns.prefs_changed()
320-333  常量 + 前向声明
332-502  dns.dissector() 主体（核心解析逻辑）
504-512  把协议挂到 udp.port 65333
514-624  heur_dissect_dns() 启发式函数 + register_heuristic 三种写法
626-678  getQueryName() 辅助函数（DNS 标签解析，递归风格的 Length-Prefixed Parser）
```

## 6.3 初始化流程

入口就是文件本身——没有额外的 `require`或模块拆分。加载时执行顺序（对照 [03-lifecycle.md](03-lifecycle.md) 的"顶层代码"阶段）：

```
1. 建 debug 基础设施（不涉及 Wireshark API）
2. 版本检查（assert ProtoExpert.new 存在）
3. local dns = Proto("mydns","MyDNS Protocol")      -- 拿到身份 (第149行)
4. 声明所有 ProtoField                                -- 141-196行
5. dns.fields = {...}                                 -- 注册字段 (第206行)
6. 声明 ProtoExpert，dns.experts = {...}              -- 注册专家信息 (第233行)
7. Field.new(...) 创建4个提取器                        -- 235-243行（必须在第5步之后）
8. dns.prefs.debug/port/heur = Pref.xxx(...)          -- 声明配置项 (281-288行)
9. function dns.prefs_changed() ... end               -- 声明配置变更回调 (第292行)
10. function dns.dissector(tvbuf,pktinfo,root) ... end -- 声明主解析函数 (第342行)
11. DissectorTable.get("udp.port"):add(65333, dns)    -- 注册到 UDP 端口 (第512行)
12. local function heur_dissect_dns(...) ... end       -- 声明启发式函数 (第529行)
13. dns:register_heuristic("udp", heur_dissect_dns)   -- 注册进启发式候选队列 (第613行前后)
14. getQueryName = function(...) ... end               -- 补上第313行 forward-declare 的实现
```

这个顺序完美验证第二部分 2.5 的依赖图：`Proto` -> `ProtoField` -> `fields=` -> `Field 提取器` -> `Preferences` -> `dissector` -> `DissectorTable:add` -> `register_heuristic`，一步都不能乱。

## 6.4 Runtime：Packet 到来以后发生什么

`dns.dissector(tvbuf,pktinfo,root)`（第342行起）内部的执行路径：

```
1. pktinfo.cols.protocol:set("MYDNS")                       写 Protocol 列
2. pktlen = tvbuf:reported_length_remaining()                取声明长度（不是已捕获长度）
3. 长度不足 DNS_HDR_LEN(12) -> tree:add_proto_expert_info(ef_too_short); return
4. tree = root:add(dns, tvbuf:range(0,pktlen))               建立本协议子树
5. tree:add(pf_trasaction_id, tvbuf:range(0,2))               解析 Transaction ID
6. pktinfo.cols.info:set("("..transid..")")                  写 Info 列
7. flag_tree = tree:add(pf_flags, flagrange)                  建 flags 子树
     -> 逐个位域字段 tree:add(pf_flag_xxx, flagrange)，位置用 ProtoField 里的 mask 参数定位
     -> isResponse() 分支决定走 Query 还是 Response 的字段组合
     -> query_flag_tree:add_proto_expert_info(ef_response/ef_query, ...)  触发专家信息
8. 继续解析 num_questions/answers/authority/additional 四个计数字段
9. num_queries = questions_field()()                          用 Field 提取器读"刚才自己写的"字段值
10. while 循环：调用 getQueryName() 解析每个 Query（DNS 标签格式，Length-Prefixed 递归解析）
      -> 畸形则 queries_tree:add_proto_expert_info(ef_bad_query); return
      -> 每解析完一个 Query 就 pktinfo.cols.info:append(" "..name)
11. return pos                                                告诉框架消费了多少字节
```

如果这是**第一次**收到一个 UDP 65333 端口的包，走的是"DissectorTable 直连"路径（步骤见4.4）；如果走的是启发式路径（协议没配到固定端口，靠猜），则会先进入 `heur_dissect_dns`（第529行），做完合法性检查后**直接调用 `dns.dissector(tvbuf,pktinfo,root)`**（第587行），然后按 `conv_regmode` 选择一种方式把 conversation 绑定到 `dns`（第594-607行，三种写法对应 API 的三代演进），让同一会话后续的包跳过启发式，直接走 DissectorTable 那样的直连路径。

## 6.5 使用了哪些基础设施

```
Proto              有（mydns）
ProtoField          有，且覆盖了普通字段/位域字段/值映射表三种写法
ProtoExpert         有，5个：3个信息性 + 2个错误性
DissectorTable      有，挂到 udp.port
Preferences         有，3个：debug(enum) / port(uint) / heur(bool)
prefs_changed()     有，同步端口变更到 DissectorTable
Conversation        有，演示了3种历史写法（新旧API对比）
Heuristic Dissector 有，且演示了3种注册写法（普通函数 / 直接用 dissector / 匿名包装函数）
Field 提取器         有，4个（读自己刚解析的字段）
Tap/Listener        无
PostDissector       无
GUI                 无
```

## 6.6 哪些地方值得学习

1. **调试基础设施先行**：`debug_level` 表 + `dprint`/`dprint2` + 命令行参数覆盖，这一整套在文件最开头，跟协议解析逻辑完全无关，但极大提升了脚本的可维护性——真实项目里这一段几乎可以原样复制到任何新插件里。
2. **版本检查放在最前面**：`assert(ProtoExpert.new, "...")`，用一个"确定新特性存在"的断言代替裸调用后崩溃，报错信息直接给出解决办法（升级到 1.11.3+）。
3. **新旧 API 对比写在注释里而不是删除**：`add_expert_info` vs `add_proto_expert_info`，`pktinfo.conversation = dns` vs `Conversation.find_from_pinfo(pktinfo).dissector = dns`——这种"保留历史写法作对比"的注释风格，对后来人理解"这个 API 为什么长这样"极有帮助。
4. **`return pos` 严格对应"消费了多少字节"**：整个 dissector 从头到尾都在维护一个准确的 `pos`/`pktlen_remaining`，从不"假装"消费了整个 buffer。
5. **启发式函数里"先做最便宜的检查"**：先查长度，再查固定取值范围的位域（opcode/rcode），最后才查"数量看起来合理不合理"，层层过滤，尽量避免误判并节省 CPU。

## 6.7 一句话总结

这是一份"故意把所有能演示的 API 都演示一遍"的参考实现——真实项目不会这么"贪心"，但正因为如此，它是理解**单文件 Lua dissector 的完整能力边界**的最好起点。
