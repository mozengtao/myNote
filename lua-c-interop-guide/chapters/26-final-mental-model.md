# 26. 最终 Mental Model：只记住 5 个概念

> 对应用户需求 Stage 29 / 30 / 32 / 33。这是全书的收束章节，不引入新 API，只做压缩。

## 26.1 API 的五类动作（Stage 29）

不要按 API 名字记忆，按"动作"记忆：

```text
                 Lua C API
                     |
       +-------------+-------------+
       |             |             |
      READ          WRITE         CONTROL
       |             |             |
   lua_to*         push*        call/pcall
   get*            set*         error
       |             |             |
       +-------------+-------------+
                     |
                     v
                 Lua Stack
```

压缩成五个动词：

```text
READ    Stack  -> C            （lua_to* / lua_touserdata）
PUSH    C      -> Stack        （lua_push*）
GET     Lua Object -> Stack    （lua_getfield / lua_getglobal / lua_rawget ……）
SET     Stack  -> Lua Object   （lua_setfield / lua_setglobal / lua_rawset ……）
CALL    Stack  -> Function -> Stack （lua_call / lua_pcall）
```

任何一个你不认识的 Lua C API 函数，先问它属于这五类里的哪一类，再去查具体签名——这比死记函数名有效得多。

## 26.2 全局 Mental Model 图（Stage 30）

```text
                         +--------------+
                         |    Lua VM    |
                         +------+-------+
                                |
                         Lua C API Boundary
                                |
                                v
                     +----------------------+
                     |      Lua Stack       |
                     |                      |
                     |  Arguments           |
                     |  Temporary Values    |
                     |  Lua Objects         |
                     |  Results             |
                     +----------+-----------+
                                |
              +-----------------+-----------------+
              |                 |                 |
           Read/To            Push              Call
              |                 |                 |
              v                 v                 v
             C             Lua Value           Lua VM
              |
              |
       +------+--------+
       |               |
   C primitive       C Object
                       |
                       v
                   Userdata
                       |
                       v
                  Metatable
                       |
                       v
                   Lua Object
```

这张图把全书的知识收拢成一句话：**Lua Stack 是唯一入口，C primitive 走"值拷贝"（Read/To、Push），C Object 走"包装"（Userdata -> Metatable -> 被当成 Lua Object 使用）。**

## 26.3 如果只允许记住 5 个概念

```text
1. lua_State
      v
   Lua/C API 的入口——没有它就无法访问任何 Lua 状态

2. Lua Stack
      v
   Lua 与 C 之间的数据交换工作台——唯一合法的跨界通道

3. lua_push* / lua_to*
      v
   C primitive ↔ Lua Value 的双向转换

4. lua_get* / lua_set*
      v
   Stack ↔ Lua Object（Table / Global / Registry）的双向读写

5. Userdata + Metatable + Registry
      v
   Lua Object ↔ C Object 的桥梁，外加"如何让它们活得够久"（Lifetime）
```

对照全书每一章，它们分别对应：

| 概念 | 对应章节 |
|---|---|
| 1. `lua_State` | Chapter 00 |
| 2. Lua Stack | Chapter 00, 02, 03 |
| 3. `lua_push*` / `lua_to*` | Chapter 01, 05 |
| 4. `lua_get*` / `lua_set*` | Chapter 06, 07, 08, 09 |
| 5. Userdata + Metatable + Registry | Chapter 09, 11, 12, 13, 14 |

## 26.4 最终一句话总结（Stage 30 结尾）

> **Lua/C 交互本质上不是"Lua 直接调用 C 变量"，而是 Lua VM 与 C 通过 `lua_State` 暴露的 Lua C API，以 Lua Stack 作为数据交换工作台，完成 Value、Object、Function 和 Resource 的双向传递与生命周期管理。**

## 26.5 反复出现的骨架图（Stage 32）

全书每一章的讲解顺序，最终都能塞进这一张图：

```text
                 WHY
                  |
                  v
              Architecture
                  |
                  v
               Lua VM
                  |
                  v
             lua_State *L
                  |
                  v
              Lua Stack
                  |
        +---------+---------+
        |                   |
      Lua -> C            C -> Lua
        |                   |
      lua_to*             lua_push*
        |                   |
        +---------+---------+
                  |
             Object Model
                  |
        +---------+---------+
        |                   |
    Lua Object           C Object
        |                   |
   Table/Function      struct/resource
        |                   |
        +---------+---------+
                  |
        Userdata / Metatable
                  |
                  v
               Lifetime
                  |
                  v
                __gc
```

## 26.6 最终验收：你现在应该能主动回答

拿到任意一个 Lua/C Binding（比如本仓库里 Wireshark 的 dissector 插件，或者 `projects/08-plugin-architecture`），你应该能不看源码注释就说出：

```text
Lua API
    v
C binding
    v
lua_State
    v
Stack
    v
userdata
    v
metatable
    v
C struct
```

- 为什么这样设计？—— 因为 Lua VM 和 C 是两套不同的内存/执行模型，Stack 是唯一安全的转换层。
- 谁调用谁？—— 看是 `CALL` 字节码触发的（Lua -> C）还是 `lua_pcall` 触发的（C -> Lua）。
- 数据在哪里？—— 永远先问"此刻 Stack 上有什么，各个 index 对应什么"。
- 对象如何传递？—— Table/Function 传"句柄"（Stack 槽位/Registry Reference），Full Userdata 传"被 GC 管理的内存块"。
- 对象如何保持存活？—— 要么在 Lua 变量里持有，要么 C 用 `luaL_ref(L, LUA_REGISTRYINDEX)` 长期占住一个 Registry 槽位。
- 异常如何传播？—— `lua_pcall` 把 Lua Error 转成状态码返回给 C；`lua_call` 则让错误直接穿透（对宿主程序通常是致命的）。
- 出现 crash / type error / stack error / lifetime bug 时先查什么？—— 见 Chapter 25 的 Debugging Checklist：先定位方向（Lua->C 还是 C->Lua）与阶段（参数/Stack/转换/调用/返回/生命周期/错误），再逐项对照 Chapter 24 的 10 个 Failure 模式。

**这就是本教程的终点，也是起点**：`Lua VM ↔ Lua Stack ↔ C Binding ↔ C Object ↔ Lifetime`——只要这条链你能随时在脑子里画出来，剩下的都只是查手册。
