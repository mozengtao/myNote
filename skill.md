- What is skill ?
[What are skills?](https://agentskills.io/what-are-skills)  
[Agent Skills](https://platform.claude.com/docs/en/agents-and-tools/agent-skills/overview)  
[Agent Skills](https://cursor.com/cn/docs/context/skills)  
[Skills](https://docs.langchain.com/oss/python/langchain/multi-agent/skills)  
[Agent Skills](https://developers.openai.com/codex/skills/)  
[]()  
[]()  

[**Coding Skills**](https://smithery.ai/skills?q=category%3ACoding)  
[python-expert](https://smithery.ai/skills/shubhamsaboo/python-expert)  
[compound-engineering-plugin](https://github.com/EveryInc/compound-engineering-plugin)  
[Skill Creator](https://mcpmarket.com/tools/skills/skill-creator)  
[MCP Advisor](https://mcp.so/server/mcpadvisor)  
[]()  
[The Complete Guide to Building Skills for Claude](https://resources.anthropic.com/hubfs/The-Complete-Guide-to-Building-Skill-for-Claude.pdf)  
[Claude Skills 完整构建指南](https://github.com/libukai/awesome-agent-skills/blob/main/docs)  
[]()  
[]()  
[]()  


## How skills work (渐进式披露)
   Skills use progressive disclosure to manage context efficiently:

   1. **Discovery**: At startup, agents load only the name and description of each available skill, just enough to know when it might be relevant.
   2. **Activation**: When a task matches a skill’s description, the agent reads the full SKILL.md instructions into context.
   3. **Execution**: The agent follows the instructions, optionally loading referenced files or executing bundled code as needed.

   This approach keeps agents fast while giving them access to more context on demand.

## explain-code
```
---
name: explain-code
description: Explains code with visual diagrams and analogies. Use when explaining how code works, teaching about a codebase, or when the user asks "how does this work?"
---

When explaining code, always include:

1. **Start with an analogy**: Compare the code to something from everyday life
2. **Draw a diagram**: Use ASCII art to show the flow, structure, or relationships
3. **Walk through the code**: Explain step-by-step what happens
4. **Highlight a gotcha**: What's a common mistake or misconception?

Keep explanations conversational. For complex concepts, use multiple analogies.
```

## Marketplace
> The open agent skills tool - npx skills
[skill creator](https://github.com/moltbot/moltbot/blob/main/skills/skill-creator/SKILL.md)  
[**superpowers**](https://github.com/obra/superpowers/tree/main)  
[skills](https://github.com/vercel-labs/skills)  
> The Open Agent Skills Ecosystem
[**skills**](https://skills.sh/)  
   - [prompt-engineering-patterns](https://skills.sh/wshobson/agents/prompt-engineering-patterns)  
[antigravity-awesome-skills](https://github.com/sickn33/antigravity-awesome-skills)  
[moltbot skills](https://github.com/moltbot/moltbot/tree/main/skills)  
[]()  
[cursor marketplace](https://cursor.com/cn/marketplace)  
[Skills Marketplace](https://www.atcyrus.com/skills)  
[skillsmp](https://skillsmp.com/)  
[]()  
[**smithery**](https://smithery.ai/skills)  
[skill-development](https://smithery.ai/skills/anthropics/skill-development)  
[**mcpmarket**](https://mcpmarket.com/tools/skills)  
[]()  


[**skills**](https://github.com/anthropics/skills)  
- PowerPoint (pptx): 创建和编辑演示文稿
- Excel (xlsx): 创建和分析电子表格
- Word (docx): 创建和编辑文档
- PDF (pdf): 生成PDF文档

> Humanizer 的汉化版本，Claude Code Skills，旨在消除文本中 AI 生成的痕迹
[Humanizer-zh](https://github.com/op7418/Humanizer-zh)  


[Equipping agents for the real world with Agent Skills](https://www.anthropic.com/engineering/equipping-agents-for-the-real-world-with-agent-skills)  
[Skills explained: How Skills compares to prompts, Projects, MCP, and subagents](https://claude.com/blog/skills-explained)  
[skill-creator](https://skillsmp.com/skills/langgenius-dify-agents-skills-skill-creator-skill-md)  
[]()  
[]()  
- What Skills were designed for
   delivering specialized context on demand without permanent overhead.

- A skill is
   a document (often markdown) containing instructions, constraints, and domain knowledge, stored in a designated directory that Claude can access through simple file-reading tools.

- Mental model
   skills are prompts and contextual resources that activate on demand, providing specialized guidance for specific task types without incurring permanent context overhead.