# Tcl 解析流水线与日常最佳实践：Mental Model

## 1. 核心 Mental Model

Tcl 可以理解为一个由三部分协作的系统：

1. **Parser：** 识别命令、单词和特殊语法边界。
2. **Substitution：** 根据语法上下文进行反斜杠、变量和命令替换。
3. **Command Dispatch：** 将第一个单词作为命令名，将其余单词作为参数，调用对应命令。

需要特别注意：这三个阶段是理解 Tcl 的有效抽象，但不是所有语法构造都机械地执行完全相同的三步。大括号中的内容不会在外层进行常规替换，而 `expr` 等命令还可能进一步解释自己收到的参数。

整体结构：

```text
Tcl Script
    |
    v
+---------------------------+
| 1. Parse Command          |
|    Identify words         |
|    Respect {} and ""      |
+---------------------------+
    |
    v
+---------------------------+
| 2. Perform Substitution   |
|    Backslash: \           |
|    Variable:   $          |
|    Command:    []         |
|                           |
|    Depends on syntax      |
+---------------------------+
    |
    v
+---------------------------+
| 3. Command Dispatch       |
|    word[0] = command name |
|    word[1..N] = arguments |
+---------------------------+
    |
    v
Execute Command
    |
    +---- expr: evaluate expression
    +---- puts: write output
    +---- set: read/write variable
    +---- list: construct list
```

**一句话记忆：Tcl 负责把脚本变成命令调用；命令负责解释自己的参数。**

---

## 2. 阶段一：分词与单词边界

Tcl 命令通常由空白分隔。换行和分号可以分隔命令，但它们在引号或大括号构成的语法上下文中可以是普通内容。

### 2.1 普通空格分隔

```tcl
puts hello world
```

分词结果：

```text
word[0] = puts
word[1] = hello
word[2] = world
```

`puts` 收到两个参数 `hello` 和 `world`，因此会输出：

```text
hello world
```

注意：`puts` 并不会自动把多个参数合并为一个参数。

### 2.2 双引号：一个参数，可以包含空格

```tcl
puts "hello world"
```

分词结果：

```text
word[0] = puts
word[1] = "hello world"
```

双引号把带空格的内容组合成一个参数，同时允许变量替换、命令替换和反斜杠替换。

```tcl
set name Morris
puts "Hello, $name"
```

输出：

```text
Hello, Morris
```

### 2.3 大括号：一个字面参数

```tcl
puts {hello world}
```

分词结果：

```text
word[0] = puts
word[1] = hello world
```

大括号内部的空格不会拆分参数，外层 Tcl 也不会对其内部进行常规的变量、命令和反斜杠替换。

```tcl
set name Morris
puts {$name}
puts {$name [expr 1 + 2]}
```

输出：

```text
$name
$name [expr 1 + 2]
```

大括号不是“让所有解释永久失效”，而是控制**当前外层解析层次**如何处理这段文本。

---

## 3. 阶段二：三种替换

日常编程中，需要认识三种常见替换：

| 替换类型  | 语法              | 作用             |
| ----- | --------------- | -------------- |
| 反斜杠替换 | `\n`、`\t`、`\\`  | 将转义序列转换为对应字符   |
| 变量替换  | `$x`、`${name}`  | 读取变量值          |
| 命令替换  | `[command ...]` | 执行内部命令，并取得返回结果 |

### 3.1 反斜杠替换

```tcl
puts "line1\nline2"
puts "A\tB"
```

输出中，第一条命令包含换行，第二条命令包含制表符。

反斜杠替换可以用于双引号中的字符串；在大括号中，反斜杠通常保持原样。

```tcl
puts "\n"
puts {\n}
```

第一条输出换行字符，第二条输出两个可见字符 `\` 和 `n`。

### 3.2 变量替换

```tcl
set x 10
puts $x
puts {$x}
puts "$x"
```

输出：

```text
10
$x
10
```

关键区别：

* `$x`：在允许替换的上下文中读取变量。
* `{$x}`：将 `$x` 作为字面文本传递。
* `"$x"`：仍然执行变量替换。

变量替换的结果不会自动递归替换。

```tcl
set y 20
set x {$y}
puts $x
```

输出：

```text
$y
```

`x` 保存的是字符串 `$y`，而不是对变量 `y` 的动态引用。

### 3.3 命令替换

```tcl
puts "sum=[expr {10 + 5}]"
```

执行过程：

```text
发现 [...]
    |
    v
执行 expr {10 + 5}
    |
    v
返回 15
    |
    v
构造字符串 "sum=15"
    |
    v
调用 puts
```

输出：

```text
sum=15
```

命令替换的结果不会自动再次进行变量替换或命令替换。

例如：

```tcl
set x {$y}
puts [set x]
```

这里 `[set x]` 返回字符串 `$y`，外层不会继续把它展开为变量 `y` 的值。

### 3.4 三种替换的顺序如何理解？

可以把反斜杠、变量和命令替换视为三个需要掌握的操作，但不应把它们理解成任意位置都严格按固定顺序执行的三个全局扫描阶段。

对于普通的、未被大括号保护的单词，Tcl 解析器会依据语法识别这些替换；替换产生的结果不会被当成全新的源代码无限重新扫描。

例如：

```tcl
set x 10
puts "\$x"
```

输出：

```text
$x
```

`\$` 阻止 `$x` 在当前上下文中作为变量引用展开。

---

## 4. 阶段三：命令执行与参数传递

Tcl 的命令调用模型非常简单：

```text
set x 10
```

等价于概念上的：

```text
command = set
argv[1] = x
argv[2] = 10
```

这里的 `argv` 仅用于帮助理解参数结构，并非 Tcl 必须使用的内部变量名。

Tcl 命令接收到的参数在接口层面都是字符串；但具体命令可以按照自身的语义进一步解析这些字符串。

### 4.1 set：管理变量

```tcl
set x 10
set x
```

第一条命令设置变量，第二条命令读取变量。

### 4.2 puts：输出字符串

```tcl
puts "x=$x"
```

`puts` 不负责解析字符串中的 `$x`。替换已经由外层 Tcl 解析器完成。

### 4.3 expr：解析表达式

```tcl
set x 10
puts [expr {$x + 5}]
```

执行流程：

```text
外层 Tcl
    |
    | 解析 expr 命令
    | 将 {$x + 5} 作为字面参数
    v
expr 接收 "$x + 5"
    |
    | 表达式求值
    | 读取变量 x
    v
10 + 5
    |
    v
15
```

这里最重要的概念是：

**外层 Tcl 的替换规则，与 expr 自己的表达式求值规则，不是同一层规则。**

---

## 5. 方括号：开启嵌套命令解析

方括号不是普通字符串括号，而是命令替换语法。

```tcl
set x 10
puts "sum=[expr {$x + 5}]"
```

概念上的调用过程：

```text
Outer Tcl command
    |
    v
puts "sum=[...]"
    |
    | Execute command inside [...]
    v
expr {$x + 5}
    |
    | expr evaluates its argument
    v
15
    |
    | Replace [...] with command result
    v
puts "sum=15"
```

可以把 `[]` 理解为一个嵌套的 Tcl 命令执行区域。

但要注意两个细节：

1. 内部命令在执行前仍然遵循 Tcl 的语法规则。
2. 外层的替换结果不会自动作为新脚本再次执行。

例如：

```tcl
set x 10
puts [expr {$x + 5}]
```

是命令替换，不是把 `15` 当成新 Tcl 程序重新解释。

### 嵌套方括号

```tcl
puts [string length [string toupper hello]]
```

执行顺序从最内层命令开始：

```text
string toupper hello
        |
        v
      HELLO
        |
        v
string length HELLO
        |
        v
        5
        |
        v
      puts 5
```

输出：

```text
5
```

---

## 6. 什么时候使用 {}，什么时候使用 ""？

这是日常 Tcl 最重要的工程实践之一。

### 6.1 优先使用 {}：表达式、条件、循环、脚本块

#### 算术表达式

推荐：

```tcl
set x 10
set y 20

set sum [expr {$x + $y}]
```

不推荐作为默认写法：

```tcl
set sum [expr "$x + $y"]
```

第一种写法将表达式完整交给 `expr`，避免外层提前替换变量。

#### if 条件

推荐：

```tcl
if {$x > 5} {
    puts "large"
}
```

这里有两个不同用途的大括号：

* `{$x > 5}`：将条件表达式交给 `if` 命令处理。
* `{ puts "large" }`：将脚本作为字面参数交给 `if`，由 `if` 在条件满足时执行。

#### 循环

```tcl
for {set i 0} {$i < 3} {incr i} {
    puts "i=$i"
}
```

四个参数分别是：

```text
for
    |
    +-- 初始化脚本：set i 0
    +-- 条件表达式：$i < 3
    +-- 更新脚本：incr i
    +-- 循环体脚本：puts "i=$i"
```

这些大括号确保 `for` 能在正确的时间执行初始化、判断、更新和循环体。

#### 传递脚本给其他命令

```tcl
after 1000 {
    puts "timer fired"
}
```

这里的大括号使回调脚本作为文本传给 `after`，而不是在调用 `after` 时立即执行。

这是 Tcl 中控制执行时机的重要模式。

### 6.2 使用 ""：需要立即构造字符串

#### 字符串插值

```tcl
set user Morris
puts "Hello, $user"
```

#### 将计算结果嵌入文本

```tcl
set x 10
puts "sum=[expr {$x + 5}]"
```

#### 路径或消息构造

```tcl
set filename report.txt
set path "/tmp/$filename"

puts "output path: $path"
```

注意：双引号只负责 Tcl 字符串组合和替换，不负责文件系统路径的特殊规则，也不自动完成 shell 转义。

### 6.3 用 {} 保存字面文本

```tcl
set pattern {^[A-Z]+[0-9]+$}
set message {$name is not expanded}
```

适合保存：

* 正则表达式
* 不希望立即替换的文本
* 包含大量特殊字符的字符串
* 希望交给其他命令进一步解释的脚本

不过，大括号内部的花括号需要正确配对；如果文本必须原样包含不配对的花括号，应考虑其他构造方式。

---

## 7. 常用命令的 {} / "" 最佳实践

| 场景        | 推荐写法                          | 原因            |
| --------- | ----------------------------- | ------------- |
| 算术表达式     | `expr {$x + 1}`               | 避免外层提前替换      |
| if 条件     | `if {$x > 0} {...}`           | 让 if 控制条件求值   |
| for 循环    | `for {...} {...} {...} {...}` | 让 for 控制执行时机  |
| while 条件  | `while {$x < 10} {...}`       | 每次循环重新判断      |
| after 回调  | `after 1000 {...}`            | 延迟执行脚本        |
| 动态消息      | `"Hello, $name"`              | 立即插入变量值       |
| 动态字符串拼接   | `"prefix-$x"`                 | 构造包含变量值的字符串   |
| 字面文本      | `{$x + 1}`                    | 保留原始字符        |
| 正则表达式     | `{^[A-Z]+$}`                  | 避免不必要的 Tcl 替换 |
| 命令结果嵌入字符串 | `"result=[command]"`          | 将命令返回值嵌入字符串   |

注意：并不是所有命令都要求使用大括号。应该根据命令的参数语义决定如何组织参数。

---

## 8. 工程实践中最值得遵循的规则

### 规则 1：表达式优先使用 {}

```tcl
if {$count > 0} {
    puts "non-empty"
}
```

不要习惯性地提前展开表达式中的变量。

### 规则 2：需要插值的字符串使用 ""

```tcl
puts "device=$device status=$status"
```

### 规则 3：需要延迟执行的脚本使用 {}

```tcl
after 1000 {
    puts "later"
}
```

### 规则 4：不要混淆 Tcl 解析与 shell 解析

```tcl
set filename "a b.txt"
```

这是 Tcl 的字符串赋值，不意味着 shell 会如何解释这个路径。

如果使用 `exec` 启动外部程序，还需要理解 Tcl 的参数传递规则，以及外部程序自身的参数解析方式。

### 规则 5：避免不必要的 eval

```tcl
set command [list puts {hello world}]
{*}$command
```

在支持 `{*}` 参数展开的 Tcl 版本中，这会将列表元素作为命令及其参数进行调用，而不需要将它们拼成字符串再交给 `eval` 重新解析。

通常应优先使用列表和参数展开，而不是拼接 Tcl 脚本字符串。

---

## 9. 一页速记

```text
Tcl Mental Model
================

1. Parser
   - 识别命令和单词边界
   - 空白通常分隔单词
   - {} 保留字面内容
   - "" 组合参数并允许替换

2. Substitution
   - \  反斜杠替换
   - $  变量替换
   - [] 命令替换
   - 实际行为取决于语法上下文

3. Command Dispatch
   - 第一个单词是命令名
   - 剩余单词是参数
   - 命令决定如何解释参数

4. Nested Evaluation
   - [] 触发内部命令执行
   - 内部命令按 Tcl 语法解析
   - expr 还会解析表达式
   - 替换结果不会自动无限递归

5. Best Practices
   - expr / if / while：优先使用 {}
   - for / after 的脚本参数：使用 {}
   - 需要立即插值的字符串：使用 ""
   - 需要保留字面文本：使用 {}
   - 避免字符串拼接脚本后再 eval
```

最终记住：

**`{}` 控制外层解析器何时不解释文本；`""` 允许外层解析器进行替换；`[]` 执行命令并取得结果；而真正的参数语义由接收参数的命令决定。**
