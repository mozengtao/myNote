# 22. 统一 Data Flow：Lua Stack 是 Data Plane，`call`/`pcall` 是 Control Flow

> 对应用户需求 Stage 24。本章的目的只有一个：把"数据往哪流动"和
> "接下来执行哪段代码"这两件经常被混在一起讲的事，彻底拆开。

## 22.1 为什么要拆开 Data Flow 和 Control Flow（Why）

Chapter 01-19 几乎每一章都在讲某一种 API（`lua_push*`、`lua_to*`、
`lua_getfield`、`lua_pcall`……），但从来没有明确回答过一个问题：
**这些 API 里，哪些只是在"搬东西"，哪些是在"决定接下来跑哪段代码"？**

这个区分很重要，因为几乎所有"莫名其妙"的 Lua/C bug，一旦分清楚
"这一步是 Data Flow 还是 Control Flow"，排查方向就会立刻清晰：

- Data Flow 出问题 -> 去查 Stack 上的内容、类型、位置（Chapter 24
  的 Failure 1-5、9 全部属于这一类）。
- Control Flow 出问题 -> 去查"谁调用了谁、错误在哪一层被捕获"
  （Chapter 24 的 Failure 6、10 属于这一类）。

## 22.2 统一 Data Flow 图

```text
                Lua Value
                    |
                    v
             +-------------+
             | Lua Stack   |
             +-------------+
               |     |     |
          lua_to*   get*  call
               |     |     |
               v     v     v
               C    Lua   Lua VM
               |
           lua_push*
               |
               v
             +-------------+
             | Lua Stack   |
             +-------------+
                    |
                    v
                Lua Value
```

三条从 Lua Stack 出发的箭头，代表一个 Stack 上的值接下来可能去的
三个方向：

1. **`lua_to*`（Read/To 系列）**：离开 Stack，变成一个纯粹的 C
   变量（`int`/`char*`/`double`……）。这一步之后，数据已经彻底
   进入 Native C World，Lua 那边"随便怎么改"都不会再影响这个 C
   变量了（字符串等需要拷贝的类型除外的语义细节见 Chapter 05）。
2. **`get*`（`lua_getfield`/`lua_rawget`/`lua_next` 等）**：值仍然
   停留在"Lua Object"这个身份里——继续被当作 Table/Function/
   Userdata 使用，可以再往下 `get*` 一层（比如 Table 里嵌 Table），
   而不是被转换成某个具体的 C 原始类型。
3. **`call`（`lua_call`/`lua_pcall`）**：这条箭头指向 **Lua VM**，
   不是指向某个"值"——因为这一步根本不是在搬运数据，而是在
   **决定接下来跳转到哪段字节码去执行**。

底部对称的另一半（`lua_push*` 把 C 值重新变成 Stack 上的 Lua
Value，再被 Lua 取走）是同一套逻辑的镜像，不重复解释。

## 22.3 Lua Stack 是 Data Plane

**直觉**：把 Lua Stack 想象成机场的行李传送带——它只负责把行李
（数据）从一个地方搬到另一个地方，传送带本身不会"决定飞机飞去
哪里"。真正决定"接下来去哪"的是登机口的调度系统（对应
`lua_call`/`lua_pcall`），传送带（Stack）对这件事一无所知，也
无权干预。

具体到 API 层面：

| 动作分类 | 例子 | 做的事 | 是否读得懂"接下来该执行什么" |
|---|---|---|---|
| Read/To 系列 | `lua_tointeger`、`lua_tostring` | Stack -> C | 否，纯数据搬运 |
| Push 系列 | `lua_pushinteger`、`lua_pushstring` | C -> Stack | 否，纯数据搬运 |
| Get 系列 | `lua_getfield`、`lua_getglobal`、`lua_rawgeti` | Lua Object -> Stack | 否，纯数据搬运 |
| Set 系列 | `lua_setfield`、`lua_setglobal` | Stack -> Lua Object | 否，纯数据搬运 |
| 类型查询 | `lua_type`、`lua_is*` | 只读判断 | 否，不产生任何跳转 |
| 校验系列 | `luaL_checkinteger` | 校验失败才报错（这是唯一的例外，见 22.4 结尾） | 部分是（报错会触发 Control Flow） |

前 5 类无一例外，全部只是在"搬东西"或者"看一眼"，没有一个会让
Lua VM 跳转去执行别的代码。这正是"Data Plane"的含义：**Lua
Stack 本身不做任何"接下来执行哪段代码"的判断，它只负责让数据在
Lua 和 C 之间可见、可读、可写。**

## 22.4 `lua_call` / `lua_pcall` 才是 Control Flow

反过来看 `lua_call`/`lua_pcall`：它们表面上"看起来"也在操作
Stack（消耗掉函数和参数、留下返回值），但这只是它们的"副作用"，
**真正的核心动作是让 Lua VM 从当前位置跳转到另一段字节码去执行**：

```text
lua_call(L, nargs, nresults)
       |
       +-- 数据层面：从 Stack 上消耗 nargs+1 个值（函数+参数），
       |             最终留下 nresults 个值（Data Flow 副作用）
       |
       +-- 控制层面：跳转执行这个 Lua Function 的字节码，
                     出错时 longjmp 穿透当前 C 栈帧
                     （这才是 Control Flow 的核心）

lua_pcall(L, nargs, nresults, msgh)
       |
       +-- 数据层面：同上
       |
       +-- 控制层面：跳转执行；出错时 **不** 穿透，而是让 longjmp
                     停在这次 lua_pcall 建立的保护边界，
                     把错误对象转换成一个返回状态码
                     （Control Flow 的"路由选择"）
```

`luaL_checkinteger` 之类的校验系列函数是一个值得单独说明的例外：
它们平时看起来只是"读数据"，但校验失败那一刻，内部会调用
`luaL_error` -> `lua_error` -> `longjmp`，瞬间从 Data Flow 切换成
Control Flow（详见 Chapter 18）。也就是说，**同一个函数在"成功"
和"失败"两条路径上，可能分别属于 Data Flow 和 Control Flow**——
这也是 Chapter 24 里很多 Failure 案例容易被误判的原因之一。

## 22.5 用真实例子重新过一遍：Chapter 09 + Chapter 10

拿 Chapter 09 的 `register_handler` 和 Chapter 10 的 `emit_event`
（内部调用同一套 `invoke_handler` 逻辑）逐行标注 Data Flow /
Control Flow：

```39:60:/home/morrism/code_extra/wireshark-plugins/lua-c-interop-guide/chapters/09-lua-function-to-c-registry/code/register_demo.c
static int l_register_handler(lua_State *L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);

    /* 如果之前已经注册过一个 handler，先把旧引用释放掉，
     * 避免每次 register_handler 都在 Registry 里留下一个再也
     * 用不到的槽位（这属于本章 Failure 部分要讲的"引用泄漏"）。 */
    if (handler_ref != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, handler_ref);
    }

    /* luaL_ref 做两件事：
     *   1. 把 Stack 顶部的值（此刻正是 Stack 位置 1 上的那个 Function，
     *      luaL_checktype 并不会移动它，它还在栈顶）搬进 Registry Table；
     *   2. 把它从 Stack 上弹出，返回一个整数——这个整数就是以后
     *      找回它的"钥匙"。
     */
    handler_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    printf("[C] register_handler: 已保存，Registry Reference = %d\n",
           handler_ref);
    return 0; /* 告诉 Lua VM：这次调用没有返回值 */
}
```

| 代码行 | 分类 | 原因 |
|---|---|---|
| `luaL_checktype(L, 1, LUA_TFUNCTION)` | Data Flow（成功路径） | 只读判断，校验通过时不产生任何跳转；失败时会切换成 Control Flow（`luaL_error`） |
| `luaL_unref(L, LUA_REGISTRYINDEX, handler_ref)` | Data Flow | 本质是对 Registry（一张 Lua Table）做 Set 系列操作，纯数据层面的清理 |
| `luaL_ref(L, LUA_REGISTRYINDEX)` | Data Flow | Stack -> Registry Table，同样是 Set 系列的变体，不涉及任何跳转 |
| `return 0;` | Data Flow 的收尾 | 声明"Stack 契约"，告诉 VM 这次没有返回值 |

再看 `invoke_handler`（`fire_handler`/`emit_event` 共用的核心）：

```68:89:/home/morrism/code_extra/wireshark-plugins/lua-c-interop-guide/chapters/09-lua-function-to-c-registry/code/register_demo.c
static void invoke_handler(lua_State *L, const char *msg) {
    if (handler_ref == LUA_NOREF) {
        printf("[C] 尚未注册任何 handler，跳过\n");
        return;
    }

    /* 第 1 步：GET —— 用 Registry Reference 把当年存进去的
     * Lua Function 重新 Push 到 Stack 顶部。此刻它才重新拥有一个
     * 有效的 Stack index，但这个 index 只在接下来几行代码内有效。 */
    lua_rawgeti(L, LUA_REGISTRYINDEX, handler_ref);

    /* 第 2 步：把参数 Push 到 Stack 上，函数和参数必须紧邻。 */
    lua_pushstring(L, msg);

    /* 第 3 步：CALL —— 用保护调用触发 Lua VM 执行这个 Function。
     * 1 个参数、期望 0 个返回值。如果 Lua 侧 error()，pcall 会把
     * 错误对象留在 Stack 顶部并返回非 0，而不会让整个宿主程序崩溃。 */
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        fprintf(stderr, "[C] handler 执行出错: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1); /* 把错误对象弹掉，保持 Stack Balance */
    }
}
```

| 代码行 | 分类 | 原因 |
|---|---|---|
| `lua_rawgeti(L, LUA_REGISTRYINDEX, handler_ref)` | Data Flow | Get 系列，Registry Table -> Stack，纯搬运 |
| `lua_pushstring(L, msg)` | Data Flow | Push 系列，C -> Stack |
| `lua_pcall(L, 1, 0, 0)` | **Control Flow** | 唯一一行真正决定"接下来去执行 Lua Function 的字节码"，并且决定了出错时错误在哪里被拦住 |
| `lua_tostring(L, -1)` | Data Flow | Read/To 系列，读错误对象 |
| `lua_pop(L, 1)` | Data Flow | 清理 Stack，不产生跳转 |

结论一目了然：**一个典型的 C -> Lua callback 函数里，90% 的代码
都是 Data Flow（准备参数、读取结果、清理现场），真正的 Control
Flow 往往只有那一行 `lua_pcall`**。这也解释了为什么 Chapter 24
的 10 个 Failure 里，跟 Control Flow 相关的只有 Failure 6 和
Failure 10 两个，剩下 8 个本质上都是 Data Flow 层面的 Stack/
Registry/生命周期问题。

## 22.6 Mental Model

> **Lua Stack 是 Data Plane，只负责让数据在 Lua 和 C 之间可见、
> 可读、可写，本身不做任何"接下来执行哪段代码"的判断；只有
> `lua_call`/`lua_pcall`（以及校验失败时的 `luaL_error`）才是
> Control Flow，真正决定了执行权跳到哪里、错误在哪一层被拦住。**
