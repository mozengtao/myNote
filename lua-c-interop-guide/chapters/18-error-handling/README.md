# 18. Error Handling：`lua_call` vs `lua_pcall`

> 对应用户需求"第十九阶段：Error Handling"。本章重点比较 `lua_call()` 与
> `lua_pcall()`，并解释为什么 C binding 里几乎总是应该用后者。

## 1. 直觉

到目前为止的所有章节，示例里的 C Function 和 Lua Function 都"正常返回"。
但真实世界里，代码会出错：参数类型不对、除数是 0、文件打不开……

Lua 对这类问题的处理方式是**抛出一个 Lua Error**，这本质上是一次
`longjmp`——它会像"传送"一样，从出错的地方直接跳到某个"安全点"，中间
经过的所有函数调用帧（无论是 Lua 的还是 C 的）全部被跳过，**不会**
执行任何清理代码（除非你显式设置了保护）。

问题只剩下一个：**这个"安全点"在哪里？**

- 如果你提前用 `lua_pcall` 建立了一个安全点，错误会稳稳地"跳"到那里，
  变成一个普通的返回值（状态码 + 错误信息），程序继续正常运行。
- 如果整条调用链上根本没有任何安全点（也就是说你全程只用了
  `lua_call`），错误会一路跳到 Lua 内置的"最后一道防线"——panic
  函数，默认行为是打印错误信息然后直接终止整个进程。

对 Wireshark 这样的长期运行宿主程序来说，"一个用户写错的 `.lua` 插件
让整个抓包程序崩溃"是完全不能接受的。这就是为什么本章要把
`lua_call` 和 `lua_pcall` 的区别讲透。

## 2. Architecture

```text
Lua Error（luaL_error / lua_error / 运行时错误）
    |
    v
lua_error(L)  -- 本质：longjmp 到某个 setjmp 点
    |
    +-- 如果调用链上存在 lua_pcall 建立的保护点
    |       |
    |       v
    |   跳回 lua_pcall 调用处
    |   lua_pcall 返回非 LUA_OK 的状态码
    |   错误信息被 push 到 Stack 顶部
    |   C 代码继续正常运行（Error propagation 被"截住"了）
    |
    +-- 如果调用链上没有任何保护点（只有 lua_call）
            |
            v
        跳到当前注册的 panic 函数
        默认 panic 函数：打印信息 + abort()/exit()
        整个宿主进程被终止
```

对照本章代码里两条路径：

```text
                lua_getglobal + push args
                        |
                        v
                +---------------+
                |  lua_call     |  普通调用：出错 = 无保护 longjmp
                +---------------+
                | lua_pcall     |  保护调用：出错 = 返回状态码
                +---------------+
                        |
                        v
                  Stack 上的返回值 / 错误信息
```

## 3. 完整代码

本章代码在 [`code/`](code/) 目录，可直接 `make run` 验证。

`code/error_demo.c`：

```c
#include <stdio.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/*
 * native_operation：故意在特定条件下报错的 C Function。
 *
 * 参数：divisor（整数）。
 *   - divisor == 0 时，用 luaL_error 抛出 Lua Error（内部就是 lua_error(L)，
 *     本质是 longjmp，本函数从这一行开始不会再往下执行）。
 *   - 否则把 100 / divisor 的结果 push 到 Stack，return 1（告诉 Lua VM：
 *     栈顶 1 个值就是这次调用的返回值）。
 */
static int l_native_operation(lua_State *L) {
    lua_Integer divisor = luaL_checkinteger(L, 1);

    if (divisor == 0) {
        return luaL_error(L, "native_operation: divisor is zero");
    }

    lua_pushinteger(L, 100 / divisor);
    return 1;
}

int main(int argc, char **argv) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_register(L, "native_operation", l_native_operation);

    printf("== Step 1: Lua 用 pcall 包裹调用会出错的 native_operation ==\n");
    if (luaL_dofile(L, "error_demo.lua") != LUA_OK) {
        fprintf(stderr, "unexpected top-level lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    printf("\n== Step 2: C 用 lua_pcall 直接调用一个会出错的 Lua Function ==\n");
    lua_getglobal(L, "risky_lua_function"); /* [1] function */
    lua_pushinteger(L, 0);                  /* [2] 参数 divisor = 0，会触发错误 */
    int status = lua_pcall(L, 1, 1, 0);     /* 保护调用：1 个参数，期望 1 个返回值 */
    if (status != LUA_OK) {
        printf("lua_pcall status = %d（不是 LUA_OK）\n", status);
        printf("stack top 上的错误信息 = %s\n", lua_tostring(L, -1));
        lua_pop(L, 1); /* 清理错误信息，恢复 Stack 平衡 */
    } else {
        printf("risky_lua_function 意外返回了 %lld\n", (long long)lua_tointeger(L, -1));
        lua_pop(L, 1);
    }

    printf("\n== Step 3: 再用 lua_pcall 调用一次不会出错的输入，验证正常路径 ==\n");
    lua_getglobal(L, "risky_lua_function");
    lua_pushinteger(L, 5);
    status = lua_pcall(L, 1, 1, 0);
    if (status == LUA_OK) {
        printf("risky_lua_function(5) = %lld\n", (long long)lua_tointeger(L, -1));
        lua_pop(L, 1);
    } else {
        printf("意外出错: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    /* 危险演示：只有传入 --crash-lua-call 才会执行，见下文 5.3 节。 */
    if (argc > 1 && strcmp(argv[1], "--crash-lua-call") == 0) {
        printf("\n== Step 4（危险演示，仅在传入 --crash-lua-call 时执行）==\n");
        printf("用 lua_call 直接调用会出错的 Lua Function，外层没有 lua_pcall 兜底：\n");
        fflush(stdout);
        lua_getglobal(L, "risky_lua_function");
        lua_pushinteger(L, 0);
        lua_call(L, 1, 1); /* 普通调用：如果这里出错，本行之后的代码不会被执行到 */
        printf("这一行不会被打印到（如果上面真的出错的话）\n");
    }

    lua_close(L);
    printf("\n程序正常退出：所有错误都被 lua_pcall 正确捕获，没有 longjmp 穿透到 main。\n");
    return 0;
}
```

`code/error_demo.lua`：

```lua
-- risky_lua_function：一个会调用 native_operation 的 Lua Function。
-- native_operation 在 C 侧报错时，错误会像普通 Lua Error 一样，
-- 沿着调用链从 C 传播回 Lua——对 Lua 使用者来说，感觉不出这是一次
-- "跨越了 C 边界"的错误。
function risky_lua_function(divisor)
    return native_operation(divisor)
end

print("-- 用 pcall 包裹调用 native_operation(0)（会触发 luaL_error）--")
local ok, err = pcall(function()
    native_operation(0)
end)
print("ok  =", ok)
print("err =", err)

print()
print("-- 用 pcall 包裹调用 native_operation(10)（正常路径）--")
local ok2, result = pcall(function()
    return native_operation(10)
end)
print("ok2    =", ok2)
print("result =", result)
```

`code/Makefile`：

```makefile
GUIDE_ROOT := $(abspath ../../..)
include $(GUIDE_ROOT)/Makefile.common
TARGET := error_demo
SRCS := error_demo.c
include $(GUIDE_ROOT)/Makefile.rules

# 危险演示专用 target：故意用 lua_call（而不是 lua_pcall）调用一个会出错的
# Lua Function，且外层没有任何保护。会直接中止整个进程，仅用于教学观察，
# 不参与常规 `make run` 验收流程。
.PHONY: run-crash
run-crash: all
	./$(TARGET) --crash-lua-call
```

`make run` 的实际输出：

```text
== Step 1: Lua 用 pcall 包裹调用会出错的 native_operation ==
-- 用 pcall 包裹调用 native_operation(0)（会触发 luaL_error）--
ok  =	false
err =	error_demo.lua:11: native_operation: divisor is zero

-- 用 pcall 包裹调用 native_operation(10)（正常路径）--
ok2    =	true
result =	10

== Step 2: C 用 lua_pcall 直接调用一个会出错的 Lua Function ==
lua_pcall status = 2（不是 LUA_OK）
stack top 上的错误信息 = error_demo.lua:6: native_operation: divisor is zero

== Step 3: 再用 lua_pcall 调用一次不会出错的输入，验证正常路径 ==
risky_lua_function(5) = 20

程序正常退出：所有错误都被 lua_pcall 正确捕获，没有 longjmp 穿透到 main。
```

## 4. Stack Diagram

**Step 2（`lua_pcall` 出错路径）**：

```text
调用前：
    [1] risky_lua_function
    [2] 0                      (divisor 参数)

lua_pcall(L, 1, 1, 0)
    |
    |  内部 risky_lua_function -> native_operation(0) -> luaL_error
    |  出错时，Lua 会自动把 Stack 收敛到"函数调用前的位置"，
    |  只留下 1 个值：错误信息字符串
    v
调用后（status != LUA_OK）：
    [1] "error_demo.lua:6: native_operation: divisor is zero"
```

**Step 3（`lua_pcall` 正常路径）**：

```text
调用前：
    [1] risky_lua_function
    [2] 5

lua_pcall(L, 1, 1, 0)
    |
    |  正常执行完毕，栈顶 1 个值是返回值
    v
调用后（status == LUA_OK）：
    [1] 20
```

两条路径的共同点：**无论成功还是失败，`lua_pcall` 返回后 Stack 上
恰好多出"你要求的返回值个数"（成功）或者"1 个错误信息"（失败）**，
调用前用掉的函数和参数槽位都被清理干净——这正是"protected"这个词
的含义：它替你兜住了 Stack 的平衡，不管里面发生了什么。

## 5. Data Flow

### 5.1 Lua 用 `pcall` 包裹调用 C（Step 1）

```text
Lua: pcall(function() native_operation(0) end)
    |
    v
Lua VM 内部建立保护点（这就是 pcall 在字节码层面做的事）
    |
    v
调用 native_operation（C Function）
    |
    v
divisor == 0 -> luaL_error(L, "...")
    |
    v
lua_error(L) -> longjmp 跳回 pcall 建立的保护点
    |
    v
pcall 返回 (false, "错误信息")
```

### 5.2 C 用 `lua_pcall` 调用 Lua（Step 2/3）

```text
C: lua_getglobal + lua_pushinteger + lua_pcall(L, 1, 1, 0)
    |
    v
Lua VM 执行 risky_lua_function
    |
    +-- 内部调用 native_operation(divisor)
    |
    +-- divisor == 0  -> luaL_error -> longjmp 跳回 lua_pcall 调用处
    |       lua_pcall 返回值 = LUA_ERRRUN（非 0）
    |       Stack 顶部 = 错误信息字符串
    |
    +-- divisor != 0  -> 正常 return
            lua_pcall 返回值 = LUA_OK（0）
            Stack 顶部 = 真正的返回值
```

### 5.3 C 用 `lua_call` 调用 Lua（危险演示，`make run-crash`）

```text
C: lua_getglobal + lua_pushinteger + lua_call(L, 1, 1)
    |
    v
Lua VM 执行 risky_lua_function -> native_operation(0) -> luaL_error
    |
    v
lua_error(L) -> longjmp
    |
    v
调用链上找不到任何 lua_pcall 建立的保护点
    |
    v
跳到当前的 panic 函数（默认实现）
    |
    v
打印 "PANIC: unprotected error in call to Lua API (...)"
    |
    v
abort() —— 整个进程被终止，lua_close 根本没有机会被执行
```

用 `make run-crash` 实际触发这条路径，观察到的真实输出是：

```text
== Step 4（危险演示，仅在传入 --crash-lua-call 时执行）==
用 lua_call 直接调用会出错的 Lua Function，外层没有 lua_pcall 兜底：
PANIC: unprotected error in call to Lua API (error_demo.lua:6: native_operation: divisor is zero)
Aborted (core dumped)
```

注意最后一行"这一行不会被打印到"确实没有出现——`lua_call` 之后的
代码，在出错时根本没有机会执行。这不是巧合，而是 `longjmp` 语义本身
决定的：它直接把 CPU 的执行位置切换到 `setjmp` 点，中间的 C 调用栈帧
（包括 `main` 里 `lua_call` 那一行往后的所有语句）全部被跳过。

## 6. Lifecycle

```text
建立保护点（可选）
    |  lua_pcall(L, nargs, nresults, msgh) 内部会 setjmp
    v
Stack -> Function invocation
    |  lua_call / lua_pcall 都是"拿 Stack 上的参数去调用 Stack 上的函数"
    v
   执行
    |
    +-- 正常结束 -> return N -> Stack 收敛为 N 个返回值
    |
    +-- 出错（lua_error / luaL_error / 运行时错误）
            |
            +-- 存在保护点 -> longjmp 回保护点 -> lua_pcall 返回非 LUA_OK
            |                  -> Stack 收敛为 1 个错误信息 -> 程序继续
            |
            +-- 不存在保护点 -> longjmp 到 panic 函数 -> 进程终止
```

**关键结论**：`lua_call` 和 `lua_pcall` 在"成功"路径上几乎没有区别
（都是拿 Stack 上的东西调用函数、把结果留在 Stack 上），区别只体现在
"出错"这一条路径上——但这条路径恰恰是**决定宿主程序生死**的路径。

## 7. Failure

### Failure 1：给长期运行的宿主程序用 `lua_call`

```c
/* 反面教材：Wireshark dissector 风格的回调里直接用 lua_call */
lua_getglobal(L, "dissect_packet");
lua_pushlightuserdata(L, packet_data);
lua_call(L, 1, 0); /* 如果 dissect_packet 里有 bug 抛错，整个 Wireshark 进程直接死掉 */
```

后果：任何一次脚本里的笔误（比如访问了 `nil` 的字段）都会变成
"抓包程序崩溃"，而不是"这个包解析失败，跳过继续下一个"。对插件系统
来说，这是致命的设计错误。正确写法永远是本章 Step 2 演示的
`lua_pcall`，并且要检查返回的状态码。

### Failure 2：以为 `lua_pcall` 出错后 Stack 上还是"调用前"的样子

```c
lua_getglobal(L, "risky_lua_function");
lua_pushinteger(L, 0);
lua_pcall(L, 1, 1, 0);
/* 忘记检查 status，直接当成功处理: */
int r = (int)lua_tointeger(L, -1); /* 拿到的其实是错误信息字符串转数字失败后的 0，而不是真正的返回值 */
```

后果：`lua_pcall` 无论成功失败，返回后栈顶都会有值，但**含义完全不
同**——不检查状态码就直接读值，会把错误信息误当成正常结果处理，
产生难以察觉的静默 bug。必须像 Step 2 那样先判断 `status`。

### Failure 3：`msgh`（消息处理函数）参数传错却毫无察觉

```c
lua_pcall(L, 1, 1, 0); /* 本章示例用 0，表示"不额外处理错误信息，原样返回" */
```

`lua_pcall` 的第 4 个参数是"消息处理函数"在 Stack 上的位置，常见用途
是在错误信息里附加 traceback（比如 `debug.traceback`）。如果不了解这
个参数，随手传一个不是函数的 Stack index（比如误传成 `1`，恰好是别的
东西），出错时 Lua 会尝试把它当函数调用，可能引发第二个错误（Lua 有
专门的状态码 `LUA_ERRERR` 描述"错误处理函数自身又出错"这种情况）。
本章示例统一使用 `0` 表示不使用消息处理函数，保持行为可预测。

## 8. Debugging

- 看到进程直接消失、终端打印 `PANIC: unprotected error in call to
  Lua API`：说明调用链上有一处直接用了 `lua_call`（或者忘了给
  `lua_pcall` 设置 msgh 却又指望它兜底更外层的错误），排查方向是
  "把最外层这次 `lua_call` 改成 `lua_pcall`"。
- 看到返回值"类型不对"或者"莫名其妙变成了错误信息字符串"：检查是不是
  忘了判断 `lua_pcall` 的返回状态码。
- 想知道某次调用到底有没有出错：永远先打印 `status` 本身
  （`LUA_OK` 是 0，非 0 都代表出错），再决定要不要读 Stack 顶部。
- 用 `pcall`/`lua_pcall` 兜底之后，**一定要记得清理错误信息**
  （`lua_pop(L, 1)`），否则又会引入 Chapter 19 讨论的 Stack Balance
  问题。

## 9. Mental Model

```text
lua_call  = normal call     ：出错 = longjmp 穿透，没有保护就是进程终止
lua_pcall = protected call  ：出错 = 变成一个普通的返回值（状态码 + 错误信息）

C binding 里几乎永远选 lua_pcall——
因为宿主程序（如 Wireshark）绝不能被一次脚本错误直接打垮。
```
