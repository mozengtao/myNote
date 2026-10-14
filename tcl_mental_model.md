# Tcl 日常开发 80/20 指南：数据类型、常用 API 与核心 Mental Model

> 目标：日常写 Tcl，而不是深入 Tcl Interpreter。
>
> 核心目标：
>
> 1. 理解 Tcl 的基本数据模型
> 2. 掌握 `set / $ / [] / {} / ""` 五个核心语法元素
> 3. 掌握 String / List / Dict / Array / Number / Boolean
> 4. 掌握日常最常用 API
> 5. 能读懂和编写常见 Tcl 脚本

---

## 1. Tcl 最重要的 Mental Model

Tcl 最值得建立的第一个直觉：

> **Tcl 程序主要由 Command + Words 组成；变量保存 Tcl Value；Value 可以被不同命令按照不同方式解释。**

例如：

```tcl
set name Morris
puts "Hello $name"
```

可以理解为：

```
Tcl Script
    │
    ▼
Command
    │
    ├── set
    ├── name
    └── Morris
```

执行：

```
set name Morris
    │
    ▼
variable
    │
    ▼
name → "Morris"
```

之后：

```tcl
puts "Hello $name"
```

发生：

```
"Hello $name"
       │
       │ variable substitution
       ▼
"Hello Morris"
       │
       ▼
      puts
```

## 2. Tcl 的核心执行模型

日常理解 Tcl，只需要记住：

```
Tcl Script
    │
    ▼
Command
    │
    ▼
Words
    │
    ▼
Substitution
    │
    ├── $variable
    ├── [command]
    └── \escape
    │
    ▼
Command receives arguments
    │
    ▼
Execute
```

因此 Tcl 代码经常可以这样读：

```
command arg1 arg2 arg3
```

例如：

```tcl
puts "hello"
```

这里有两个 word：

- `puts`
- `"hello"`

再例如：

```tcl
set name Morris
```

有三个 word：

- `set`
- `name`
- `Morris`

## 3. set：变量的读写入口

`set` 是 Tcl 最重要的基础命令之一。

### 3.1 写变量

```tcl
set name Morris
```

Mental Model：

```
set VARIABLE VALUE
        │
        ▼
name → Morris
```

### 3.2 读变量

```tcl
set name
```

返回：

```
Morris
```

所以：

```tcl
set name Morris
set name
```

可以理解成：

> `set` = variable write/read API

### 3.3 修改变量

```tcl
set count 10
set count 20
```

最终：

```
count → 20
```

### 3.4 删除变量

```tcl
unset name
```

检查是否存在：

```tcl
info exists name
```

常见模式：

```tcl
if {[info exists name]} {
    puts $name
}
```

## 4. $：读取变量值

`$` 本身不是变量。

它表示：

> 把变量的 value 取出来，进行 substitution。

例如：

```tcl
set name Morris
puts $name
```

执行：

```
$name
  │
  ▼
"Morris"
```

所以：

```tcl
set name Morris
puts $name
```

可以理解成：

```
name
 │
 │ $name
 ▼
Morris
```

### 4.1 $ 和 set 的关系

这是非常重要的一组：

```tcl
set name Morris
```

相当于：

> 写：name → Morris

而：

```tcl
set name
```

是：

> 读：name → Morris

而：

```tcl
$name
```

是：

> 读取 value 并进行 substitution

所以：

```
set x VALUE
     │
     ▼
   x → VALUE
     │
     ├── set x      → 读取
     │
     └── $x         → substitution
```

## 5. []：Command Substitution

`[]` 是 Tcl 中另一个核心概念。

```tcl
[command ...]
```

意思：

> 执行里面的 command，并把 command 的返回值作为结果。

例如：

```tcl
set result [expr {10 + 20}]
```

执行过程：

```
[expr {10 + 20}]
        │
        ▼
       30
        │
        ▼
set result 30
```

所以 `[]` 可以理解成：

```
execute command
      │
      ▼
capture result
```

### 5.1 常见使用

```tcl
set length [string length $name]
```

流程：

```
$name
  │
  ▼
Morris
  │
  ▼
[string length $name]
  │
  ▼
6
  │
  ▼
set length 6
```

### 5.2 嵌套 command

```tcl
puts [string toupper [string trim $name]]
```

Mental Model：

```
[string trim $name]
        │
        ▼
      Morris
        │
        ▼
[string toupper Morris]
        │
        ▼
      MORRIS
        │
        ▼
puts MORRIS
```

因此 `[]` 是 Tcl 中组合 command 的核心机制。

## 6. {}：Grouping + Prevent Substitution

`{}` 是 Tcl 最容易让初学者困惑的地方之一。

日常可以先记住：

> `{}` 主要用于把内容作为一个整体，并阻止里面的大多数 substitution。

例如：

```tcl
set name Morris

set x {$name}
```

结果：

```
x → "$name"
```

而：

```tcl
set x "$name"
```

结果：

```
x → "Morris"
```

这是非常重要的区别。

## 7. ""：Grouping + Allow Substitution

双引号：

```tcl
"hello $name"
```

主要作用：

> 把多个字符组织成一个 word，同时允许 substitution。

例如：

```tcl
set name Morris
puts "Hello $name"
```

结果：

```
Hello Morris
```

## 8. {} vs ""

这是 Tcl 日常开发必须掌握的核心区别。

```tcl
set name Morris

set a "$name"
set b {$name}
```

结果：

```
a → Morris
b → $name
```

因为：

```
"$name"
   │
   └── substitution
       ↓
     Morris
```

而：

```
{$name}
   │
   └── no variable substitution
       ↓
      $name
```

### 8.1 对 [] 也是一样

```tcl
set a "[expr {10 + 20}]"
```

得到：

```
30
```

而：

```tcl
set b {[expr {10 + 20}]}
```

得到：

```
[expr {10 + 20}]
```

因此：

- `""` → grouping + substitution
- `{}` → grouping + literal

## 9. set / $ / [] / {} / "" 五者关系

这是整个 Tcl 80/20 中最值得背下来的图。

```
                    Tcl
                     │
                     ▼
                 set x VALUE
                     │
                     ▼
                  x → VALUE
                     │
             ┌───────┴────────┐
             │                │
            $x              set x
             │                │
             ▼                ▼
        variable value     read value
             │
     ┌───────┴────────┐
     │                │
    "$x"             {$x}
     │                │
     ▼                ▼
 substitution       literal
     │
     ▼
   VALUE



[command]
     │
     ▼
execute command
     │
     ▼
return value
```

可以压缩成一句话：

- `set` → 管变量
- `$` → 取变量值
- `[]` → 执行命令并取得结果
- `""` → 分组，同时允许 substitution
- `{}` → 分组，同时尽量保持原样

## 10. String：字符串

String 是 Tcl 日常开发中最基础的数据。

```tcl
set name "Morris"
set path "/tmp/test.txt"
set message "hello world"
```

常用 API：

```tcl
string length $s
string equal $a $b
string match "*.txt" $filename
string first "abc" $text
string last "abc" $text
string range $text 0 5
string trim $text
string trimleft $text
string trimright $text
string tolower $text
string toupper $text
string replace $text 0 3 "xxxx"
string map {old new} $text
```

### 10.1 String 比较

推荐：

```tcl
if {[string equal $a $b]} {
    puts "same"
}
```

也经常看到：

```tcl
if {$status eq "connected"} {
    puts "OK"
}
```

日常最重要的是：

- `string equal` → 字符串比较
- `eq` → 表达式中的字符串比较

## 11. List：有序序列

List 是 Tcl 最重要的数据结构之一。

```tcl
set fruits {apple banana orange}
```

Mental Model：

```
List
 │
 ├── index 0 → apple
 ├── index 1 → banana
 └── index 2 → orange
```

类似 Python：

```python
["apple", "banana", "orange"]
```

### 11.1 创建 List

```tcl
set fruits [list apple banana orange]
```

或者：

```tcl
set fruits {apple banana orange}
```

如果元素本身包含空格：

```tcl
set users [list "John Smith" "Alice Brown"]
```

### 11.2 List 常用 API

```tcl
llength $list
lindex $list index
lappend list value
lset list index value
lrange $list first last
lsearch $list value
lsort $list
linsert $list index value
lreplace $list first last value
concat $list1 $list2
join $list separator
split $string separator
```

### 11.3 最常用的几个

获取长度：

```tcl
llength $fruits
```

获取元素：

```tcl
lindex $fruits 0
```

添加：

```tcl
lappend fruits grape
```

修改：

```tcl
lset fruits 1 mango
```

遍历：

```tcl
foreach fruit $fruits {
    puts $fruit
}
```

## 12. List 的关键 Mental Model

不要把：

```tcl
set x "a b c"
```

简单理解成"字符串"。

它可以作为 List 被解释：

```tcl
lindex $x 1
```

得到：

```
b
```

因此：

```
Tcl Value
    │
    ├── string operation
    │
    └── list operation
```

重点：

> Tcl value 的结构意义很大程度上由你对它执行什么操作决定。

## 13. Dict：Key → Value

Dict 是现代 Tcl 日常开发非常重要的数据结构。

```tcl
set user [dict create \
    name Morris \
    age 30 \
    language Tcl]
```

Mental Model：

```
user
 │
 ├── name     → Morris
 ├── age      → 30
 └── language → Tcl
```

类似 Python：

```python
{
    "name": "Morris",
    "age": 30,
    "language": "Tcl"
}
```

## 14. Dict 常用 API

```tcl
dict create
dict get
dict set
dict exists
dict unset
dict keys
dict values
dict size
dict for
dict filter
dict map
dict merge
```

### 14.1 创建

```tcl
set user [dict create \
    name Morris \
    age 30]
```

### 14.2 获取

```tcl
dict get $user name
```

### 14.3 修改

```tcl
dict set user age 31
```

这里有一个非常 Tcl 的特点：

> `dict set user age 31` 可以直接修改变量 `user`。

### 14.4 判断 Key

```tcl
dict exists $user name
```

### 14.5 删除

```tcl
dict unset user age
```

### 14.6 遍历

```tcl
dict for {key value} $user {
    puts "$key = $value"
}
```

## 15. Nested Dict

工程代码中非常常见：

```tcl
set device [dict create \
    name router01 \
    network [dict create \
        ip 192.168.1.10 \
        port 22] \
    status connected]
```

读取：

```tcl
dict get $device network ip
```

Mental Model：

```
device
 │
 ├── name
 │    └── router01
 │
 ├── network
 │    ├── ip
 │    │   └── 192.168.1.10
 │    └── port
 │        └── 22
 │
 └── status
      └── connected
```

Dict 很适合：

- configuration
- state
- JSON-like data
- command parameters
- structured information

## 16. Array：一组 Tcl Variables

Tcl Array 和 Dict 不完全一样。

```tcl
set user(name) Morris
set user(age) 30
set user(language) Tcl
```

Mental Model：

```
user
 │
 ├── user(name)
 ├── user(age)
 └── user(language)
```

而 Dict 是：

```
user
 │
 ▼
one Tcl value
 │
 ├── name → Morris
 ├── age  → 30
 └── language → Tcl
```

所以：

- Dict → 一个结构化 Value
- Array → 一组相关 Variables

## 17. Array 常用 API

```tcl
array exists user
array names user
array size user
array get user
array set user ...
array unset user
```

例如：

```tcl
array set user {
    name Morris
    age 30
}
```

遍历：

```tcl
foreach key [array names user] {
    puts "$key = $user($key)"
}
```

日常 Tcl 中：

> 如果只是保存结构化数据，通常优先考虑 Dict；Array 更多用于需要 Tcl variable-array 语义的场景。

## 18. Number：整数和浮点数

```tcl
set x 10
set y 3.14
```

计算使用：`expr`

例如：

```tcl
set result [expr {$x + 20}]
set result [expr {$x * 2}]
set result [expr {$x / 2.0}]
```

### 18.1 expr Mental Model

```
expr
 │
 ▼
interpret value as expression
 │
 ▼
calculate
 │
 ▼
return result
```

例如：

```tcl
set a 10
set b 20

set c [expr {$a + $b}]
```

流程：

```
$a → 10
$b → 20

expr {$a + $b}
       │
       ▼
      10 + 20
       │
       ▼
       30

[capture result]
       │
       ▼
set c 30
```

## 19. Boolean

日常常见：

```tcl
if {$connected} {
    puts "connected"
}
```

或者：

```tcl
if {$count > 0} {
    puts "not empty"
}
```

常见 Boolean 表达：

- `1`
- `0`
- `true`
- `false`
- `yes`
- `no`
- `on`
- `off`

最重要的场景：`if`、`while`、`expr`

## 20. if + expr 的 Mental Model

例如：

```tcl
if {$count > 10} {
    puts "large"
}
```

可以理解：

```
$count
   │
   ▼
  20
   │
   ▼
20 > 10
   │
   ▼
  true
   │
   ▼
execute body
```

## 21. foreach：处理 List 的核心工具

最常见的 Tcl 数据处理模式之一：

```tcl
set devices {router1 router2 router3}

foreach device $devices {
    puts "checking $device"
}
```

Mental Model：

```
List
 │
 ├── router1 ──→ iteration 1
 ├── router2 ──→ iteration 2
 └── router3 ──→ iteration 3
```

## 22. foreach + Dict

```tcl
dict for {key value} $user {
    puts "$key = $value"
}
```

因此：

- List → `foreach`
- Dict → `dict for`

这是日常 Tcl 数据处理最常见的两个循环模型。

## 23. split / join

字符串和 List 之间经常需要转换。

```tcl
set text "a,b,c"

set list [split $text ","]
```

得到：

```
a
b
c
```

反过来：

```tcl
set text [join $list ","]
```

得到：

```
a,b,c
```

Mental Model：

```
String
  │
 split
  ▼
List
  │
 join
  ▼
String
```

这是处理：

- CSV-like data
- CLI output
- environment variables
- protocol text
- configuration

时非常常见的模式。

## 24. format

不要到处使用复杂字符串拼接。

常见场景：

```tcl
set msg [format "device=%s status=%s" $name $status]
```

数字：

```tcl
set msg [format "count=%d" $count]
```

浮点：

```tcl
set msg [format "%.2f" $value]
```

Mental Model：

```
format
  │
  ▼
structured string construction
```

## 25. string map

处理简单文本替换非常方便：

```tcl
set text "hello world"

set result [string map {
    hello hi
    world Tcl
} $text]
```

结果：

```
hi Tcl
```

Mental Model：

```
input string
     │
     ▼
replacement rules
     │
     ▼
output string
```

## 26. regexp

当 String API 不够时使用正则表达式：

```tcl
regexp {^eth[0-9]+$} $interface
```

例如：

```tcl
set interface eth10

if {[regexp {^eth[0-9]+$} $interface]} {
    puts "valid interface"
}
```

最常见用途：

- validation
- parsing
- extraction
- pattern matching

提取：

```tcl
regexp {([0-9]+)\.([0-9]+)} $text -> major minor
```

Mental Model：

```
String
  │
regexp
  │
  ├── match?
  └── extract values
```

## 27. switch

多个字符串/状态分支时：

```tcl
switch $status {
    connected {
        puts "OK"
    }
    disconnected {
        puts "DOWN"
    }
    default {
        puts "UNKNOWN"
    }
}
```

适合：

- state
- command
- type
- mode
- status

## 28. Tcl 日常数据处理 Mental Model

可以把日常 Tcl 程序理解成：

```
Input
  │
  ▼
String
  │
  ├── split ─────→ List
  │                  │
  │                  ├── foreach
  │                  ├── lindex
  │                  ├── lsort
  │                  └── lsearch
  │
  ├── regexp ─────→ extracted values
  │
  └── string APIs

List / Values
      │
      ▼
     Dict
      │
      ├── dict get
      ├── dict set
      ├── dict exists
      └── dict for

Values
  │
  ▼
expr
  │
  └── number / boolean
```

## 29. 一个完整的日常 Tcl 示例

假设收到设备信息：

```
router1,192.168.1.10,connected
router2,192.168.1.20,disconnected
router3,192.168.1.30,connected
```

先放进 String：

```tcl
set input {
router1,192.168.1.10,connected
router2,192.168.1.20,disconnected
router3,192.168.1.30,connected
}
```

按行拆分：

```tcl
set lines [split [string trim $input] "\n"]
```

Mental Model：

```
String
  │
  │ split
  ▼
List
  │
  ├── router1,...
  ├── router2,...
  └── router3,...
```

遍历：

```tcl
foreach line $lines {
    set fields [split $line ","]

    set name   [lindex $fields 0]
    set ip     [lindex $fields 1]
    set status [lindex $fields 2]

    puts "$name $ip $status"
}
```

这已经是非常典型的 Tcl 数据处理代码。

## 30. 使用 Dict 改进

可以把每一行转换成 Dict：

```tcl
set device [dict create \
    name   $name \
    ip     $ip \
    status $status]
```

于是：

```
line
 │
 ▼
split
 │
 ▼
List
 │
 ├── name
 ├── ip
 └── status
 │
 ▼
Dict
 │
 ├── name → router1
 ├── ip → 192.168.1.10
 └── status → connected
```

然后：

```tcl
if {[dict get $device status] eq "connected"} {
    puts "[dict get $device name] is UP"
}
```

这就是一个非常典型的：

```
String
   ↓
List
   ↓
Dict
   ↓
Business Logic
```

数据流。

## 31. 80/20 API Cheat Sheet

### Variable

```tcl
set x value
set x
unset x
info exists x
```

### String

```tcl
string length $s
string equal $a $b
string match pattern $s
string first needle $s
string range $s first last
string trim $s
string tolower $s
string toupper $s
string map {...} $s
```

### List

```tcl
list a b c
llength $list
lindex $list index
lappend list value
lset list index value
lrange $list first last
lsearch $list value
lsort $list
foreach x $list {...}
split $string sep
join $list sep
```

### Dict

```tcl
dict create key value
dict get $dict key
dict set dict key value
dict exists $dict key
dict unset dict key
dict keys $dict
dict values $dict
dict size $dict
dict for {key value} $dict {...}
dict merge $d1 $d2
```

### Array

```tcl
set a(key) value
array exists a
array names a
array size a
array get a
array set a {...}
array unset a
```

### Number / Boolean

```tcl
expr {$a + $b}
expr {$a > $b}
expr {$a == $b}
```

### Control

```tcl
if {...} {...}
switch $x {...}
foreach x $list {...}
for {...} {...} {...} {...}
while {...} {...}
```

### Text / Parsing

```tcl
format ...
regexp pattern string
```

## 32. 最终 Mental Model

日常 Tcl 不需要一开始深入 Interpreter。

只需要牢牢记住下面这张图：

```
                         Tcl Script
                              │
                              ▼
                         Command
                              │
                    ┌─────────┴─────────┐
                    │                   │
                 set x value         puts ...
                    │
                    ▼
                  Variable
                    │
                    ▼
                  Value
                    │
        ┌───────────┼────────────┐
        │           │            │
      String       List         Dict
        │           │            │
        │           │            ├── key/value
        │           │            │
        │           ├── index    │
        │           │            │
        └───────────┴────────────┘
                    │
                    ▼
               Application Data
```

而 Tcl 语法的五个核心元素：

| 元素 | 作用 |
|------|------|
| `set` | 读写变量 |
| `$` | 获取变量值 / variable substitution |
| `[]` | 执行 command / command substitution |
| `{}` | grouping，抑制 substitution |
| `""` | grouping，允许 substitution |

最终形成一个简单的阅读公式：

- 看到 `set` → 想"变量"
- 看到 `$` → 想"取变量值"
- 看到 `[]` → 想"执行里面的命令，并取得返回值"
- 看到 `{}` → 想"把内容原样组织起来，不要急着展开"
- 看到 `""` → 想"组织成一个 word，但允许 $ 和 [] 展开"

## 33. Tcl 80/20 学习优先级

推荐按照这个顺序掌握：

```
Level 1
│
├── set
├── $
├── []
├── {}
└── ""
       │
       ▼
Level 2
│
├── String
├── List
└── Dict
       │
       ▼
Level 3
│
├── if
├── foreach
├── switch
└── expr
       │
       ▼
Level 4
│
├── split / join
├── format
├── regexp
└── string map
       │
       ▼
Level 5
│
└── Array
       │
       ▼
Level 6
│
└── Namespace / Procedure / Package
```

80/20 的核心不是记住所有 Tcl 命令，而是形成下面这个数据流直觉：

```
               Tcl Program
                    │
                    ▼
                 String
                    │
          ┌─────────┼─────────┐
          │         │         │
        split     regexp    string
          │
          ▼
         List
          │
       foreach
          │
          ▼
         Dict
          │
       dict get/set
          │
          ▼
     Application State
          │
          ▼
        expr
          │
          ▼
    Decision / Control
```

再加上最核心的语法模型：

- `set` → 管变量
- `$` → 取值
- `[]` → 调用并取返回值
- `{}` → 原样 grouping
- `""` → 可 substitution 的 grouping

掌握这套模型后，再学习 Tcl 的 `proc`、`namespace`、`package`、`uplevel`、`upvar`、`eval` 等高级机制，会容易很多。

---

> 下面是 80/20 之外、工程代码中迟早会遇到的进阶机制：**函数（proc）、作用域（upvar / uplevel）、动态执行（eval）、以及模块化（namespace / package）**。

## 34. proc：自定义函数（过程）

Tcl 没有"函数"这个关键字，而是用 `proc` 定义一个新的 **command**。

> **proc 的本质：把一段 script 包装成一个新命令，供后续像内置命令一样调用。**

### 34.1 基本语法

```tcl
proc greet {name} {
    return "Hello, $name"
}

puts [greet Morris]
```

Mental Model：

```
proc greet {name} { ... }
     │
     ▼
新的 Command：greet
     │
     ▼
greet Morris
     │
     ▼
执行 body，substitution name → Morris
     │
     ▼
return "Hello, Morris"
```

### 34.2 默认参数

```tcl
proc greet {name {greeting "Hello"}} {
    return "$greeting, $name"
}

greet Morris          ;# Hello, Morris
greet Morris "Hi"     ;# Hi, Morris
```

### 34.3 可变参数（args）

最后一个参数名为 `args` 时，会把多余的参数收集成一个 List。

```tcl
proc sum {args} {
    set total 0
    foreach n $args {
        set total [expr {$total + $n}]
    }
    return $total
}

sum 1 2 3   ;# 6
```

Mental Model：

```
sum 1 2 3
     │
     ▼
args → {1 2 3}
     │
     ▼
foreach n $args
     │
     ▼
total → 6
```

### 34.4 返回值（return）

- 遇到 `return` 立即结束 proc 并返回指定值。
- 如果没有显式 `return`，proc 返回 **body 中最后一条命令的结果**。

```tcl
proc square {x} {
    expr {$x * $x}
}

puts [square 5]   ;# 25（最后一条命令的结果被当作返回值）
```

### 34.5 变量作用域：local vs global

proc 内部变量默认是 **local**，和外部变量互不影响；需要访问外部（全局）变量时用 `global`。

```tcl
set counter 0

proc increment {} {
    global counter
    incr counter
}

increment
puts $counter   ;# 1
```

Mental Model：

```
Global Scope
 │
 └── counter → 0

proc increment {}
 │
 ├── local scope（默认是空的，看不到 counter）
 │
 └── global counter
          │
          ▼
     把 counter 引入 local scope
          │
          ▼
     incr counter
          │
          ▼
   Global Scope.counter → 1
```

## 35. upvar：引用调用者的变量

`upvar` 用于在当前 proc 中创建一个变量，它是调用栈中**更高层某个变量的别名**（类似"传引用"）。

```tcl
proc setValue {varName value} {
    upvar 1 $varName ref
    set ref $value
}

set x 10
setValue x 99
puts $x   ;# 99
```

Mental Model：

```
caller frame（level 0）
 │
 └── x → 10

proc setValue（level 1）
 │
 └── ref ──upvar 1──→ caller.x
            │
            ▼
        set ref 99
            │
            ▼
      caller.x → 99
```

常见用途：

- 模拟 "pass by reference"
- 实现 `swap`、`increment` 这类需要修改调用者变量的工具 proc
- `global varName` 本质上就是 `upvar #0 varName varName` 的语法糖

## 36. uplevel：在调用者的作用域中执行代码

`uplevel` 不是建立变量别名，而是**切换到更高层的 stack frame，去执行一段 script**。

```tcl
proc setXTo10 {} {
    uplevel 1 {set x 10}
}

setXTo10
puts $x   ;# 10
```

Mental Model：

```
caller frame（level 0）
 │
 └── (执行 set x 10 之后) x → 10

proc setXTo10（level 1）
 │
 └── uplevel 1 {set x 10}
            │
            ▼
     跳到 level 0 执行 {set x 10}
            │
            ▼
      caller.x → 10
```

`upvar` vs `uplevel` 的核心区别：

- `upvar` → 在当前作用域里**引用**另一层的变量
- `uplevel` → 跳到另一层**执行**一段代码

两者常被用来实现"看起来像内置语法"的自定义控制结构（例如自定义的 `while` / `assert`）。

## 37. eval：动态构造并执行命令

`eval` 把一个字符串或 List **当作 Tcl 命令**来执行。

```tcl
set cmd {puts "dynamic"}
eval $cmd
```

Mental Model：

```
String / List（被当作 command）
      │
      ▼
     eval
      │
      ▼
   执行该命令
```

拼接执行：

```tcl
set command "puts"
set message "hello from eval"
eval $command [list $message]
```

常见用途：

- 元编程：根据配置/参数动态拼出命令再执行
- 把多个 Tcl word 拼成一条命令统一执行

> ⚠️ 注意：对用户输入直接 `eval` 存在命令注入风险，日常使用需谨慎，优先考虑 `{*}$list`（argument expansion）等更安全的替代方式。

## 38. namespace：组织变量与命令

`namespace` 用于把变量和 proc 组织到独立的命名空间，避免大型程序中的命名冲突。

```tcl
namespace eval ::myapp {
    variable version "1.0"

    proc hello {} {
        variable version
        puts "myapp v$version"
    }
}

myapp::hello   ;# myapp v1.0
```

Mental Model：

```
::myapp（namespace）
   │
   ├── variable version → "1.0"
   │
   └── proc hello
          │
          └── variable version → 访问 namespace 自己的 version，
                                   而不是 global version
```

常用 API：

```tcl
namespace eval name {...}
namespace export cmd1 cmd2
namespace import ::ns::cmd
namespace current
namespace delete name
```

用途：

- 大型工程中按模块组织代码
- 库作者避免命令名和用户代码冲突
- 配合 `variable` 关键字管理模块级状态

## 39. package：模块化与依赖管理

`package` 用于声明和加载可复用的 Tcl 模块（library）。

```tcl
# mylib.tcl
package provide mylib 1.0

namespace eval ::mylib {
    proc greet {} {
        puts "hello from mylib"
    }
}
```

```tcl
# main.tcl
package require mylib 1.0
mylib::greet
```

Mental Model：

```
mylib.tcl
   │
   ▼
package provide mylib 1.0
   │
   ▼
注册模块：mylib → 1.0

main.tcl
   │
   ▼
package require mylib 1.0
   │
   ▼
查找 / 加载 mylib
   │
   ▼
校验版本是否满足 1.0
   │
   ▼
mylib::greet 可用
```

常用 API：

```tcl
package provide name version
package require name ?version?
package names
package versions name
package ifneeded name version script
```

## 40. 进阶机制 Cheat Sheet

### Procedure（函数）

```tcl
proc name {args} {body}
proc name {a {b default}} {body}
proc name {a args} {body}
return value
```

### Scope（作用域）

```tcl
global varName
upvar level otherVar localVar
upvar #0 varName localVar
```

### Dynamic Execution（动态执行）

```tcl
eval command
eval $commandList
uplevel level {script}
uplevel #0 {script}
```

### Namespace（命名空间）

```tcl
namespace eval name {...}
namespace export cmd
namespace import ::ns::cmd
namespace current
namespace delete name
```

### Package（模块）

```tcl
package provide name version
package require name version
package names
```

## 41. 进阶 Mental Model 总图

把 proc / upvar / uplevel / eval / namespace / package 放进同一张图：

```
                 Tcl Script
                      │
                      ▼
                 namespace eval ::myapp
                      │
         ┌────────────┴────────────┐
         │                         │
    variable state            proc hello {...}
         │                         │
         │                         ▼
         │                 新的 Command：myapp::hello
         │                         │
         │                         ▼
         │                 调用栈 frame（level N）
         │                         │
         │             ┌───────────┴───────────┐
         │             │                       │
         │          upvar level              uplevel level
         │             │                       │
         │             ▼                       ▼
         │     引用更高层变量             在更高层执行 script
         │
         ▼
   package provide myapp 1.0
         │
         ▼
   package require myapp 1.0（供其他脚本加载）
```

一句话总结：

- `proc` → 定义新 Command（函数）
- `global` / `upvar` → 跨作用域**引用**变量
- `uplevel` → 跨作用域**执行**代码
- `eval` → 把字符串 / List 当作命令**动态执行**
- `namespace` → 组织变量和 Command，避免命名冲突
- `package` → 声明、加载、管理模块版本依赖

至此，从「`set / $ / [] / {} / ""`」到「`proc / namespace / package / uplevel / upvar / eval`」，构成了一条从 80/20 日常语法到工程化模块组织的完整学习路径。

---

## 42. 真实场景综合示例

前面 41 节讲的都是**孤立**的概念：单独看 `dict`、单独看 `upvar`、单独看 `namespace`。但真实项目代码从来不会只用一个概念——一段真实的 Tcl 脚本，往往是 String、Dict、`proc`、`namespace` 等好几个概念**同时组合**在一起工作。

[tcl-real-world-scenarios/](tcl-real-world-scenarios/README.md) 目录设计了 4 个由浅入深、可以直接 `tclsh` 运行的完整场景，把本文件讲过的概念串联起来：

| # | 场景 | 核心问题 | 综合运用的概念 | 链接 |
|---|---|---|---|---|
| 1 | 日志文件分析与统计报表 | 一段文本日志，怎么变成一份结构化的统计报表？ | String / `split` / `regexp` / Dict / Nested Dict / `foreach` / `switch` / `format` / `proc` | [01-log-analyzer](tcl-real-world-scenarios/01-log-analyzer/README.md) |
| 2 | 键值配置文件管理器 | 一份 `key=value` 配置文件，怎么安全地读写、带默认值？ | Nested Dict / Dict vs Array / `regexp` / `switch` / `proc` 默认参数 / `namespace` + `variable` | [02-config-manager](tcl-real-world-scenarios/02-config-manager/README.md) |
| 3 | 命令行子命令框架 | 类似 `git <subcommand>` 的工具，内部是怎么组织的？ | `proc` + `args` / `switch` / `upvar` / `uplevel` / `namespace` / `package provide` / `{*}` | [03-cli-framework](tcl-real-world-scenarios/03-cli-framework/README.md) |
| 4 | 插件化任务执行引擎 | 怎么做到"新增任务类型不用改主程序"？ | Nested Dict / `foreach` / `eval` / `namespace` / `package` 思想 / `catch` | [04-plugin-task-runner](tcl-real-world-scenarios/04-plugin-task-runner/README.md) |

每个场景的 README 都遵循统一结构：真实背景 → 涉及的 Tcl 概念（逐条对照本文件的章节号）→ Mental Model → 完整代码 → 执行讲解 → 工程实践要点（含真实踩坑记录）→ 练习。总览见 [tcl-real-world-scenarios/README.md](tcl-real-world-scenarios/README.md)。
