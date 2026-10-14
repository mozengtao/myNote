# Tcl/Tk GUI 开发与 Mental Model 全套教程

> 面向有 C / C++ / Go / Rust 背景、正在学习 Tcl 的工程师。
> 目标：建立 Tcl/Tk GUI 开发的第一性原理 Mental Model，而不是背 API。
> 原生 Tcl/Tk，不涉及 Python Tkinter。

---

## 0. 语言规范

本教程全套文档遵循统一的语言约定，阅读前请先了解：

- **Tcl 代码本体**（变量名、过程名、命名空间、widget 路径、`-command` 脚本内容等）：使用**英文**标识符，符合 Tcl/Tk 社区惯例。
- **代码注释**：使用**中文**，解释该行/该块在做什么、为什么这样写。
- **ASCII 架构图 / 执行流程图 / 时序图**：图中的框线、箭头、节点标签使用**英文**，避免中英文混排时字符宽度不一致，破坏对齐。
- **正文讲解**（Why / Mental Model / Execution Walkthrough / Common Failure / Engineering Practice / Exercises / Knowledge Check 等）：使用**中文**撰写。
- **概念映射表**：术语列使用英文原词（更准确），说明/区别列使用中文。

---

## 1. 环境检查

本教程所有 `.tcl` 示例遵循 Tcl 8.6 + Tk 8.6 API（如使用更高版本特性会在对应文件中明确标注）。

### 1.1 确认 Tcl/Tk 已安装

```bash
# 查看 tclsh（纯 Tcl 解释器）和 wish（Tcl + Tk 解释器）是否存在
which tclsh wish

# 查看版本号
echo 'puts [info patchlevel]' | tclsh

# 查看 Tk 版本（需要能连接到显示环境）
echo 'package require Tk; puts [info patchlevel]; exit' | wish
```

- `tclsh`：只包含 Tcl 解释器，**没有** GUI 能力，适合运行纯逻辑脚本（例如 `model.tcl`、`config.tcl`）。
- `wish`：在 `tclsh` 基础上自动 `package require Tk` 并初始化一个 GUI 环境（创建主窗口 `.`），运行 `.tcl` GUI 脚本应使用 `wish`。

两者的本质关系见 [00-architecture/README.md](00-architecture/README.md)。

### 1.2 运行一个 `.tcl` 文件

```bash
# 运行纯 Tcl 脚本（无 GUI）
tclsh path/to/file.tcl

# 运行 Tk GUI 脚本（需要图形显示环境，例如 X11 / Xorg / XQuartz / Windows 桌面）
wish path/to/file.tcl
```

如果脚本以 `#!/usr/bin/env wish` 开头并且有可执行权限，也可以直接 `./file.tcl` 运行。

### 1.3 关于本教程编写环境的重要说明

本教程中的所有 `.tcl` 文件都是在**无图形显示环境（headless，没有 `$DISPLAY`，没有 Xvfb）**的终端中编写和校验的。校验方式如下：

- **不依赖 Tk 的纯 Tcl 文件**（如 `model.tcl`、`config.tcl`、命名空间/过程示例的逻辑部分）：直接用 `tclsh` 完整运行，确认输出符合预期。
- **依赖 Tk 的 GUI 文件**：用 `tclsh`（而不是 `wish`）尝试执行，确认程序能顺利执行到 `package require Tk` 并在"无法连接到显示"处才报错，而不是在更早的位置出现语法错误（花括号/引号不匹配、未定义变量等）。这证明脚本在 Tk 初始化之前的所有 Tcl 语法都是合法的。

这意味着：**本教程所有 GUI 示例的 Tcl 语法已尽力核实，但实际的窗口外观、控件排布、点击效果，请你在有图形界面的机器上用 `wish` 亲自运行查看**。这正是学习 Tcl/Tk 的必要步骤——光看代码和看运行效果是两种不同的理解。

---

## 2. 整体 Mental Model（先读这个）

```text
+-------------------------------------------------------+
| Tcl Application (your script)                         |
|                                                       |
|  Application Logic (proc / namespace)                 |
|       |                                               |
|       +---- Widget Commands   (button, entry, ...)    |
|       +---- Tcl Variables     (set, global, trace)    |
|       +---- Event Bindings    (-command, bind, after) |
+-------------------------+------------------------------+
                          |
                          v
+--------------------------------------------------------+
| Tcl Interpreter + Tk Toolkit                           |
|                                                        |
|  Command Evaluation      (parse -> substitute -> eval) |
|  Widget Management       (widget tree, widget commands)|
|  Variable Traces         (textvariable / trace add)    |
|  Event Processing        (event queue, event loop)     |
+-------------------------+------------------------------+
                          |
                          v
+------------------------------------------------------+
| Window System / Display (X11, Win32, Aqua, ...)      |
|                                                      |
|  Native windows, controls, rendering, input events   |
+------------------------------------------------------+
```

一句话 Mental Model：

> **Tcl 解释器负责"执行命令"；Tk 是一组把"命令执行"映射为"GUI 对象操作"的命令集合；Event Loop 负责把操作系统的输入事件转换成对这些命令的调用。**

更详细的拆解见 [00-architecture/README.md](00-architecture/README.md)。

---

## 3. 学习路径（按顺序阅读）

| Layer | 主题 | 核心问题 | 目录 |
|---|---|---|---|
| 0 | 整体架构与运行环境 | Tcl/Tk/wish/Widget/Event Loop 是什么关系？ | [00-architecture](00-architecture/README.md) |
| 1 | Widget 与命令系统 | 为什么控件可以当命令用？ | [01-widget-and-commands](01-widget-and-commands/README.md) |
| 2 | Geometry Manager 与布局 | 为什么创建控件后还要单独布局？ | [02-geometry-manager](02-geometry-manager/README.md) |
| 3 | Tcl 变量与 GUI 状态绑定 | 用户改输入框，程序怎么知道？ | [03-variables-and-state](03-variables-and-state/README.md) |
| 4 | 事件驱动与 Callback | 为什么 GUI 程序不是从上到下顺序执行？ | [04-event-loop-and-callback](04-event-loop-and-callback/README.md) |
| 5 | 过程、作用域与组件封装 | 怎么把一堆命令组织成可维护模块？ | [05-proc-scope-namespace](05-proc-scope-namespace/README.md) |
| 6 | 表单验证与错误处理 | 用户输入怎么安全地进入业务逻辑？ | [06-form-validation](06-form-validation/README.md) |
| 7 | 文件对话框、菜单 | 怎么做一个真正能用的小工具？ | [07-dialogs-and-menu](07-dialogs-and-menu/README.md) |
| 8 | 综合项目：工程配置管理 GUI | 把前面所有概念整合成一个真实项目 | [08-config-tool-project](08-config-tool-project/README.md) |
| 9 | 调试与故障定位 | GUI 不显示/回调不执行/布局错乱怎么查？ | [09-debugging](09-debugging/README.md) |
| 10 | 与 C/C++ 的概念映射 | 用已有知识类比，但不要类比过头 | [10-concept-mapping](10-concept-mapping/README.md) |

建议严格按顺序阅读：Layer 3（变量绑定）和 Layer 4（事件驱动）是整个 Tcl/Tk Mental Model 的核心，之后的所有内容都建立在这两层之上。

---

## 4. 每个 Layer 的文档结构

每个 Layer 的 `README.md` 都遵循同一套结构，方便你带着问题去读：

1. **Why** —— 这个机制解决什么问题
2. **Mental Model** —— 一句话直觉 + 图
3. **Architecture** —— 参与对象及其关系
4. **Complete Example** —— 完整可运行的 `.tcl` 文件
5. **Execution Walkthrough** —— 按执行顺序逐条解释
6. **Data Flow / Control Flow** —— 输入、变量、回调、界面更新之间的关系
7. **Common Failure** —— 典型错误与诊断方法
8. **Engineering Practice** —— 实际工程中怎么组织代码
9. **Exercises** —— 2~3 个由浅入深的练习
10. **Mental Model Summary** —— 不超过 10 条关键认知
11. **Knowledge Check** —— 3 个检查问题

---

## 5. 快速开始

```bash
cd tcl-tk-gui-mastery/00-architecture
wish hello.tcl
```

如果看到一个标题为 "Hello Tk" 的窗口，里面有一行文字和一个 Exit 按钮，说明环境没有问题，可以开始学习。

如果没有看到窗口（例如你也在 headless 环境中），请先阅读 [00-architecture/README.md](00-architecture/README.md) 中的"安装检查与故障排查"一节。
