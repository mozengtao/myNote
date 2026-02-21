- What is skill ?
[Agent Skills](https://platform.claude.com/docs/en/agents-and-tools/agent-skills/overview)  
[Agent Skills](https://cursor.com/cn/docs/context/skills)  

- Skill 与 LLM 交互过程
1. Discovery
At startup, Claude loads only the name and description of each available Skill. This keeps startup fast while giving Claude enough context to know when each Skill might be relevant.
2. Activation
When your request matches a Skill's description, Claude asks to use the Skill. You'll see a confirmation prompt before the full SKILL.md is loaded into context. Claude matches requests against descriptions using semantic similarity, so write descriptions that include keywords users would naturally say.
3. Execution
Claude follows the Skill's instructions, loading referenced files or running bundled scripts as needed.

- Skill best practices
[Skill authoring best practices](https://platform.claude.com/docs/en/agents-and-tools/agent-skills/best-practices)  

- Progressive Disclosure
```
================================================================================
  Layer            Content                        Loading
--------------------------------------------------------------------------------
  Metadata         Name, Description              Always Load
  Instruction      SKILL.md (excl. name/desc)     Load on Demand
  Resource         Reference, Script              Conditional Load
                     Read / Execute
================================================================================
```

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

## Ref
> The open agent skills tool - npx skills
[skill creator](https://github.com/moltbot/moltbot/blob/main/skills/skill-creator/SKILL.md)  
[**superpowers**](https://github.com/obra/superpowers/tree/main)  
[skills](https://github.com/vercel-labs/skills)  
> The Open Agent Skills Ecosystem
[**skills**](https://skills.sh/)  
[antigravity-awesome-skills](https://github.com/sickn33/antigravity-awesome-skills)  
[moltbot skills](https://github.com/moltbot/moltbot/tree/main/skills)  
[]()  
[]()  
[]()  


[**skills**](https://github.com/anthropics/skills)  
- PowerPoint (pptx): 创建和编辑演示文稿
- Excel (xlsx): 创建和分析电子表格
- Word (docx): 创建和编辑文档
- PDF (pdf): 生成PDF文档

> Humanizer 的汉化版本，Claude Code Skills，旨在消除文本中 AI 生成的痕迹
[Humanizer-zh](https://github.com/op7418/Humanizer-zh)  

Marketplace
[cursor marketplace](https://cursor.com/cn/marketplace)  
[Skills Marketplace](https://www.atcyrus.com/skills)  
[skillsmp](https://skillsmp.com/)  
[]()  
[]()  
[]()  

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