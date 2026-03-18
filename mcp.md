## What is MCP
[What is the Model Context Protocol (MCP)?](https://modelcontextprotocol.io/docs/getting-started/intro)  
[Model Context Protocol](https://developers.openai.com/codex/mcp/)  
[Architecture overview](https://modelcontextprotocol.io/docs/learn/architecture)  
[Build your MCP server](https://developers.openai.com/apps-sdk/build/mcp-server/)  
[]()  

## 基本概念
### MCP Host (运行 LLM 并管理 MCP Server 的程序)
- 职责
1. 启动 MCP Server
2. 发现可用 tools
3. 把 tools 提供给 LLM
4. 当 LLM 需要工具时调用 MCP Server
5. 把结果返回给 LLM
```
User
 │
 ▼
Cursor (MCP Host + LLM)
 │
 ├─ 管理 MCP Servers
 ├─ 提供 tools 给 LLM
 └─ 调用 tools
```
### MCP Server (提供工具能力的进程)
- 职责
    向 MCP Host 提供工具（tools / resources / prompts）
```
Cursor (MCP Host)
      │
      │ JSON-RPC
      ▼
mcp-atlassian (MCP Server)
      │
      ▼
Jira / Confluence
```

#### MCP Server 的运行模式
1. 本地进程
```
Cursor
  │
  ▼
local MCP server

mode: stdin/stdout
```
2. HTTP MCP server
```
Cursor
  │
  ▼
remote MCP server

mode: HTTP
```
3. container MCP server
```
Cursor
  │
  ▼
Docker MCP server
```


### MCP Host 和 MCP Server 的关系
```
Host = orchestrator
Server = plugin

               MCP Host
           (e.g Cursor / IDE)
                │
     ┌──────────┼──────────┐
     ▼          ▼          ▼
 MCP Server  MCP Server  MCP Server
   GitHub      Atlassian     Slack

每个 server 提供不同能力
```

### MCP 的真实运行流程
```
User
 │
 ▼
Cursor (Host)
 │
 │ 1 发现 tools
 │
 ▼
LLM
 │
 │ 2 选择 tool
 │
 ▼
Cursor
 │
 │ 3 tools/call
 ▼
MCP Server
 │
 │ 4 调用外部API
 ▼
External System
 │
 │ 5 返回结果
 ▼
Cursor
 │
 │ 6 LLM生成最终回答
 ▼
User
```

## 整体机制
当 Cursor 启动 MCP Server 后，它会：
1. 向 MCP Server 请求工具列表
2. 将工具信息转换为 LLM可理解的函数定义
3. 将这些定义 附加到系统 prompt 中
4. LLM 推理是否调用工具
```
User Prompt
     │
     ▼
Cursor (MCP Host)
     │
     ├─ tools/list  → MCP Server
     │
     ▼
Tool Definitions
     │
     ▼
LLM Prompt Injection
     │
     ▼
LLM decides:
    call tool?
```

- Cursor Prompt 的真实结构
```
SYSTEM PROMPT
--------------
You are an AI coding assistant.

You have access to the following tools:

Tool: get_jira_issue
Description: Retrieve a Jira issue by key

Parameters:
issue_key (string)

Tool: search_confluence
Description: Search Confluence pages.

Rules:
- Use tools when needed
- Do not fabricate data

--------------------------------

CHAT HISTORY
--------------------------------
User: Summarize Jira issue VEC-123

--------------------------------

TOOLS
--------------------------------
[get_jira_issue definition]
[search_confluence definition]
```
> tools 被 作为 metadata 提供给模型

- Cursor Tool Calling 时序
```
User
 │
 ▼
Cursor
 │
 │ inject tools
 ▼
LLM
 │
 │ choose tool
 ▼
Cursor
 │
 │ tools/call
 ▼
MCP Server
 │
 │ fetch data
 ▼
Cursor
 │
 │ add result to context
 ▼
LLM
 │
 │ final answer
 ▼
User
```

ai/mcp/
├── mcp_llm_tool
│   ├── [README.md](./ai/mcp/mcp_llm_tool/README.md)  
│   ├── [api_server.py](./ai/mcp/mcp_llm_tool/api_server.py)
│   ├── [config.py](./ai/mcp/mcp_llm_tool/config.py)
│   ├── [llm_client.py](./ai/mcp/mcp_llm_tool/llm_client.py)
│   ├── [main.py](./ai/mcp/mcp_llm_tool/main.py)
│   ├── [mcp_base.py](./ai/mcp/mcp_llm_tool/mcp_base.py)
│   ├── [requirements.txt](./ai/mcp/mcp_llm_tool/requirements.txt)
│   └── [text_analyzer.py](./ai/mcp/mcp_llm_tool/text_analyzer.py)
├── [mcp_llm_tool_part1_concepts.md](./ai/mcp/mcp_llm_tool_part1_concepts.md)
├── [mcp_llm_tool_part2_implementation.md](./ai/mcp/mcp_llm_tool_part2_implementation.md)
└── [mcp_llm_tool_part3_deployment_testing.md](./ai/mcp/mcp_llm_tool_part3_deployment_testing.md)

## MCP Market
[1](https://mcp.so/)  
[2](https://mcpmarket.com/)  
[3](https://smithery.ai/)  
[]()  

## Related Tools
- uvx
- npx

## Common MCP Servers
> Web content fetching and conversion for efficient LLM usage
- fetch (https://mcp.so/server/fetch/test)  

- spec-workflow-mcp (https://github.com/Pimzino/spec-workflow-mcp)  
- 
- 