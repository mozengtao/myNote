# 10 · Pattern 选择决策树

> 前置阅读：[09-cross-pattern-comparison.md](09-cross-pattern-comparison.md)。本文把前面的分析压缩成一个可以在写下第一行代码之前直接使用的决策流程。**决策树的对象是"一个文件/一个 Module"，不是"整个项目"**——一个项目里的不同文件会分别走到树的不同分支，这是正常现象，见第 3 节。

## 1. 决策树

```text
我正在设计一个 Lua Module
          |
          v
只是提供一组相关 API？
          |
        Yes
          v
    Table Module
          |
          No
          v
需要隐藏内部实现？
          |
        Yes
          v
Private + Public
          |
          No
          v
需要创建多个独立对象？
          |
        Yes
          v
Class / Constructor
          |
          No
          v
需要隐藏具体实现选择？
          |
        Yes
          v
      Factory
          |
          No
          v
需要允许 Framework / 第三方扩展？
          |
        Yes
          v
      Plugin
```

## 2. 每个判断点的具体检验方法

决策树的问题本身比较抽象，容易被"感觉上都符合"卡住。下面给每个问题一个可以立刻检验的具体标准：

- **"只是提供一组相关 API？"** —— 检验方法：这个 Module 会不会创建多个"实例"？会不会有可变的、跨调用共享的数据？如果两个答案都是"不会"，就是 Table Module。
- **"需要隐藏内部实现？"** —— 检验方法：这个 Module 内部有没有"分成好几步才能完成一次公开操作"的逻辑（tokenize -> parse_value -> parse_object 才能拼出一次 `parse()`）？如果有，且这些步骤本身不需要被外部单独调用，就该用 `local function` 关起来，进入 Private + Public。
- **"需要创建多个独立对象？"** —— 检验方法：如果同时创建两个"东西"（两个连接、两个 session、两个 device），它们的数据是否必须互相独立？如果是，就是 Class / Constructor；如果这两个"东西"其实应该共享同一份数据，说明它们不该是两个对象，可能本来就该是同一个 Module State。
- **"需要隐藏具体实现选择？"** —— 检验方法：调用者传进来的是一个**运行时才知道**的字符串/配置（`"tcp"`/`"sqlite"`/环境变量），而不是在写代码的时候就能写死 `require("tcp_client")`？如果是，说明存在"选择"这个决策，就该用 Factory 把选择逻辑收拢起来。
- **"需要允许 Framework / 第三方扩展？"** —— 检验方法：这个能力的实现者，在写 Framework 代码的当下，数量和名字是否**未知**（以后随便谁都可能新增一个）？如果 Framework 自己就能穷举所有实现（数量固定、名字在编译期已知），Factory 已经够用，不需要 Plugin 额外的 Discovery/Register 机制；如果做不到穷举，就该用 Plugin。

## 3. 决策树的正确用法：逐文件使用，不是逐项目使用

一个常见误用是"看完决策树，给整个项目选一个 Pattern"。正确用法是**每写一个新文件，就单独走一遍这棵树**——同一个项目里，`constants.lua` 走到 Table Module 分支，`connection.lua` 走到 Class 分支，`framework.lua` 走到 Plugin 分支，这是完全正常、甚至是**期望**的结果（详见 [08-pattern-composition-and-real-world-cases.md](08-pattern-composition-and-real-world-cases.md) 的三个真实案例）。

## 4. 实际项目中的组合关系

多个 Pattern 组合使用是常态，一个典型的组合层次是：

```text
Plugin
  |
  +-- 内部使用 Factory 决定这次创建哪个具体对象
        |
        +-- Factory 创建出来的是 Class
              |
              +-- Class 的方法内部调用 Private + Public 模块隐藏实现步骤
```

这不是"把 4 个 Pattern 都塞进同一个文件"，而是**沿着一次调用链，从外到内，每一层用它自己最合适的形状**：Plugin 是这条链最外层的入口形状（响应 Framework 事件），Factory 是这条链中间的一个决策点（选哪个具体类型），Class 是这条链里承载状态的实体，Private + Public 是这条链最内层的实现隔离。反过来问"这一层能不能省掉"往往就能推出该不该组合：如果 Plugin 内部创建的对象种类固定死只有一种，可以省掉 Factory 直接 `.new()`；如果这个对象不需要维护跨方法调用的状态，可以省掉 Class 直接用 Private+Public 模块。

## 5. 一个易错的边界：Table Module vs Private + Public

决策树里第一和第二个问题最容易混淆，给一个额外的判断标准：

```text
问自己："如果调用者绕过我提供的公开函数，直接访问了某个内部字段/函数，会发生什么？"

Table Module 的正确答案应该是："没什么，因为我压根没打算隐藏任何东西，
                              所有字段本来就是公开数据/工具函数的一部分。"

Private + Public 的正确答案应该是："那是不允许的，那些是我的实现细节，
                                  绕过公开函数访问它们，将来我换实现就会破坏你的代码。"
```

如果一个 Table Module 写着写着,发现自己开始用第二种语气思考"这个字段不能让外面碰",这就是决策树该往下走一层、进入 Private + Public 的信号。

下一篇 [11-final-mental-model.md](11-final-mental-model.md) 给出整套笔记最终的统一图、五句话记忆，以及从代码反推架构职责的 Code Reading Checklist。
