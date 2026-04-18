[Agent Skills](https://cursor.com/cn/docs/context/skills)  
[CURSOR-SKILLS Community Repository](https://cursor-skills.vercel.app/)  
[]()  
[]()  
[]()  
[]()  
[]()  
[]()  

| 功能场景 | 快捷键 | 作用说明 |
| :--- | :--- | :--- |
| 打开 AI 聊天面板 | `Ctrl + L/I` | 快速调出侧边 AI 对话窗口，可直接提问或解释代码 |
| 切换 Agent 模式| `Shift + Table` | 快速切换不同 Agent 模式 |
| 切换 AI 模型| `Ctrl + /` | 快速切换不同 AI 模型 |
| 新建聊天| `Ctrl + N` | 新建聊天 |
| 行内 AI 编辑指令 | `Ctrl + K` | 对选中/当前行发起 AI 修改，输入指令直接改写代码 |
| 接受 AI 全部修改 | `Ctrl + Enter` | 确认应用 AI 生成/修改的代码 |
| 拒绝 AI 全部修改 | `Ctrl + Backspace` | 放弃本次 AI 修改，恢复原代码 |
| 终止 AI 生成 | `Ctrl + Shift + Backspace` | 停止正在生成中的 AI 响应 |
| 格式化文档 | `Shift + Alt + F` | 自动格式化代码排版 |

[How I use Cursor](https://www.builder.io/blog/cursor-tips)  
[cursor.directory](https://cursor.directory/)  
[Rules](https://docs.cursor.com/en/context/rules)  
[]()  
[agent modes](https://cursor.com/cn/docs/agent/modes)  
- Agent
   复杂功能、重构
   自主探索、多文件编辑
- Ask
   学习、规划、提问
   只读探索，无自动修改
- Plan
   需要规划的复杂功能
   在执行前创建详细计划，并提出澄清性问题
- Debug
   棘手 Bug、回归问题
   生成假设、日志埋点、运行时分析
```shortcuts
Settings
    Ctrl + Shift + j
```

## Tips
```
在 Windows 环境下使用 Cursor 通过 Remote SSH 连接远程服务器时，对话记录（包括 Chat 和 Agent/Composer 的历史）并不是保存在远程服务器上，而是保存在你本地的 Windows 电脑中

Cursor 遵循了 VS Code 的设计理念：插件状态和工作区元数据由客户端（本地）管理

在 Windows 上，具体的存储路径为:
%APPDATA%\Cursor\User\workspaceStorage\
```

