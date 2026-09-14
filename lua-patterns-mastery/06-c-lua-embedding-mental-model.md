# 06 · C ↔ Lua Embedding Mental Model

> 前置阅读：[00-mental-model-overview.md](00-mental-model-overview.md)。本文不再讲某一个具体 Pattern，而是回到最底层——Lua 是怎么被"嵌"进 C 程序里的。01~05 五个 Pattern 目录里的所有示例，本质上都只是在下面这几组 C API 之上，用不同的方式组织调用时机和返回值用法。读完本文，你应该能一眼看出任意一个 Pattern 示例代码里，哪一行属于"C 调 Lua"、哪一行属于"Lua 调 C"、哪一行属于"错误边界"。

面向读者：熟悉 C/C++，第一次系统看 Lua embedding API，不需要重新解释 Lua 语言语法本身。

## 1. 统一图：C Host 与 Lua 之间只有一条通路

```text
                C Host
                  |
                  v
             lua_State
                  |
             Lua VM Stack
                  |
       +----------+----------+
       |          |          |
     number     string     table
       |          |          |
       +----------+----------+
                  |
                  v
                 Lua
```

- `C Host`：拥有主循环、真实资源（socket、文件、内存）、决定 Lua VM 何时存在。
- `lua_State`：一个 `struct lua_State *`，是 C 侧持有的"Lua 世界的全部句柄"——全局表、调用栈、GC 状态都挂在它上面。没有这个指针，C 完全无法访问任何 Lua 值。
- `Lua VM Stack`：C 与 Lua 之间**唯一**合法的数据交换通道。C 不能直接读写 Lua 的内部数据结构，只能通过一组 `lua_push*`/`lua_to*`/`lua_get*`/`lua_set*` 函数把值放到这个栈上，再让 Lua 侧或 C 侧的下一步操作去消费它。
- `number / string / table`：栈上能放的值的类型示例（完整列表还有 `boolean`/`nil`/`function`/`userdata`/`thread`），无论多复杂的数据，跨越 C/Lua 边界时都要先拆成这几种基本类型的组合。
- `Lua`：真正执行 `.lua` 脚本代码的地方，从 C 的角度看，它只在 `lua_pcall` 被调用的那一小段时间里"活"过来，其余时间只是 `lua_State` 里的一堆静态数据。

一句话记忆：**C 和 Lua 之间没有共享内存、没有直接的函数指针调用，只有"往同一个栈上放值，再由对方按约定的顺序取值"这一种通信方式。**

## 2. C 调 Lua：四步协议

标准序列固定为四步，顺序不能颠倒：

```text
lua_getglobal()
      |
      v
lua_push* 系列
      |
      v
  lua_pcall()
      |
      v
lua_get*/lua_to* 系列
```

1. **`lua_getglobal(L, "name")`**：把 Lua 全局环境里名为 `name` 的值（通常是一个函数）压到栈顶。这一步等价于"找到要调用的目标"。
2. **`lua_push*` 系列**（`lua_pushnumber` / `lua_pushstring` / `lua_pushnil` / `lua_pushboolean` ...）：把调用参数按顺序依次压栈，压栈顺序就是 Lua 函数收到的参数顺序。
3. **`lua_pcall(L, nargs, nresults, errfunc)`**：从栈顶往下数 `nargs` 个值是参数，再往下一个是函数本身，Lua 侧执行这次调用，执行完把最多 `nresults` 个返回值重新压回栈顶，同时把参数和函数本身从栈上清除。
4. **`lua_get*`/`lua_to*` 系列**（`lua_tonumber` / `lua_tostring` / `lua_toboolean` / `lua_istable` + `lua_getfield` ...）：从栈顶按顺序取出返回值，转换成 C 类型使用完之后，用 `lua_pop` 把它们从栈上清理掉。

### 栈变化示意（假设调用 Lua 里的 `add(a, b)`，期望 1 个返回值）

```text
调用前（假设栈原本是空的）
+-----+
|     |  <- 栈顶
+-----+

lua_getglobal(L, "add")
+-----+
| add |  <- 栈顶（索引 1 / -1）
+-----+

lua_pushnumber(L, 10)
+-----+-----+
| add | 10  |  <- 栈顶（索引 2 / -1）
+-----+-----+

lua_pushnumber(L, 20)
+-----+-----+-----+
| add | 10  | 20  |  <- 栈顶（索引 3 / -1）
+-----+-----+-----+

lua_pcall(L, 2, 1, 0)
-- 弹出 2 个参数 + 1 个函数，Lua 侧执行 add(10, 20)，
-- 把 1 个返回值重新压回栈顶
+-----+
| 30  |  <- 栈顶（索引 1 / -1），这就是 add(10, 20) 的返回值
+-----+

lua_tonumber(L, -1)  -- 取出 30
lua_pop(L, 1)         -- 清理，栈恢复为空
```

这正是 [01-extension/code/01-c-command-extension/main.c](01-extension/code/01-c-command-extension/main.c) 里 `dispatch_command` 的做法（只是换成用 `lua_rawgeti` 从注册表取函数，而不是 `lua_getglobal`，原因见第 4 节）：先把回调函数压栈，再把命令行参数逐个 `lua_pushstring`，然后 `lua_pcall(L, argc, 1, 0)`，最后用 `lua_tostring`/`lua_pop` 处理返回值和错误。

## 3. Lua 调 C：三种注册方式

Lua 脚本能调用的"C 函数"，本质上都是签名为 `int (*)(lua_State *L)` 的 `lua_CFunction`——参数和返回值都通过栈传递，返回值是"这次调用往栈上压了几个返回值"。把这样一个函数暴露给 Lua，有三种常见方式：

| 方式 | 典型写法 | 适用场景 |
|---|---|---|
| `lua_register()` | `lua_register(L, "register_command", l_register_command);` | 只需要注册**一两个**全局函数，不需要组织成模块，写起来最直接 |
| `lua_pushcfunction()` + `lua_setglobal()`/`lua_setfield()` | `lua_pushcfunction(L, fn); lua_setglobal(L, "name");` 或 `lua_pushcclosure(L, fn, n)` 带 upvalue 后再 `setfield` | 需要**带 upvalue 的闸包**（例如把插件名字绑进函数里，见下方 `host_log` 例子），或者要把函数塞进某个 table 的字段，而不是全局变量 |
| `luaL_newlib()` | `luaL_newlib(L, mylib_funcs); lua_setglobal(L, "mylib");` （`mylib_funcs` 是一个 `luaL_Reg[]` 数组） | 需要一次性注册**一整组相关函数**，组织成一个模块表（`mylib.foo()`、`mylib.bar()`），是官方推荐的"库"注册方式 |

`lua_register` 实际上是 `lua_pushcfunction` + `lua_setglobal` 的简写宏，`luaL_newlib` 内部是对一整个 `luaL_Reg` 数组循环调用类似的注册逻辑。三者的差别只是"注册几个函数"和"要不要挂 upvalue/放进子表"，底层机制完全一样。

一个带 upvalue 的真实例子：[05-plugin/code/01-c-plugin-manager/plugin_manager.c](05-plugin/code/01-c-plugin-manager/plugin_manager.c) 里的 `register_host_api`：

```c
lua_pushstring(L, plugin_name);      /* upvalue 1: 插件名 */
lua_pushcclosure(L, l_host_log, 1);
lua_setglobal(L, "host_log");
```

这里没有用 `lua_register`，因为每个插件的 `host_log` 需要"记住自己是哪个插件"——`lua_pushcclosure` 把 `plugin_name` 绑成这个 C 函数的第 1 个 upvalue，`l_host_log` 内部用 `lua_upvalueindex(1)` 取回它。而 [01-extension/code/01-c-command-extension/main.c](01-extension/code/01-c-command-extension/main.c) 里的 `register_command` 不需要任何 upvalue，所以直接用最简单的 `lua_register(L, "register_command", l_register_command)`。

## 4. Callback 模式：为什么必须用注册表引用，不能只存函数指针

```text
C stores Lua function
        |
        v
   event happens
        |
        v
 C calls Lua callback
        |
        v
   Lua executes
```

这四步看起来很直观，但第一步"C stores Lua function"藏着一个容易踩的坑：**Lua 的函数不是 C 意义上的函数指针，它是 `lua_State` 栈上的一个值（一个 closure 对象），生命周期由 Lua 的垃圾回收器（GC）管理**。如果 C 侧只是临时把它从栈上"看一眼"就存一个裸的地址下来，下一次 GC 扫描时完全可能把这个 closure 判定为"没有任何东西引用它"而回收掉——之后 C 再试图调用这个地址，就是典型的 use-after-free。

正确做法是调用 `luaL_ref(L, LUA_REGISTRYINDEX)`：把当前栈顶的值复制一份，存进一个叫"注册表"（Registry）的特殊全局 table 里，返回一个整数引用 ID。这个整数 ID 是纯粹的 C 数据（可以放进 `struct`、数组、长期持有），而且**只要这个引用还在注册表里，Lua GC 就不会回收对应的函数**，因为注册表本身永远被 GC 视为"根对象"的一部分。需要真正调用这个函数时，用 `lua_rawgeti(L, LUA_REGISTRYINDEX, ref)` 把函数值重新压回栈顶，走前面第 2 节的四步协议正常调用；不再需要时，用 `luaL_unref` 释放引用，让 GC 有机会回收它。

两个真实例子：

- [01-extension/code/01-c-command-extension/main.c](01-extension/code/01-c-command-extension/main.c)：`l_register_command` 里 `lua_pushvalue(L, 2); int ref = luaL_ref(L, LUA_REGISTRYINDEX);` 把用户注册的命令回调存成引用，`dispatch_command` 里再用 `lua_rawgeti(L, LUA_REGISTRYINDEX, entry->lua_ref)` 取回来调用——这正是"C stores Lua function"到"C calls Lua callback"之间必须经过注册表的完整闭环。
- [05-plugin/code/01-c-plugin-manager/plugin_manager.c](05-plugin/code/01-c-plugin-manager/plugin_manager.c)：这里走的是另一条同样合法的路径——每个插件独占一个 `lua_State`，`on_event`/`plugin_init`/`plugin_shutdown` 直接是该 `lua_State` 的全局函数，`dispatch_event` 用 `lua_getglobal(L, "on_event")` 现取现用，不需要额外的注册表引用，因为"存函数"这件事已经被"每个插件独占一个一直存活的 `lua_State`"替代了——只要 `lua_State` 不关闭，它的全局表和其中的函数就不会被回收。这说明"用注册表存引用"和"给每个逻辑单元一个独立的、长期存活的 `lua_State`"是解决"如何安全长期持有 Lua 函数"这个问题的两种不同策略，具体选哪种取决于 Pattern 的颗粒度（单个回调 vs 一整个独立模块）。

## 5. Error Boundary：`pcall` 是唯一的错误穿越机制

```text
C
 |
 +-- lua_pcall()
 |
 +-- Lua error
       |
    captured
```

Lua 语言内部有 `error()` / `pcall()` / `xpcall()`：Lua 代码里调用 `error(msg)` 会像抛异常一样中断当前执行流，直到被某个 `pcall`/`xpcall` 捕获；如果一路没人捕获，这个错误最终会传到 C 侧的调用边界。

C 侧调用 Lua 代码，永远应该用 `lua_pcall`，而不是 `lua_call`：`lua_call` 一旦 Lua 侧出错会直接 `longjmp` 到最近的错误处理点，如果 C 侧没有设置任何保护，等价于让程序退出；`lua_pcall` 会在内部设置好保护点，把任何错误（语法错误、运行时错误、`error()` 显式抛出的错误）都转换成一次正常的函数返回——返回值不是 `LUA_OK`，栈顶被放上一条错误信息字符串，C 侧可以像处理普通返回值一样读取它、决定怎么办（打印日志、跳过、重试），**整个 C 进程不会因为一段 Lua 代码写错而崩溃**。

这就是为什么本仓库前面 5 个 Pattern 的所有 C 示例，凡是调用 Lua 的地方，无一例外用的是 `lua_pcall` 而不是 `lua_call`（见 [01-extension/code/01-c-command-extension/main.c](01-extension/code/01-c-command-extension/main.c) 的 `dispatch_command`、[02-policy/code/01-c-server-access-policy/server.c](02-policy/code/01-c-server-access-policy/server.c)、[03-configuration/code/01-c-app-configuration/main.c](03-configuration/code/01-c-app-configuration/main.c)、[04-rule-engine/code/01-firewall-rule-engine/main.c](04-rule-engine/code/01-firewall-rule-engine/main.c) 都是同一个模式）。

这个错误边界在 Plugin 和 Rule Engine 两种 Pattern 里尤其关键，因为它们天生要面对"多个互相独立的单元，其中一个出错不该拖垮其它单元"的需求：

- [05-plugin/code/01-c-plugin-manager/plugin_manager.c](05-plugin/code/01-c-plugin-manager/plugin_manager.c) 的 `dispatch_event` 在 `for` 循环里逐个插件调用 `on_event`，每次调用都单独 `lua_pcall`；[05-plugin/code/01-c-plugin-manager/plugin_b.lua](05-plugin/code/01-c-plugin-manager/plugin_b.lua) 故意在收到 `"boom"` 事件时 `error(...)`，`plugin_manager.c` 捕获到这次失败后只是打印错误、`continue` 到下一个插件——`plugin_a.lua` 完全不受影响，同一批事件里其余插件照常收到通知。如果这里用的是 `lua_call` 而不是 `lua_pcall`，Plugin B 的一次 bug 就会直接终止整个 Manager 进程，这正是"一个插件报错不能拖垮其它插件"这条设计约束在代码层面的落地。
- [04-rule-engine/00-why-and-architecture.md](04-rule-engine/00-why-and-architecture.md) 里描述的 Rule Engine 架构——`Lua Rule Engine` 内部遍历 `Rule1`/`Rule2`/`Rule3` 逐条做 `match`/`action` 判断——同样要求"一条规则的 `match` 或 `action` 报错，不能让整条规则链停止匹配剩余规则"，其代码落地方式与 Plugin 完全一致：在遍历规则表的循环内部，对每条规则的求值单独包一层 `pcall`（Lua 侧的 `pcall`）或者由 Host 侧对整个规则引擎入口函数调用做 `lua_pcall`，出错时跳过当前规则、继续下一条，而不是让整个引擎入口函数的报错中断所有后续规则的判断。

## 6. 五个 Pattern 分别典型使用了哪些 C API

| Pattern | 典型 API | 对应真实代码文件 |
|---|---|---|
| Extension | `lua_register()` 注册入口 + `luaL_ref`/`lua_rawgeti` 保存并调用回调 | [01-extension/code/01-c-command-extension/main.c](01-extension/code/01-c-command-extension/main.c) |
| Policy | `lua_getglobal()` 取决策函数 + `lua_pushstring`/`lua_pushnumber` 传场景事实 + `lua_pcall()` + `lua_toboolean`/`lua_tostring` 取 Decision | [02-policy/code/01-c-server-access-policy/server.c](02-policy/code/01-c-server-access-policy/server.c) |
| Configuration | `luaL_loadfile()` + `lua_pcall()` 执行脚本一次 + `lua_getglobal()`/`lua_getfield()` 逐字段读出 table，转换成 C struct | [03-configuration/code/01-c-app-configuration/main.c](03-configuration/code/01-c-app-configuration/main.c) |
| Rule Engine | `lua_newtable()`/`lua_setfield()` 把 Input 组装成 table 传入 + `lua_pcall()` 调用规则引擎入口 + 遍历返回的 Action table | [04-rule-engine/code/01-firewall-rule-engine/main.c](04-rule-engine/code/01-firewall-rule-engine/main.c) |
| Plugin | 每插件独立 `luaL_newstate()`/`lua_close()` + `lua_pushcclosure()` 带 upvalue 注册 Host API + `lua_pcall()` 隔离每次 `on_event` | [05-plugin/code/01-c-plugin-manager/plugin_manager.c](05-plugin/code/01-c-plugin-manager/plugin_manager.c) |

下一篇 [07-cross-pattern-comparison.md](07-cross-pattern-comparison.md) 会把这五种 Pattern 放在同一张表里横向对比它们在"角色分工"层面（而不是本文的"API 使用"层面）的差异。
