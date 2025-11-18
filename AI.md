[OpenAI Cookbook](https://cookbook.openai.com/)  
[Prompt Engineering Guide](https://www.promptingguide.ai/)  
[Prompt Engineering Guide](https://learnprompting.org/docs/introduction)  
[Five proven prompt engineering techniques](https://www.lennysnewsletter.com/p/five-proven-prompt-engineering-techniques)  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  
[gemini-cli](https://github.com/google-gemini/gemini-cli)  
[Gemini CLI documentation](https://geminicli.com/docs/)  


## Coding Platform
[Cursor](https://cursor.com/home)  
[Claude](https://claude.ai/)  
[Qoder](https://qoder.com/)  

### Cursor
[How I use Cursor](https://www.builder.io/blog/cursor-tips)  
[cursor.directory](https://cursor.directory/)  
[Rules](https://docs.cursor.com/en/context/rules)  
[]()  
[]()  
```shortcuts
Settings
    Ctrl + Shift + j
```

## Online
[ChatGPT](https://chatgpt.com/)  
[Grok](https://grok.com/)  
[KiMi](https://kimi.moonshot.cn/)  
[Gmini](https://gemini.google.com/app)  
[DeepSeek](https://chat.deepseek.com/)  
[hix.ai/](https://hix.ai/)  
[Ithy](https://ithy.com/)  
[字节豆包](https://www.doubao.com/chat/)  
[腾讯元宝](https://yuanbao.tencent.com/chat/)  
[阿里通义](https://tongyi.aliyun.com/qianwen/)  




[Learn The Art of Prompting](https://www.learnprompt.org/)  
[A Comprehensive Guide to Using ChatGPT Prompts for Coding Tasks](https://www.learnprompt.org/chat-gpt-prompts-for-coding/)  
[Unleashing ChatGPT for Programmers](https://www.learnprompt.org/chatgpt-prompts-for-programmers/)  
[Prompts for Code Generation, Debugging, Optimization, and More](https://www.learnprompt.org/chatgpt-prompts-for-developers/)  
[My Top 17 ChatGPT Prompts for Coding](https://www.fullstackfoundations.com/blog/chatgpt-prompts-for-coding#chatgpt-prompts-for-learning-to-code)  
[DEV ChatGPT Prompts](https://github.com/PickleBoxer/dev-chatgpt-prompts)  
[awesome-chatgpt-prompts](https://github.com/f/awesome-chatgpt-prompts)  
[280+ ChatGPT Prompts & How to Write Your Own](https://writesonic.com/blog/chatgpt-prompts)  
[Check These 100 Powerful ChatGPT Prompts For Every Situation](https://growthtribe.io/blog/chatgpt-prompts/)  
[500+ Best Prompts for ChatGPT](https://www.godofprompt.ai/blog/500-best-prompts-for-chatgpt-2024?srsltid=AfmBOorDz97w7PxISB5pLqsCH1hYRaXwd7yqJdi-pBhu8a8UBRZD66mA)  
[]()  
[]()  


[LLM 应用开发实践笔记](https://aitutor.liduos.com/)  
[面向开发者的大模型手册](https://datawhalechina.github.io/llm-cookbook/#/)  

## Prompt
- 核心原则 (5W1H + R)
```
| 原则                          | 含义               | 说明              |
| -------------------------    | ----------         | --------------- |
| **1. What – 主题明确**        | 告诉我你想要什么    | 模糊的问题 → 模糊的答案   |
| **2. Why – 目的清晰**         | 让我知道用途或目标  | 我能据此调整深度与角度     |
| **3. Who – 目标受众**         | 面向谁解释          | 不同受众→不同语言和细节    |
| **4. How – 风格/形式**        | 希望输出什么形式    | 列表？总结？教学？代码？报告？ |
| **5. Scope – 范围限定**       | 要多深、多长、多专业 | 控制回答粒度和复杂度      |
| **6. Restriction – 约束条件** | 指定语气、风格、格式 | 避免啰嗦或跑题         |
```

- 结构化提问模板
```
[角色设定] 你现在是...
[任务目标] 我希望你...
[内容范围] 请解释/分析/生成关于...
[输出形式] 输出应包括...
[风格要求] 风格要...
[限制条件] 不要... / 避免...

好 Prompt = 角色 + 任务 + 目标 + 范围 + 格式 + 风格

例子:
你是一名资深软件工程师。
请用教学风格解释“操作系统内核的线程调度机制”，
面向大学计算机系学生。
输出包含：调度原理、算法对比、优缺点分析。
使用Markdown小标题和图示结构。
字数控制在800字以内。
```

- 示例
```
普通提问
    什么是区块链？

优化提问（结构化版本）
    你现在是一名区块链领域的技术讲师。
    请以系统化、分层结构的方式讲解“什么是区块链”，
    目标读者是有编程基础的工程师。
    输出包含：①定义 ②数据结构 ③共识机制 ④安全原理 ⑤应用场景。
    每部分使用小标题和简要示例。
    风格要技术向、逻辑清晰、无营销口吻。
```

- 不同场景下的 Prompt 示例
```
| 目标               | 优化提问模板                                              |
| --------------     | --------------------------------------------------- |
| 📘 **学习概念**    | “请像我是一名新手程序员一样解释[概念]，用类比和例子说明原理，最后总结3个关键点。”         |
| 💻 **代码讲解**    | “请逐行解释下面的C代码，说明每一行的作用和背后的语言机制。”                     |
| 🧪 **调试或错误分析** | “以下是我在编译C程序时的错误信息。请分析原因并给出解决步骤。假设我的环境是Linux + gcc。” |
| 📑 **总结报告**    | “请把下面内容总结为技术报告，包含摘要、问题分析、解决方案、结论四部分。”               |
| 💬 **写作改进**    | “请帮我改进下面的段落，使其更简洁专业，保持原意，风格偏技术文档。”                  |

```

- 进阶技巧
```
角色设定法（Role prompting）
    “你是一名资深C语言编译器工程师”

分步推理法（Step-by-step prompting）
    “请先解释基本原理，再分析实现，再总结优缺点。”

迭代优化法（Refinement prompting）
    第一次提问后，说“请简化成要点版”或“请补充代码实例”

结构控制法
明确输出格式，例如：
用markdown格式输出，包含：
1. 定义
2. 原理
3. 示例
4. 总结


避免歧义词
避免“讲详细一点”“帮我优化”这种模糊表达，
改为“请在解释中增加底层原理与代码实现示例”。
```

- 技术学习类好提示词的5大原则 (LAYER模型)
```
| 原则                     | 含义             | 示例                   |
| ----------------------   | ---------       | -------------------- |
| **L — Level (层次)**     | 指定讲解深度     | “假设我已经懂C语言，但不了解内核原理” |
| **A — Audience (受众)**  | 告诉我你是谁     | “面向有编程经验的工程师”        |
| **Y — Yield (产出)**     | 明确输出形式     | “请输出结构化讲解，包含示例和图解”   |
| **E — Explain (讲解方式)** | 控制讲解风格    | “请像在课堂上教学一样分步骤讲解”    |
| **R — Refine (优化)**    | 给出约束与改进要求 | “语言简洁，使用类比，不超过800字”  |
```

- 模板
```
你是一名[技术领域]的资深工程师兼讲师。
请系统化讲解[技术主题]。
假设读者是[目标受众]，已有[相关知识背景]。
讲解内容应包含：
1. 基本概念和定义
2. 内部工作原理（分步骤说明）
3. 常见应用或示例
4. 可能的陷阱或误区
5. 简短总结（3–5行）

输出要求：
- 使用 Markdown 格式，带标题与小节
- 语言清晰、结构分明
- 既有理论又有实践角度


示例1：学习网络协议
你是一名计算机网络讲师。
请系统讲解 TCP三次握手的全过程，假设我已经了解IP层的概念。
输出应包含：
1. 三次握手每一步的报文内容和目的
2. 为什么是三次而不是两次或四次
3.报文丢失或延迟的处理机制
最后附上一段总结。
要求：用 Markdown 格式输出、图文结合、清晰易懂。

示例2：学习编译器原理
你是一名编译器专家。
请以教学方式讲解 C语言编译过程的4个阶段（预处理、编译、汇编、链接）。
假设我有C语言基础但不了解编译器内部结构。
请说明：
    - 每个阶段的输入与输出文件
    - 主要做了哪些转换
    - 常见的编译错误示例
    - 用gcc -E/-S/-c举例说明
风格要求：清晰、逻辑性强、有表格。

✅ 示例3：学习Rust特性
你是一名Rust语言讲师。
请讲解 Rust的所有权与借用机制。
假设我懂C++指针，但刚开始学Rust。
请用对比的方式解释：
    1. 所有权的三条核心规则
    2. 借用与引用的区别
    3. 生命周期的含义
最后给出一段Rust代码示例并逐行解释。

✅ 示例4：学习系统编程
你是一名Linux系统工程师。
请解释 fork() 系统调用的工作原理。
包括：
    - 内核如何复制进程
    - 父子进程的区别
    - fork() 返回值的意义
    - 使用示例代码与输出说明
语言要求：简洁、带代码注释。

- 强化输出的附加技巧
| 目标         | 提示技巧                   |
| -------      | ---------------------- |
| 想学得系统    | “请以课程形式分章节讲解”          |
| 想快速理解    | “请用类比和比喻解释”            |
| 想看底层机制  | “请深入到实现细节和系统调用层面”      |
| 想结合代码    | “请附上简短C/Rust代码示例并逐行解释” |
| 想打印总结笔记 | “请输出要点总结表格，方便复习”       |


- 技术学习类好 Prompt 的公式
角色 + 目标主题 + 背景 + 内容结构 + 输出格式 + 风格要求

示例:
你是一名操作系统讲师。
请以系统化、教学方式讲解“Linux 进程调度机制”。
假设我熟悉C语言但不了解内核。
输出包括：调度策略、CFS算法、时间片、上下文切换。
用Markdown格式输出，带小标题与表格说明。

```

- 代码调试类高质量提问的核心原则 (DEBUG 模型)
```
| 原则                    | 含义         | 要点               |
| ---------------------   | ------      | ---------------- |
| **D — Describe**        | 明确描述问题 | 出现什么错误？在什么情况下？   |
| **E — Environment**     | 指出环境     | 操作系统、编译器、语言版本等   |
| **B — Background**      | 提供上下文   | 代码目的、你想实现的功能     |
| **U — Understanding**   | 表达你的理解 | 你认为问题可能的原因（哪怕猜测） |
| **G — Goal**            | 明确目标     | 想要什么样的结果或输出      |

- 通用模板
我在调试一段 [编程语言/框架] 代码时遇到问题。

### 🧩 背景
- 功能目标：我想实现 [说明代码功能或逻辑]
- 运行环境：操作系统 [如 Debian 12]，编译器/解释器版本 [如 gcc 13.2 / Python 3.11]
- 第三方依赖或库（如有）

### 💻 出错代码
```[语言名]
(请粘贴出错代码片段，足以重现问题)

⚠️ 错误信息
(完整粘贴错误日志、编译输出、堆栈信息等)

🧠 我的理解
(可选：你认为问题出在哪？尝试过哪些修复办法？)

🎯 我的期望
请帮我分析：
问题产生的原因
如何修复
是否有更好的写法/最佳实践


## 🧩 六、调试类提问的附加技巧

| 目标        | 提示技巧 |
|------       |-----------|
| 想要详细分析 | “请逐行解释代码行为，并说明哪一行可能出错” |
| 想学调试方法 | “请告诉我如何用 gdb / strace / lldb 来定位这个问题” |
| 想要重构建议 | “请提供更安全或更简洁的实现方式” |
| 想做性能优化 | “请帮我分析这段代码可能的性能瓶颈” |
| 需要跨语言解释 | “请比较这段C代码与Rust中等价写法的内存行为差异” |

---

## 🧾 七、总结：调试类高效 Prompt 黄金公式

> 🧠 **背景 + 环境 + 代码 + 错误 + 理解 + 期望**

示例：
我在 Linux 上用 gcc 编译一个动态库时出错。

**背景：** 想写一个 collectd 插件  
**命令：** `gcc -fPIC -shared -o plugin.so plugin.c -I/usr/include/collectd`  
**错误：** `fatal error: plugin.h: No such file or directory`  
**环境：** Debian 12, gcc 13.2  
**我的理解：** 可能是 include 路径不对。  
**请帮我分析：** 头文件路径在哪里？正确编译参数是什么？collectd 插件一般怎么编译？

```