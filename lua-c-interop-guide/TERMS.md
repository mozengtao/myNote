# 术语表（Terminology）

本教程全篇（所有 `chapters/` 与 `projects/`）统一使用以下中英对照术语。
**规则：概念用中文表达，API 名 / C 类型名 / Lua 关键字始终保留英文原名，不翻译、不加中文括注之外的别名。**

写作者（包括各并行 subagent）必须严格遵守本表用词，禁止对同一概念使用不同的中文译法。

## 核心角色

| 英文 | 中文术语 | 说明 |
|---|---|---|
| Lua VM | Lua 虚拟机 | 执行编译后 Lua 字节码的运行时 |
| `lua_State *L` | Lua/C API 入口句柄 | 一次 Lua 解释器实例的完整上下文；几乎所有 C API 函数的第一个参数 |
| Lua C API | Lua C API | 不翻译，统一写作 "Lua C API" |
| Lua Stack | Lua Stack（栈） | `L` 内部的一段值数组，是 Lua 与 C 之间**唯一**的数据交换通道 |
| C Call Stack | C 调用栈 | 操作系统/编译器意义上的函数调用栈，与 Lua Stack 是两回事，注意不要混淆 |
| Native C World | C 原生世界 | 普通 C 变量、指针、结构体所在的世界，Lua Stack 之外 |

## Stack 相关

| 英文 | 中文术语 |
|---|---|
| Stack top | 栈顶 |
| Stack bottom | 栈底 |
| Positive index | 正索引（从栈底数，1 起始） |
| Negative index | 负索引（从栈顶数，-1 起始） |
| Stack Balance | 栈平衡 |
| Stack Contract | 栈契约（一个 C 函数对 Stack 的"进入前 / 离开后"约定） |
| Temporary value | 临时值 |
| Argument | 参数 |
| Return value / Result | 返回值 |

## 动作分类（贯穿 API Map 与所有章节）

| 英文 | 中文术语 | 含义 |
|---|---|---|
| `lua_push*` | Push 系列 | C Value -> Stack |
| `lua_to*` | To/Read 系列 | Stack -> C Value |
| `lua_is*` | 类型查询 | 只读判断，不报错 |
| `luaL_check*` | 校验系列 | 校验失败直接抛 Lua Error（`luaL_error`） |
| `lua_get*`（`lua_getfield` / `lua_gettable` / `lua_getglobal` 等） | Get 系列 | Lua Object -> Stack |
| `lua_set*`（`lua_setfield` / `lua_settable` / `lua_setglobal` 等） | Set 系列 | Stack -> Lua Object |
| `lua_call` / `lua_pcall` | 调用系列 | Stack 上的参数 + 函数 -> 触发 Lua VM 执行 -> Stack 上的返回值 |

## 对象模型

| 英文 | 中文术语 |
|---|---|
| Table | Table（不译"表"，避免与数据库表混淆） |
| Function（Lua 侧） | Lua Function |
| C Function | C Function |
| Closure | Closure（闭包） |
| Upvalue | Upvalue（上值） |
| Userdata | Userdata |
| Light userdata | Light Userdata |
| Full userdata | Full Userdata |
| Metatable | Metatable（元表） |
| Metamethod（如 `__index` / `__gc`） | Metamethod（元方法），具体写作 `__index` 等原名 |
| Registry | Registry（注册表），特指 `LUA_REGISTRYINDEX` 指向的那张全局私有 Table |
| Reference（`luaL_ref` 产生的整数） | Registry Reference（注册表引用） |

## 生命周期 / 控制流

| 英文 | 中文术语 |
|---|---|
| Lifecycle | 生命周期 |
| Garbage Collection (GC) | GC（垃圾回收），不额外翻译 |
| `__gc` | `__gc`（GC Metamethod），中文描述为"GC 时触发的清理钩子" |
| Protected call | 保护调用（即 `lua_pcall`） |
| Normal call | 普通调用（即 `lua_call`，出错时会 `longjmp`/直接中止宿主程序，除非宿主自己再包一层 `lua_pcall`） |
| Error propagation | 错误传播 |
| Data Flow | 数据流 |
| Control Flow | 控制流 |
| Data Plane | 数据平面（指 Lua Stack，只搬运数据不决定跳转） |

## 写作约定

1. 每章 Markdown 的小节标题固定使用九段式（用户原文顺序）：
   `直觉` -> `Architecture` -> `完整代码` -> `Stack Diagram` -> `Data Flow` -> `Lifecycle` -> `Failure` -> `Debugging` -> `Mental Model`。
2. ASCII 图使用 `+ - |`，禁止复杂 Unicode 边框；文字左对齐；方框对称。复杂关系图改用 Mermaid（`graph`/`flowchart`），节点 ID 用驼峰或下划线，禁止 ID 中出现空格。
3. 所有可编译示例遵循 `Makefile.rules` 里定义的"宿主程序"约定：一个 `main()` 创建 `lua_State*`，注册本章要讲的 C 能力，然后 `luaL_dofile()` 运行同目录下的配套 `.lua` 脚本并打印结果。构建方式统一为：
   ```makefile
   GUIDE_ROOT := $(abspath ../../..)   # projects/xx/ 下改成 $(abspath ../..)（projects/xx/ 到仓库根是 2 层）
   include $(GUIDE_ROOT)/Makefile.common
   TARGET := xxx_demo
   SRCS := xxx_demo.c
   include $(GUIDE_ROOT)/Makefile.rules
   ```
   验收标准：`make run` 必须成功编译并执行，输出符合章节叙述的预期结果。
4. `return N;`（C 函数返回值）永远描述为"告诉 Lua VM：从当前 Stack 顶部数 N 个值就是这次调用的返回值"，不得写成"C 函数的返回值是 N"或类似会与普通 C `return` 语义混淆的措辞。
5. Stack index 一律视为**本次 C 函数调用内的临时坐标**，不得在文字或代码注释中把它当作可跨调用保存的对象引用；需要跨调用持有 Lua Object 时必须使用 `luaL_ref(L, LUA_REGISTRYINDEX)`。
