# Python Regular Expression Complete Reference

基于 [Python 官方 `re` 文档](https://docs.python.org/3/library/re.html)（当前对应 CPython **3.14**）整理的正则表达式语法参考。

- 重点是 **Regex Syntax**，不是 `re` 模块 API 百科。
- 中文解释；Regex 与 Python 代码用英文。
- 版本号只标注官方文档出现过的 `Added` / `Changed`。
- 不收录 PCRE / Perl / Java / JavaScript / .NET / 第三方 [`regex`](https://pypi.org/project/regex/) 模块中、但标准库 `re` 未支持的语法。
- 3.11 的占有量词 / 原子组、3.14 的 `\z` 与空串 `\B` 按官方文档记录；在 3.10 上编译这些写法会失败。

辅助阅读：[Regular Expression HOWTO](https://docs.python.org/3/howto/regex.html)

---

## 目录

1. [Regex 核心心智模型](#1-regex-核心心智模型)
2. [Metacharacters](#2-metacharacters)
3. [Character Classes](#3-character-classes)
4. [Quantifiers](#4-quantifiers)
5. [Greedy / Lazy / Possessive](#5-greedy--lazy--possessive)
6. [Special Sequences](#6-special-sequences)
7. [Anchors](#7-anchors)
8. [Groups](#8-groups)
9. [Backreferences](#9-backreferences)
10. [Lookahead](#10-lookahead)
11. [Lookbehind](#11-lookbehind)
12. [Atomic Groups](#12-atomic-groups)
13. [Conditional Patterns](#13-conditional-patterns)
14. [Inline Flags](#14-inline-flags)
15. [Python re Flags](#15-python-re-flags)
16. [Escape Sequences](#16-escape-sequences)
17. [Unicode / ASCII](#17-unicode--ascii)
18. [Regex Syntax 对比](#18-regex-syntax-对比)
19. [Regex 阅读方法](#19-regex-阅读方法)
20. [Regex 编写方法](#20-regex-编写方法)
21. [常见陷阱](#21-常见陷阱)
22. [一页 Regex Cheat Sheet](#22-一页-regex-cheat-sheet)

---

## 1. Regex 核心心智模型

Regex 不是一堆需要背诵的特殊符号，而是一套对字符流施加约束的规则语言。

```text
输入:     一串字符
Pattern:  一组关于字符、数量、位置和关系的规则
Engine:   从输入中寻找满足这些规则的路径
Result:   Match / No Match
```

### 1.1 五层结构

```text
Regex
|
+-- 1. Character          匹配什么
|     +-- literal
|     +-- .
|     +-- []
|     +-- \d  \w  \s
|
+-- 2. Quantity           匹配多少
|     +-- *  +  ?  {m,n}
|     +-- lazy
|     +-- possessive
|
+-- 3. Structure          如何组合
|     +-- ()  (?:)
|     +-- |
|     +-- named group
|     +-- backreference
|
+-- 4. Position / Condition  在什么位置 / 满足什么条件
|     +-- ^  $  \A  \Z  \z
|     +-- \b  \B
|     +-- lookahead / lookbehind
|     +-- conditional
|
+-- 5. Matching Policy    多种可能时怎么选
      +-- greedy
      +-- lazy
      +-- possessive
      +-- atomic
      +-- flags
```

### 1.2 第一性原理

```text
Regex Pattern
|
+-- What?          匹配什么字符？
|                    literal / [] / . / \d / \w / \s
|
+-- How many?      匹配多少次？
|                    * / + / ? / {m,n}
|
+-- Where?         在什么位置？
|                    ^ / $ / \b / lookaround
|
+-- Relationship?  字符之间是什么组合关系？
|                    () / | / backreference
|
+-- Policy?        遇到多种匹配可能时如何选择？
                     greedy / lazy / possessive / atomic / flags
```

一句话：

```text
Regex
=
Character
+
Quantity
+
Structure
+
Position
+
Policy
```

看到任何复杂 Regex，都先还原成：

```text
字符约束 + 数量约束 + 结构约束 + 位置约束 + 匹配策略
```

---

## 2. Metacharacters

普通字符（如 `A`、`a`、`0`）匹配自身。`last` 匹配字符串 `'last'`。

下面这些字符是特殊的：它们要么代表一类字符，要么改变周围表达式的解释方式。

| Regex | 名称 | 含义 | 示例 | 匹配示例 | 备注 |
| --- | --- | --- | --- | --- | --- |
| `.` | 任意字符 | 默认匹配除换行外任意 **1** 个字符 | `a.c` | 匹配 `abc`、`axc`；默认不匹配 `a\nc` | `re.DOTALL` / `(?s)` 下也可匹配换行；`(?s:.)` 无视外层 flag，始终匹配含换行的任意字符 |
| `^` | 行首 / 串首 | 匹配字符串开始位置；不消费字符 | `^foo` | 匹配 `foo` 的开头；不匹配 `xfoo` 中的 `foo` | 默认只认整个字符串开头。`re.MULTILINE` 下也匹配每个换行后的位置 |
| `$` | 行尾 / 串尾 | 匹配字符串末尾，或末尾换行之前；不消费字符 | `foo$` | 匹配 `foo`；不匹配 `foobar` 的整串 | 对 `'foo\n'` 搜索单独 `$` 会找到 **两处** 空匹配：换行前 + 真正末尾。`re.MULTILINE` 下也匹配每个换行前 |
| `*` | 零次或多次 | 前一个表达式重复 **0~N** 次，默认贪婪 | `ab*` | 匹配 `a`、`ab`、`abbb` | Greedy。不能直接再套一个量词 |
| `+` | 一次或多次 | 前一个表达式重复 **1~N** 次，默认贪婪 | `ab+` | 匹配 `ab`、`abbb`；**不匹配** 单独 `a` | Greedy |
| `?` | 零次或一次 | 前一个表达式重复 **0 或 1** 次，默认贪婪 | `ab?` | 匹配 `a`、`ab`；不匹配 `abb` 的整串（只会吃到 `ab`） | Greedy |
| `{m}` | 恰好 m 次 | 前一个表达式必须刚好出现 m 次 | `a{6}` | 匹配 `aaaaaa`；不匹配 `aaaaa` | m 少了则整个失败 |
| `{m,n}` | m 到 n 次 | 重复 m~n 次，默认尽量多 | `a{3,5}` | 在 `aaaaaa` 上先吃 5 个 `a` | 省略 m 则下界为 0；省略 n 则上界无限。逗号不能省，否则会变成 `{m}` |
| `{m,}` | 至少 m 次 | 重复 m~无限次，默认贪婪 | `a{4,}b` | 匹配 `aaaab`、很长的 `aaa...ab`；不匹配 `aaab` | `{4,}` 的逗号必须保留 |
| `{,n}` | 至多 n 次 | 重复 0~n 次，默认贪婪 | `a{,2}` | 匹配 ``（空）、`a`、`aa` | 官方：省略 m 表示下界 0 |
| `[]` | 字符类 | 匹配集合中的 **1** 个字符 | `[A-Z]` | 匹配 `A`、`F` | 内部多数元字符失效，见第 3 节 |
| `\` | 转义 / 特殊序列 | 取消后一元字符的特殊含义，或引入特殊序列 | `a\.txt` | 匹配 `a.txt`；不匹配 `aXtxt` | 见第 6、16 节。推荐 raw string |
| `\|` | 分支 | 从左到右尝试，先完全匹配的分支获胜 | `cat\|dog` | 匹配 `cat` 或 `dog` | 优先级低于拼接。`ab\|cd` 是 `ab` OR `cd`，不是 `a(b\|c)d`。`\|` 从不贪婪 |
| `()` | 捕获组 | 分组，并保存匹配内容、占用组号 | `(\d+)-(\d+)` | 对 `123-456`：`group(0)='123-456'`，`group(1)='123'`，`group(2)='456'` | 也用于量词作用范围。字面量括号写 `\(` `\)` 或 `[(]` `[)]` |

### 2.1 `^` / `$` 与 `re.MULTILINE`

| 模式 | `^` | `$` |
| --- | --- | --- |
| 默认 | 只在整个字符串开头 | 整个字符串末尾，或末尾那个换行之前 |
| `re.MULTILINE` | 字符串开头 **以及** 每个换行之后 | 字符串末尾 **以及** 每个换行之前 |

官方例子：在 `'foo1\nfoo2\n'` 中搜索 `foo.$`

- 默认：匹配 `foo2`
- `re.MULTILINE`：还能匹配 `foo1`

`\A` / `\Z` / `\z` **不受** `re.MULTILINE` 影响。见 [第 7 节](#7-anchors)。

### 2.2 `.` 与 `re.DOTALL`

| 模式 | `.` 能否匹配 `\n` |
| --- | --- |
| 默认 | 否 |
| `re.DOTALL` / `(?s)` | 是 |
| `(?s:.)` | 是，且只作用于这个局部 |

### 2.3 拼接与 `|` 优先级

Regex 默认是拼接：`AB` 表示先匹配 A，再匹配 B。

`|` 优先级更低：

```text
ab|cd     =  (ab)  OR  (cd)
不是        a + (b OR c) + d
```

扫描时从左到右尝试各分支。左边一旦完整匹配，右边即使能匹配得更长也不会再试。所以 `|` 从不贪婪。

匹配字面 `|`：写 `\|` 或 `[|]`。

### 2.4 量词不能直接嵌套

官方明确：`*`、`+`、`?`、`{m,n}` 等不能直接叠在一起，以免和懒惰后缀 `?`、占有后缀 `+` 混淆。

```text
(?:a{6})*     合法：每 6 个 a 作为一轮，再重复 0~N 轮
a{6}*         非法
```

### 示例

```python
import re

print(re.search(r"a.c", "abc").group())          # abc
print(re.search(r"foo$", "foo").group())         # foo
print(re.search(r"foo$", "foobar"))              # None
print(re.findall(r"foo.$", "foo1\nfoo2\n"))      # ['foo2']
print(re.findall(r"foo.$", "foo1\nfoo2\n", re.M))# ['foo1', 'foo2']
print(re.findall(r"ab+", "a ab abb"))            # ['ab', 'abb']  没有单独的 'a'
print(re.search(r"ab|cd", "xcdy").group())       # cd
```

---

## 3. Character Classes

`[]` 匹配其中 **恰好一个** 字符。进入字符类后，大多数外部元字符失去特殊意义。

| Regex | 名称 | 含义 | 示例 | 匹配示例 | 备注 |
| --- | --- | --- | --- | --- | --- |
| `[abc]` | 枚举字符类 | 匹配 `a` 或 `b` 或 `c` 中的 1 个 | `[amk]` | 匹配 `a`、`m`、`k` | 列出的是字符，不是串 |
| `[a-z]` | 小写 ASCII 范围 | 匹配 `a` 到 `z` 的 1 个字符 | `[a-z]` | 匹配 `b`；不匹配 `B`（无 `re.I`） | `-` 表示范围 |
| `[A-Z]` | 大写 ASCII 范围 | 匹配 `A` 到 `Z` 的 1 个字符 | `[A-F]` | 匹配 `B`、`F` | 与 `re.I` 联用时行为见第 15 节 |
| `[0-9]` | ASCII 数字 | 匹配 ASCII `0`~`9` 的 1 个字符 | `[0-9]` | 匹配 `7` | 默认 Unicode 下 **窄于** `\d` |
| `[0-9A-Fa-f]` | 十六进制数字 | 匹配 1 个 hex 字符 | `[0-9A-Fa-f]` | 匹配 `B`、`5` | 可组合多个范围 |
| `[0-5][0-9]` | 两位范围 | 连续两个字符类 | `[0-5][0-9]` | 匹配 `00`~`59` | 这是两个字符，不是一个集合 |
| `[^abc]` | 否定字符类 | 匹配 **不属于** 集合的 1 个字符 | `[^5]` | 匹配 `a`、`6`；不匹配 `5` | 只有 `^` 在类的 **开头** 才是否定 |
| `[^^]` | 否定 caret | 匹配除 `^` 外任意 1 个字符 | `[^^]` | 匹配 `a`；不匹配 `^` | `^` 不在开头则是普通字符 |
| `[-abc]` | 字面连字符（前置） | `-` 在开头，匹配字面 `-` | `[-abc]` | 匹配 `-`、`a` | `-` 放开头或末尾即字面量 |
| `[abc-]` | 字面连字符（后置） | `-` 在末尾，匹配字面 `-` | `[abc-]` | 匹配 `c`、`-` | 也可写成 `[a\-z]` |
| `[\]]` | 转义右括号 | 在类中匹配字面 `]` | `[()[\]{}]` | 匹配 `]`、`(`、`{` | 也可把 `]` 放在类开头：`[]()[{}]` |
| `[(+*)]` | 类内字面元字符 | 匹配 `(`、`+`、`*`、`)` 之一 | `[(+*)]` | 匹配 `+` | 类内 `. * + ? ( )` 通常失去特殊意义 |
| `[\b]` | 退格字符 | 匹配 1 个 backspace（`\x08`） | `[\b]` | 匹配 `'\b'` | **不是** word boundary。`\b` 在类外才是边界 |
| `[\n\t]` | 类内转义 | 匹配换行或 tab | `[\t]` | 匹配 tab | `\` 在类内仍负责转义或引入单字符特殊序列 |

### 3.1 类内 vs 类外：元字符行为不同

| 符号 | 在 `[]` 外 | 在 `[]` 内 |
| --- | --- | --- |
| `.` `*` `+` `?` `(` `)` `\|` | 元字符 | 通常是普通字符 |
| `^` | 锚：串首 / 行首 | 仅在类开头表示否定 |
| `-` | 普通字符 | 在两个字符之间表示范围 |
| `]` | 普通字符 | 结束字符类；要匹配它需转义或放开头 |
| `\` | 转义 / 特殊序列 | 仍转义；也可引入 `\w`、`\n`、`\xa0` 等 |
| `\b` | word boundary（位置） | 退格字符 |
| `\1` | 第 1 组反向引用 | **八进制字符**，不是组引用 |
| `\A` `\Z` `\z` | 锚 | **不允许**（它们不匹配单个字符） |

### 3.2 必须特别处理的四个字符

1. **`^`**：只有位于 `[` 后的第一个位置才是否定。`[a^]` 匹配 `a` 或 `^`。
2. **`-`**：夹在两个字符之间是范围。要匹配字面 `-`：放开头、放末尾，或写成 `\-`。
3. **`]`**：会提前结束字符类。写成 `\]`，或放在类的最开头。
4. **`\`**：永远保持特殊。要匹配反斜杠写 `\\`。

### 3.3 未来可能变化的写法（3.7+）

官方提到未来可能支持 Unicode 技术标准中的嵌套集合与集合运算。为避免语义突变，下列歧义写法会触发 `FutureWarning`（Changed in Python 3.7）：

- 字符类以字面 `[` 开头
- 类中出现字面序列 `--`、`&&`、`~~`、`||`

当前应写成转义形式，例如 `\[`、`\-\-`。

### 示例

```python
import re

print(re.findall(r"[amk]", "mask"))              # ['m', 'a', 'k']
print(re.findall(r"[^5]", "a5b"))                # ['a', 'b']
print(re.findall(r"[-abc]", "a-x"))              # ['a', '-']
print(re.findall(r"[(+*)]", "(a+b)*"))           # ['(', '+', ')', '*']
print(re.search(r"[\b]", "x\x08y").group())      # '\x08'
print(re.search(r"\bcat\b", "cat"))              # 匹配单词 cat（位置锚）
```

---

## 4. Quantifiers

量词作用于 **紧邻的前一个** 表达式（一个字符、一个字符类、一个分组）。默认贪婪：能多则多，失败再回溯。

| Regex | 名称 | 含义 | 示例 | 匹配示例 | 备注 |
| --- | --- | --- | --- | --- | --- |
| `*` | 零次或多次 | 前一表达式 0~N 次，贪婪 | `ab*` | `a`、`ab`、`abbb` | Greedy |
| `+` | 一次或多次 | 前一表达式 1~N 次，贪婪 | `ab+` | `ab`、`abbb`；不匹配单独 `a` | Greedy |
| `?` | 零次或一次 | 前一表达式 0 或 1 次，贪婪 | `ab?` | `a`、`ab` | Greedy |
| `{m}` | 恰好 m 次 | 必须刚好 m 次 | `a{6}` | `aaaaaa`；不匹配 `aaaaa` | 少一次则失败 |
| `{m,n}` | m 到 n 次 | 尽量接近 n | `a{3,5}` | 在 `aaaaaa` 上匹配 `aaaaa` | 贪婪 |
| `{m,}` | 至少 m 次 | m~无限，贪婪 | `a{4,}b` | `aaaab`；不匹配 `aaab` | 逗号必须有 |
| `{,n}` | 至多 n 次 | 0~n，贪婪 | `a{,2}b` | `b`、`ab`、`aab` | 省略 m = 0 |
| `*?` | 懒惰 `*` | 0~N 次，尽量少 | `<.*?>` | 在 `'<a> b <c>'` 上先匹配 `'<a>'` | Lazy |
| `+?` | 懒惰 `+` | 1~N 次，尽量少 | `a+?` | 在 `aaa` 上只吃 1 个 `a` | Lazy |
| `??` | 懒惰 `?` | 0 或 1 次，尽量 0 | `ab??` | 优先匹配 `a` | Lazy |
| `{m,n}?` | 懒惰范围 | m~n 次，尽量接近 m | `a{3,5}?` | 在 `aaaaaa` 上匹配 3 个 `a` | Lazy |
| `*+` | 占有 `*` | 0~N 次，尽量多，**不回溯** | `a*+a` | 对 `'aaaa'` **失败** | Added in Python 3.11。等价 `(?>a*)` |
| `++` | 占有 `+` | 1~N 次，尽量多，不回溯 | `a++a` | 对 `'aaaa'` **失败** | Added in Python 3.11。等价 `(?>a+)` |
| `?+` | 占有 `?` | 0 或 1 次，不回溯 | `a?+a` | 对 `'a'`：`a?+` 吃掉唯一的 `a`，后面的 `a` 失败 | Added in Python 3.11。等价 `(?>a?)` |
| `{m,n}+` | 占有范围 | m~n 次，尽量多，不回溯 | `a{3,5}+aa` | 对 6 个 `a` **失败** | Added in Python 3.11。等价 `(?>a{m,n})` |

### 示例

```python
import re

print(re.findall(r"ab*", "a ab abb"))     # ['a', 'ab', 'abb']
print(re.findall(r"ab+", "a ab abb"))     # ['ab', 'abb']
print(re.match(r"a{6}", "aaaaaa").group())# aaaaaa
print(re.match(r"a{6}", "aaaaa"))         # None
print(re.match(r"a{3,5}", "aaaaaa").group())   # aaaaa
print(re.match(r"a{3,5}?", "aaaaaa").group())  # aaa
print(re.search(r"<.*?>", "<a> b <c>").group()) # <a>
```

---

## 5. Greedy / Lazy / Possessive

同一份文本、同一个“能重复”的表达式，三种策略选出的路径不同。

| 策略 | 写法 | 先怎么做 | 失败后怎么办 |
| --- | --- | --- | --- |
| Greedy | `*` `+` `?` `{m,n}` | 尽量多吃 | 吐回去（回溯），再让后面的表达式试试 |
| Lazy | `*?` `+?` `??` `{m,n}?` | 尽量少吃 | 不够再多吃一点 |
| Possessive | `*+` `++` `?+` `{m,n}+` | 尽量多吃 | **不吐**。后面失败则整段失败 |

### 5.1 为什么 `a*a` 能匹配 `'aaaa'`，`a*+a` 不能

```text
输入:  a a a a
       1 2 3 4
```

**`a*a`（贪婪 + 可回溯）**

1. `a*` 先吞掉 4 个 `a`
2. 最后的 `a` 没字符可吃，失败
3. 引擎回溯：`a*` 改吃 3 个
4. 第 4 个 `a` 被最后的 `a` 吃掉
5. 成功

**`a*+a`（占有，Added in Python 3.11）**

1. `a*+` 吞掉 4 个 `a`，并扔掉内部回溯点
2. 最后的 `a` 失败
3. 不能回到 `a*+` 内部减字符
4. 整次匹配失败

官方等价关系：

```text
x*+      ==  (?>x*)
x++      ==  (?>x+)
x?+      ==  (?>x?)
x{m,n}+  ==  (?>x{m,n})
```

占有量词 = 对应贪婪量词包进原子组。原子组一旦离开，内部回溯点全部丢弃。见 [第 12 节](#12-atomic-groups)。

### 5.2 `{m,n}` / `{m,n}?` / `{m,n}+` 对照

对 6 个字符的 `'aaaaaa'`：

| Regex | 结果 | 原因 |
| --- | --- | --- |
| `a{3,5}` | 匹配 5 个 `a` | 贪婪尽量多 |
| `a{3,5}?` | 匹配 3 个 `a` | 懒惰尽量少 |
| `a{3,5}aa` | 成功 | 先吃 5 个，后面 `aa` 不够，回溯到吃 4 个，再交给 `aa` |
| `a{3,5}+aa` | 失败 | 吃 5 个后不回溯，剩下 1 个不够 `aa` |

### 5.3 经典贪婪陷阱

| Regex | 输入 | 实际匹配 | 原因 |
| --- | --- | --- | --- |
| `<.*>` | `'<a> b <c>'` | `'<a> b <c>'` | `.*` 先吞到末尾，再回溯到最后一个 `>` |
| `<.*?>` | `'<a> b <c>'` | `'<a>'` | `.*?` 尽早遇到第一个 `>` 就停 |

### 示例

```python
import re

s = "<a> b <c>"
print(re.search(r"<.*>", s).group())    # <a> b <c>
print(re.search(r"<.*?>", s).group())   # <a>
print(re.match(r"a*a", "aaaa").group()) # aaaa

# 下面两行需要 Python 3.11+
# print(re.match(r"a*+a", "aaaa"))      # None
# print(re.match(r"a{3,5}+aa", "aaaaaa"))  # None
```

---

## 6. Special Sequences

`\` + 后续字符构成特殊序列。若 `\` 后面不是 ASCII 字母或数字，则匹配该字符本身，例如 `\$` 匹配 `$`。

未知的 `\ASCII字母` 自 **Python 3.6** 起是错误。

| Regex | 名称 | 含义 | 示例 | 匹配示例 | 备注 |
| --- | --- | --- | --- | --- | --- |
| `\d` | 十进制数字 | str：Unicode 类别 `Nd`；bytes / `re.ASCII`：`[0-9]` | `\d+` | 匹配 `123`；默认 str 也可匹配 `１２３`（全角） | 默认 **不等于** `[0-9]` |
| `\D` | 非数字 | `\d` 的补集 | `\D+` | 匹配 `abc` | `re.ASCII` 下等价 `[^0-9]` |
| `\s` | 空白 | str：`str.isspace()` 为真的字符；bytes / `re.ASCII`：`[ \t\n\r\f\v]` | `\s+` | 匹配 `"  "`、`\t`、`\n` | 默认还包括不间断空格等 |
| `\S` | 非空白 | `\s` 的补集 | `\S+` | 匹配 `Gi0/1` | `re.ASCII` 下等价 `[^ \t\n\r\f\v]` |
| `\w` | 单词字符 | str：`str.isalnum()` 为真的字符 **加上** `_`；bytes / `re.ASCII`：`[a-zA-Z0-9_]` | `\w+` | 匹配 `eth0`、`变量`（默认 Unicode） | 默认 **宽于** `[a-zA-Z0-9_]` |
| `\W` | 非单词字符 | `\w` 的补集 | `\W+` | 匹配 `:/-` | `re.LOCALE` 时按 locale（仅 bytes） |
| `\b` | 单词边界 | 匹配 `\w` 与 `\W` 之间（或 `\w` 与串首/串尾之间）的 **空位置** | `\bcat\b` | 匹配 `cat`、`at.` 中的 `at`（用 `r'\bat\b'`）；不匹配 `attempt` | **不消费字符**。在 `[]` 内变成退格 |
| `\B` | 非单词边界 | 不在单词边界上的空位置 | `at\B` | 匹配 `athens`、`atom` 中的 `at`；不匹配单独 `at`、`at.`、`at!` | Changed in Python 3.14：空输入也匹配 |
| `\A` | 绝对串首 | 只在整个字符串开头 | `\Afoo` | 匹配 `foo\nbar` 开头的 `foo`；`re.M` 下也不匹配第二行 | 不受 `MULTILINE` 影响 |
| `\Z` | 绝对串尾 | 只在整个字符串末尾 | `foo\Z` | 匹配 `foo`；对 `foo\n`，`foo\Z` 失败（末尾是换行） | 不受 `MULTILINE` 影响。兼容旧写法 |
| `\z` | 绝对串尾 | 与 `\Z` 相同 | `foo\z` | 同 `\Z` | **Added in Python 3.14** |
| `\number` | 数字反向引用 | 匹配第 n 个捕获组已经吃到的 **同样文本** | `(.+) \1` | 匹配 `the the`、`55 55`；不匹配 `thethe` | 仅前 99 组。以 `0` 开头或满 3 位八进制时按字符处理 |

官方对 `r'\bat\b'` 的例子：匹配 `'at'`、`'at.'`、`'(at)'`、`'as at ay'`；不匹配 `'attempt'`、`'atlas'`。

### 6.1 `\b` 的两个含义

| 写法 | 含义 |
| --- | --- |
| `\b`（类外） | word boundary，零宽位置 |
| `[\b]`（类内） | 退格字符 `\x08` |

这是为了兼容 Python 字符串字面量。见 [第 17 节](#17-unicode--ascii) 了解 `\b` 的“单词字符”集合如何随 Unicode / ASCII / LOCALE 变化。

### 示例

```python
import re

print(re.findall(r"\d+", "abc123xyz"))           # ['123']
print(re.findall(r"\bcat\b", "cat category cat."))# ['cat', 'cat']
print(re.findall(r"at\B", "at athens atom at.")) # ['at', 'at']  -> athens, atom
print(re.search(r"\Afoo", "foo\nbar").group())   # foo
print(re.search(r"^bar", "foo\nbar", re.M).group())  # bar
print(re.search(r"\Abar", "foo\nbar", re.M))     # None
print(re.search(r"(.+) \1", "the the").group())  # the the
```

---

## 7. Anchors

锚匹配的是 **位置**，不是字符。成功时消费 0 个字符。

### 7.1 Word Boundary 心智模型

输入：

```text
cat dog
```

字符之间的位置可以画成：

```text
|c a t| |d o g|
 ^     ^ ^     ^
 边界   边界 边界  边界
```

更完整地标出每个空隙：

```text
位置:  0 1 2 3 4 5 6
字符:   c a t   d o g
       | | | | | | |
\w?:    Y Y Y N Y Y Y
```

`\b` 出现在“左边是单词字符、右边不是”或反过来，以及“单词字符紧贴串首/串尾”的位置。

因此 `\bcat\b`：

- 不是去找字符 `|`
- 而是要求 `cat` 两侧都是单词边界
- 匹配的可见文本仍是 `cat`，两侧锚不出现在 `group()` 里

`\B` 是这些位置的补集：在单词内部才成功。

```text
\bcat\b    匹配独立单词 cat（两侧都是边界）
\Bcat\B    匹配夹在单词字符中间的 cat（两侧都不是边界）
```

`r'\Bcat\B'` 要求 `c` 前面和 `t` 后面都不是单词边界。因此它匹配 `concatenate`、`xcaty` 中间的 `cat`，但不匹配 `category` 开头的 `cat`：串首紧贴 `\w` 本身就是 `\b`，左侧的 `\B` 已经失败。

### 7.2 Anchor 对照

| Regex | 名称 | 含义 | 示例 | 匹配示例 | 备注 |
| --- | --- | --- | --- | --- | --- |
| `^` | 行首锚 | 默认：字符串开头。`re.M`：每行开头 | `^foo` | 默认只匹配整串开头的 `foo` | 位置，不消费 |
| `$` | 行尾锚 | 默认：串尾或末尾换行前。`re.M`：每行行尾 | `foo$` | 匹配 `foo`；也匹配 `foo\n` 中换行前的 `foo` | 位置，不消费 |
| `\A` | 绝对串首 | 只在整个字符串开始处 | `\Afoo` | `foo\nbar` 的第一行；从不认第二行 | `re.M` 无效 |
| `\Z` | 绝对串尾 | 只在整个字符串结束处 | `foo\Z` | 匹配 `foo`；不匹配 `foo\n` 的整串（末尾是 `\n`） | `re.M` 无效 |
| `\z` | 绝对串尾 | 与 `\Z` 相同 | `foo\z` | 同 `\Z` | Added in Python 3.14 |
| `\b` | 单词边界 | `\w`/`\W`（或串端）交界处 | `\bcat\b` | 独立 `cat` | 位置 |
| `\B` | 非单词边界 | 非交界处 | `at\B` | `atom` 中的 `at` | 位置。3.14 起空串也匹配 |

### 7.3 `^` vs `\A`，`$` vs `\Z`

```text
文本:  foo\nbar\n

^foo     默认成功；re.M 也成功
\Afoo    始终成功（只认绝对开头）

^bar     默认失败；re.M 成功
\Abar    始终失败

foo$     默认失败（foo 后面不是末尾）；re.M 成功（foo 在换行前）
foo\Z    始终失败（绝对末尾是最后的 \n 之后，不是 foo 后面）

bar$     默认成功（$ 可停在末尾换行前）；re.M 也成功
bar\Z    失败（绝对末尾在最后 \n 之后）
bar\n\Z  成功
```

记忆：

- 需要“整段文本的真正两端”用 `\A` / `\Z`（或 3.14 的 `\z`）
- 需要“每一行的两端”用 `^` / `$` + `re.MULTILINE`
- `$` 对 **末尾换行** 比 `\Z` 更宽松

### 示例

```python
import re

text = "foo\nbar\n"
print(bool(re.search(r"^bar", text)))           # False
print(bool(re.search(r"^bar", text, re.M)))     # True
print(bool(re.search(r"\Abar", text, re.M)))    # False
print(bool(re.search(r"foo$", text)))           # False
print(bool(re.search(r"foo$", text, re.M)))     # True
print(bool(re.search(r"foo\Z", text)))          # False
print(bool(re.search(r"bar$", "bar\n")))        # True   $ 允许停在末尾换行前
print(bool(re.search(r"bar\Z", "bar\n")))       # False
print(bool(re.search(r"bar\n\Z", "bar\n")))     # True
```

---

## 8. Groups

`(?` 开头的是扩展记号。除 `(?P<name>...)` 外，扩展通常 **不创建** 新的捕获组。

| Regex | 名称 | 含义 | 示例 | 匹配示例 | 备注 |
| --- | --- | --- | --- | --- | --- |
| `(...)` | 捕获组 | 分组 + 捕获 + 占用下一个组号 | `(\d+)-(\d+)` | `123-456` | `group(0)` 是整次匹配 |
| `(?:...)` | 非捕获组 | 只分组，不捕获、不占组号 | `(?:http\|https)://` | `http://`、`https://` | 用于 `|` 或量词作用域 |
| `(?P<name>...)` | 命名捕获组 | 同时是命名组和编号组 | `(?P<year>\d{4})` | `2026` → `group("year")` | name 必须是合法 Python 标识符；每个名字只能定义一次 |
| `(?>...)` | 原子组 | 匹配成功后丢弃组内回溯点 | `(?>.*).` | **永不匹配** | Added in Python 3.11。见第 12 节 |
| `(?#...)` | 组内注释 | 括号内容被忽略，匹配空串 | `abc(?#x)def` | `abcdef` | 与 `re.VERBOSE` 的 `#` 不同，见下 |

### 8.1 `group(0)` / `group(1)` / `group(2)`

Pattern：`(\d+)-(\d+)`  
输入：`123-456`

| 调用 | 值 | 含义 |
| --- | --- | --- |
| `group(0)` 或 `group()` | `'123-456'` | 整次匹配 |
| `group(1)` | `'123'` | 第 1 个捕获组 |
| `group(2)` | `'456'` | 第 2 个捕获组 |

编号从 1 起，按开括号 `(` 从左到右分配。命名组也占编号。

### 8.2 `(...)` vs `(?:...)`

| | `(...)` | `(?:...)` |
| --- | --- | --- |
| 限定 `|` / 量词范围 | 是 | 是 |
| 写入 `group(n)` | 是 | 否 |
| 占用组号 | 是 | 否 |
| 可供 `\1` / `(?P=name)` 引用 | 是 | 否 |

```text
(http|https)://     group(1) = http 或 https
(?:http|https)://   没有 group(1)；后面的捕获组从 1 开始
```

### 8.3 命名组 `(?P<name>...)`

```text
(?P<year>\d{4})
```

读取：

```python
m.group("year")
m.group(1)          # 命名组同时也是编号组
```

官方限制：

- 组名必须是合法 **Python 标识符**
- 同一 pattern 里每个名字只能出现一次
- **Changed in Python 3.12**：bytes pattern 中，组名只能包含 ASCII 字节 `b'\x00'`–`b'\x7f'`

Python `re` **不支持** PCRE 的 `(?<name>...)`、`(?'name'...)`、`\k<name>`。

命名组可在三个地方引用：

| 场景 | 写法 |
| --- | --- |
| 同一 pattern 内 | `(?P=name)` 或 `\1` |
| Match 对象 | `m.group("name")`、`m.end("name")` 等 |
| `re.sub()` 的替换串 | `\g<name>`、`\g<1>`、`\1` |

官方例子：`(?P<quote>['"]).*?(?P=quote)` 匹配用同一引号包起来的串。

### 8.4 注释：`(?#...)` vs `re.VERBOSE` 的 `#`

| 写法 | 作用范围 | 空白是否忽略 |
| --- | --- | --- |
| `(?#comment)` | 仅这对括号 | 否，pattern 其他空白仍有效 |
| `re.VERBOSE` / `(?x)` 中的 `#` | 从 `#` 到行尾 | 是（字符类内、反斜杠后、`*?` `(?:` `(?P<...>` 这类 token 内除外） |

```text
abc(?#comment)def     仍要求 abc 与 def 紧挨着
(?x)abc  # comment
    def               VERBOSE 下等价 abcdef
```

### 示例

```python
import re

m = re.search(r"(\d+)-(\d+)", "id=123-456")
print(m.group(0), m.group(1), m.group(2))        # 123-456 123 456

m = re.search(r"(?:http|https)://(\w+)", "https://host")
print(m.group(0), m.group(1))                    # https://host host

m = re.search(r"(?P<year>\d{4})", "year=2026")
print(m.group("year"), m.group(1))               # 2026 2026

print(re.search(r"abc(?#skip)def", "abcdef").group())  # abcdef
```

---

## 9. Backreferences

反向引用匹配的不是“再来一套同样的语法”，而是 **前面那个组实际捕获到的文本**。

| Regex | 名称 | 含义 | 示例 | 匹配示例 | 备注 |
| --- | --- | --- | --- | --- | --- |
| `\1` | 数字反向引用 | 匹配第 1 组已捕获的相同内容 | `(\w+)\s+\1` | 匹配 `cat cat`；不匹配 `cat dog` | `\2`、`\3` 类推，最多 99 组 |
| `\number` | 第 n 组引用 | 同上 | `(.+) \1` | `the the`；不匹配 `thethe`（缺空格） | 以 0 开头或 3 位八进制 → 按字符，不是组 |
| `(?P=name)` | 命名反向引用 | 匹配命名组已捕获的相同内容 | `(?P<word>\w+)\s+(?P=word)` | 匹配 `hi hi`；不匹配 `hi ho` | 组必须已经出现 |

用途：找重复单词、配对引号、成对分隔符。

```text
数字引用:   (\w+)\s+\1
命名引用:   (?P<word>\w+)\s+(?P=word)
配对引号:   (?P<quote>['"]).*?(?P=quote)
```

注意：

- 引用的是捕获 **结果**，不是 pattern 本身
- 未参与匹配的组被引用时，该引用失败
- 字符类里的 `\1` 不是反向引用

### `re.sub` 替换串中的引用（不是 pattern 语法，但常一起用）

| 替换写法 | 含义 | 备注 |
| --- | --- | --- |
| `\1` | 第 1 组文本 | `\20` 表示第 20 组，不是第 2 组 + `0` |
| `\g<1>` | 第 1 组文本 | `\g<2>0` = 第 2 组 + 字面 `0` |
| `\g<name>` | 命名组文本 | 对应 `(?P<name>...)` |
| `\g<0>` | 整次匹配 | 整段被替换的文本 |

**Changed in Python 3.12**：替换串里的组 id 只能是 ASCII 数字；bytes 替换串中的组名只能是 ASCII 字节。

### 示例

```python
import re

print(re.search(r"(\w+)\s+\1", "cat cat").group())
print(re.search(r"(?P<word>\w+)\s+(?P=word)", "hi hi").group())
print(re.search(r'''(?P<quote>['"]).*?(?P=quote)''', "'hi'").group())
print(re.sub(r"(\w+)=(\d+)", r"\2=\1", "w=20"))   # 20=w
print(re.sub(r"(?P<k>\w+)=(?P<v>\d+)", r"\g<v>=\g<k>", "w=20"))
```

---

## 10. Lookahead

Lookahead 只检查当前位置 **右边** 是否满足条件，**不消费** 任何字符。

| Regex | 名称 | 含义 | 示例 | 匹配示例 | 备注 |
| --- | --- | --- | --- | --- | --- |
| `(?=...)` | 正向先行断言 | 右边能匹配 `...` 才成功 | `foo(?=bar)` | 在 `foobar` 中匹配 `foo`；`group()` 不含 `bar` | 不消费。官方例：`Isaac (?=Asimov)` |
| `(?!...)` | 负向先行断言 | 右边 **不能** 匹配 `...` 才成功 | `foo(?!bar)` | 匹配 `foofoo` 中的 `foo`；不匹配 `foobar` 中的 `foo` | 不消费。官方例：`Isaac (?!Asimov)` |

关键：

```text
foo(?=bar)   看见右边是 bar，但只收下 foo
foo(?!bar)   看见右边不是 bar，才收下 foo
```

所以可以在“不把后缀写进匹配结果”的前提下加约束，也可以把多个 lookahead 叠在同一位置（都是零宽）。

### 示例

```python
import re

print(re.search(r"foo(?=bar)", "foobar").group())     # foo
print(re.search(r"foo(?=bar)", "foofoo"))             # None
print(re.search(r"foo(?!bar)", "foofoo").group())     # foo
print(re.search(r"foo(?!bar)", "foobar"))             # None
print(re.search(r"Isaac (?=Asimov)", "Isaac Asimov").group())  # 'Isaac '
```

---

## 11. Lookbehind

Lookbehind 检查当前位置 **左边** 是否满足条件，同样 **不消费** 字符。

| Regex | 名称 | 含义 | 示例 | 匹配示例 | 备注 |
| --- | --- | --- | --- | --- | --- |
| `(?<=...)` | 正向后行断言 | 左边紧邻的文本能匹配 `...` | `(?<=\$)\d+` | 在 `$99` 中匹配 `99`；不匹配 `x99` 里的 `99`（无 `$`） | 固定长度。以 lookbehind 开头的 pattern 不会在串首成功，宜用 `search` 而非 `match` |
| `(?<!...)` | 负向后行断言 | 左边 **不能** 匹配 `...` | `(?<!\$)\d+` | 匹配 `x99` 中的 `99`；不匹配 `$99` 中的 `99` | 固定长度。**可以** 在串首成功 |

官方例子：

```python
re.search(r"(?<=abc)def", "abcdef").group()   # 'def'
re.search(r"(?<=-)\w+", "spam-egg").group()   # 'egg'
```

### 11.1 Python lookbehind 的固定长度限制

内部 pattern 必须只匹配 **某一固定长度** 的字符串：

| 允许 | 不允许 |
| --- | --- |
| `abc`（长度 3） | `a*`（可变长） |
| `a\|b`（两分支都是长度 1） | `a{3,4}`（3 或 4，不固定） |
| 固定长度的 group reference（**Changed in Python 3.5**） | 会改变长度的量词 |

引擎需要知道“往回看几步”。变长就无法确定起点。

`a|bc` 两个分支长度不同（1 与 2），也不允许。

### 示例

```python
import re

print(re.search(r"(?<=\$)\d+", "price $99").group())   # 99
print(re.search(r"(?<!\$)\d+", "x99").group())         # 99
print(re.search(r"(?<=\$)\d+", "x99"))                 # None
print(re.match(r"(?<=abc)def", "abcdef"))              # None  串首无法 lookbehind
print(re.search(r"(?<=abc)def", "abcdef").group())     # def
```

---

## 12. Atomic Groups

| Regex | 名称 | 含义 | 示例 | 匹配示例 | 备注 |
| --- | --- | --- | --- | --- | --- |
| `(?>...)` | 原子组 | 把 `...` 当独立表达式匹配；成功离开后，**不再回溯进组内** | `(?>.*).` | 对任何非空串都失败 | **Added in Python 3.11** |

### 12.1 和普通组的差别

| | `(.*).` | `(?>.*).` |
| --- | --- | --- |
| `.*` 先吞掉全部 | 是 | 是 |
| 最后的 `.` 失败后 | `.*` 吐出 1 个字符 | 不能吐 |
| 结果 | 通常成功（最后 1 个字符给 `.`） | 永远失败 |

官方说明：原子组离开时扔掉自己内部所有栈点。后续失败时，只能退到原子组 **之前**，不能退到组内。

```text
(?>.*) .     先把整串吃光，再要 1 个字符 → 失败，且不能回溯
(.*).        同样先吃光，但可以吐 1 个给最后的 .
```

占有量词是原子组的语法糖：

```text
a*+     ==  (?>a*)
```

### 示例

```python
import re

print(re.match(r"(.*).", "abcd").group())   # abcd   普通组可回溯

# Python 3.11+
# print(re.match(r"(?>.*).", "abcd"))       # None
# print(re.match(r"(?>a*)a", "aaaa"))       # None
# print(re.match(r"(a*)a", "aaaa").group()) # aaaa
```

---

## 13. Conditional Patterns

| Regex | 名称 | 含义 | 示例 | 匹配示例 | 备注 |
| --- | --- | --- | --- | --- | --- |
| `(?(id/name)yes\|no)` | 条件匹配 | 若编号/命名组 **已经参与匹配**，用 yes-pattern，否则用 no-pattern | `(<)?(\w+@\w+(?:\.\w+)+)(?(1)>\|$)` | 见下 | `no-pattern` 可省略。**Changed in Python 3.12**：id 只能含 ASCII 数字；bytes 中 name 只能是 ASCII 字节 |

官方例子（略去表格里的空格）：

```text
(<)?(\w+@\w+(?:\.\w+)+)(?(1)>|$)
```

含义拆解：

```text
(<)?                         可选的左尖括号，捕获为 group 1
(\w+@\w+(?:\.\w+)+)          简陋的邮箱
(?(1)>|$)                    若 group 1 成功过 → 必须再来一个 >
                             否则 → 必须是字符串结尾
```

| 输入 | 整串能否对上 | 原因 |
| --- | --- | --- |
| `<user@host.com>` | 是 | 有 `<`，条件走 `>` |
| `user@host.com` | 是 | 无 `<`，条件走 `$` |
| `<user@host.com` | 否（整串） | 有 `<` 却缺 `>`。`re.search` 仍可能只吃到中间的邮箱 |
| `user@host.com>` | 否（整串） | 无 `<`，条件要求结尾，但后面多了 `>` |

这就是“根据前面某个组是否匹配成功，决定后面用哪条路”。

### 示例

```python
import re

p = r"(<)?(\w+@\w+(?:\.\w+)+)(?(1)>|$)"
print(re.fullmatch(p, "<user@host.com>").group())  # <user@host.com>
print(re.fullmatch(p, "user@host.com").group())    # user@host.com
print(re.fullmatch(p, "<user@host.com"))           # None
print(re.fullmatch(p, "user@host.com>"))           # None
print(re.search(p, "<user@host.com").group())      # user@host.com  只找到内部邮箱
```

---

## 14. Inline Flags

| Regex | 名称 | 含义 | 示例 | 匹配示例 | 备注 |
| --- | --- | --- | --- | --- | --- |
| `(?aiLmsux)` | 全局内联 flag | 匹配空串，并为 **整个** 表达式打开对应 flag | `(?i)foo` | 匹配 `FOO`、`Foo` | **Changed in Python 3.11**：只能写在表达式最开头 |
| `(?i)` | 忽略大小写 | 等价 `re.I` | `(?i)ab` | 匹配 `Ab` | 全局 |
| `(?m)` | 多行 | 等价 `re.M`，改变 `^` `$` | `(?m)^bar` | 匹配 `foo\nbar` 的 `bar` | 全局 |
| `(?s)` | DOTALL | 等价 `re.S`，`.` 含换行 | `(?s)a.b` | 匹配 `a\nb` | 全局 |
| `(?x)` | VERBOSE | 等价 `re.X`，可写空白和 `#` 注释 | `(?x)a b` | 匹配 `ab` | 全局 |
| `(?a)` | ASCII | 等价 `re.A` | `(?a)\w+` | `café` 只吃到 `caf` | 仅对 str 有意义 |
| `(?u)` | Unicode | 等价 `re.U` | `(?u)\w+` | 默认 str 已是 Unicode | Python 3 中冗余 |
| `(?L)` | LOCALE | 等价 `re.L` | 仅 bytes | 按当前 locale | 不推荐 |
| `(?aiLmsux-imsx:...)` | 作用域 flag | 只对括号内开关 flag | `(?i:abc)` | 仅这段忽略大小写 | **Added in Python 3.6** |
| `(?i:...)` | 局部忽略大小写 | 只影响括号内 | `x(?i:ab)x` | 匹配 `xAbx`；`Xabx` 仍看外层 | scoped |
| `(?a:...)` / `(?u:...)` | 局部 ASCII/Unicode | 覆盖包围组的匹配模式 | `(?a:\w+)` | 临时按 ASCII 认 `\w` | **Changed in Python 3.7**：`a`/`L`/`u` 也可写在组里。三者互斥，不能写在 `-` 后面 |

字母对应：

| 字母 | Flag |
| --- | --- |
| `a` | `re.ASCII` |
| `i` | `re.IGNORECASE` |
| `L` | `re.LOCALE` |
| `m` | `re.MULTILINE` |
| `s` | `re.DOTALL` |
| `u` | `re.UNICODE` |
| `x` | `re.VERBOSE` |

### 14.1 Global vs Scoped

```text
(?i)abc          整个 pattern 忽略大小写
(?i:abc)         只有 abc 忽略大小写
(?i-s:...)       打开 I，关掉 S，且只作用于括号内
```

`a`、`L`、`u` 互斥：

- str pattern：`(?a:...)` 切到 ASCII；`(?u:...)` 切回 Unicode（默认）
- bytes pattern：`(?L:...)` 切到 locale；`(?a:...)` 切到 ASCII（默认）
- 离开这个窄组后，恢复外层模式

### 示例

```python
import re

print(re.search(r"(?i)foo", "FOO").group())          # FOO
print(re.search(r"x(?i:ab)x", "xAbx").group())       # xAbx
print(re.search(r"x(?i:ab)x", "Xabx"))               # None
print(re.search(r"(?s)a.b", "a\nb").group())         # a\nb
print(re.search(r"(?a)\w+", "café").group())         # caf
print(re.search(r"\w+", "café").group())             # café
```

---

## 15. Python re Flags

`re` 的 flag 是 `RegexFlag`（`enum.IntFlag`，Changed in Python 3.6），可用 `|` 组合。

| Flag | 简写 | 含义 | Regex 中对应形式 | 示例 |
| --- | --- | --- | --- | --- |
| `re.ASCII` | `re.A` | 让 `\w` `\W` `\b` `\B` `\d` `\D` `\s` `\S` 按 ASCII 工作。只对 str 有意义，bytes 下忽略 | `(?a)` | `re.search(r"\w+", "café", re.A)` → `caf` |
| `re.IGNORECASE` | `re.I` | 大小写不敏感。Unicode 下 `Ü` 也可对上 `ü`，除非再加 `re.ASCII` | `(?i)` | `re.search(r"ab", "AB", re.I)` |
| `re.LOCALE` | `re.L` | `\w` `\W` `\b` `\B` 以及忽略大小写依赖当前 locale。**只能用于 bytes** | `(?L)` | 官方不推荐，更宜用 Unicode |
| `re.MULTILINE` | `re.M` | `^` `$` 也匹配每行行首/行尾 | `(?m)` | `re.search(r"^bar", "foo\nbar", re.M)` |
| `re.DOTALL` | `re.S` | `.` 匹配换行 | `(?s)` | `re.search(r"a.b", "a\nb", re.S)` |
| `re.VERBOSE` | `re.X` | 忽略 pattern 中多数空白；`#` 到行尾是注释 | `(?x)` | 见下 |
| `re.UNICODE` | `re.U` | Python 3 中 str 默认就是 Unicode，此 flag **无效果**，仅为兼容 | `(?u)` | 不要指望它改变 bytes |
| `re.NOFLAG` | 无 | 值为 `0`，表示不加 flag。适合做默认参数 | 无 | **Added in Python 3.11** |
| `re.DEBUG` | 无 | 编译时打印调试信息 | **无对应 inline flag** | `re.compile(r"a+", re.DEBUG)` |

### 15.1 `re.IGNORECASE` 与 `[a-z]`

无 `re.ASCII` 时，`[a-z]` / `[A-Z]` 加上 `re.I` 除 52 个 ASCII 字母外，还会匹配 4 个额外字母：

- `İ` (U+0130)
- `ı` (U+0131)
- `ſ` (U+017F)
- `K` (U+212A)

加上 `re.ASCII` 后，只剩 `a-z` / `A-Z`。

### 15.2 `re.LOCALE`

- 仅 bytes（Changed in Python 3.6），且与 `re.ASCII` 不兼容
- 官方警告：一次只能处理一种 culture，且只适合 8-bit locale，不可靠
- Changed in Python 3.7：编译期不再冻结 locale，以 **匹配时** 的 locale 为准

### 15.3 `re.VERBOSE` 空白规则

空白被忽略，除非：

- 在字符类里
- 前面有未转义的 `\`
- 在 `*?`、`(?:`、`(?P<...>` 这类 token 内部（所以不能写成 `(? :` 或 `* ?`）

`#` 若不在字符类、且前面无未转义 `\`，则从该 `#` 到行尾都是注释。

```python
import re

a = re.compile(r"""\d +  # the integral part
                   \.    # the decimal point
                   \d *  # some fractional digits""", re.X)
b = re.compile(r"\d+\.\d*")
# a 与 b 功能相同
```

### 15.4 `re.match` 即使开了 `MULTILINE`

`re.match()` 仍然只从 **整个字符串开头** 尝试，不会因为 `re.M` 就从每一行开头匹配。要找任意位置用 `re.search()`。

### 示例

```python
import re

print(re.search(r"ü", "Ü", re.I).group())
print(re.search(r"\w+", "café").group())                 # café
print(re.search(r"\w+", "café", re.A).group())           # caf
print(re.search(r"^bar", "foo\nbar", re.M).group())      # bar
print(re.match(r"^bar", "foo\nbar", re.M))               # None
```

---

## 16. Escape Sequences

### 16.1 Python 字符串转义 vs Regex 转义

两套语言都用 `\`，会叠在一起。

| 写法 | Python 字符串里实际得到 | Regex 引擎看到 |
| --- | --- | --- |
| `"\n"` | 换行 1 个字符 | 换行 |
| `r"\n"` | `\` + `n` 两个字符 | 换行（regex 自己解释 `\n`） |
| `"\\d+"` | `\` + `d` + `+` | `\d+`（数字） |
| `r"\d+"` | `\` + `d` + `+` | `\d+`（数字） |
| `"\d+"` | 取决于 Python：`\d` 不是合法字符串转义，3.x 会 `SyntaxWarning`，将来可能 `SyntaxError` | 即使今天能跑，也不应依赖 |

推荐：**所有 pattern 都用 raw string** `r"..."`。

要匹配字面反斜杠：regex 需要 `\\`，raw string 写成 `r"\\"`。普通字符串要写成 `"\\\\"`。

### 16.2 Regex 引擎接受的转义

官方列出与 Python 字符串字面量重合的转义：

| Regex | 名称 | 含义 | 示例 | 匹配示例 | 备注 |
| --- | --- | --- | --- | --- | --- |
| `\\` | 反斜杠 | 匹配 1 个 `\` | `a\\b` | 匹配 `a\b` | raw string 写 `r"a\\b"` |
| `\n` | 换行 | LF | `a\nb` | 匹配 `a` + 换行 + `b` | |
| `\t` | 水平制表 | TAB | `a\tb` | 匹配 `a` + tab + `b` | |
| `\r` | 回车 | CR | `\r\n` | Windows 换行 | |
| `\f` | 换页 | FF | `\f` | 换页符 | |
| `\v` | 垂直制表 | VT | `\v` | 垂直 tab | |
| `\a` | 响铃 | BEL | `\a` | `\x07` | |
| `\b` | 边界或退格 | 类外=单词边界；类内=退格 | `[\b]` | `\x08` | 见第 6 节 |
| `\xhh` | 十六进制字节/码位 | 2 位 hex | `\x41` | `A` | |
| `\uhhhh` | 16 位 Unicode | 4 位 hex | `\u4e2d` | `中` | **Added in Python 3.3**。仅 str；bytes 中是错误 |
| `\Uhhhhhhhh` | 32 位 Unicode | 8 位 hex | `\U0001F600` | `😀` | **Added in Python 3.3**。仅 str |
| `\N{name}` | 按 Unicode 名 | 展开为该字符 | `\N{EM DASH}` | `—` | **Added in Python 3.8**。仅 str |
| `\ooo` | 有限八进制 | 首位为 0，或刚好 3 位八进制 | `\141` | `a` | 否则 `\1`~`\99` 是组引用 |

`\u`、`\U`、`\N` 只在 Unicode（str）pattern 中合法。

### 示例

```python
import re

print(re.search(r"\d+", "abc123").group())       # 123
print(re.search("\\d+", "abc123").group())       # 123  能跑，但更啰嗦
print(re.search(r"a\tb", "a\tb").group())
print(re.search(r"\x41", "A").group())           # A
print(re.search(r"\u4e2d", "中").group())        # 中
print(re.search(r"\N{EM DASH}", "—").group())    # —
print(re.search(r"a\\b", r"a\b").group())        # a\b
```

---

## 17. Unicode / ASCII

str pattern 与 bytes pattern **不能混用**。替换串类型也必须与它们一致。

### 17.1 默认 str（Unicode）

| 序列 | 默认含义 | `re.ASCII` / `(?a)` 之后 |
| --- | --- | --- |
| `\d` | Unicode 类别 `Nd`（含 `[0-9]` 以及 `١`、`１` 等） | `[0-9]` |
| `\D` | 非上述数字 | `[^0-9]` |
| `\w` | `str.isalnum()` 为真的字符，再加上 `_` | `[a-zA-Z0-9_]` |
| `\W` | `\w` 的补集 | `[^a-zA-Z0-9_]` |
| `\s` | `str.isspace()` 为真（含 `[ \t\n\r\f\v]` 以及不间断空格等） | `[ \t\n\r\f\v]` |
| `\S` | `\s` 的补集 | `[^ \t\n\r\f\v]` |
| `\b` `\B` | 按上面的 `\w` 定义边界 | 按 ASCII `\w` 定义边界 |

### 17.2 bytes pattern

| 序列 | 默认（ASCII） | `re.LOCALE` |
| --- | --- | --- |
| `\d` | `[0-9]` | 仍是数字集合（文档按 ASCII 十进制数字描述） |
| `\w` | `[a-zA-Z0-9_]` | 当前 locale 下的字母数字，再加上 `_` |
| `\s` | `[ \t\n\r\f\v]` | ASCII 空白 |
| `\b` `\B` | 按 ASCII `\w` | 按 locale 的 `\w` |

`re.ASCII` 对 bytes **无意义**（本来就是 ASCII 语义）。
`re.UNICODE` 对 bytes **不允许** 做 Unicode 匹配。
`re.LOCALE` **只能** 配 bytes。

### 17.3 直观例子

```python
import re

print(re.match(r"\w+", "café").group())          # café
print(re.match(r"\w+", "café", re.A).group())    # caf
print(re.match(r"\d+", "１２３").group())         # １２３   全角数字，类别 Nd
print(re.match(r"\d+", "１２３", re.A))           # None
print(re.match(r"[0-9]+", "１２３"))              # None    字符类仍是 ASCII 范围
print(re.match(rb"\w+", "café".encode()).group())# b'caf'  bytes 默认 ASCII
```

不要把 `\d` 写成“就是 `[0-9]`”。在默认 Unicode str 下，这句话是错的。

---

## 18. Regex Syntax 对比

| Regex A | Regex B | 核心区别 |
| --- | --- | --- |
| `*` | `*?` | 都是 0~N。`*` 先尽量多，失败再吐；`*?` 先尽量少，不够再添 |
| `*` / `*?` | `*+` | `*+` 也尽量多，但失败 **不回溯**（3.11+）。`a*a` 能匹配 `aaaa`，`a*+a` 不能 |
| `(...)` | `(?:...)` | 都能分组。只有 `(...)` 捕获并占组号 |
| `(...)` / `(?:...)` | `(?>...)` | 原子组离开后不回溯进组内（3.11+）。`(.*).` 常成功，`(?>.*).` 永不成功 |
| `(?=...)` | `(?<=...)` | 都是零宽。`(?=)` 看 **右边**；`(?<=)` 看 **左边**，且必须定长 |
| `(?!...)` | `(?<!...)` | 负向。`(?!)` 右边不能是 `...`；`(?<!)` 左边不能是 `...` |
| `^` | `\A` | 都是开头位置。`^` 在 `re.M` 下也认每行行首；`\A` 永远只认整串开头 |
| `$` | `\Z` / `\z` | `$` 还可停在末尾换行前，且 `re.M` 认每行行尾；`\Z`/`\z` 只认绝对末尾。`\z` 为 3.14 新增别名 |
| `\b` | `\B` | 都是零宽。`\b` 在单词边缘；`\B` 在非边缘（含单词内部） |
| `\d` | `[0-9]` | 默认 str 下 `\d` 含所有 Unicode 十进制数字；`[0-9]` 只有 ASCII。`re.A` 或 bytes 下二者才重合 |
| `\w` | `[a-zA-Z0-9_]` | 默认 str 下 `\w` 含各国字母数字和 `_`；字符类只有 ASCII。`re.A` 或 bytes 下才重合 |
| `(?P<name>...)` | `(?P=name)` | 前者 **定义并捕获**；后者 **引用** 已捕获的相同文本 |
| `\1` | `(?P=name)` | 都是反向引用。`\1` 按编号；`(?P=name)` 按名字。命名组也可用 `\1` |

---

## 19. Regex 阅读方法

从左到右拆，每一段只问一层：这是位置、字符、数量、结构，还是策略？

目标：

```text
^(?P<name>[A-Za-z_]\w*)\s*=\s*(\d+)$
```

```text
^
    字符串开始（默认模式）

(?P<name>...)
    命名捕获组 name

[A-Za-z_]
    第一个字符：ASCII 字母或下划线

\w*
    随后 0~N 个单词字符（默认 Unicode）

\s*
    0~N 个空白

=
    字面量等号

\s*
    0~N 个空白

(\d+)
    捕获组：1~N 个数字

$
    字符串结束
```

对应五层：

```text
^                Position
(?P<name>...)    Structure（命名捕获）
[A-Za-z_]        Character
\w*              Character + Quantity
\s*              Character + Quantity
=                Character（字面量）
\s*              Character + Quantity
(\d+)            Structure + Character + Quantity
$                Position
```

阅读公式：

```text
位置
  |
  v
主体
  |
  v
数量
  |
  v
分组
  |
  v
条件
  |
  v
边界
```

### 示例

```python
import re

p = r"^(?P<name>[A-Za-z_]\w*)\s*=\s*(\d+)$"
m = re.match(p, "count = 123")
print(m.group("name"), m.group(2))    # count 123
print(re.match(p, "1bad = 123"))      # None  变量名不能以数字开头
```

---

## 20. Regex 编写方法

需求：匹配一个合法的简单变量赋值：

```text
name = 123
```

不要一次写出整串。按字段拆，再组装。

```text
需求
  |
  v
拆成字段
  |
  v
每个字段确定 Character
  |
  v
确定 Quantity
  |
  v
使用 Group 组合
  |
  v
加入 Boundary / Anchor
  |
  v
最后优化匹配策略
```

| Step | 目标 | 选择 |
| --- | --- | --- |
| 1 | 变量名 | Character：`[A-Za-z_]` 然后 `\w`；Quantity：后面 `\w*` |
| 2 | 等号附近空格 | Character：`\s`；Quantity：`*`；中间字面量 `=` |
| 3 | 整数 | Character：`\d`；Quantity：`+` |
| 4 | 整串而不是子串 | Position：`^` 与 `$` |
| 5 | 需要取出名字和数字 | Structure：命名组 + 捕获组 |

拼起来：

```text
[A-Za-z_]\w*          Step 1
\s*=\s*               Step 2
\d+                   Step 3
^[A-Za-z_]\w*\s*=\s*\d+$          Step 4
^(?P<name>[A-Za-z_]\w*)\s*=\s*(\d+)$   Step 5
```

若还要允许 `name=123`（无空格）或 `name = 123`（有空格），`\s*` 已经覆盖。若不允许两端空白以外的内容，`^` `$` 负责。

若发现 `.*` 吞得太多，再把策略从 greedy 改成 lazy / possessive / 更窄的字符类。策略是最后一步，不是第一步。

### 示例

```python
import re

p = r"^[A-Za-z_]\w*\s*=\s*\d+$"
print(bool(re.match(p, "name = 123")))   # True
print(bool(re.match(p, "name=123")))     # True
print(bool(re.match(p, "1name = 123")))  # False
print(bool(re.match(p, "name = 12x")))   # False
```

---

## 21. 常见陷阱

### 21.1 语法与引擎行为

| 陷阱 | 错误直觉 | 实际行为 |
| --- | --- | --- |
| `<.*>` 配 `'<a> b <c>'` | 只吃第一对标签 | 贪婪先吞到末尾，结果是整串 |
| `ab+` 配 `a` | `+` 也能是 0 次 | `+` 至少 1 次，单独 `a` 失败 |
| `ab\|cd` | 当成 `a(b\|c)d` | 实际是 `(ab)\|(cd)`，`\|` 优先级更低 |
| `cat\|cattle` 配 `cattle` | 一定匹配更长的 `cattle` | 左分支 `cat` 先完整成功就停 |
| `\b` 写进 `[]` | 还是单词边界 | `[\b]` 是退格字符 |
| `\d` 等于 `[0-9]` | 永远成立 | 默认 Unicode str 下 `\d` 更宽 |
| `^` / `$` 开了 `re.M` | `\A` `\Z` 也会认每行 | 只有 `^` `$` 变；`\A` `\Z` `\z` 不变 |
| `$` 配 `foo\n` | 和 `\Z` 一样失败 | `$` 可以停在末尾换行前 |
| lookbehind 写 `a*` | 和其他引擎一样 | Python 要求固定长度，编译失败 |
| `re.match(r"^bar", text, re.M)` | 能匹配第二行 | `match` 仍只从整串开头试 |
| `a{6}*` | 六的倍数 | 量词不能直接嵌套，应写 `(?:a{6})*` |
| `(?i)` 写在 pattern 中间 | 3.x 一直可以 | **3.11 起** `(?aiLmsux)` 只能在表达式开头 |
| 普通字符串 `"\d+"` | 和 `r"\d+"` 一样稳 | `\d` 不是合法字符串转义，会警告，将来可能报错 |

### 21.2 Python `re` 明确不支持

下列写法常见于 PCRE / Perl / Java / .NET / 第三方 `regex` 模块。标准库 `re` **没有** 它们。判断依据只能是官方 `re` 文档。

| 语法 | 常见于 | Python `re` |
| --- | --- | --- |
| `\p{L}` `\P{L}` `\p{Nd}` | PCRE、Java、第三方 `regex` | 不支持 |
| `(?<name>...)` `(?'name'...)` | Perl / PCRE / .NET | 不支持。用 `(?P<name>...)` |
| `\k<name>` `\k'name'` | PCRE | 不支持。用 `(?P=name)` 或 `\1` |
| `(?R)` `(?0)` `(?1)` `(?&name)` | 递归 / 子程序 | 不支持 |
| `\G` | 上次匹配结束处 | 不支持 |
| `\K` | 重置匹配起点 | 不支持 |
| `(?\|...)` | branch reset | 不支持 |
| 可变长 lookbehind `(?<=a*)` | .NET 等 | 不支持 |
| 嵌套字符类 / `[a-z&&[^aeiou]]` | Java 等 | 尚未实现；歧义写法会 `FutureWarning` |
| `*+` `++` `?+` `{m,n}+` `(?>...)` | 现代 PCRE | **3.11 之前不支持** |
| `\z` | 许多引擎的绝对串尾 | **3.14 之前只有 `\Z`** |

第三方 PyPI 包 [`regex`](https://pypi.org/project/regex/) API 与 `re` 相近，但 Unicode 和语法更宽。不要把它的特性当成标准库行为。

### 21.3 版本备忘（仅语法相关）

| 版本 | 语法变化 |
| --- | --- |
| 3.3 | pattern 增加 `\u`、`\U` |
| 3.5 | lookbehind 允许固定长度的 group reference |
| 3.6 | 增加 scoped flag `(?aiLmsux-imsx:...)`；未知 `\字母` 成为错误 |
| 3.7 | 字符类歧义写法 `FutureWarning`；`a`/`L`/`u` 可用于 inline group |
| 3.8 | 增加 `\N{name}` |
| 3.11 | 占有量词 `*+` `++` `?+` `{m,n}+`；原子组 `(?>...)`；`re.NOFLAG`；`(?aiLmsux)` 只能出现在表达式开头 |
| 3.12 | bytes 组名仅 ASCII；条件/替换中的组 id 仅 ASCII 数字 |
| 3.14 | 增加 `\z`（与 `\Z` 相同）；`\B` 匹配空输入 |

---

## 22. 一页 Regex Cheat Sheet

日常速查。细节回前面章节。

| Regex | 名称 | 含义 | 示例 | 匹配示例 | 备注 |
| --- | --- | --- | --- | --- | --- |
| `.` | 任意字符 | 除 `\n` 外 1 个字符 | `a.c` | `abc` | `re.S` 含换行 |
| `^` | 行首 | 位置：串首；`re.M` 也认行首 | `^foo` | `foo...` | 不消费 |
| `$` | 行尾 | 位置：串尾或末尾 `\n` 前 | `foo$` | `foo` | `re.M` 也认行尾 |
| `\A` | 绝对串首 | 只认整串开头 | `\Afoo` | 第一行 `foo` | 不受 `re.M` 影响 |
| `\Z` `\z` | 绝对串尾 | 只认整串末尾 | `foo\Z` | `foo`，不是 `foo\n` | `\z` 为 3.14 |
| `\b` | 单词边界 | `\w`/`\W` 交界处 | `\bcat\b` | 独立 `cat` | `[\b]` 是退格 |
| `\B` | 非单词边界 | 非交界处 | `at\B` | `atom` | 3.14 起空串也匹配 |
| `*` | 0~N | 贪婪 | `ab*` | `a` `ab` `abb` | |
| `+` | 1~N | 贪婪 | `ab+` | `ab` `abb` | 不匹配单独 `a` |
| `?` | 0~1 | 贪婪 | `ab?` | `a` `ab` | |
| `{m}` `{m,n}` `{m,}` `{,n}` | 次数 | 贪婪 | `a{3,5}` | 3~5 个 `a` | 逗号决定是否范围 |
| `*?` `+?` `??` `{m,n}?` | 懒惰 | 尽量少 | `<.*?>` | `'<a>'` | |
| `*+` `++` `?+` `{m,n}+` | 占有 | 尽量多且不回溯 | `a*+a` | `'aaaa'` 失败 | 3.11+ |
| `[abc]` | 字符类 | 其中 1 个 | `[amk]` | `a` | 类内多数元字符失效 |
| `[^abc]` | 否定类 | 非其中 1 个 | `[^5]` | `a` | `^` 必须在开头 |
| `[-abc]` `[abc-]` | 字面 `-` | `-` 放两端 | `[-a]` | `-` `a` | 或 `[a\-z]` |
| `[\]]` | 字面 `]` | 转义或放开头 | `[]abc]` | `]` | |
| `\d` `\D` | 数字 / 非数字 | 默认 Unicode `Nd` | `\d+` | `123` | `re.A` 才是 `[0-9]` |
| `\w` `\W` | 单词 / 非单词 | 默认 `isalnum` + `_` | `\w+` | `eth0` `变量` | `re.A` 才是 `[A-Za-z0-9_]` |
| `\s` `\S` | 空白 / 非空白 | 默认 `isspace()` | `\s+` | 空格 tab 换行 | `re.A` 才是 `[ \t\n\r\f\v]` |
| `\|` | 分支 | 左到右，先完整成功者胜 | `cat\|dog` | `cat` 或 `dog` | 优先级低于拼接 |
| `(...)` | 捕获组 | 分组 + 捕获 + 组号 | `(\d+)-(\d+)` | `123-456` | `group(0/1/2)` |
| `(?:...)` | 非捕获组 | 只分组 | `(?:http\|https)://` | `https://` | 不占组号 |
| `(?P<name>...)` | 命名组 | 捕获且可按名读取 | `(?P<year>\d{4})` | `2026` | 合法标识符 |
| `(?P=name)` `\1` | 反向引用 | 相同已捕获文本 | `(\w+)\s+\1` | `cat cat` | |
| `(?=...)` `(?!...)` | 先行断言 | 看右边，不消费 | `foo(?=bar)` | `foo` in `foobar` | |
| `(?<=...)` `(?<!...)` | 后行断言 | 看左边，不消费 | `(?<=\$)\d+` | `99` in `$99` | 必须定长 |
| `(?>...)` | 原子组 | 离开后不回溯 | `(?>.*).` | 永不匹配 | 3.11+ |
| `(?(id)yes\|no)` | 条件 | 组成功走 yes，否则 no | `(<)?... (?(1)>\|$)` | 配对尖括号邮箱 | 3.12 id 仅 ASCII 数字 |
| `(?aiLmsux)` | 全局内联 flag | 整表表达式 | `(?i)foo` | `FOO` | 3.11 起必须在开头 |
| `(?i:...)` | 局部 flag | 只作用于括号 | `x(?i:ab)x` | `xAbx` | 3.6+ |
| `(?#...)` | 注释 | 忽略括号内容 | `a(?#x)b` | `ab` | 与 `re.X` 的 `#` 不同 |
| `\n` `\t` `\xhh` `\u` `\U` `\N{name}` | 转义 | 控制字符 / Unicode | `\x41` | `A` | `\u` `\U` 3.3；`\N` 3.8；仅 str |

### Flags 速查

| Flag | 简写 | 含义 | Regex 中对应形式 | 示例 |
| --- | --- | --- | --- | --- |
| `re.ASCII` | `re.A` | `\w` `\d` `\s` 等改 ASCII | `(?a)` | `\w+` 配 `café` → `caf` |
| `re.IGNORECASE` | `re.I` | 忽略大小写 | `(?i)` | `ab` 配 `AB` |
| `re.LOCALE` | `re.L` | bytes + locale（不推荐） | `(?L)` | 仅 bytes |
| `re.MULTILINE` | `re.M` | `^` `$` 认每行 | `(?m)` | `^bar` 配第二行 |
| `re.DOTALL` | `re.S` | `.` 含换行 | `(?s)` | `a.b` 配 `a\nb` |
| `re.VERBOSE` | `re.X` | 可写空白和 `#` | `(?x)` | `\d+ \# n` |
| `re.UNICODE` | `re.U` | Py3 冗余 | `(?u)` | 无效果 |
| `re.NOFLAG` | — | 0，默认“无 flag” | 无 | 3.11+ |
| `re.DEBUG` | — | 打印编译调试 | 无 inline | `re.compile(..., re.DEBUG)` |

### 五层公式（贴在旁边）

```text
Regex = Character + Quantity + Structure + Position + Policy
```

```text
What        literal  .  []  \d \w \s
How many    * + ? {m,n}  lazy  possessive
Where       ^ $ \A \Z \b  lookaround
Relationship  ()  |  backreference
Policy      greedy lazy possessive atomic flags
```
