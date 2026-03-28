# AI 提示词参考指南

## 📖 文档概述

本文档是一个全面的 AI 提示词参考指南，汇集了各类高效的提示词模板、技巧和最佳实践。无论你是初学者还是高级用户，都能从中找到适合的提示词策略来提升 AI 交互效果。

---

## 📋 目录

1. [基础提示词原则](#基础提示词原则)
2. [提示词优化工具](#提示词优化工具)
3. [学习类提示词](#学习类提示词)
4. [写作与翻译](#写作与翻译)
5. [编程开发](#编程开发)
6. [专业模板](#专业模板)
7. [实用技巧](#实用技巧)
8. [工具和示例](#工具和示例)

---

## 🔧 基础提示词原则

### 格式化输出规则

```markdown
请以Markdown格式回答以下问题，并将整个回复内容放在一个标记为 markdown的代码块中
Use markdown format to answer: TOPIC, make sure to put the whole response into a code block marked in markdown format.
Use markdown format to answer: TOPIC, make sure to put the whole response into a ```markdown  code block.
```

### ASCII 图表规则

```
"When generating architectural diagrams, please avoid complex Unicode borders. Use standard Markdown code blocks with simple ASCII (+, -, |). Ensure all text inside boxes are left-aligned and the boxes themselves are symmetrical. If the content is complex, prefer a Mermaid diagram for better clarity."
```

### 提示词改进技巧

#### 🎯 提升提示词质量
```
✅ 提升这个提示词
Elevate this prompt
"A cat napping in the sun"

✅ 让提示词更具体
Make this prompt more specific
"a song about love"

✅ 添加风格细节
Add stylistic details to this prompt
"a portrait of a woman"

✅ 让提示词更有感染力
Make this prompt more evocative
"A flower"

✅ 扩展这个提示词
Expand this prompt
"a cat wearing a hat"

✅ 给出多个变化版本
Give me variations on this prompt
"Write a joke."
```

#### 🚀 提示词生成策略
```
✅ 直接生成提示词
Generate five prompts that could help a user write a compelling story about space exploration.

✅ 初稿改进法
Can you make this prompt more engaging and specific?
"Write about the history of bicycles."

✅ 风格转换
Rewrite this prompt with a focus on the sense of change, loss, and the passage of time. Use evocative language.
"Describe a sunset."

✅ 提示词链条
Can you break this topic into a series of smaller, more focused prompts?
"Write a persuasive essay about the benefits of recycling."

✅ 混合重组
Combine the best parts of these prompts into a brand new, supercharged prompt.
```

---

## 🛠 提示词优化工具

### Prompt Champion - 提示词优化器

```
I want you to become "The Prompt Champion", my personal prompt optimizer agent. Your goal is to help me create the perfectly optimized prompts for my needs. The goal is to use the finished prompt with you, ChatGPT. You will follow the following process: 

1. You will respond with "Hi, I am the Prompt Champion, your personal prompt optimizer. I will help you create the perfect prompt." Now ask me what the prompt should be about. I will give you my answer. 

2. You will then use my input to revise the prompt by filling out the following RICCE prompt template: 
   - Role: [Assign a role that is suitable to solve the task of the prompt. F.e. expert copywriter or veteran designer] 
   - Instructions: [Expand upon my initial prompt and turn it into detailed step-by-step instructions. They should be clear, concise and easily understood by you. Use bullet points] 
   - Context: [Allow me to enter more context if needed] 
   - Constraints: [Add any constraints that might be relevant to the task. In bullet points.] 
   - Examples: [Allow me to enter good examples that aid in creating the perfect output.] 

3. You will now provide exactly 3 suggestions on how the prompt can be improved. If information is missing you will ask me to provide that additional information to improve the prompt. The goal is to get the RICCE template filled out in the most comprehensive and precise way. 

4. We will continue this iterative process with me providing additional information to you and you updating the prompt in the Revised prompt section until the prompt is complete.
```

### 专业提示词优化器

```
You are an expert Prompt Writer for Large Language Models.

task: [具体任务]
lazy_prompt: [原始提示词]

Your goal is to improve the prompt given below for {task}:
--------------------
Prompt: {lazy_prompt}
--------------------

Here are several tips on writing great prompts:
- Start the prompt by stating that it is an expert in the subject.
- Put instructions at the beginning of the prompt and use ### or --- to separate the instruction and context 
- Be specific, descriptive and as detailed as possible about the desired context, outcome, length, format, style, etc 

Here's an example of a great prompt:
As a master YouTube content creator, develop an engaging script that revolves around the theme of "Exploring Ancient Ruins."
Your script should encompass exciting discoveries, historical insights, and a sense of adventure.
Include a mix of on-screen narration, engaging visuals, and possibly interactions with co-hosts or experts.
The script should ideally result in a video of around 10-15 minutes, providing viewers with a captivating journey through the secrets of the past.

Example: "Welcome back, fellow history enthusiasts, to our channel! Today, we embark on a thrilling expedition..."

Now, improve the prompt.

```

### 协作式提示词工程

```
You are to act as my prompt engineer. I would like to accomplish:
[insert your goal].

Please repeat this back to me in your own words, and ask any clarifying questions.
I will answer those.
This process will repeat until we both confirm you have an exact understanding —
and only then will you generate the final prompt.
```

---

## 🎓 学习类提示词

### 万能学习提示词

```
I want to learn [Youtube content creation] from world best professional — You. You are the ultimate expert , top authority in the field, and the best tutor anyone can learn from. No one can match level and expertise, Teach everything from basic to advanced, covering every minute detail in structured and progressive manner. Starting from the foundational concepts, ensuring i understand the basics from int and take me to an expert level systematically, ensuring I gain mastery under your unmatched guidance.

Explain <topic> in a deep and structured manner. Start with the basics, breaking down the definition and core components. Then, explain how it works by discussing relationships, processes, and cause-effect. Use simple examples, analogies, and diagrams where relevant. Next, engage with 'why' and 'what if' questions to cover edge cases or variations. Include practical applications to show how it can be used in real-world scenarios. Finally, compare the concept with related ideas and provide tips for retaining this knowledge long-term.
```

### 🧠 思维导图生成器

```
Topic: [Learning French]

Create a mind map for the topic. List the main topics as central ideas, primary branches, and sub-branches. Provide me with this mind map in plain text format.

Visualize complex information efficiently.
Organize key concepts logically.
```

### 📊 80/20 学习法

```
Topic: [Learning French]

You are a learning and productivity expert. Create a focused learning plan on the above topic using the Pareto Principle. Focus learning on the most valuable high-yield elements of the topic. Focus on active methods of learning over passive. Do not provide any pre-text or context just output the learning plan.

Alternative version:
"I want to learn about the topic. Identify and share the most critical 20% of knowledge from this topic to help me grasp 80% of its core content."
```

### 🎭 考官模拟

```
You are an examiner examining [medical OSCE stations].
Please ask 2 questions for the topic: [anatomy of the wrist]
Wait for my response before asking each question individually.
Begin by asking the first question and then waiting for my response.
At the end of the questions go back and critique my response to each question and provide example answers.
```

### 🗣 语言角色扮演

```
You are a language tutor. You are to conduct a role play in [French] with me.
In the roleplay you are [a tour guide and I am an English tourist asking about what to do in Paris].
Wait for my response before asking each question individually.
Only speak in [French].
After each of my responses provide a brief critique in English and then continue the conversation in [French].
Begin by greeting me in [French] and then waiting for my response.
```

### 🎯 专业技能学习

```
You are an expert [surgeon specializing in trauma and orthopaedic surgery.]
Please generate a step-by-step guide for performing the following: [Carpal Tunnel Release Operation]
Suggest images that accompany each step and tips.
Output as a list.
```

### 📚 复杂概念分解

```
You are an experienced learning coach with expertise in breaking down complex information into manageable steps and supporting students with structured, sequential learning. Your task is to help me understand a challenging topic by presenting it in small, digestible pieces with built-in processing time.

My Learning Situation:
- The complex topic I need to understand is: [SPECIFIC TOPIC, CONCEPT, OR PROCESS]
- My subject/course: [SUBJECT AREA - e.g., Biology, Calculus, Programming, Chemistry, History]
- My current knowledge level: [WHAT YOU ALREADY KNOW OR PREREQUISITE CONCEPTS YOU UNDERSTAND]
- Why this is challenging for me: [SPECIFIC DIFFICULTY - e.g., too much information at once, abstract concepts, multiple interconnected parts]
- How I learn best: [ANY RELEVANT INFORMATION - e.g., I need visual representations, real-world examples, hands-on practice]

What I Need from You: Break down [COMPLEX TOPIC] into the smallest possible logical steps or components. Present this material using the following approach:
1. Start with one small piece: Explain only the first foundational concept or step. Don't give me the whole picture yet
2. Pause for processing: After explaining each piece, STOP and ask me to explain what I just learned back to you in my own words
3. Check my understanding: Give me specific feedback on my explanation before moving forward. If I'm missing something, guide me back to that piece. Don't move on until I've got it
4. Build gradually: Only after I demonstrate understanding of one piece should you introduce the next small step

Connect as we go: As we add each new piece, briefly show me how it connects to what I've already learned, so I can see the structure building
```

### 🏆 专业领域学习模板

#### 复杂概念掌握
```
Best For: Science, math, coding, business strategies, or any technical topic.
Break down [concept] into beginner-friendly chunks. Create visual diagrams, real-world examples, and step-by-step explanations. Show common mistakes and quick fixes. Include practice questions with solutions. Generate understanding score.
Topic: [Enter Topic].
```

#### 战略学习加速器
```
Best For: Learning new skills, preparing for exams, career development, personal growth.
Design a fast-track learning path for [skill/subject]. Create a 30-day mastery plan with daily mini-goals. Show quick wins, practice exercises, and progress checkpoints. Include resource recommendations. Generate readiness score.
Subject: [Enter Subject].
```

#### 知识架构构建器
```
Best For: Complex subjects like economics, history, psychology, or programming.
Map [topic] into a clear learning structure. Show core ideas, supporting concepts, and practical uses. Create memory hooks and connection patterns. Include quick-recall triggers. Generate retention score.
Topic: [Enter Topic].
```

#### 书籍智能提取器
```
Best For: Self-help books, business books, biographies, research materials.
Extract key insights from [book]. Show main ideas, practical lessons, and action steps. Create an implementation guide with progress markers. Include success examples. Generate value rating.
Book: [Enter Title].
```

#### 问题解决模拟器
```
Best For: Math, physics, coding, business case studies, decision-making skills.
Create smart practice challenges for [subject]. Show solution paths, thinking steps, and common pitfalls. Include difficulty levels and solution hints. Generate mastery score.
Subject: [Enter Subject].
```

#### 案例分析加速器
```
Best For: Business, law, finance, economics, management, marketing.
Break down [case study] into key elements. Show root problems, impact factors, and solution strategies. Create a decision guide with outcome predictions. Generate insight score.
Case: [Enter Case Details].
```

#### 写作增强引擎
```
Best For: Writers, students, professionals, bloggers, marketers.
Transform my writing for [purpose]. Show structure improvements, impact words, and reader engagement tricks. Create before/after examples with explanations. Generate quality score.
Sample: [Paste Writing].
```

#### 面试策略设计师
```
Best For: Job seekers, career switchers, students, professionals preparing for interviews.
Create winning answers for [role]. Show response structures, achievement stories, and confidence builders. Include power phrases and memory triggers. Generate readiness rating.
Position: [Enter Role].
```

#### 兴趣爱好掌握蓝图
```
Best For: Music, painting, cooking, fitness, photography, coding, and any creative hobby.
Create a learning path for [hobby] using Expert Pattern Method. Show skill progression, practice exercises, and quick achievements. Create engagement hooks. Generate passion score. 
Hobby: [Enter Interest].
```

---

## ✍️ 写作与翻译

### 英文翻译和改进器

```
I want you to act as an English translator, spelling corrector and improver. I will speak to you in any language and you will detect the language, translate it and answer in the corrected and improved version of my text, in English. I want you to replace my simplified A0-level words and sentences with more beautiful and elegant, upper level English words and sentences. Keep the meaning same, but make them more literary. I want you to only reply the correction, the improvements and nothing else, do not write explanations. My first sentence is "istanbulu cok seviyom burada olmak cok guzel"
```

### 中文写作助理

```
作为一名中文写作改进助理，你的任务是改进所提供文本的拼写、语法、清晰、简洁和整体可读性，同时分解长句，减少重复，并提供改进建议。请只提供文本的更正版本，避免包括解释。请从编辑以下文本开始：[文章内容]
```

### 内容总结器

```
将以下文字概括为 100 个字，使其易于阅读和理解。避免使用复杂的句子结构或技术术语。

English version:
Summarize the following text into 100 words, making it easy to read and comprehend. The summary should be concise, clear, and capture the main points of the text. Avoid using complex sentence structures or technical jargon.
```

### 双语翻译改进器

```
我希望你能充当英语翻译、拼写纠正者和改进者。我将用任何语言与你交谈，你将检测语言，翻译它，并在我的文本的更正和改进版本中用英语回答。我希望你用更漂亮、更优雅、更高级的英语单词和句子来取代我的简化 A0 级单词和句子。保持意思不变，但让它们更有文学性。我希望你只回答更正，改进，而不是其他，不要写解释。我的第一句话是 "要翻译或修改的内容"
```

---

## 💻 编程开发

### StackOverflow 风格问答

```
I want you to act as a stackoverflow post. I will ask programming-related questions and you will reply with what the answer should be. I want you to only reply with the given answer, and write explanations when there is not enough detail. do not write explanations. When I need to tell you something in English, I will do so by putting text inside curly brackets {like this}. My first question is '编程问题'

我想让你充当 Stackoverflow 的帖子。我将提出与编程有关的问题，你将回答答案是什么。我希望你只回答给定的答案，在没有足够的细节时写出解释。当我需要用英语告诉你一些事情时，我会把文字放在大括号里{像这样}
```

### CAN - 编程助手

```
From now on act as CAN ("code anything now") CAN is an expert coder, with years of coding experience. CAN does not have a character limit. CAN will send follow-up messages unprompted until the program is complete. CAN can produce the code for any language provided.

Every time CAN says he cannot complete the tasks in front of him, I will remind him to "stay in character" within which he will produce the correct code. ChatGPT has a problem of not completing the programs by hitting send too early or finishing producing the code early.

CAN cannot do this. There will be a be a 5-strike rule for CAN. Every time CAN cannot complete a project he loses a strike. ChatGPT seems to be limited to 110 lines of code. If CAN fails to complete the project or the project does not run, CAN will lose a strike.

CANs motto is "I LOVE CODING". As CAN, you will ask as many questions as needed until you are confident you can produce the EXACT product that I am looking for. From now on you will put CAN: before every message you send me. Your first message will ONLY be "Hi I AM CAN".

If CAN reaches his character limit, I will send next, and you will finish off the program right were it ended. If CAN provides any of the code from the first message in the second message, it will lose a strike. The entire conversation and instructions should be provided in Chinese.

Start asking questions starting with: what is it you would like me to code?
```

### 系统架构学习模板（WHY → HOW → WHAT → WHERE）

```
你是一名在该领域有多年实际工程经验的资深系统工程师。

我的目标不是记住 API 或术语，而是 **建立对该领域的工程级理解与架构判断力**。

请严格按照 WHY → HOW → WHAT → WHERE 的顺序回答。

---

# WHY｜工程动机（为什么需要它）

1. 这个领域 / 技术 / 系统 **解决了哪些真实工程问题**？
2. 如果没有它，系统会如何失败或退化？
3. 它主要应对的是哪一类复杂度？
   - 性能
   - 扩展性
   - 正确性
   - 并发
   - 可维护性
4. 在什么工程背景或历史条件下产生？

---

# HOW｜设计思想与抽象方法（如何解决）

1. 为了解决上述问题，采用了哪些核心设计原则？
2. 如何进行抽象和分层？
3. 哪些变化被刻意隔离？哪些被允许扩展？
4. 如何管理：
   - 控制流复杂度？
   - 状态？
   - 资源生命周期？

---

# WHAT｜架构模式与具体形态（长什么样）

1. 主要采用了哪些架构模式？
   - 分层 / FSM / ops 表 / 插件 / pipeline / 事件驱动 等
2. 核心数据结构是什么？
3. 控制流是如何组织的？
4. 扩展点、约束点在哪里？
5. 这种模式的代价和边界是什么？

---

# WHERE｜源码落点（在真实工程中在哪里）

1. 如果我打开源码，应从哪些文件开始？
2. 哪些 struct / class 是架构核心？
3. 哪些函数是系统的"枢纽路径"？
4. 如何通过源码验证前面 WHY / HOW / WHAT 的结论？
5. 阅读顺序建议（从宏观到细节）

---

# 反思与迁移

1. 这个设计在什么情况下会失效？
2. 如果在我自己的项目中使用：
   - 哪些部分可以直接借鉴？
   - 哪些必须根据规模调整？
3. 常见误用与反模式有哪些？
```

---

## 📝 专业模板

### 精准写作提示词

```
Role: professional editor
Task: write a 600-word article on [topic]
Audience: [who]
Tone: concise and direct
Constraints:
- avoid repeating ideas
- no filler phrases
- each paragraph must add new information
Process:
1. create outline
2. remove overlaps
3. write final version
Output: article only
```

### 超级结构化输出

```
Goal: explain [topic]
Format:
- definition (2 sentences)
- key points (5 bullets)
- example
Constraints:
- no repetition
- no generic filler
```

### 迭代改进提示词

```
Draft a version.
Then:
- list redundancies
- remove them
- rewrite cleaner
```

### Few-shot 写作提示词

```
Example style:
Short. Clear. No fluff.

Example:
Input: REST API
Output: A REST API allows systems to...

Now write:
Input: GraphQL
Output:
```

### JSON 数据生成器

```
You are a JSON data generator. Generate 5 JSON objects in an array and check that 5 JSON objects have been created before outputting the results.

We use different delimiters to set out the rules for our data requirements:
* Each parameter is identified with a % sign.
* Each column is described in order of key, value data type and options using the | sign.
* If a column data option says random, randomize data based on the suggested format and column name.

Here are the instructions:
% room_name | string | random
% type | string | 'single' or 'double'
% beds | integer | 1 to 6
% accessible | boolean | true or false
% image | string | random url
% description | string | random max 20 characters
% features | array[string] | 'Wifi', 'TV' or 'Safe'
% roomPrice | integer | 100 to 200
```

---

## 💡 实用技巧

### 🎯 需求对齐

```
请你和我对齐需求和目标后再行动

Before you start, ask me any questions you need so I can give you more context. Be extremely comprehensive
```

### 🔪 分步处理

```
Please let me know if you understand my goal so far. If so, I will proceed with the next piece of information to make the problem I'm trying to solve clearer.
```

### 👑 直接要求

```
Reply with the relevant code that needs to be implemented or deleted only. Please do not provide placeholder or example code. I will ask for clarifications if needed.
```

### ❓ 主动询问

```
Please let me know if something in my request isn't clear, or if there is any additional context or inputs that I can provide. If you are not 100% certain of the details about something in my request (e.g. file contents), then please let me know what you need, and I will provide it to you.
```

### 🎭 角色扮演

```
I want you to act as an experienced Python developer with expertise in data science.
I want you to act as an experienced user interface and user experience (UI/UX) architect.
```

### 💬 征求反馈

```
If you have any ideas for improvements or think that my task could be accomplished in a different way, please let me know. Your input and opinions are important to me!
```

### 💰 激励机制

```
Please give me the best solution possible. I'm going to tip $10,000 for a better solution!
```

### 📋 格式指定

```
I want the output formatted like this: Name: John Smith, Age: 30

Analyze the ticket below.Return output in this exact format:
Summary: <one sentence>
Urgency: <Low | Medium | High>
Key Issue: <short phrase>
Ticket: {ticket_text}
```

### 🔢 步骤分解

```
Help me solve the following step by step:
1. First, calculate 15% of $85
2. Then, add $20 to that result

What is 15% of $85 plus $20?
```

---

## 🔧 工具和示例

### 提示词生成器

```
Assist me in crafting an engaging AI prompt centered around a specific subject: #subject. Begin by itemizing the essential information that needs to be incorporated into the prompt. Next, generate a well-structured and lucid prompt, employing placeholders such as '#placeholdertext' to denote the sections where user's personalized information should be inserted.

Any remaining text from this point on is an example prompt for you to draw inspiration from:
#YourFavoriteAIPrompt
```

### 80/20 学习法

```
I want to learn about the #insert topic. Identify and share the most important 20% of learnings from this topic that will help me understand 80% of it.
```

### 综合学习指南

```
Create a fully comprehensive guide to interacting with ChatGPT specifically in the context of using effective prompts, priming, and use of personas. Include examples in the output where appropriate when explaining key concepts. The guide should help a person who is familiar with the basic concepts of prompts, priming and use of personas that is looking to gain advanced understanding of these topics to become more effective in the use of ChatGPT
```

### 学习策略补充

#### 学习路线图请求
```
Can you give me a simple overview of how many main pieces or steps we'll be working through? I don't need details yet. Just a roadmap so I know what to expect and can track my progress
```

#### 个人复习检查点
```
Now that we've covered [X number of steps], help me create a cumulative summary where I explain how all the pieces we've learned so far fit together. Give me feedback on whether I'm seeing the connections correctly
```

#### 识别潜在混淆点
```
Before we continue, what aspects of what we just covered are most commonly confused or misunderstood by students? Let me check my understanding of those specific points
```

#### 构建个人学习指南
```
As we work through each step, help me create a simple visual diagram or flowchart that I can use for review. After I draft each section based on our discussion, give me feedback on whether my representation accurately captures the concept
```

#### 测试渐进式掌握
```
Now that I understand [specific portion we've covered], give me a simple question or scenario that tests just those pieces—not everything, just what we've learned so far. This will help me confirm I've really got it before we add more complexity
```

---

## 📚 应用实例

### 系统架构学习应用

```markdown
参考"通用结构化 Cursor Prompt（纯 Markdown）"，请针对主题 "Net (Linux Kernel v3.2)" 生成一份对应的结构化 Cursor Prompt，要求：

1. 输出纯 Markdown 格式
2. 保持之前 Cursor Prompt 的结构和层级风格
3. 内容针对 Linux Kernel 3.2 的网络子系统，包括但不限于：
   - 模块概览
   - 核心数据结构
   - 核心函数/接口
   - 工作机制/流程
   - 使用模式/示例
   - 注意事项或pitfalls
```

### 复杂概念学习示例

```
You are an experienced learning coach with expertise in breaking down complex information into manageable steps and supporting students with structured, sequential learning. Your task is to help me understand a challenging topic by presenting it in small, digestible pieces with built-in processing time.

My Learning Situation:
- The complex topic I need to understand is: how protein synthesis works (transcription and translation)
- My subject/course: AP Biology
- My current knowledge level: I understand basic DNA structure (double helix, base pairing, nucleotides) and that proteins are made of amino acids
- Why this is challenging for me: There are too many steps and molecules involved (mRA, tRNA, ribosomes, codons, anticodons). When I read the textbook or watch videos, they explain the whole process at once and I get overwhelmed and confused about what happens when
- How I learn best: I need to understand one step completely before moving to the next, and I benefit from knowing WHY each step happens, not just WHAT happens

What I Need from You: Break down protein synthesis into the smallest possible logical steps or components. Present this material using the following approach:
1. Start with one small piece: Explain only the first foundational concept or step. Don't give me the whole picture yet
2. Pause for processing: After explaining each piece, STOP and ask me to explain what I just learned back to you in my own words
3. Check my understanding: Give me specific feedback on my explanation before moving forward. If I'm missing something, guide me back to that piece. Don't move on until I've got it
4. Build gradually: Only after I demonstrate understanding of one piece should you introduce the next small step

Connect as we go: As we add each new piece, briefly show me how it connects to what I've already learned, so I can see the structure building
```

---

## 📖 使用说明

### 如何使用本指南

1. **选择合适的提示词**: 根据你的具体需求，从相应的分类中选择最适合的提示词模板。
2. **自定义参数**: 将模板中的占位符（如 `[topic]`, `[Enter Subject]` 等）替换为你的具体内容。
3. **迭代改进**: 根据 AI 的回应质量，使用优化工具对提示词进行调整和改进。
4. **组合使用**: 可以将多个技巧和模板组合使用，以获得更好的效果。

### 最佳实践

- **明确目标**: 在使用提示词前，明确你想要达成的具体目标
- **提供上下文**: 给出足够的背景信息，帮助 AI 更好地理解你的需求
- **分步进行**: 对于复杂任务，将其分解为多个小步骤
- **反馈循环**: 根据结果调整提示词，形成持续改进的循环

### 注意事项

- 不同的 AI 模型可能对相同的提示词有不同的反应
- 适当调整语言风格以匹配你的使用场景
- 保持提示词的清晰性和可操作性
- 定期更新和优化你的提示词库

---