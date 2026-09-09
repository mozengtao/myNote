# 第七部分：通用开发流程

## 7.1 从三个案例反推出的步骤

把 [06](06-case-study-dns-dissector.md)/[07](07-case-study-postdissector.md)/[08](08-case-study-rls-dissector.md) 三个案例的"初始化流程"小节叠在一起看，会发现尽管三者用途完全不同（教学/后处理/生产），**顶层代码的先后顺序高度收敛到同一条链路**：

```
1. 建 Proto
     |
     v
2. 定义 Field (ProtoField)，赋值给 proto.fields
     |
     v
3. 定义 Preferences (proto.prefs.xxx = Pref.xxx(...))
     |
     v
4. 获取已有 Dissector / DissectorTable（如果要挂靠别的协议，或要转发给别的协议）
     |
     v
5. 实现 dissector() 主体
     |
     v
6. 调用已有 Subdissector（如果协议内部还封装了别的协议）
     |
     v
7. 注册 Port / 挂进 DissectorTable（或 register_postdissector / register_heuristic）
     |
     v
8. 测试（用真实/构造的 pcap，Decode As 验证、tshark -X lua_script 跑通）
     |
     v
9. 优化（性能：减少不必要的字段生成；正确性：补齐边界检查）
     |
     v
10. 增加 Expert（畸形包/异常情况需要可过滤地展示出来）
     |
     v
11. 增加 Preferences 的 prefs_changed()（如果第3步定义了 Preferences）
     |
     v
12. 增加 Conversation Cache（如果协议有跨包状态）
     |
     v
13. 完成
```

## 7.2 三个案例分别停在了这条链路的哪一步

```
案例2 (PostDissector)：
   1 -> 2(可选) -> 5 -> 7(register_postdissector)  就结束了
   完全不需要 3/4/6/10/11/12——因为 PostDissector 的问题域本身就不涉及
   "端口配置""转发给别人""跨包状态"

案例3 (RLS)：
   1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7 -> (8/9 在仓库的 pcap/ 目录和 CI 配置里能看到痕迹)
   停在这里，没有走到 10/11/12
   -> 对照第六部分的结论：这恰好是"协议特性不需要"，而不是"作者偷懒"

案例1 (dns_dissector)：
   完整走完全部13步，包括10(Expert)/11(prefs_changed)/12(Conversation)
   -> 因为它的目的就是"演示尽可能多的能力"，所以特意把链路走满
```

## 7.3 为什么几乎所有插件都收敛到这个顺序

这不是约定俗成的风格，而是**由第二部分 2.5 的依赖图和第三部分 3.2 的加载规则强制决定的**：

```
+---------------------------------------------------------------+
|  强制依赖关系（做不到就会报错或注册失败）                          |
+---------------------------------------------------------------+
|  ProtoField 的名字前缀依赖 Proto 已存在        =>  1 必须在 2 之前
|  Field 提取器依赖 proto.fields 已赋值过         =>  2 必须在"创建提取器"之前
|  dissector() 函数体里才会用到 ProtoField/Pref   =>  2、3 必须在 5 之前
|  DissectorTable:add(key, proto) 需要 proto 已有 dissector 才有意义
|                                                =>  5 通常在 7 之前（虽然技术上
|                                                    Lua 允许先 add 再定义 dissector，
|                                                    但逻辑上后定义会让人困惑）
|  Expert Info 触发依赖 tree:add_proto_expert_info，写在 dissector() 里
|                                                =>  10 依赖 5 已经存在
|  prefs_changed() 里通常要重新调整 DissectorTable
|                                                =>  11 依赖 3 和 7 都已存在
+---------------------------------------------------------------+
```

反过来说，**10/11/12 这三步（Expert/prefs_changed/Conversation Cache）之所以总是排在最后，是因为它们是"增量能力"**：去掉它们，协议仍然能被正确识别和展示，只是少了"异常提示""动态生效""跨包记忆"这些锦上添花的能力。这是一种自然的"最小可用版本(MVP) -> 逐步增强"的演进顺序，跟软件工程里"先让核心路径work，再补边界情况"的一般智慧完全一致——只是在 Lua Plugin 这个具体领域里，恰好被这套 API 的依赖关系"物理性地"强制了下来。

## 7.4 一份可复用的起步 Checklist

```
[ ] Proto("<abbr>", "<description>")，abbr 全小写、不与已有协议冲突
[ ] 列出协议头部的字段清单，先只做"必要字段"，不要一次性照抄协议规范里的每一个 bit
[ ] proto.fields = {...}，命名统一用 "<abbr>.xxx" 前缀
[ ] 决定"怎么被触发"：固定端口(DissectorTable:add) / 猜(Heuristic) / 后处理(PostDissector)
[ ] dissector() 里先写"长度不足就返回"的防御检查，再写正常解析路径
[ ] 用官方测试脚本 test/lua/*.lua 或自己的 pcap 跑一遍，确认 Decode As 能选到你的协议
[ ] 补 Preferences（哪怕只有一个端口号），并配 prefs_changed()
[ ] 补 Expert Info：先想清楚"什么情况算畸形/异常"，再决定 severity
[ ] 如果协议有跨包状态：设计 Conversation Cache 的 key 和 init() 清空逻辑
[ ] 如果协议内部还封装别的已知协议：用 Dissector.get(...):call(...) 转发，别自己再解析一遍
```

下一部分（[11-design-patterns.md](11-design-patterns.md)）把"5. 实现 dissector() 主体"这一步内部，反复出现的代码结构总结成命名清晰的设计模式。
