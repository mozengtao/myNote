# 24. Failure Analysis：10 个最常见的 Lua/C Bug

> 对应用户需求 Stage 26。本章是全书 Failure 案例的汇总与升级：
> Chapter 01-19 每一章的 Failure 小节都只演示"这一章讲的那个 API
> 用错了会怎样"，本章把最常见的 10 种错误集中到一起，并且**尽量
> 不靠脑补描述报错信息**——8 个案例都写了真实反例代码，实际编译
> 运行，把终端里出现的真实文字原样记录下来；只有 2 个（Failure 5、
> 8）因为本质上是"设计选择"或者"未定义行为"，难以用一个安全、
> 确定性的小程序稳定复现，标注为"未实测，理论分析"。
>
> 全部反例代码在 `chapters/24-failure-analysis/code/`，`make run`
> 会依次编译并运行 8 个反例程序。

## 24.0 为什么最常见的 Bug 长这个样子（Why）

Chapter 22 已经说明：一个典型 C Function 里，大部分代码是 Data
Flow（搬运数据），只有少数几行是 Control Flow（决定执行路径）。
这个比例直接决定了 Bug 的分布：**绝大多数 Lua/C Bug 出在 Data
Flow 层面**——用错了 Stack 位置、忘记清理临时值、返回值数量搞错、
把一个只在当前调用内有效的坐标当成了永久句柄。真正跟 Control
Flow 相关的错误（`lua_call` 穿透、Registry Reference 失效后继续
调用）数量少得多，但一旦发生，后果通常更严重（进程直接被中止，
而不只是"数据不对"）。

下面按照这个分布，把 10 个案例逐一过一遍。

## 24.1 Failure 1：错误的 Stack index

**场景**：C 函数只声明/期望处理固定数量的参数，却直接用一个越界
的正索引去读，比如 Lua 侧只传了 2 个参数（Stack 上只有 `[1] [2]`），
C 侧却写了 `lua_tostring(L, 3);`。

**反例代码**（`code/failure01_wrong_stack_index.c`，调用参数见
`code/failure01_wrong_stack_index.lua`：`bad_index("a", "b")`）：

```17:36:/home/morrism/code_extra/wireshark-plugins/lua-c-interop-guide/chapters/24-failure-analysis/code/failure01_wrong_stack_index.c
static int l_bad_index(lua_State *L) {
    printf("[C] 进入时 lua_gettop(L) = %d\n", lua_gettop(L));
    printf("[C] index 1: type = %s\n", luaL_typename(L, 1));
    printf("[C] index 2: type = %s\n", luaL_typename(L, 2));
    printf("[C] index 3（越界，本次调用根本不存在这个参数!): type = %s\n",
           luaL_typename(L, 3));

    const char *s = lua_tostring(L, 3);
    printf("[C] lua_tostring(L, 3) = %s\n", s ? s : "(NULL)");

    int isnum = 0;
    lua_Integer n = lua_tointegerx(L, 3, &isnum);
    printf("[C] lua_tointegerx(L, 3) = %lld, isnum = %d\n", (long long)n, isnum);

    printf("[C] lua_isnone(L, 3) = %d\n", lua_isnone(L, 3));
    printf("[C] lua_isnil(L, 3)  = %d  (注意：none 和 nil 在 lua_is* 上表现不同)\n",
           lua_isnil(L, 3));

    return 0;
}
```

**实测结果**（`./failure01_wrong_stack_index`，退出码 0）：

```text
[C] 进入时 lua_gettop(L) = 2
[C] index 1: type = string
[C] index 2: type = string
[C] index 3（越界，本次调用根本不存在这个参数!): type = no value
[C] lua_tostring(L, 3) = (NULL)
[C] lua_tointegerx(L, 3) = 0, isnum = 0
[C] lua_isnone(L, 3) = 1
[C] lua_isnil(L, 3)  = 0  (注意：none 和 nil 在 lua_is* 上表现不同)
```

真实结论（不是猜测）：Lua 5.4 对一个越界的正索引，**不会崩溃**，
而是把它当成一个"无效值"（类型是 `LUA_TNONE`，`lua_typename`
显示为 `"no value"`，注意它和 `nil`——`LUA_TNIL`——是两种不同的
类型，`lua_isnone` 为真而 `lua_isnil` 为假）：

- `lua_tostring` 在这种值上返回 `NULL`；
- `lua_tointegerx` 静默返回 `0`（`isnum` 输出参数会是 0，说明
  "转换失败"，但如果调用者没检查这个输出参数，看到的只是一个
  "看起来合法"的 `0`）。

这正是这类 bug 最危险的地方：**它不报错、不崩溃，只是安静地给
你一个错误的数据**，很容易在后续逻辑里被当成"合法的 0"或者
"空字符串"继续往下传，直到很远的地方才暴露出无关的症状。

**正确修复**：进入函数第一件事永远是 `luaL_checktype`/
`luaL_checknumber`/`luaL_checkstring` 之类的校验系列，或者至少
`luaL_argcheck`，绝不裸用 `lua_to*` 去读一个"可能不存在"的索引；
更进一步，函数入口处用 `lua_gettop` 确认参数数量是最便宜的第一
道防线（详见 Chapter 25 Debugging Checklist 第 1 条）。

## 24.2 Failure 2：忘记 pop（`lua_getfield` 后不清理）

**场景**：用 `lua_getfield` 把 Table 的一个字段读到 Stack 顶部，
读完之后没有 `lua_pop`，紧接着又用相对索引 `-1` 去做下一步操作，
以为 `-1` 还是原来的 Table，但实际上 `-1` 已经变成了刚刚读出来
的那个字段值。

**反例代码**（`code/failure02_forgot_pop.c`，调用
`configure_leaky({ port = 8080, host = "127.0.0.1" })`）：

```19:36:/home/morrism/code_extra/wireshark-plugins/lua-c-interop-guide/chapters/24-failure-analysis/code/failure02_forgot_pop.c
static int l_configure_leaky(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    printf("[C] 进入时 lua_gettop(L) = %d\n", lua_gettop(L));

    lua_getfield(L, 1, "port");           /* Stack: [table, port] */
    int port = (int)luaL_checkinteger(L, -1);
    printf("[C] port = %d，读完之后 lua_gettop(L) = %d"
           "（预期应该是 1，多出来的这 1 个就是没 pop 的 port）\n",
           port, lua_gettop(L));

    /* BUG：应该在上面先 lua_pop(L, 1)，再回到 table（index 1）读 host。
     * 忘了 pop，于是这里的 -1 实际是数字 port，不是 table。 */
    lua_getfield(L, -1, "host");
    const char *host = lua_tostring(L, -1);
    printf("[C] host = %s\n", host ? host : "(NULL)");

    return 0;
}
```

**实测结果**（`./failure02_forgot_pop`，退出码 1）：

```text
[C] 进入时 lua_gettop(L) = 1
[C] port = 8080，读完之后 lua_gettop(L) = 2（预期应该是 1，多出来的这 1 个就是没 pop 的 port）
lua error: attempt to index a number value
```

真实结论：`lua_gettop` 的变化清楚地暴露了"多出来一个没清理的
值"；紧接着对一个数字（`8080`）调用 `lua_getfield`，Lua 5.4
的真实报错是 **`attempt to index a number value`**——因为数字
没有默认的 `__index` metatable（跟字符串不同，字符串有一张内置
的 metatable 把 `__index` 指向 `string` 库，所以对字符串
`lua_getfield` 反而不会报错，只会静默拿到 `nil`，是一种更隐蔽的
变体）。这个 Lua Error 通过 `luaL_dofile` 被捕获，返回非
`LUA_OK`，`main` 里 `fprintf(stderr, ...)` 打印出来，进程以退出码
1 结束——不是崩溃，是一次"正常"的 Lua Error 传播。

**正确修复**：每一次 `lua_getfield`/`lua_gettable` 之后，只要
不再需要那个值，立刻 `lua_pop(L, 1)`；养成"get - use - pop"三步
不分离的习惯（Chapter 06/23 的 `l_configure` 就是这个习惯的
标准写法）。

## 24.3 Failure 3：错误的 return count

**场景**：

```c
lua_pushinteger(L, 10);
lua_pushinteger(L, 20);
return 1;
```

**问题**：Stack 从栈底到栈顶依次是 `10, 20`。`return 1` 的语义
是"告诉 Lua VM：从当前 Stack 顶部数 1 个值就是返回值"——Lua 最终
拿到的到底是 10 还是 20？

**反例代码**（`code/failure03_wrong_return_count.c`）：

```20:25:/home/morrism/code_extra/wireshark-plugins/lua-c-interop-guide/chapters/24-failure-analysis/code/failure03_wrong_return_count.c
static int l_two_values(lua_State *L) {
    (void)L;
    lua_pushinteger(L, 10);   /* Stack: [10] */
    lua_pushinteger(L, 20);   /* Stack: [10, 20] */
    return 1;                 /* 取栈顶 1 个值 = 20，10 被留在 Stack 上（脏数据） */
}
```

**实测结果**（`./failure03_wrong_return_count`，退出码 0）：

```text
单个接收: r =	20
两个接收: a =	20	 b =	nil
```

真实结论确认：`return 1` 拿到的是 **20**（栈顶那个），不是先
push 的 10——10 被永久留在了 Stack 上，成为一段"脏数据"（虽然
Lua VM 在函数返回时会根据"当时的 base + N"重新调整 Stack 顶部，
把这次调用留下的多余值截掉，所以不会真的"泄漏"到下一次调用，但
在这次调用内部，任何以为"10 是返回值之一"的逻辑都是错的）。用
`local a, b = two_values()` 进一步验证：`a` 是 20，`b` 是 `nil`
——因为这次调用只声明了 1 个返回值，Lua 侧多接收的变量自动补
`nil`，不会去"翻栈"找那个被丢弃的 10。

**正确修复**：`return N` 之前，永远用 Stack Diagram 确认"栈顶
往下数 N 个"到底是不是你想要的那几个值；如果中途 push 过任何
不该作为返回值的临时值，必须在 `return` 之前 `lua_pop` 掉，或者
用 `lua_remove`/`lua_insert` 调整顺序（见 Failure 4）。

## 24.4 Failure 4：返回 Stack 中错误的位置

**场景**：该 push 结果的地方没问题，但结果 push 完之后又多
push（或者顺序搞反）了别的临时值，导致 `return N` 读到的"栈顶"
其实是那个临时值，不是本该返回的结果。

**反例代码**（`code/failure04_wrong_return_position.c`）：

```16:25:/home/morrism/code_extra/wireshark-plugins/lua-c-interop-guide/chapters/24-failure-analysis/code/failure04_wrong_return_position.c
static int l_bad_position(lua_State *L) {
    int a = (int)luaL_checkinteger(L, 1);

    lua_pushinteger(L, a * 2);   /* Stack: [.., a*2]  <- 这才是我们想要的结果 */

    /* BUG：调试时顺手多 push 了一个东西，忘记在 return 之前 pop 掉 */
    lua_getglobal(L, "print");    /* Stack: [.., a*2, print] */

    return 1; /* return 1 = 取栈顶 1 个值 = print 这个函数对象，不是 a*2! */
}
```

**实测结果**（`./failure04_wrong_return_position`，输入
`bad_position(21)`，退出码 0）：

```text
type(r) =	function	 r =	function: 0x41f8b0
```

真实结论：`r` 根本不是期望的整数 `42`，而是全局函数 `print` 本身
（`type(r)` 是 `"function"`）。这类 bug 在调试阶段插入一行
"顺手看看某个全局变量/临时值"的代码时极容易发生，而且往往因为
后面 `return N` 的 `N` 没有跟着调整，编译器和 Lua 都不会报任何
警告或错误——纯粹是"数错了栈顶的位置"。

**正确修复**：在 `return N` 之前，只保留"确实要返回的那些值"在
Stack 顶部；任何调试用的临时 `lua_getglobal`/`lua_pushxxx`都必须
在 `return` 之前配对 `lua_pop`，或者干脆用独立的调试构建/日志
而不是往 Stack 上塞东西。

## 24.5 Failure 5：把 Stack index 当成长期 Object Reference（未重复实测，直接引用已验证的证据）

**场景**：把"某个值在这次调用里的 Stack 位置（一个整数，比如
`1`）"当成"以后随时可以用来找回这个值"的句柄，存进一个 C 全局
变量或结构体里，期待下一次进入 C 时还能用这个数字找到"同一个
值"。

这个案例在本书里**已经有真实的编译验证**，不需要重复写一份新
代码：

- Chapter 09 的 `register_demo.c` 正面证明了"正确做法"：一个
  Stack index 只在当前这次调用内有效，必须用 `luaL_ref(L,
  LUA_REGISTRYINDEX)` 把值提升为 Registry Reference，才能跨调用
  使用（该章节代码里 `handler_ref` 存的是 `luaL_ref` 返回的整数，
  不是任何 Stack index）。
- 本章 Failure 9（见 24.9）从反面**实测**证明了"错误做法"会
  发生什么：把 Stack index `1` 直接存进全局变量，下一次调用时
  这个位置上出现的是完全不同的值（`123` 变成了字符串 `"123"`
  被读出来，第三次调用甚至是一个布尔值）。

这两份已验证的证据放在一起，就是 Failure 5 的完整答案：**Stack
index 的有效期只到"当前这次 C 函数调用结束"为止**，想跨调用、
跨脚本执行、甚至跨越 `lua_close` 之前的任意时间点持有一个 Lua
Object，唯一正确的方式是 `luaL_ref`/`luaL_unref`。

**正确修复**：任何"我想在下次进入这个 C 函数时还能用到同一个
Lua 值"的需求，一律用 Registry Reference，不使用裸的整数索引。

## 24.6 Failure 6：用 `lua_call()` 导致 Lua Error 穿透 C

**场景**：完全不加任何 `lua_pcall` 保护，直接用 `lua_call` 调用
一个会 `error()` 的 Lua Function。

**反例代码**（`code/failure06_lua_call_unprotected.c`）：

```16:44:/home/morrism/code_extra/wireshark-plugins/lua-c-interop-guide/chapters/24-failure-analysis/code/failure06_lua_call_unprotected.c
static const char *BOOM_SRC =
    "function boom(a, b)\n"
    "  if b == 0 then error('boom: b is zero') end\n"
    "  return a / b\n"
    "end\n";

int main(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    if (luaL_dostring(L, BOOM_SRC) != LUA_OK) {
        fprintf(stderr, "load boom failed: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    printf("[C] 即将用 lua_call（没有任何 lua_pcall 保护）调用 boom(10, 0) ...\n");
    fflush(stdout);

    lua_getglobal(L, "boom");   /* Stack: [boom] */
    lua_pushinteger(L, 10);     /* Stack: [boom, 10] */
    lua_pushinteger(L, 0);      /* Stack: [boom, 10, 0] */

    lua_call(L, 2, 1);          /* boom 内部 error() -> 没有保护点可以接住 -> panic */

    printf("[C] 这一行永远不会被打印\n");
    lua_close(L);
    return 0;
}
```

**实测结果**（`./failure06_lua_call_unprotected`）：

```text
[C] 即将用 lua_call（没有任何 lua_pcall 保护）调用 boom(10, 0) ...
PANIC: unprotected error in call to Lua API ([string "function boom(a, b)..."]:2: boom: b is zero)
Aborted (core dumped)
```

进程退出码是 **134**（即 128 + 信号 6，`SIGABRT`）。

真实结论：`error()` 触发的 `longjmp` 一路找不到任何 `lua_pcall`
建立的保护边界，一直"穿透"到 `lua_State` 内部默认的 panic 处理
函数——它打印 `PANIC: unprotected error in call to Lua API`
加上原始错误信息，然后调用 `abort()`。`lua_call` 那一行之后的
`printf` 确实**从未被执行**（终端上完全没有出现"这一行永远不会
被打印"）——这不是"函数返回后继续走"的正常语义，而是彻底跳出了
当前 C 栈帧，宿主进程直接被中止，任何原本期望在那之后执行的
清理代码全部被跳过。

**正确修复**：任何"C 主动调用一个可能出错的 Lua Function"的场景，
一律使用 `lua_pcall`，检查返回状态码，绝不用 `lua_call`（除非
你能 100% 确定被调用的 Lua Function 不会 `error()`，且外层还有
更高一层的 `lua_pcall` 兜底——即便如此也建议直接改用
`lua_pcall`，多一层保护几乎没有额外成本）。

## 24.7 Failure 7：userdata 类型错误

**场景**：对一个挂着某个 metatable 的 Full Userdata，用一个错误
的 metatable 名字去调用 `luaL_checkudata`。

**反例代码**（`code/failure07_userdata_wrong_metatable.c`）：

```19:33:/home/morrism/code_extra/wireshark-plugins/lua-c-interop-guide/chapters/24-failure-analysis/code/failure07_userdata_wrong_metatable.c
static int l_new_widget(lua_State *L) {
    Widget *w = (Widget *)lua_newuserdata(L, sizeof(Widget));
    w->value = 42;
    luaL_getmetatable(L, "Widget");
    lua_setmetatable(L, -2);
    return 1;
}

static int l_use_as_gadget(lua_State *L) {
    /* BUG：这个 userdata 实际挂的 metatable 名字是 "Widget"，
     * 这里却错误地用 "Gadget" 去检查/取出它。 */
    Widget *w = (Widget *)luaL_checkudata(L, 1, "Gadget");
    printf("[C] value = %d\n", w->value);
    return 0;
}
```

**实测结果**（`local w = new_widget(); use_as_gadget(w)`，
退出码 1）：

```text
lua error: [string "local w = new_widget()..."]:2: bad argument #1 to 'use_as_gadget' (Gadget expected, got Widget)
```

真实结论：`luaL_checkudata` 校验失败时给出的错误信息非常明确，
直接点名"期望的 metatable 名字"和"实际的 metatable 名字"
（`Gadget expected, got Widget`），并且带上了函数名
`use_as_gadget` 和参数序号 `#1`——这是本章 10 个案例里报错信息
最"友好"的一个，因为 `luaL_checkudata` 内部就是按 Chapter 18 的
校验系列规范实现的。

**正确修复**：每一类 C Object 使用唯一、专属的 metatable 名字
字符串（通常直接用 C struct 的类型名，保证不会跟别的模块冲突），
并且在每一个操作该类型的 C Function 里，一律通过
`luaL_checkudata` 校验，不要绕过它直接 `lua_touserdata` + 强转
指针类型（那样类型错误时不会报错，而是直接把内存当成错误的
struct 布局去解释，属于比这里演示的更严重的一类问题）。

## 24.8 Failure 8：userdata 生命周期错误（未实测，理论分析）

**场景**：`__gc` 里重复释放同一块内存；或者 C 侧持有一个"曾经
对应某个 Lua Userdata"的裸指针，那个 Userdata 已经被 GC 回收，
C 却继续通过这个裸指针访问内存。

这个案例被标注为"未实测，理论分析"是因为它本质上是 C 语言层面
的**未定义行为**（double free / use-after-free）：

- 是否会立即崩溃、崩溃在哪一行、还是"看起来正常运行"若干次之后
  才在毫不相关的地方出错，取决于具体的内存分配器实现、当时堆
  的状态、编译器优化——写一个"每次都稳定复现同一种崩溃"的小
  程序本身就违背了 UB 的本质（不稳定才是 UB 的常态）。
- 故意构造这类崩溃需要精确控制 GC 时机（`lua_close`/`collectgarbage`
  的具体触发点），这类演示的教学价值远低于"讲清楚为什么会发生"。

**理论后果**：

```text
1. Lua 侧创建一个 userdata，__gc 元方法里 free(ptr)
2. Lua 侧那个 userdata 变量失效（比如被设为 nil，且没有其它引用）
3. 下一次 GC 周期，__gc 被调用，ptr 被 free
4. 情况 A（重复释放）：如果 __gc 因为某种逻辑错误被多次触发
   （比如同一个指针被两个不同的 userdata 共享，各自的 __gc
   都尝试 free 它），第二次 free 是对已经释放的内存再次 free
   —— glibc 的 malloc 实现通常会检测到这种损坏并直接
   abort（"free(): double free detected"），但也可能在更复杂的
   分配模式下表现为看似无关的堆损坏，很久之后才在别的地方崩溃。
5. 情况 B（释放后使用）：如果 C 侧另外保存了这个 ptr 的一份裸拷贝
   （没有通过 Stack/Registry，只是普通 C 变量），__gc 释放内存之后
   C 继续通过这份裸拷贝去读写——这是典型的 use-after-free，
   轻则读到垃圾数据，重则该内存已经被系统重新分配给别的对象，
   写操作会破坏那个完全无关对象的数据，且不一定马上触发任何
   报错或崩溃信号。
```

**正确修复**：

1. `__gc` 内部释放资源之后，把 userdata 内部的指针字段显式置为
   `NULL`，并且在每一次使用之前检查它是否为 `NULL`（防止同一个
   userdata 被误用两次触发释放逻辑，即使正常情况下 Lua 不会对
   同一个对象调用两次 `__gc`，这层防御几乎零成本）。
2. **不要在 C 侧另外保存一份"脱离 Lua 生命周期管理"的裸指针**
   长期使用；如果确实需要，必须自己在 Lua 侧同时持有一个
   Registry Reference，保证只要 C 侧还需要用这块内存，对应的
   Lua Userdata 就不会被 GC，两者的生命周期显式绑定在一起。
3. 需要提前主动释放资源时（比如一个 `close()` 方法），应该让
   `close()` 和 `__gc` 共用同一段"资源已释放"的判断逻辑（步骤 1
   的 `NULL` 检查），而不是各自独立实现一遍释放逻辑。

## 24.9 Failure 9：C 保存 Lua Object，但没有 Registry Reference

**场景**：C 把"这次调用里参数所在的 Stack 位置"（一个整数）存进
全局变量，以为这样就"保住了"那个参数对应的值；下一次进入这个
C 函数（一次完全独立的调用），那个位置上早已经是别的东西。

**反例代码**（`code/failure09_stack_index_as_reference.c`）：

```20:35:/home/morrism/code_extra/wireshark-plugins/lua-c-interop-guide/chapters/24-failure-analysis/code/failure09_stack_index_as_reference.c
static int saved_index = 0;

static int l_remember_bad(lua_State *L) {
    saved_index = 1; /* 以为"记住了参数1的位置"就等于"记住了这个值" */
    printf("[C] remember_bad: 记下 saved_index = %d，此刻这个位置的值 = %s\n",
           saved_index, lua_tostring(L, saved_index));
    return 0;
}

static int l_use_remembered_bad(lua_State *L) {
    const char *s = lua_tostring(L, saved_index);
    printf("[C] use_remembered_bad: Stack 位置 %d 现在的类型 = %s, "
           "lua_tostring 结果 = %s\n",
           saved_index, luaL_typename(L, saved_index), s ? s : "(NULL)");
    return 0;
}
```

调用序列：`remember_bad("first-call-value")` -> `use_remembered_bad(123)`
-> `use_remembered_bad(true)`。

**实测结果**（退出码 0）：

```text
[C] remember_bad: 记下 saved_index = 1，此刻这个位置的值 = first-call-value
[C] use_remembered_bad: Stack 位置 1 现在的类型 = string, lua_tostring 结果 = 123
[C] use_remembered_bad: Stack 位置 1 现在的类型 = boolean, lua_tostring 结果 = (NULL)
```

真实结论：第一次调用时，"位置 1" 对应的确实是那次调用传入的
`"first-call-value"`。但 `saved_index` 存的只是数字 `1` 这个
坐标本身——第二次调用传入 `123`（数字），Lua 的数字在
`lua_tostring` 下会被转换成字符串 `"123"`（Lua 的 `lua_tostring`
对数字有隐式转换，这一点本身没问题，但显然不再是当年那个
`"first-call-value"`）；第三次调用传入 `true`（布尔值），
`lua_tostring` 直接返回 `NULL`（布尔值不能转字符串，与
Chapter 05 的 `demo_pitfalls` 演示的 Failure 2 完全一致）。三次
调用，`saved_index` 这个数字从未改变，但它"指向"的东西每次都
不一样——因为它压根不是一个对象引用，只是"这次调用里第几个
参数"这一层坐标信息。

**正确修复**：与 Failure 5 完全相同的答案——需要跨调用保存的
是**值本身**，不是它在某次调用里恰好所在的坐标；正确做法是在
`remember_bad` 里立刻 `luaL_ref(L, LUA_REGISTRYINDEX)`，把结果
（一个 Registry Reference）存进全局变量，`use_remembered_bad`
里用 `lua_rawgeti(L, LUA_REGISTRYINDEX, ref)` 取回——这正是
Chapter 09 `register_demo.c` 的写法。

## 24.10 Failure 10：C callback 已被回收/失效，但 C 仍尝试调用

**场景**：`luaL_unref` 释放了一个 Registry Reference 之后，忘记
把保存这个引用的变量重置为 `LUA_NOREF`，后续代码继续拿着这个
"数值上没变、但已经失效"的整数去 `lua_rawgeti` + 调用。

**反例代码**（`code/failure10_stale_registry_ref.c`）：

```19:52:/home/morrism/code_extra/wireshark-plugins/lua-c-interop-guide/chapters/24-failure-analysis/code/failure10_stale_registry_ref.c
static int l_register_handler(lua_State *L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    handler_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    printf("[C] register_handler: ref = %d\n", handler_ref);
    return 0;
}

static int l_unregister_handler(lua_State *L) {
    (void)L;
    luaL_unref(L, LUA_REGISTRYINDEX, handler_ref);
    printf("[C] unregister_handler: 已经 luaL_unref(%d)，"
           "但 BUG 是这里忘记把 handler_ref 重置为 LUA_NOREF\n", handler_ref);
    /* 正确写法应该是紧接着一行：handler_ref = LUA_NOREF;
     * 这里故意不做，模拟"引用已失效但变量还留着旧值"的真实 bug。 */
    return 0;
}

static int l_fire_stale(lua_State *L) {
    (void)L;
    printf("[C] fire_stale: 继续用已经 unref 过的 ref = %d 尝试取回并调用\n",
           handler_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, handler_ref); /* 这个槽位已经被释放 */
    printf("[C] lua_rawgeti 取回的类型 = %s\n", luaL_typename(L, -1));

    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        printf("[C] 调用失败（这正是我们要观察的真实报错）: %s\n",
               lua_tostring(L, -1));
        lua_pop(L, 1);
    } else {
        printf("[C] 调用成功？不符合预期\n");
    }
    return 0;
}
```

**实测结果**（调用序列：`register_handler(function() ... end)` ->
`unregister_handler()` -> `fire_stale()`，退出码 0）：

```text
[C] register_handler: ref = 4
[C] unregister_handler: 已经 luaL_unref(4)，但 BUG 是这里忘记把 handler_ref 重置为 LUA_NOREF
[C] fire_stale: 继续用已经 unref 过的 ref = 4 尝试取回并调用
[C] lua_rawgeti 取回的类型 = number
[C] 调用失败（这正是我们要观察的真实报错）: attempt to call a number value
```

真实结论**和最初凭直觉猜测的不一样，这正是本章要求"实测而不是
脑补"的原因**：直觉上会以为 `luaL_unref` 之后那个槽位会变成
`nil`，`lua_rawgeti` 应该取回 `nil`，报错应该是"attempt to call
a nil value"。但实测结果是 `lua_rawgeti` 取回的类型是
**`number`**，最终报错是 **`attempt to call a number value`**。

原因在于 `luaL_unref`/`luaL_ref` 的真实内部实现：Registry 用一张
Lua Table 存放所有引用，被释放的槽位不会被单纯清空成 `nil`，而是
被复用成一个**内部空闲链表**的节点——`luaL_unref` 会把这个槽位
的内容改写成"下一个空闲槽位的编号"（一个整数），`luaL_ref` 分配
新引用时优先从这条空闲链表里取节点复用。也就是说，`ref = 4` 这个
槽位在被 `unref` 之后，存的不是 `nil`，而是空闲链表里的下一个
节点编号（本例中恰好是一个数字）——这是一个只有实际编译运行才
能观察到的实现细节，凭直觉很容易猜错真实的报错文本。

**正确修复**：`luaL_unref` 之后立即把保存引用的变量重置为
`LUA_NOREF`，并且在任何使用之前先判断
`if (handler_ref == LUA_NOREF) { ...跳过... }`（Chapter 10 的
`event_demo.c` 就是这么写的），绝不假设"引用失效后取回来的东西
一定是 `nil`"。

## 24.11 十个案例的分类总览

| # | Failure | 层面 | 是否实测 |
|---|---|---|---|
| 1 | 错误的 Stack index | Data Flow / Stack | 已实测 |
| 2 | 忘记 pop | Data Flow / Stack Balance | 已实测 |
| 3 | 错误的 return count | Data Flow / Stack Contract | 已实测 |
| 4 | 返回位置错误 | Data Flow / Stack Contract | 已实测 |
| 5 | Stack index 当长期引用 | Lifetime | 已用 Chapter 09 + Failure 9 的实测证据交叉验证 |
| 6 | `lua_call` 导致错误穿透 | Control Flow | 已实测 |
| 7 | userdata 类型错误 | Object Model | 已实测 |
| 8 | userdata 生命周期错误 | Lifetime / UB | 未实测，理论分析 |
| 9 | Stack index 当对象引用（跨调用） | Lifetime | 已实测 |
| 10 | 引用失效后继续调用 | Lifetime / Control Flow | 已实测 |

## 24.12 Mental Model

> **10 个案例里有 8 个可以在几十行 C 代码内稳定复现，而且真实结果
> 经常和"凭直觉猜测"的不一样（越界索引不崩溃但给错数据、unref
> 之后槽位不是 nil 而是一个空闲链表节点）——这正说明 Lua/C 交互
> 的 Bug 排查必须依赖对 Stack/Registry 实际状态的观察，而不能靠
> "这个 API 应该是这样工作的"这种想象；剩下 2 个难以安全复现的
> 案例（userdata 生命周期）之所以危险，恰恰是因为它们是未定义
> 行为，连"稳定复现"这个前提本身都不成立。**
