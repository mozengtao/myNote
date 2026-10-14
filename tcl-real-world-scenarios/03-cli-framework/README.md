# 场景 3：命令行子命令框架

> 运行（演示全部子命令）：`tclsh cli_tool.tcl`
> 真实用法：`tclsh cli_tool.tcl greet Morris` / `sum 1 2 3 4` / `counter 5` / `retry-demo`

## 1. 真实背景

很多内部工具都是 `<program> <subcommand> [args...]` 这种形态（类似 `git <subcommand>`）。本例实现一个最小但完整的子命令框架，并借机演示 `proc`/`upvar`/`uplevel`/`namespace`/`package` 这几个 mental model 中"进阶机制"在真实代码里如何组合使用。

## 2. 涉及的 Tcl 概念

对照 [tcl_mental_model.md](../../tcl_mental_model.md)：

| 概念 | 对应章节 | 在本例中的作用 |
|---|---|---|
| `proc` + `args` | 第 34.3 节 | 子命令实现用可变参数接收剩余命令行参数 |
| `switch` | 第 27 节 | 按子命令名分发 |
| `upvar` | 第 35 节 | `cli::bump` 直接修改调用者的计数变量 |
| `uplevel` | 第 36 节 | 自定义控制结构 `retry n { script }` |
| `namespace` | 第 38 节 | 把所有子命令组织到 `::cli` 下 |
| `package provide` | 第 39 节 | 把整个工具声明成可复用模块 |
| `{*}`（参数展开） | 第 37 节末尾提示 | 代替 `eval` 把 List 展开成多个参数，更安全 |

## 3. Mental Model

```
argv (List: subcommand + rest args)
    |
    | cli::run
    v
switch subcommand
    +-- greet       -> cli::cmdGreet  {*}rest
    +-- sum         -> cli::cmdSum    {*}rest
    +-- counter     -> cli::cmdCounter {*}rest
    |                       |
    |                       | cli::bump varName   (upvar 1)
    |                       v
    |                  调用者的局部变量被直接修改
    +-- retry-demo  -> cli::cmdRetryDemo
                            |
                            | retry 3 { script }  (uplevel 1)
                            v
                       script 在"调用 retry 的那一层"里执行
```

## 4. 完整代码

见 [cli_tool.tcl](cli_tool.tcl)。整体结构：

```tcl
namespace eval cli { namespace export run }

proc cli::bump {varName {step 1}} { upvar 1 $varName ref; ... }   ;# upvar 工具
proc retry {times script} { ...; uplevel 1 $script; ... }          ;# uplevel 控制结构

proc cli::cmdGreet {args} { ... }
proc cli::cmdSum {args} { ... }
proc cli::cmdCounter {args} { ... }      ;# 内部调用 cli::bump
proc cli::cmdRetryDemo {args} { ... }    ;# 内部调用 retry

proc cli::run {argv} {
    switch -- [lindex $argv 0] { greet {...} sum {...} ... }
}

package provide clitool 1.0
```

## 5. 执行讲解

1. **`cli::bump`（upvar）**：`upvar 1 $varName ref` 在 `cli::bump` 自己的作用域里创建了一个名叫 `ref` 的变量，它是**调用者（上一层栈帧）里那个同名变量的别名**。所以 `incr ref $step` 实际修改的是调用者的变量。`cli::cmdCounter` 里 `cli::bump counter` 每调用一次，`cmdCounter` 自己的局部变量 `counter` 就真的被加了 1——这和直接写 `incr counter` 效果一样，但证明了"一个独立的工具 proc 也能修改调用者的变量"，这是实现通用工具函数（而不是每次都复制粘贴逻辑）的关键机制。
2. **`retry`（uplevel）**：`uplevel 1 $script` 跳到"调用 `retry` 的那一层"去执行 `script`，所以 `script` 里 `incr ::cli::attempt`、`set result ...` 等语句看起来就像是直接写在 `cmdRetryDemo` 里一样自然。`catch {...} result` 捕获每次尝试的错误（或正常返回值）；失败就打印提示并进入下一轮循环，直到成功立即 `return`，或者次数用尽后用 `error` 把最后一次失败原因抛出去。这就是用 Tcl 原生机制实现一个"看起来像内置语法"的控制结构的标准套路。
3. **`cli::run`**：`switch -- $subcommand { ... }` 对第一个参数做分发；`{*}$rest` 把 List 形式的剩余参数展开成多个独立的 word 传给子命令 proc——如果写成 `cli::cmdSum $rest`，`$rest` 会被当成**一个参数**（一个 List 字符串）传进去，而不是多个数字参数，`cmdSum` 里的 `foreach n $args` 就只会拿到一个元素。`{*}` 是 mental model 第 37 节末尾提到的、比 `eval` 更安全的参数展开方式。
4. **`package provide clitool 1.0`**：声明这个文件是一个名为 `clitool`、版本 1.0 的模块。真实项目里如果要让其他脚本 `package require clitool` 使用它，通常会把本文件放进独立目录并配一个 `pkgIndex.tcl`（声明到哪里能找到这个包）；本例作为单文件教学脚本省略了这一步，但 `package provide` 本身已经演示了"模块声明"这个动作。

## 6. 工程实践要点

- **顶层控制流（`cli::run`）和具体业务逻辑（`cmdXxx`）分离**：新增一个子命令只需要写一个新的 `cli::cmdXxx` proc，再在 `switch` 里加一行，不会牵动其他子命令的代码。
- **把"引用传递"和"动态执行"包装成具名工具（`bump`/`retry`），而不是到处裸写 `upvar`/`uplevel`**：业务代码（`cmdCounter`/`cmdRetryDemo`）里完全看不到 `upvar`/`uplevel` 字样，阅读业务逻辑时不需要先理解这两个相对底层的机制，复杂度被封装在两个小工具 proc 里。
- **`retry` 这类自定义控制结构命名不加 `cli::` 前缀**：因为它的使用场景不是"某个子命令专属"，而是像 `if`/`foreach` 一样的通用语言扩展，放在全局命名空间更符合它被使用的方式（`retry 3 { ... }` 读起来像内置语法）。

## 7. 练习

1. 给 `cli::run` 增加一个 `help` 子命令，打印所有已注册子命令的名字和简介（提示：可以维护一个 `dict` 作为"子命令名 -> 描述"的注册表）。
2. 把 `retry` 改造成支持"重试间隔"：`retry 3 1000 { script }` 表示失败后等待 1000 毫秒再重试（提示：`after` 命令）。
