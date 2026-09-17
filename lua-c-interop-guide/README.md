# Lua <-> C API 深度学习：Mental Model 优先的完整教程

这是一套系统学习 **Lua 与 C 语言交互机制**（Lua C API / Lua Stack）的中文教程。全篇不按官方 API Reference 罗列函数，而是按

```text
Why -> Architecture -> Lifecycle -> Relationship -> Data Flow -> Control Flow -> State -> Failure -> Mental Model
```

的顺序讲解，每个知识点都有**可以真实编译运行**的 Lua + C 代码作为支撑。

## 0. 快速开始

```bash
# 1. 本地构建 Lua 5.4.7（下载源码 + 本地 make，不需要 sudo，不依赖系统 liblua-dev）
./scripts/build-vendor-lua.sh

# 2. 编译并运行全部 26 章代码示例 + 8 个完整项目，做一次全量回归
./scripts/build-all.sh --quiet

# 3. 单独看某一章，例如 Chapter 01
cd chapters/01-first-call-add/code
make run
```

所有代码统一遵循同一种"宿主程序"模式（见 [`Makefile.rules`](Makefile.rules)）：一个 `main()` 创建 `lua_State*`，注册这一章要讲的 C 能力，然后 `luaL_dofile()` 运行同目录下配套的 `.lua` 脚本并打印结果。这样每个例子都是一个独立、可重复运行、不依赖网络或系统权限的最小教学单元。

## 1. 怎么读这套教程

不建议直接跳到某一章。请先读 [`chapters/00-overview-architecture.md`](chapters/00-overview-architecture.md) 建立总体 Mental Model，再按顺序往下读；最后用 [`chapters/26-final-mental-model.md`](chapters/26-final-mental-model.md) 收束成 5 个概念。

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

写作时全篇统一使用 [`TERMS.md`](TERMS.md) 里定义的中文术语（代码/API 名保留英文），如果发现某处译法不一致，以 `TERMS.md` 为准。

## 2. 目录（chapters/）

| 章节 | 主题 | 对应知识点 |
|---|---|---|
| [00](chapters/00-overview-architecture.md) | 总体架构 | Why 需要 C API、Lua VM、`lua_State`、Lua Stack |
| [01](chapters/01-first-call-add/README.md) | Lua -> C 最简单调用 | `add(10,20)` 完整生命周期、`return N` 的真正含义 |
| [02](chapters/02-stack-basics/README.md) | Lua Stack 基础 | gettop/settop/pop/push/to/pushvalue/remove/insert/replace 六大分类 |
| [03](chapters/03-stack-index/README.md) | 正负索引 | Positive vs Negative index，何时用哪个 |
| [04](chapters/04-c-calls-lua/README.md) | C -> Lua | `lua_getglobal`/`lua_call` vs `lua_pcall` |
| [05](chapters/05-basic-types/README.md) | 基础类型双向转换 | nil/boolean/integer/number/string |
| [06](chapters/06-table-lua-to-c/README.md) | Table: Lua -> C | `configure({...})`，Stack Balance 直觉 |
| [07](chapters/07-table-c-to-lua/README.md) | Table: C -> Lua | `get_config()`，`lua_newtable`+`lua_setfield` |
| [08](chapters/08-table-iteration/README.md) | Table 遍历 | `lua_next` 的逐步 Stack 变化 |
| [09](chapters/09-lua-function-to-c-registry/README.md) | Lua Function -> C | Registry、`luaL_ref`/`luaL_unref` |
| [10](chapters/10-c-callback-event-system/README.md) | C Callback -> Lua | 完整事件系统、错误处理、cleanup |
| [11](chapters/11-userdata-basics/README.md) | Userdata 基础 | `struct device`、`lua_newuserdata`、Metatable 挂载 |
| [12](chapters/12-userdata-metatable-methods/README.md) | Userdata + Metatable + Method | `dev:start()` 方法派发链路 |
| [13](chapters/13-gc-lifecycle/README.md) | `__gc` 与生命周期 | Lua GC + `__gc` + C destructor 协作 |
| [14](chapters/14-light-vs-full-userdata/README.md) | Light vs Full Userdata | 设计取舍与适用场景 |
| [15](chapters/15-closures-upvalues/README.md) | C Closure / Upvalue | `create_counter()`，`lua_pushcclosure` |
| [16](chapters/16-c-creates-lua-function/README.md) | C 创建 Lua Function | `lua_pushcfunction` |
| [17](chapters/17-bidirectional-calls/README.md) | 双向调用 | Lua -> C -> Lua -> C 完整链路 |
| [18](chapters/18-error-handling/README.md) | Error Handling | `lua_call` vs `lua_pcall` 异常传播 |
| [19](chapters/19-stack-balance/README.md) | Stack Balance | Stack Contract，5 个错误示例 |
| [20](chapters/20-api-map.md) | API 分类地图 | A-L 十二类完整 API 一览 |
| [21](chapters/21-full-lifecycle.md) | 完整 Lifecycle | Lua->C 与 C->Lua 两条完整时间线 |
| [22](chapters/22-data-flow.md) | Data Flow | Lua Stack = Data Plane，`call`/`pcall` = Control Flow |
| [23](chapters/23-state-machine-view.md) | State Machine 视角 | 把一次 C 函数调用理解成状态机 |
| [24](chapters/24-failure-analysis.md) | Failure Analysis | 10 个真实（多数已实测）常见错误 |
| [25](chapters/25-debugging-checklist.md) | Debugging Checklist | 排查方法论 + 与 Chapter 24 的对照表 |
| [26](chapters/26-final-mental-model.md) | 最终 Mental Model | 5 个核心概念、全书收束 |

## 3. 完整项目案例（projects/）

| 项目 | 场景 | 重点串联的知识点 |
|---|---|---|
| [01-math-library](projects/01-math-library/README.md) | 数学函数库 | `luaL_Reg`+`luaL_setfuncs` 打包 C 库 |
| [02-string-utility](projects/02-string-utility/README.md) | 字符串处理库 | `lua_tolstring`/`lua_pushlstring`，返回 Table |
| [03-config-parser](projects/03-config-parser/README.md) | 配置解析器 | 嵌套 Table 递归解析 + 校验 |
| [04-event-callback-system](projects/04-event-callback-system/README.md) | 多事件回调系统 | 多个 Registry Reference 管理、错误隔离 |
| [05-file-fd-wrapper](projects/05-file-fd-wrapper/README.md) | 文件描述符封装 | Userdata + `__gc` 兜底释放资源 |
| [06-network-socket-wrapper](projects/06-network-socket-wrapper/README.md) | 网络 Socket 封装 | `socketpair` 本地双端 userdata |
| [07-state-machine](projects/07-state-machine/README.md) | C 状态机暴露给 Lua | Lua=policy，C=mechanism |
| [08-plugin-architecture](projects/08-plugin-architecture/README.md) | 插件宿主架构（压轴） | 综合运用 Chapter 00-25 全部知识点 |

## 4. 目录结构

```text
lua-c-interop-guide/
+-- README.md              本文件
+-- TERMS.md                统一术语表（写作/阅读的唯一真相来源）
+-- Makefile.common          公共构建配置（指向本地构建的 Lua 5.4.7）
+-- Makefile.rules           统一的构建规则（单 TARGET 的"宿主程序"模式）
+-- .gitignore               忽略 vendor/ 与所有编译产物
+-- scripts/
|   +-- build-vendor-lua.sh  下载并本地构建 liblua.a（无需 sudo）
|   +-- build-all.sh         对全部 chapters/projects 跑 `make clean && make run` 做回归
+-- vendor/                  本地构建产物（不入库，用脚本重新生成）
+-- chapters/                26 个概念章节（00-26）
+-- projects/                8 个完整项目案例
```

## 5. 环境说明

- 不依赖系统 `liblua-dev`，也不需要 `sudo`：`scripts/build-vendor-lua.sh` 会从 `lua.org` 下载 Lua 5.4.7 官方源码，在 `vendor/lua-5.4.7/` 下本地编译出 `liblua.a` 和头文件。
- 所有示例都是可执行文件（内嵌 Lua 解释器），不涉及 `.so` 动态模块加载，避免 `package.cpath` 相关的环境差异。
- `scripts/build-all.sh --quiet` 目前对 **28 个** 编译单元（20 个 chapters/*/code + 8 个 projects/*）做 `make clean && make run`，全部通过。

## 6. 最终一句话

> Lua/C 交互本质上不是"Lua 直接调用 C 变量"，而是 Lua VM 与 C 通过 `lua_State` 暴露的 Lua C API，以 Lua Stack 作为数据交换工作台，完成 Value、Object、Function 和 Resource 的双向传递与生命周期管理。

详见 [`chapters/26-final-mental-model.md`](chapters/26-final-mental-model.md)。
