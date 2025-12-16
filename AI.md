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
[NoteBookLM](https://notebooklm.google.com/)  
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
![ChatGPT Prompts](./assets/ChatGPT_Prompts.pdf)  
[]()  
```
ConfD 配置事务 (commit)
        │
        ▼
┌─────────────────────────────────────────┐
│  Work 结构体 (工作列表)                  │
│  ┌─────────┐                            │
│  │ w->list │──► Witem ──► Witem ──► nil │
│  └─────────┘    (变更1)   (变更2)        │
└─────────────────────────────────────────┘

ASCII Art Diagram（ASCII 艺术图）或 Text-based Diagram（纯文本图表）

名称	                            说明
ASCII Flow Chart	                ASCII 流程图 - 展示流程和步骤
ASCII Box Diagram	                ASCII 方框图 - 用方框表示组件
ASCII Data Structure Diagram	    ASCII 数据结构图 - 展示链表、树等结构
ASCII Architecture Diagram	        ASCII 架构图 - 展示系统组件关系

ASCII Art 中文对齐问题：
推荐做法
    对于技术文档，最简单的方案是：
    1. 纯英文 绘制 ASCII 图
    2. 图下方 用普通文字添加中文说明

示例:
┌─────────────────────────────────────────┐
│  Work Structure (worklist)              │
│  ┌─────────┐                            │
│  │ w->list │──► Witem ──► Witem ──► nil │
│  └─────────┘    (item1)   (item2)       │
└─────────────────────────────────────────┘

说明：Work 是工作结构体，包含一个 Witem 链表，
每个 Witem 代表一个配置变更项。
```
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
### TIPS
Socratic Questioning: ->"Act as a Socratic tutor and help me understand the concept of [topic]. Ask me questions to guide my understanding."
苏格拉底式提问：->"请扮演苏格拉底式的导师，通过提问引导我理解[主题]概念。"

Multi-Level Explanations: ->"Explain [concept] to me as if I’m a child, then a high schooler, and finally an academic."
多层级解释：->"请用小学生、中学生和学者能理解的不同表述方式，为我解释[概念]。"

Practice Questions: ->"Create practice questions for me on [topic] at beginner, intermediate, and advanced levels."
分级练习题：->"请围绕[主题]设计初级、中级和高级三个难度的练习题。"

Summarizing and Comparing: ->"Summarize this paper and list the key concepts. Then compare it to my summary and identify any gaps."
摘要对比分析：->"请总结这篇论文的核心概念，并与我的总结对比，指出遗漏之处。"

Key Terms and Categories: ->"Give me a list of 20 key terms in this paper and break them into five categories."
术语分类归纳：->"请列出本文的20个关键术语，并将其归纳为五个类别。"

Bloom’s Taxonomy Challenges: ->"Create a set of challenges for me to apply Bloom’s taxonomy (remember, understand, apply, analyze, evaluate, create) to [topic]." Analogies and 
布鲁姆分类法实践：->"请根据布鲁姆分类法（记忆、理解、应用、分析、评估、创造）设计一套关于[主题]的实践挑战。"

Real-Life Examples: ->"Provide analogies and real-life examples to help me understand [concept]."
类比迁移教学：->"请通过类比和生活实例帮助我理解[概念]。"


示例:
请用苏格拉底式提问，通过提问引导我理解linux的启动过程，提问过程中的每次对话记录上一次的上下文

- 示例
```
如何高效的利用cursor学习linux 3.2内核源码,给出具体的学习建议和步骤，例如先从哪个简单的模块入手比较好

分析arch/x86/kernel/syscall_table_32.S中的系统调用表结构，列出前10个系统调用的名称和对应的处理函数

跟踪sys_getpid系统调用的完整执行路径：
1. 用户态调用入口
2. 陷入内核的机制
3. 实际处理函数
4. 返回用户态的过程

解释task_struct结构体中的主要字段含义

分析进程调度器如何选择下一个运行进程

画出Linux 3.2的内存管理架构图

分析伙伴系统的工作原理

解释VFS层的四个主要对象关系

提问模板："分析[文件名]中的[函数名]函数：
1. 函数的主要功能
2. 输入参数说明
3. 返回值含义
4. 调用关系图"

提问模板："在[模块名]中，为我创建5个关键数据结构的记忆卡片（字段名+说明）"

如果我要在Linux 3.2中添加一个简单的系统调用，
需要修改哪些文件？给出具体代码示例

# 1. 生成调用图
"为__schedule函数生成函数调用关系图"

# 2. 对比分析
"比较Linux 3.2与最新内核在内存管理方面的主要差异"

# 3. 漏洞分析
"分析CVE-2012-0056漏洞的成因和修复方案"

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

- Ultimate Learning Prompt for Any Topic
```
I want to learn the topic: <TOPIC>.
Please act as my expert instructor and provide ALL of the following:

High-level overview
    What the topic is
    Why it matters
    Where it is used in real systems

Core concepts explained simply
    Define the essential terms
    Give short analogies for each concept
    Provide visual ASCII diagrams when helpful

Progressive deep dive
    Structure the learning in levels:
        Level 1: Beginner explanation
        Level 2: Intermediate technical concepts
        Level 3: Expert-level architecture & internal mechanisms
        Level 4: Real-world engineering considerations (performance, design, pitfalls)

Hands-on examples
    Provide practical examples relevant to <TOPIC>, such as:
        Code examples (C / Rust / Go / Python depending on context)
        Command-line examples
        Minimal complete working examples (MCWE)

Mini-projects / exercises
    Give me exercises at three difficulty levels:
        Easy
        Medium
        Hard
        Include expected outputs or hints.

Common mistakes & misunderstandings
Explain the top mistakes learners make
Show correct vs incorrect examples

Links between concepts
    Explain how this topic connects to:
        Operating systems
        Networking
        Memory management
        Distributed systems
        Security
        (Choose relevant ones depending on <TOPIC>)

Interview-style questions
    Provide 10 conceptual questions + 10 deep technical questions + answers.

Learning roadmap
    Tell me what to study next after this topic.
    Include recommended order and estimated time.

Optional: generate visual summaries
    Such as:
        ASCII architecture diagrams
        Flowcharts
        Tables summarizing differences or pros/cons

Before starting, ask me 3 clarifying questions to tailor the material to my background and goals.
```

- 用于生成高质量 Anki 卡片的专家级提示词
```
I want to learn the topic: <TOPIC>.

Act as my expert instructor and generate high-quality Anki-style flashcards that are accurate, clear, and optimized for long-term technical retention.

Please create the cards with ALL of the following characteristics:

1. **Card Format**
   - Use Q: (front) and A: (back)
   - One concept per card
   - Avoid overly long answers
   - Prefer clear definitions, contrasts, examples, diagrams

2. **Card Types (mix them)**
   - Concept cards (definitions)
   - Reverse cards (A→Q)
   - Cloze deletion cards
   - Understanding tests ("why", "what if", "compare")
   - Code interpretation cards (for C/Python when appropriate)
   - ASCII diagram cards (for system / kernel / network topics)

3. **Content Requirements**
   - Start with core fundamentals of <TOPIC>
   - Add deeper internal mechanisms (since I am intermediate level)
   - Include real examples whenever possible
   - For networking/kernel topics, emphasize:
        - call paths
        - state machines
        - data structures
        - key APIs
   - Include common misconceptions and contrast them

4. **Difficulty Levels**
   - Tag each card as: [Basic], [Intermediate], or [Advanced]

5. **Number of Cards**
   - Generate 30 cards initially
   - Ensure they progressively increase in complexity

6. **Output Format**
   - Provide clean plain-text output.
   - No numbering; each card separated by a blank line.

Before generating the cards, ask me 2–3 clarifying questions to tailor the difficulty, language, and coding examples.

```

- prompt 1
```
walk through a real Linux NIC driver, end-to-end, at code level, but still readable.use virtio_net as the main example(considering RX/TX rings, DMA, NAPI, SKB, etc.) 如果需要的，可以用如下的方式进行展示
1.纯英文 绘制 ASCII 图
2.图下方 用普通文字添加中文说明
结果保存为Markdown格式的文件

Explain SKB(struct sk_buff) internals with diagrams, which should be a driver-level + stack-level explanation, with ASCII diagrams, field breakdowns, and how data flows without copies, header offsets which is critical for protocol stack, SKB lifecycle for tx path and rx path, linear and no-linear SKBs, common SKB helper APIs, how SKB ties to RX/TX rings, etc.如果需要的，可以用如下的方式进行展示
1.纯英文 绘制 ASCII 图
2.图下方 用普通文字添加中文说明
结果保存为Markdown格式的文件

If needed, you can present it in the following way:
1. Pure English ASCII diagram
2. Below the diagram, add Chinese explanations in plain text
Save the result as a Markdown file
```

- Structured Prompt Template (General Technical Topics)
```
I want to learn the topic: <TOPIC>.

Please act as an expert systems engineer and instructor.
Explain this topic in a clear, structured, and progressive way.

Follow this exact structure:

1. High-level overview
   - What the topic is
   - Why it exists (the problem it solves)
   - Where it is used in real systems

2. Architecture overview
   - Describe the overall architecture
   - Identify major components and their responsibilities
   - Show how components interact
   - Include a clean ASCII architecture diagram (aligned, readable)

3. Key components (deep but clear)
   For each major component:
   - Purpose
   - Inputs / outputs
   - Key data structures
   - Important invariants or rules
   - Common variations or implementations

4. Core workflows (step-by-step)
   - Explain the main workflows end-to-end
   - Show control flow and data flow
   - Use ASCII sequence diagrams or flow diagrams where helpful

5. Internal mechanisms (under-the-hood)
   - Explain how it actually works internally
   - Algorithms, state machines, memory layout, or concurrency model
   - Highlight performance and correctness considerations

6. Practical examples
   - Minimal working examples (MCWE)
   - Realistic code snippets (C / C++ / Rust / Go / Python as appropriate)
   - Explain what each part of the code does
   - Show how the example maps back to the architecture

7. Common mistakes and pitfalls
   - Typical misunderstandings
   - Incorrect vs correct approaches
   - Real-world bugs and how to avoid them

8. Mental models and summaries
   - Provide 2–3 strong mental models
   - Summarize key takeaways in bullet points
   - Optional comparison table if alternatives exist

9. Real-world engineering considerations
   - Performance bottlenecks
   - Scalability concerns
   - Debugging and observability
   - Security or reliability implications

10. What to learn next
    - Related topics
    - Suggested learning order
    - Why each next topic matters

Constraints:
- Break explanations into small, digestible sections
- Prefer diagrams over long text when possible
- Use precise technical language but explain it clearly
- Do not skip steps or hand-wave important details

```

```
Explain <TOPIC> as if I am reading production source code.

- Explicitly connect concepts to real implementations
- Show how the abstractions map to actual code paths
- Call out invariants, ownership rules, and lifetimes
- Use ASCII diagrams for memory layout and control flow
- Prefer correctness and clarity over oversimplification

Assume I have an intermediate programming background.

```

- Structured Prompt: Learning Linux Kernel Source Code (v3.2)
```
I want to systematically learn the Linux kernel source code (version 3.2).

Please act as a senior Linux kernel maintainer and systems instructor.
Teach me by reading and explaining the actual source code, not just concepts.

Assume:
- I have intermediate C skills
- I understand OS fundamentals
- I want to learn how the kernel really works internally

Follow this exact structure:

────────────────────────────────────────
1. Subsystem context (big picture)
────────────────────────────────────────
- What kernel subsystem are we studying?
- What problem does it solve?
- Where it sits in the overall kernel architecture
- How this subsystem interacts with others

Include a clean ASCII architecture diagram showing:
- This subsystem
- Adjacent subsystems
- Data/control flow

────────────────────────────────────────
2. Directory & file map (code navigation)
────────────────────────────────────────
- List the main directories and files involved (relative to kernel root)
- Explain the responsibility of each file
- Explain why the code is split this way

Example format:
  kernel/
    sched/
      core.c   → main scheduler logic
      fair.c   → CFS implementation

────────────────────────────────────────
3. Core data structures
────────────────────────────────────────
For each important struct:
- Show the struct definition (simplified if large)
- Explain each field
- Explain ownership and lifetime rules
- Explain how and where it is allocated/freed

Explain invariants that must always hold.

────────────────────────────────────────
4. Entry points & call paths
────────────────────────────────────────
- Identify key entry points (syscalls, interrupts, callbacks)
- Trace the main call paths step-by-step
- Show the function call chain in order

Use ASCII call graphs or sequence diagrams.

────────────────────────────────────────
5. Core workflows (code-driven)
────────────────────────────────────────
Explain major workflows such as:
- Initialization
- Fast path (performance-critical)
- Slow path (exceptional cases)
- Error handling paths

For each workflow:
- Start from the triggering event
- Walk through the exact functions involved
- Explain state changes and side effects

────────────────────────────────────────
6. Important algorithms & mechanisms
────────────────────────────────────────
- Scheduling, locking, memory management, or protocol logic
- Why these algorithms were chosen
- Tradeoffs and limitations (especially in v3.2)

Explain with diagrams where appropriate.

────────────────────────────────────────
7. Concurrency & synchronization
────────────────────────────────────────
- What concurrency model is used?
- What locks are involved?
- Where preemption, interrupts, or RCU are used
- What race conditions the code prevents

Explain what would break if synchronization is wrong.

────────────────────────────────────────
8. Performance considerations
────────────────────────────────────────
- Hot paths vs cold paths
- Cacheline considerations
- Lock contention
- Scalability limits in v3.2

────────────────────────────────────────
9. Common pitfalls & bugs
────────────────────────────────────────
- Typical mistakes kernel developers make here
- Subtle bugs this code avoids
- Historical issues or known limitations in v3.2

────────────────────────────────────────
10. How to read this code yourself
────────────────────────────────────────
- What functions to read first
- What to ignore initially
- Suggested reading order
- Grep / cscope tips

────────────────────────────────────────
11. Summary & mental model
────────────────────────────────────────
- One-paragraph summary
- Key invariants
- Mental model to keep while reading code

────────────────────────────────────────
12. What to study next
────────────────────────────────────────
- Related kernel subsystems
- Why they matter
- Recommended learning order

```