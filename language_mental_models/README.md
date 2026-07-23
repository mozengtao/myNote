# 语言心智模型文档集（Language Mental Models）

> 本文件夹是 [unified_mental_model.md](../unified_mental_model.md) 的延伸阅读。
> 总览文档回答"每种语言的核心驱动力是什么"，这里的每一篇文档则通过
> **至少 10 个典型代码片段**，把这个抽象的"驱动力"落到具体、可运行的代码上，
> 帮助你在真正开始写代码之前，先建立正确的心智模型。

## 如何使用这套文档

1. 先读 [unified_mental_model.md](../unified_mental_model.md) 的总览表，了解 10 种语言各自的"第一性原理"。
2. 挑一门你要上手的语言，打开对应文档。
3. 不要跳着看代码片段——按顺序读，每个片段都在为下一个片段搭台阶。
4. 读完"典型代码片段"后，务必看一下"常见误区对比"，那是新手最容易踩的坑。
5. 用文末的"快速上手 Checklist"自查：如果每一条你都能在脑子里回答出来，说明心智模型已经建立。

## 文档列表

| 语言 | 核心驱动力 | 文档 |
|------|-----------|------|
| Shell | 数据流（Data Flow） | [shell.md](shell.md) |
| Python | 协议（Protocol） | [python.md](python.md) |
| Go | 组合（Composition） | [go.md](go.md) |
| Rust | 所有权（Ownership） | [rust.md](rust.md) |
| C | 内存（Memory） | [c.md](c.md) |
| C++ | 生命周期（Lifetime） | [cpp.md](cpp.md) |
| Java | 职责（Responsibility） | [java.md](java.md) |
| JavaScript | 事件（Event） | [javascript.md](javascript.md) |
| Haskell | 变换（Transformation） | [haskell.md](haskell.md) |
| SQL | 声明（Declaration） | [sql.md](sql.md) |

## 每篇文档的统一结构

- **核心驱动力**：一句话点题
- **心智模型图解**：一张 ASCII 图，描述这门语言思考问题的路径
- **核心驱动力详解**：这门语言最希望你关注什么
- **典型代码片段（≥10）**：场景 → 代码 → 心智模型解读
- **黄金法则**：不要想 X，而要想 Y
- **常见误区对比**：错误心智模型 vs 该语言习惯写法
- **快速上手 Checklist**：自查清单
