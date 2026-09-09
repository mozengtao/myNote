# Lua —— 关联与原型（Associative Prototype）

> **核心驱动力：一门围绕 Value + Function + Table 构建的极简原型胶水语言。**
> Lua 程序员不问"类的继承层级是什么"，而问"数据通过什么 Key 关联，行为通过哪张元表委托"。

---

# 1. Lua 的 Mental Model（总览）

## （1）Lua 为什么这样设计？

Lua 诞生于 1993 年的巴西天主教大学（PUC-Rio），最初是为了给巴西国家石油公司（Petrobras）定制一套极度轻量、易于嵌入 C 语言宿主程序的配置与胶水语言。

这一初衷决定了它的所有底层设计决策：

1. **机制优于策略（Mechanism, not Policy）**：Lua 不在语言内置层面强加复杂的面向对象范式、类继承系统或包管理体系，而是提供极其少量的正交原语（如 Table、Metatable、Closure），让开发者按需拼装。
2. **轻量与极致嵌入性（Lightweight & Embedded）**：完整的 Lua 解释器内核仅数百 KB（C 源码仅 2~3 万行），能够在极低内存的嵌入式设备、游戏引擎、Nginx 进程或 Redis 内部稳定运行。
3. **与 C 语言零摩擦互操作（Seamless C Interop）**：通过简洁明了的虚拟栈（Virtual Stack），C 代码与 Lua 代码可以双向透明调用与数据传递。

## （2）Lua 与 C++ / Go / Python 的本质区别

对于拥有强系统编程或现代动态语言背景的工程师，下表帮助你快速建立坐标系转换：

| 维度 | C / C++ | Go | Python | Lua |
| :--- | :--- | :--- | :--- | :--- |
| **类型系统** | 静态强类型、编译期决定内存布局 | 静态强类型、显式类型转换 | 动态强类型、对象携带类型头 | 动态弱/强类型、**变量无类型，值有类型** |
| **内存与对象** | 手动管理 / RAII，显式指针与值语义 | GC 管理堆内存，显式指针与值结构体 | 全局 PyObject 引用计数 + 分代 GC | **增量/分代标记清除 GC**，仅值与指针底层 |
| **多态与复用** | 虚函数表（vtable）、类继承与模板 | 结构体嵌入、小接口隐式实现 | 类继承、MRO 线性查找、魔术方法 | **Table 原型委托（Metatable `__index`）** |
| **并发模型** | 原生多线程、内存屏障、锁 | 原生 Goroutine + Channel 抢占调度 | GIL 锁、Asyncio 事件循环、多进程 | **协作式协程（Coroutine），单线程无抢占** |
| **执行模型** | 编译为机器码直接在硬件执行 | 编译为单一静态机器码二进制 | 栈式虚拟机（Stack-based VM）字节码 | **基于寄存器的虚拟机（Register-based VM）** |
| **定位** | 独立大型系统构建语言 | 高并发微服务与网络系统语言 | 大而全的通用全栈/科学计算语言 | **宿主系统内的高性能嵌入式胶水语言** |

## （3）Lua 的对象模型是什么？

**Lua 没有类（Class）的运行时实体。**

Lua 采用的是类似于 JavaScript 最初的**原型委托模型（Prototype Delegation Model）**：
- 每一个所谓的"对象"在本质上就是一个普通的 `Table`（关联数组）。
- 所谓的"类"也是一个普通的 `Table`，里面保存了可供调用的函数。
- 所谓的"实例化"与"继承"，就是在实例 Table 上挂载一个 `Metatable`，并设置元方法 `__index` 指向原型 Table。
- 当在实例中找不到某个字段或方法时，虚拟机沿 `__index` 委托链向上回溯。

## （4）Lua 的执行模型是什么？

Lua 虚拟机从 5.0 开始采用**基于寄存器的虚拟机（Register-Based VM）**，而非 Python / JVM / CPython 的栈式虚拟机：
- **虚拟寄存器映射**：Lua 的字节码指令直接操作虚拟寄存器（实际对应当前函数调用栈帧中的槽位 `slots`）。例如加法指令 `ADD R(A) R(B) R(C)` 一条指令即可完成计算，避免了栈式 VM 频繁的 push/pop 开销。
- **协程与 C 宿主栈协同**：Lua 的 Coroutine 是非对称、协作式的。Lua 的主状态机与每个协程拥有独立的执行栈，但共享全局堆、全局环境与垃圾收集器。

## （5）Lua 最重要的抽象是什么？

Lua 真正的核心抽象只有三个正交的基石：
1. **Value**：动态带标签的值（TValue），承载原子类型，定义数据实质。
2. **Function**：第一类公民（First-Class Function），代码与状态的封装体，可作为参数、返回值或 Table 字段存储。
3. **Table**：Lua 世界中唯一的复合数据载体，统一了数组、哈希表、结构体、类、实例与命名空间。

## （6）Lua 整体心智模型图

```text
+---------------------------------------------------------------+
| Lua Runtime                                                   |
+---------------------------------------------------------------+
                                |
        +-----------------------+-----------------------+
        |                                               |
        v                                               v
+---------------+                               +---------------+
| Value         |                               | Function      |
| (Dynamic Type)|                               | (First-Class) |
+---------------+                               +---------------+
        |                                               |
        +-----------------------+-----------------------+
                                |
                                v
                    +-----------------------+
                    | Table                 |
                    | (Associative Array)   |
                    +-----------------------+
                                |
                                v
                    +-----------------------+
                    | Metatable             |
                    | (Behavior Hook)       |
                    +-----------------------+
                                |
                                v
                    +-----------------------+
                    | Module                |
                    | (Engineering Scope)   |
                    +-----------------------+
```

> **一句话总结：Lua = 一门围绕 Value + Function + Table 构建的小语言。**

---

# 2. 核心概念优先级清单（二八原则）

按照 80/20 原则，Lua 编程的思考深度取决于以下 7 个核心概念，覆盖实际项目开发的绝大部分机制：

| 优先级 | 概念 | 为什么重要 | C/C++/Go/Python 对应心智锚点 |
| :--- | :--- | :--- | :--- |
| ⭐⭐⭐⭐⭐ | **1. Value** | 变量无类型，值有类型；底层 TValue 联合体与 nil 的删除语义 | C 的 `union + type tag`，Python 的 PyObject |
| ⭐⭐⭐⭐⭐ | **2. Table** | Lua 唯一的复合数据结构，哈希表与连续数组二合一，引用传递 | Go 的 `map + slice` 统一体，Python 的 `dict` |
| ⭐⭐⭐⭐⭐ | **3. Function** | 一等公民，天然闭包，冒号语法糖实现面向对象方法调用 | Go 的函数值与方法集，Python 的绑定方法 |
| ⭐⭐⭐⭐ | **4. Scope (local)** | 局部变量分配于虚拟机寄存器，全局变量退化为 `_ENV` 表查询 | C 的栈局部变量，Go 的短变量声明 `:=` |
| ⭐⭐⭐⭐ | **5. Closure & Upvalue** | 状态封装的物理底层，上值由栈帧迁移至堆，私有属性与状态机基石 | Go 的闭包捕获，C++ lambda 引用捕获 |
| ⭐⭐⭐⭐ | **6. Metatable** | 行为拦截与面向对象基石，通过 `__index` 实现动态原型委托查找链 | C++ 的 vtable 虚表，Python 的 `__getattr__` |
| ⭐⭐⭐⭐ | **7. Module (require)** | 工程化组织与依赖解耦，基于 `package.loaded` 的单例缓存机制 | Go 的 package，Python 的 sys.modules |

---

# 3. 核心概念深度剖析

---

## 核心概念 1：Value（值与动态类型系统）

### （1）为什么存在（Why）
在静态类型语言（如 C/C++/Go）中，类型与变量内存槽紧密绑定；在大型系统中，若将业务配置或逻辑脚本编写进 C 代码，每次微调都需重新编译。
Lua 作为嵌入式胶水语言，必须支持运行期的动态类型分发。Lua 放弃了复杂的类类型体系，仅定义了 8 种基础数据类型（`nil`, `boolean`, `number`, `string`, `function`, `table`, `userdata`, `thread`），使系统可以在极简内核下灵活处理外部异构数据。

### （2）核心思想（Mental Model）
> **变量本身没有任何类型，它只是一个普通的引用容器；只有被赋于其中的"值（Value）"才拥有明确的类型。**

在 Lua 虚拟机 C 底层中，每个变量本质上是一个 `TValue` 结构：
```text
+-----------------------+
| TValue                |
| +-------------------+ |
| | type_tag: LUA_T*  | |
| +-------------------+ |
| | Value: Union      | |
| | - int/float       | |
| | - bool            | |
| | - GCObject* ptr   | |
| +-------------------+ |
+-----------------------+
```

```text
Variable Name (Identifier)
          |
          v
   +--------------+
   | TValue       |  (Type Tag + Data/Pointer)
   +--------------+
          |
          +---- (Number/Bool: Stored directly in union)
          |
          v
     [GC Object]     (Table / String / Function / Thread: Managed by GC)
```

### （3）典型代码（10个典型场景）

#### 示例 1：变量无类型，动态重赋
```lua
local x = 100
print(type(x))  -- "number"

x = "hello"
print(type(x))  -- "string"
```
> **第一反应**：`x` 只是寄存器上的一个槽位，先装入带 number 标签的值，后替换为带 string 标签的字符串指针。

---

#### 示例 2：nil 作为变量未初始化或被删除的墓碑
```lua
local user = { name = "Alice", age = 30 }
user.age = nil
print(user.age) -- nil
```
> **第一反应**：在 Table 中将字段赋值为 `nil`，等价于从底层哈希表中将此 Key 彻底删除并标记为墓碑，而非存储字面量 NULL。

---

#### 示例 3：多变量同时赋值与惰性丢弃
```lua
local a, b, c = 1, 2
print(a, b, c)  -- 1  2  nil
```
> **第一反应**：右侧列表与左侧变量逐一匹配，不足补 `nil`，多余丢弃；这是基于寄存器槽位批量填充机制。

---

#### 示例 4：布尔真值规则（只有 nil 和 false 为假）
```lua
local count = 0
if count then
    print("0 is truthy in Lua!")
end
```
> **第一反应**：C/Python 程序员极易踩坑！Lua 中数字 `0` 和空字符串 `""` 在布尔上下文中都是**真（truthy）**，唯有 `false` 和 `nil` 判定为假。

---

#### 示例 5：String 的不可变性与全局内化（Interning）
```lua
local s1 = "hello world"
local s2 = "hello " .. "world"
print(s1 == s2) -- true (指针级别瞬时比较)
```
> **第一反应**：Lua 的短字符串是全局唯一内化的（String Interning）。任何相同内容的短字符串在内存中只有一份，相等性判断直接比较 C 指针，复杂度为 \(O(1)\)。

---

#### 示例 6：Number 的双精度与整数统一
```lua
local n1 = 42
local n2 = 42.0
print(n1 == n2)        -- true
print(math.type(n1))   -- "integer" (Lua 5.3+)
print(math.type(n2))   -- "float"
```
> **第一反应**：Lua 5.3+ 引入整数子类型，但运算逻辑高度平滑，数值比较时自动转换并判断数值等价性。

---

#### 示例 7：字符串与数字的隐式自动类型转换
```lua
local result = "10" + 20
print(result)          -- 30 (算术运算触发隐式强转 number)
print("count: " .. 10) -- "count: 10" (连接操作触发隐式强转 string)
```
> **第一反应**：算术运算符（`+`, `-`, `*`, `/`）强制将操作数当做 Number 解析；连接运算符（`..`）强制将操作数转为 String。

---

#### 示例 8：短路求值实现三元运算
```lua
local score = 85
local status = (score >= 60) and "pass" or "fail"
print(status) -- "pass"
```
> **第一反应**：Lua 没有 `condition ? a : b` 语法。`and` 返回第一个假值或最后一个真值，`or` 返回第一个真值；当 `a` 不为 nil/false 时，该模式等价于三元运算符。

---

#### 示例 9：Lightuserdata vs Full Userdata
```lua
-- 通常在 C API 模块中由宿主暴露
-- local socket = uv.new_tcp()
```
> **第一反应**：看到 userdata，立即想到这是宿主 C 分配的内存。`lightuserdata` 仅是一个 C `void*` 指针；`userdata` 是带 GC 和元表的宿主 C 内存块。

---

#### 示例 10：值类型的垃圾收集（GC）边界
```lua
local a = 123          -- 无需 GC，存储在 TValue 内部
local b = "short"      -- 引用全局内化字符串
local c = { x = 1 }    -- 独立分配在堆上的 GCObject，参与标记清除
```
> **第一反应**：标量值（number/bool/nil/lightuserdata）不增加 GC 负担；引用类型（table/string/function/userdata/thread）由垃圾回收器统一管理。

### （4）运行过程（Execution Flow）

赋值与类型变更在虚拟机内部的演进流程：

```text
Step 1: local x = 100
+---------------+
| Register R(0) | ----> [ TValue: Type = LUA_TNUMINT, Value = 100 ]
+---------------+

Step 2: x = "lua"
+---------------+
| Register R(0) | ----> [ TValue: Type = LUA_TSTRING, Ptr = 0x7fff01 ]
+---------------+                                           |
                                                            v
                                            +-------------------------------+
                                            | Global String Table ("lua")   |
                                            +-------------------------------+
```

### （5）与其它概念关系（Relationship）
- **Value 与 Table**：Table 能够容纳除 `nil` 之外的任意类型的 Value 作为 Key，容纳任意 Value 作为 Value。
- **Value 与 Function**：Function 本身也是一种带有 `LUA_TFUNCTION` 标签的 First-Class Value。
- **Value 与 Scope**：`local` 声明的变量对应 VM 寄存器中存储 `TValue` 的槽位。

### （6）真实项目中最常见写法
在 OpenResty / Redis 等工业级场景中利用短路逻辑赋缺省值：
```lua
-- OpenResty / Redis 常见参数默认值防空范式
local function query_cache(key, timeout)
    timeout = timeout or 5000  -- 当 timeout 为 nil 时赋默认值
    -- ...
end
```

### （7）容易踩坑（Gotchas）
1. **0 是真值**：`if 0 then` 会执行分支，与 C/Python 完全不同！
2. **三元表达式陷阱**：`condition and a or b` 中，若 `a` 本身就是 `false` 或 `nil`，表达式会错误地穿透执行 `b`。

### （8）一句话总结
> **变量只是名牌，TValue 才是实体；nil 是虚无，0 也是真。**

---

## 核心概念 2：Table（关联数组与统一容器）

### （1）为什么存在（Why）
C 语言提供原生定长数组与 `struct`，Python 提供 `list`, `dict`, `tuple`, `set`，Go 提供 `slice`, `map`, `struct`。
而 Lua 追求极致精简，**坚决不提供多种复合容器，而是只提供一种：Table**。
Lua 将"按连续整数索引访问"（Array）与"按任意 Key 哈希映射"（Hash/Map）融合在一套数据结构中，通过一套原语搞定数组、字典、对象与命名空间。

### （2）核心思想（Mental Model）
> **Table 本质就是 Key → Value 的关联集合，且永远以引用语义传递。**

Table 底层采用"双核设计"：内部包含一个连续的线性数组部分（Array Part）和一个哈希散列部分（Hash Part）：
```text
+---------------------------------------------------------------+
| Table                                                         |
| +-----------------------------------------------------------+ |
| | Array Part (1-indexed, continuous ints: 1, 2, 3...)        | |
| | [1] -> val1  | [2] -> val2  | [3] -> val3                 | |
| +-----------------------------------------------------------+ |
| +-----------------------------------------------------------+ |
| | Hash Part (Key -> Value node list: strings, non-int keys) | |
| | "name" -> "Tom" | "port" -> 8080 | [1000] -> "large"      | |
| +-----------------------------------------------------------+ |
+---------------------------------------------------------------+
```

### （3）典型代码（10个典型场景）

#### 示例 1：纯连续数组（1-indexed）
```lua
local list = { "apple", "banana", "cherry" }
print(list[1])  -- "apple"
print(#list)    -- 3 (获取数组长度)
```
> **第一反应**：Lua 数组下标从 1 开始！它直接存储在 Table 的 Array Part 中，访问开销等同于 C 语言数组寻址。

---

#### 示例 2：纯 Hash 映射（字典/Record）与 Key 表示的本质
```lua
local user = {
    name = "Tom",
    age = 18,
    ["user-role"] = "admin"
}
print(user.name)          -- "Tom"
print(user["name"])       -- "Tom"
print(user["user-role"])  -- "admin"
```
> **第一反应**：`name = "Tom"` 和 `user.name` 纯粹是语法糖；`["..."]` 才是 Table 底层统一的表达式寻址基石。

**深入解构：Key 的两种声明与三种访问方式**

1. **声明等价性：字面量标识符 vs 表达式括号**
   上述写法真正完全等价于：
   ```lua
   local user = {
       ["name"] = "Tom",
       ["age"] = 18,
       ["user-role"] = "admin"
   }
   ```
   - **为什么 `name =` 可以，而 `user-role =` 不行？**  
     因为未加中括号的键名必须是**合法标识符（Identifier）**（由字母、数字、下划线组成，不能以数字开头，且不能包含 `-`、空格、`.`、`:`、`+`、`#` 等操作符）。
   - **`[]` 的真正含义是什么？**  
     在 Lua 中，`[]` **根本不是传统的"数组下标"，而是"计算括号内的表达式，将求值结果作为 Key"**。
     ```lua
     local key = "name"
     print(user[key])              -- 计算变量 key -> user["name"] -> "Tom"

     local prefix = "user-"
     print(user[prefix .. "role"]) -- 计算表达式 -> user["user-role"] -> "admin"
     ```

2. **访问陷阱：为什么 `user.user-role` 会语法错乱？**
   - `user.name` 会被 Lua 编译器直接脱糖为 `user["name"]`。
   - 但是若写成 `user.user-role`，因为 `.` 后面只允许接合法标识符，编译器会将其解析为减法运算：`(user.user) - role`，从而引发运行时崩溃！因此 `user["user-role"]` 是访问该字段的唯一合法写法。

3. **心智模型图解：底层 Table 与三种寻址方式**

```text
                  user (Table in Heap)
                +-----------------------+
  "name" -----> | "Tom"                 |
  "age" ------> | 18                    |
  "user-role" ->| "admin"               |
                +-----------------------+

Access Styles:
  user.name         -- 语法糖: 仅限合法标识符常量
       |
       v
  user["name"]      -- 规范底座: 字符串字面量 Key

  user[key]         -- 动态求值: 表达式/变量值作为 Key
       |
       v
  user["user-role"] -- 非合法标识符字符串的唯一合法写法
```

4. **80% 场景记忆准则**

| 写法 | 第一反应（Mental Trigger） | 机制本质 |
| :--- | :--- | :--- |
| `name = "Tom"` | `["name"] = "Tom"` 的语法糖 | 构造语法糖（要求合规标识符） |
| `user.name` | `user["name"]` 的语法糖 | 访问语法糖（要求合规标识符） |
| `user[expr]` | **Lua Table 统一的寻址基石** | 计算表达式 `expr` 的求值结果作为 Key |

---

#### 示例 3：混合形态 Table
```lua
local mixed = {
    "first",
    "second",
    title = "Overview",
    [100] = "sparse"
}
print(mixed[1])      -- "first" (Array Part)
print(mixed.title)   -- "Overview" (Hash Part)
print(mixed[100])    -- "sparse" (Hash Part, 稀疏索引不进 Array Part)
```
> **第一反应**：看到混合 Table，立刻意识到：连续整数键走 Array Part，非连续键、大整数键、字符串键自动落入 Hash Part。

---

#### 示例 4：Table 赋值是引用复制，不是内容深拷贝
```lua
local a = { x = 10 }
local b = a

b.x = 20
print(a.x)  -- 20
```
> **第一反应**：`a` 与 `b` 持有同一个堆上 Table 的指针。改变 `b` 的字段，`a` 随之改变。

---

#### 示例 5：遍历之 pairs vs ipairs
```lua
local config = { "cache", "log", timeout = 30, retry = 3 }

-- ipairs: 仅遍历连续整数索引 1, 2, 3... 遇 nil 即停
for i, v in ipairs(config) do
    print(i, v)
end

-- pairs: 遍历整个 Table 的所有 Key（包括 Array 与 Hash），无序
for k, v in pairs(config) do
    print(k, v)
end
```
> **第一反应**：有序数组用 `ipairs`；任意字典或包含字符串键的集合用 `pairs`。

---

#### 示例 6：Table 充当 Set（集合查找）
```lua
local allowed_roles = {
    admin = true,
    editor = true,
    guest = true
}

if allowed_roles["admin"] then
    print("Access granted")
end
```
> **第一反应**：Lua 没有内置 Set 类型，将元素作为 Table 的 Key，Value 置为 `true`，实现 \(O(1)\) 成员查找。

---

#### 示例 7：Table 动态扩容与插入
```lua
local stack = {}
table.insert(stack, "frame1")
table.insert(stack, "frame2")
local top = table.remove(stack)
print(top)  -- "frame2"
```
> **第一反应**：使用标准库 `table.insert` / `table.remove` 将 Table 方便地作为栈（LIFO）或队列（FIFO）使用。

---

#### 示例 8：快速向尾部追加元素的性能惯用法
```lua
local logs = {}
for i = 1, 1000 do
    logs[#logs + 1] = "event_" .. i
end
```
> **第一反应**：`logs[#logs + 1] = v` 比调用 `table.insert(logs, v)` 避开了一次 C 函数调用栈开销，是 Lua 高频循环中追加数组的黄金范式。

---

#### 示例 9：多维数组的构建与嵌套
```lua
local matrix = {}
for i = 1, 3 do
    matrix[i] = {}
    for j = 1, 3 do
        matrix[i][j] = i * j
    end
end

外层 Table 保存了三个内层 Table:
matrix
   │
   ▼
+------------------------------+
|1 ───────┐                    |
|2 ───────┼──────┐             |
|3 ───────┼──────┼──────┐      |
+---------│------│------│------+
          ▼      ▼      ▼
       Table   Table   Table

```
> **第一反应**：Lua 没有原生连续多维内存块，多维数组本质是 Table of Tables（指针链），逐行分配。

---

#### 示例 10：稀疏数组中的空洞（Hole）陷阱
```lua
local sparse = { [1] = "a", [2] = "b", [4] = "d" }
print(#sparse) -- 结果未定义！可能是 2 也可能是 4
```
> **第一反应**：警惕！数组中间包含 `nil`（Hole）时，长度操作符 `#` 的行为是未定义的（通过二分查找寻找边界），切勿对稀疏数组使用 `#` 或 `ipairs`！

### （4）运行过程（Execution Flow）

引用传递与修改在内存中的变化：

```text
local a = {}
local b = a
b.x = 10

   +---------+
a: | Pointer | ----+
   +---------+     |      +---------------------------+
                   +----> | Table (GC Object in Heap) |
   +---------+     |      | Hash Part:                |
b: | Pointer | ----+      |   "x" -> 10               |
   +---------+            +---------------------------+
```

### （5）与其它概念关系（Relationship）
- **Table 与 Function**：Table 的字段可以为函数，这便构成了"对象的方法"或"模块的接口"。
- **Table 与 Metatable**：Table 可以绑定 Metatable，使其获得运算符重载、原型继承以及未知 Key 查找重定向的能力。
- **Table 与 Module**：Lua 模块在加载后就是一个包含导出符号的普通 Table。

### （6）真实项目中最常见写法
OpenResty 中构建高效数据包与响应上下文：
```lua
-- OpenResty 常见结构体构建与复用
local ctx = {
    uri = ngx.var.uri,
    headers = ngx.req.get_headers(),
    start_time = ngx.now(),
    is_authorized = false
}
```

### （7）容易踩坑（Gotchas）
1. **下标从 1 开始**：从 C/Python/Go 转来的工程师非常容易习惯性写出 `arr[0]`，这不仅不会报错，反而会将 `0` 存入 Hash Part，导致 `#arr` 与 `ipairs` 遗漏该元素！
2. **`#` 操作符的二分边界**：存在 `nil` 洞的数组使用 `#` 计算长度时，会返回任意一个后继为 `nil` 的索引位置。
3. **`pairs` 遍历顺序随机**：`pairs` 底层遍历哈希桶，不可依赖其顺序。

### （8）一句话总结
> **Table 是 Lua 世界唯一的物理结构，既是万能字典，也是连续数组，更是对象的容器。**

---

## 核心概念 3：Function（函数作为一等公民）

### （1）为什么存在（Why）
在传统 C 语言中，函数仅是代码段中的一段可执行指令，函数指针无法脱离环境捕获动态上下文；在面向对象语言中，方法强绑定在 Class 定义内。
Lua 需要极简地表达回调、事件钩子、配置计算以及面向对象行为，因此将函数视作**第一类公民（First-Class Value）**：函数就是一种值，与数字、字符串地位平等，可以在运行时动态创建、赋值、作为参数或返回值传递。

### （2）核心思想（Mental Model）
> **Function 就是一个普通的值；所谓的方法调用，不过是将所属 Table 显式或隐式作为第一个参数传递。**

```text
Syntax Sugar:
  obj:method(arg)   <=======>   obj.method(obj, arg)

Function Declaration Sugar:
  function M.foo() ... end   <=======>   M.foo = function() ... end
  function M:foo() ... end   <=======>   M.foo = function(self) ... end
```

### （3）典型代码（10个典型场景）

#### 示例 1：函数赋值与匿名函数
```lua
local add = function(a, b)
    return a + b
end
print(add(2, 3)) -- 5
```
> **第一反应**：`function(a, b) ... end` 生成了一个函数对象（Closure Proto），并将其赋给局部变量 `add`。

---

#### 示例 2：高阶函数（以函数为参数）
```lua
local function filter(arr, predicate)
    local result = {}
    for _, v in ipairs(arr) do
        if predicate(v) then
            result[#result + 1] = v
        end
    end
    return result
end

local evens = filter({1, 2, 3, 4, 5, 6}, function(n) return n % 2 == 0 end)
```
> **第一反应**：函数与数据无异，可以作为过滤器谓词自由流动。

---

#### 示例 3：多返回值（Multiple Return Values）
```lua
local function divide(dividend, divisor)
    if divisor == 0 then
        return nil, "division by zero error"
    end
    return dividend / divisor, nil
end

local result, err = divide(10, 0)
if not result then
    print("Error:", err)
end
```
> **第一反应**：类似 Go 语言的 `(val, err)` 惯用多返回值范式；Lua 原生在虚拟机栈上支持多值返回。

---

#### 示例 4：多返回值的截断与展开规则
```lua
local function nums()
    return 10, 20, 30
end

local x, y = nums()        -- x = 10, y = 20 (多余被截断)
local t = { nums(), 99 }   -- nums() 不在列表末尾，强制截断为只保留第 1 个值 -> { 10, 99 }
local t2 = { 99, nums() }  -- nums() 在列表末尾，完全展开 -> { 99, 10, 20, 30 }
```
> **第一反应**：多返回值只有出现在表达式列表的**最后一位**时才会完全展开，否则只保留第一个值！

---

#### 示例 5：变长参数（Variadic Arguments）与 `select`

`...` **不是 Table**，而是一串独立的值（Value List）。处理它只有两条路：

1. `{...}`：打包成临时 Table，方便 `ipairs` / 随机访问，但会分配垃圾。
2. `select`：直接操作这串值，无临时 Table。这是高性能路径。

```lua
local function sum(...)
    local total = 0
    local args = {...}  -- 把 Value List 打包成 Table
    for _, v in ipairs(args) do
        total = total + v
    end
    return total
end
print(sum(1, 2, 3, 4)) -- 10
```
> **第一反应**：`{...}` 把一串独立的值装进数组 Table；方便，但每次调用都分配一块堆内存。

**`select` 的本质**

```text
select("#", ...)  ->  参数个数（类似 len(...)）
select(i, ...)    ->  从第 i 个开始的整串剩余值，不是单个值，也不是 Table
```

`select` 最大的价值：直接操作 `...`，避免 `{...}` 带来的临时 Table 分配。

---

**5.1 统计参数数量**

```lua
local function foo(...)
    print(select("#", ...))
end

foo("A", "B", "C")  -- 3
```
> **第一反应**：`select("#", ...)` = `len(...)`，只问个数，不创建 Table。

```text
foo("A", "B", "C")

...  =  "A"  "B"  "C"
              |
              v
select("#", ...)  ->  3
```

---

**5.2 跳过前几个具名参数，转发剩余值**

```lua
local function log(level, ...)
    print("level:", level)
    print(select(1, ...))
end

log("INFO", "hello", "world")
-- level: INFO
-- hello    world
```
> **第一反应**：具名参数 `level` 先剥走 `"INFO"`；剩下的 `...` 是 `"hello" "world"`；`select(1, ...)` 把这串剩余值原样展开给 `print`。

```text
log("INFO", "hello", "world")

level  ->  "INFO"
...    ->  "hello"  "world"

print(select(1, ...))
              |
              v
        "hello"  "world"
```

这是日志包装、中间件转发的标准写法：固定前缀留下，其余整串透传。

---

**5.3 遍历参数（无 Table）—— 初学者最容易惊的一点**

```lua
local function dump(...)
    local n = select("#", ...)
    for i = 1, n do
        print(select(i, ...))
    end
end

dump(10, 20, 30)
```

实际输出不是很多人预期的：

```text
10
20
30
```

而是：

```text
10      20      30
20      30
30
```

> **第一反应**：`select(i, ...)` 返回的是「从第 i 个开始的**所有**参数」，不是第 i 个单独那个值。

```text
...  =  10  20  30

select(1, ...)  ->  10  20  30
select(2, ...)  ->      20  30
select(3, ...)  ->          30
```

`print(select(i, ...))` 会把这一整串剩余值全部打印出来。

**项目中真正常见的写法：先接到单个变量，丢掉后续返回值**

```lua
local function dump(...)
    local n = select("#", ...)
    for i = 1, n do
        local value = select(i, ...)
        print(value)
    end
end

dump(10, 20, 30)
-- 10
-- 20
-- 30
```
> **第一反应**：`local value = select(i, ...)` 只有一个接收槽，Lua 自动截断多返回值，于是 `select(2, ...)` 的 `20 30` 变成 `value = 20`。这才是「取第 i 个参数本身」。

```text
select(2, ...)
      |
      v
  20  30     <-- 一串独立的值
      |
local value = ...
      |
      v
   value = 20   (后续 30 被丢弃，与多返回值截断规则相同)
```

无垃圾累加的等价写法也依赖同一截断规则：

```lua
local function sum(...)
    local total = 0
    for i = 1, select("#", ...) do
        total = total + select(i, ...)  -- 算术运算只取第一个返回值
    end
    return total
end
```

| 写法 | 第一反应 | 是否创建 Table |
| :--- | :--- | :--- |
| `{...}` | 把 Value List 打包成 Table，方便 `ipairs`、随机访问和传递 | 是 |
| `select("#", ...)` | 获取参数个数，等价于 `len(...)` | 否 |
| `select(i, ...)` | 返回从第 *i* 个开始的整串剩余值，不是单个值 | 否 |
| `local v = select(i, ...)` | 利用多返回值截断，取出第 *i* 个参数本身 | 否 |


---

#### 示例 6：点号 `.` 与冒号 `:` 的完全等价转换
```lua
local Counter = { count = 0 }

function Counter:increment(step)
    self.count = self.count + step
end

-- 冒号调用：自动传入 self
Counter:increment(2)

-- 点号等价调用：显式传入 self
Counter.increment(Counter, 2)
```
> **第一反应**：Lua 没有原生的类与方法绑定！冒号调用 `a:b(x)` 纯粹是 `a.b(a, x)` 的语法糖；定义时的 `function a:b()` 也纯粹是 `function a.b(self)` 的语法糖。

---

#### 示例 7：Table 存储函数实现派发字典（取代 Switch-Case）
```lua
local handlers = {
    ["GET"] = function(req) return "Handling GET" end,
    ["POST"] = function(req) return "Handling POST" end,
}

local method = "GET"
local response = handlers[method](req)
```
> **第一反应**：Lua 没有 `switch-case` 语句，最地道的写法是将函数作为 Value 存入 Table，直接按 Key \(O(1)\) 派发。

---

#### 示例 8：尾调用优化（Tail Call Elimination）
```lua
local function loop(n)
    if n <= 0 then return "done" end
    return loop(n - 1)  -- 严格的尾调用：return func(args)
end
```
> **第一反应**：Lua 虚拟机完全支持严格尾调用优化。`return f(...)` 不会增加新的栈帧，可以安全地利用递归编写无限事件循环或状态机，永不爆栈。

---

#### 示例 9：函数重命名与猴子补丁（Monkey Patching）
```lua
local original_print = print
print = function(...)
    original_print("[LOG]", ...)
end
```
> **第一反应**：函数是指针值，可以随时备份原函数并在全局环境中拦截替换，常用于打桩和测试。

---

#### 示例 10：只传单参数时的字符串与 Table 字面量调用括号省略
```lua
local function print_name(u) print(u.name) end

print_name { name = "Bob" }  -- 等价于 print_name({ name = "Bob" })
print "hello"                -- 等价于 print("hello")
```
> **第一反应**：当实参是唯一的字符串字面量或 Table 构造表达式时，函数调用的括号可以省略，这构成了 Lua 强大 DSL（领域特定语言）的基石。

### （4）运行过程（Execution Flow）

冒号调用的栈展开过程：

```text
Source Code:
  player:move(10, 20)

Step 1: 语法糖解折 (Desugar)
  player.move(player, 10, 20)

Step 2: 虚拟机栈寄存器参数排列
+---------------+-----------------------------------------------+
| Register R(0) | Target Function (player.move)                 |
+---------------+-----------------------------------------------+
| Register R(1) | Argument 1: self (holds reference to player)  |
+---------------+-----------------------------------------------+
| Register R(2) | Argument 2: 10                                |
+---------------+-----------------------------------------------+
| Register R(3) | Argument 3: 20                                |
+---------------+-----------------------------------------------+
|               | CALL R(0), 3 arguments, 0 returns             |
+---------------+-----------------------------------------------+
```

### （5）与其它概念关系（Relationship）
- **Function 与 Scope**：定义在函数内部的 `local` 变量拥有局限于函数生命周期的词法作用域。
- **Function 与 Closure**：当函数捕获了外层函数的局部变量时，该函数值升级为一个闭包（Closure）。
- **Function 与 Metatable**：在元表中定义 `__call` 元方法，可以让一个 Table 实例像函数一样直接被调用。

### （6）真实项目中最常见写法
Redis 脚本与 OpenResty 核心模块中的函数导出：
```lua
-- 标准导出库函数写法
local _M = {}

function _M.new(opts)
    opts = opts or {}
    return { id = opts.id }
end

function _M.process(self, data)
    -- ...
end

return _M
```

### （7）容易踩坑（Gotchas）
1. **混淆 `:` 与 `.`**：使用 `.` 调用了冒号定义的方法，导致忘记传 `self`，方法内部报 `attempt to index a nil value (global 'self')`。
2. **尾调用必须形如 `return f(...)`**：如果是 `return 1 + f(...)` 或 `local res = f(...); return res`，都无法触发尾调用优化，栈空间会被消耗。

### （8）一句话总结
> **函数就是带参数的代码块值；冒号不是魔法，只是一次隐式的 self 参数传递。**

---

## 核心概念 4：Scope (local) 与寄存器模型

### （1）为什么存在（Why）
在许多语言中，声明变量有不同的关键字，而在早期 Lua 中，任何未声明的变量**默认都会被视为全局变量**。
全局变量在底层存储在一个特殊的全局环境表（`_ENV` 或 `_G`）中，每次访问都需要经历一次哈希散列表查找。
引入 `local` 词法作用域，不仅是为了保证工程规模扩大时的变量封装隔离，更是为了让 Lua 编译期能够将局部变量直接绑定到**虚拟机的连续寄存器（Stack Slots）**中，实现近乎 C 语言局部变量的极速读写。

### （2）核心思想（Mental Model）
> **`local` 变量映射虚拟机寄存器，按偏移量瞬间访问；未加 `local` 退化为 `_ENV["var"]` 的全局表查找。**

```text
local x = 1
+---------------------------------------+
| Local variable: Stored in Slot R(0)   |  <--- 1 CPU Cycle (Register Access)
+---------------------------------------+

y = 1
+---------------------------------------+
| Global variable: Stored in _ENV["y"]  |  <--- Hash lookup in table (Slow)
+---------------------------------------+
```

### （3）典型代码（10个典型场景）

#### 示例 1：局部变量的作用域限定
```lua
local x = 10
do
    local x = 20
    print(x)  -- 20
end
print(x)      -- 10
```
> **第一反应**：`do ... end` 构造了一个显式的词法块。内部的 `local x` 覆盖外部，离开作用域后立即释放槽位。

---

#### 示例 2：意外漏写 `local` 导致灾难性全局污染
```lua
local function calculate()
    sum = 0  -- 极其危险！漏掉了 local，sum 成为了全局变量
    for i = 1, 10 do
        sum = sum + i
    end
    return sum
end
```
> **第一反应**：警钟长鸣！任何未声明 `local` 的变量直接写入全局 `_G` / `_ENV`，在多协程或高并发环境中会导致难以追踪的数据覆盖竞态。

---

#### 示例 3：高频调用的局部化缓存（Local Caching）
```lua
-- 经典性能优化手法
local sin = math.sin
local concat = table.concat

local function fast_compute(t)
    for i = 1, 100000 do
        local val = sin(i)  -- 避免了每次通过 math 表逐级查找
    end
end
```
> **第一反应**：把模块函数通过 `local` 提升到当前 Chunk 顶部，将全局哈希查找转为虚拟机的 `GETLOCAL` 寄存器指令，这是大型 Lua 项目（如 OpenResty）的标准性能优化准则。

---

#### 示例 4：控制流语句自动创建的局部作用域
```lua
for i = 1, 3 do
    -- 变量 i 是循环体专有的 local 变量
    print(i)
end
-- print(i) -- 错误：i 在循环外部不可见
```
> **第一反应**：`for`、`while`、`repeat` 的循环变量天生是 `local`，每次迭代都具备独立的词法边界。

---

#### 示例 5：多变量局部声明
```lua
local width, height = 800, 600
local area = width * height
```
> **第一反应**：在同一行内同时分配多个寄存器槽位并依次绑定。

---

#### 示例 6：局部递归函数必须显式前置声明
```lua
-- 错误写法：
-- local fib = function(n) if n <= 1 then return n end return fib(n-1) + fib(n-2) end
-- 解析 fib(n-1) 时，右侧函数内部还没完成 fib 绑定，会误查找全局 fib！

-- 正确写法：
local fib
fib = function(n)
    if n <= 1 then return n end
    return fib(n - 1) + fib(n - 2)
end

-- 或者使用语法糖（二者等价）：
local function fib_sugar(n)
    if n <= 1 then return n end
    return fib_sugar(n - 1) + fib_sugar(n - 2)
end
```
> **第一反应**：局部递归函数必须先声明 `local name` 再赋值，或者使用 `local function name()` 语法糖，否则递归体内部解析到的同名符号是全局的！

---

#### 示例 7：单文件 Chunk 顶级作用域也是 local
```lua
-- my_script.lua
local MODULE_VERSION = "1.0.0"  -- 仅在当前文件内部可见，不泄漏到外部
```
> **第一反应**：文件本身也是一个匿名的函数（Chunk）。文件开头的 `local` 变量是该 Chunk 的局部变量，天然具备文件级隔离。

---

#### 示例 8：`_ENV` 的本质解密
```lua
-- Lua 5.2+ 中：
a = 100
-- 完全等同于：
_ENV.a = 100
```
> **第一反应**：Lua 没有神秘的全局作用域。所谓全局变量，就是当前环境表 `_ENV` 中的一个字段。通过重设 `_ENV`，可以轻易实现代码沙箱（Sandbox）。

---

#### 示例 9：沙箱环境隔离（重置 `_ENV`）
```lua
local sandbox = { print = print }
local function run_user_code()
    local _ENV = sandbox  -- 劫持当前块的环境表
    secret = "hacked"     -- 写入的是 sandbox.secret，外部全局不受影响
    print("Inside sandbox")
end
run_user_code()
```
> **第一反应**：局部 `_ENV` 变量会覆盖外层环境表，安全隔离未知脚本。

---

#### 示例 10：寄存器分配限制（MAXVARS = 200）
```lua
-- 在一个作用域内声明超过 200 个 local 变量会导致编译报错：too many local variables
```
> **第一反应**：寄存器是有限资源（标准 Lua 每个栈帧最多支持 200 个活动局部变量），超大结构应当使用 Table 封装而非扁平堆砌。

### （4）运行过程（Execution Flow）

局部变量与全局变量在虚拟机指令层面的性能鸿沟：

```text
Lua Code:
  local a = 10
  local b = a + 5
  g = b

VM Bytecode (Generated):
  1  LOADI    0  10        ; R(0) := 10       (直接写入寄存器槽位 0)
  2  ADDI     1  0  5      ; R(1) := R(0) + 5 (直接从寄存器 0 读取，结果写入寄存器 1)
  3  SETTABUP 0  0  1      ; _ENV["g"] := R(1) (去上值 _ENV 表中做哈希查找并写入)
```

### （5）与其它概念关系（Relationship）
- **Scope 与 Closure**：当内层函数在外部作用域执行，却仍持有被包含函数块的 `local` 变量时，这个 `local` 变量就被捕获为**Upvalue（上值）**。
- **Scope 与 Module**：优秀的 Lua 模块 100% 由 `local` 变量和函数构成，只显式 `return` 一个 Table，杜绝污染全局。

### （6）真实项目中最常见写法
OpenResty / Neovim 插件的标准文件头定义：
```lua
-- 强规范：文件头部集中导入与缓存依赖
local type = type
local pairs = pairs
local tostring = tostring
local tbl_insert = table.insert

local _M = {}
-- ...
return _M
```

### （7）容易踩坑（Gotchas）
1. **全局变量打错字母不报错**：由于未声明即全局，如果你把 `local username` 误敲成 `usrename = "Bob"`，编译器完全不会报错，运行时会隐秘地将其写进 `_G`。
2. **闭包内的递归局部声明未定义**。

### （8）一句话总结
> **尽可能给每一个变量加上 local；它是极速的寄存器，也是安全的护城河。**

---

## 核心概念 5：Closure & Upvalue（闭包与上值）

Closure 和 Upvalue 是 Lua 最重要、也最容易误解的概念之一。很多教程只说「闭包就是函数引用了外部变量」——这句话没错，但对运行机制帮助很小。

按 Mental Model 的正确顺序是：

```text
Why (局部变量为何该死)
  -> 生命周期 (函数返回后变量去哪)
  -> Upvalue (捕获的是变量本身，不是值)
  -> Closure (Function + Upvalue)
  -> 运行过程 (栈提升到堆)
  -> 典型代码
```

### （1）为什么存在（Why）

先问一个问题：**函数执行结束后，局部变量去哪了？**

```lua
local function foo()
    local x = 10
end

foo()
```

普通局部变量的生命周期：

```text
foo()
  |
  v
x 创建  ->  x 使用  ->  foo 返回  ->  x 被销毁
```

这是最普通的栈变量命运。C 里也一样：离开作用域，栈槽回收。

但有时我们希望：**函数结束以后，某些局部变量还能继续存在。**

```lua
local function counter()
    local n = 0
    return function()
        n = n + 1
        return n
    end
end

local c = counter()
print(c())  -- 1
print(c())  -- 2
print(c())  -- 3
```

按常理：`counter()` 返回后 `n` 应该销毁。可输出是 `1 2 3`，说明 **`n` 没有消失**。

为什么？因为 `n` 被内部函数捕获了。C 语言要达到同样效果，只能用全局/静态变量（不安全）或显式传上下文指针；面向对象语言把状态绑在对象属性上。Lua 用更轻的机制完成数据隐藏与状态持久化：**Upvalue + Closure**。

> **第一反应**：看到「外层函数返回了一个还在用外层 `local` 的内层函数」，立刻想到：这个 `local` 本该死，却被救下来了。

### （2）核心思想（Mental Model）

> **Upvalue = 被内部函数捕获的外部局部变量（变量本身，不是变量值）。**
> **Closure = Function + 它引用的 Upvalue 环境。**
> **因此：Closure 是对象，Upvalue 是 Closure 持有的状态。**

#### 2.1 什么是 Upvalue：捕获变量，不是复制值

```lua
local function foo()
    local x = 100
    return function()
        print(x)
    end
end
```

脑海里**不要**想「函数保存了 100」：

```text
function
   |
   v
 保存 100     <-- 错误：这是值拷贝心智
```

真正应该想「函数引用了变量 x」：

```text
function
   |
   v
 引用 x       <-- 正确：x 就是 Upvalue
```

所以：

- Upvalue **不是一个值**。
- Upvalue **是一个变量对象**（同一块可读写的存储）。
- 对熟悉 C++ 的人：它更接近 `[&](){ ++n; }` 的引用捕获，而不是 `[=]` 的值捕获。

为什么叫 **Up**value？相对当前函数，这个变量来自**上一层**作用域：

```text
local x = 10          <-- 外层

function foo()
    print(x)          <-- 对 foo 来说，x 来自上面，所以是 Up-value
end
```

#### 2.2 Closure 到底是什么

很多语言里：`Closure = Function + Environment`。Lua 也是。

```lua
local function counter()
    local n = 0
    return function()
        n = n + 1
        return n
    end
end
```

调用 `counter()` 时：

```text
counter()
   |
   +---- n = 0
   |
   +---- function()   <-- 即将被 return
```

`return function()` 发生以后：

```text
           Closure
        +----------------+
        | function()     |
        |                |
        | Upvalue -----> n
        +----------------+
```

内层函数带走了对 `n` 的引用。外层栈可以销毁，`n` 仍然活着。

#### 2.3 底层：Open Upvalue -> Closed Upvalue

只要一个 `local` 被内层函数捕获，它就变成 Upvalue：

- **开放状态（Open Upvalue）**：外层函数还在栈上运行时，Upvalue 仍是指向寄存器栈槽的指针。
- **闭合状态（Closed Upvalue）**：外层函数退出、栈帧即将销毁时，VM 把该变量从栈槽**提升到堆**，闭包指针改指向堆地址。这就是「`n` 本该死却没死」的物理原因。

```text
Stack Frame (Active)                         Heap (after outer returns)
+-----------------------+                    +-----------------------+
| Local Var n = 0       | <--+               | Heap box: n = 0       | <--+
+-----------------------+    |               +-----------------------+    |
                             | Open                                       | Closed
+-----------------------+    |               +-----------------------+    |
| Closure               |    |               | Closure               |    |
| - Proto (Bytecode)    |    |               | - Proto (Bytecode)    |    |
| - Upvalues[1] --------+----+               | - Upvalues[1] --------+----+
+-----------------------+                    +-----------------------+
```

#### 2.4 与 C++ Lambda 的对应

```lua
local function counter()
    local n = 0
    return function()
        n = n + 1
        return n
    end
end
```

大致相当于 C++ **按引用捕获**：

```cpp
int n = 0;
auto f = [&]() {
    ++n;
    return n;
};
```

两者都不是把 `n` 的值复制进函数，而是捕获**同一个变量**。多次调用持续修改同一份状态。差别在于：C++ 的 `n` 仍受外层作用域生命周期约束（引用捕获悬垂是未定义行为）；Lua 会在外层返回时自动把 Upvalue **闭合到堆**，所以返回闭包是安全的。

#### 2.5 一张图：以后看到这段代码立刻浮现

```lua
local function outer()
    local x = 10
    return function()
        return x
    end
end
```

```text
            outer()
               |
      local x = 10
               |
               |  <-- Upvalue（被捕获的变量本身）
               v
        +----------------+
        | Closure        |
        | function()     |
        +----------------+
               |
               v
          return x
```

牢记：

| 概念 | 第一反应 |
| :--- | :--- |
| Upvalue | 被内部函数捕获的外部 `local`（变量对象，不是值拷贝） |
| Closure | Function + 它引用的 Upvalue 环境 |
| `return function() ... x ... end` | 外层栈可以销毁，`x` 被提升到堆，随闭包继续活着 |

### （3）典型代码（10个典型场景）

#### 示例 1：状态累加器（经典计数器）
```lua
local function make_counter(start)
    local count = start or 0
    return function()
        count = count + 1
        return count
    end
end

local c1 = make_counter(0)
print(c1())  -- 1
print(c1())  -- 2

local c2 = make_counter(100)
print(c2())  -- 101
```
> **第一反应**：`count` 被内层函数捕获，从栈提升为 Upvalue；`c1()` 改的是这份 `count`，不是新建变量。`c1` 与 `c2` 来自两次 `make_counter()` 调用，各自拥有**独立**的 Upvalue，互不影响。

```text
c1                         c2
 |                          |
 v                          v
count = 0               count = 100     <-- 两次调用，两份独立 Upvalue
 |                          |
c1() -> 1               c2() -> 101
c1() -> 2
```

错误心智（千万不要这样想）：

```text
c1 -----> count <----- c2     <-- 错：共享同一份 n
```

---

#### 示例 2：面向对象之纯闭包私有字段模型
```lua
local function create_account(initial_balance)
    local balance = initial_balance  -- 真正的私有属性！

    local obj = {
        deposit = function(amount)
            balance = balance + amount
        end,
        get_balance = function()
            return balance
        end
    }
    return obj
end

local acc = create_account(1000)
acc.deposit(500)
print(acc.get_balance()) -- 1500
print(acc.balance)       -- nil (外部绝对无法通过属性访问泄露)
```
> **第一反应**：利用闭包而非 Table 属性保存私有数据。安全性极高，但每个实例都会创建新的函数对象，内存占用高于原型继承。

---

#### 示例 3：函数柯里化与参数偏特化
```lua
local function multiply(a)
    return function(b)
        return a * b
    end
end

local double = multiply(2)
local triple = multiply(3)

print(double(5))  -- 10
print(triple(5))  -- 15
```
> **第一反应**：固定部分参数生成专用函数，广泛用于配置解析与中间件流水线。

---

#### 示例 4：迭代器（Iterator）状态闭合
```lua
local function list_iter(t)
    local i = 0
    local n = #t
    return function()
        i = i + 1
        if i <= n then return t[i] end
    end
end

for val in list_iter({ "red", "green", "blue" }) do
    print(val)
end
```
> **第一反应**：Lua 的泛型 for 循环接受一个迭代函数。闭包自带状态游标 `i`，一行代码即可实现生成器行为。

---

#### 示例 5：异步回调中的上下文透传
```lua
local function fetch_user(uid, callback)
    local start_time = os.time()
    -- 模拟网络延迟后的异步回调
    network_request("/user/" .. uid, function(response)
        local elapsed = os.time() - start_time
        callback(response, elapsed)
    end)
end
```
> **第一反应**：回调函数天然携带外层 `uid` 和 `start_time` 上下文，无需像 C 语言那样传递 `void* user_data`。

---

#### 示例 6：循环中创建闭包捕获局部变量
```lua
local funcs = {}
for i = 1, 3 do
    funcs[i] = function() return i end
end

print(funcs[1]())  -- 1
print(funcs[2]())  -- 2
print(funcs[3]())  -- 3
```
> **第一反应**：Python 程序员在此经常被坑！Python 的 `for i in range(3)` 中 `i` 共享作用域；而 Lua 的 `for` 循环**每次迭代都会重新创建独立的局部变量 i**，因此闭包各自捕获独立上值！

---

#### 示例 7：多个闭包共享同一个 Upvalue
```lua
local function foo()
    local x = 0
    local function inc()
        x = x + 1
    end
    local function get()
        return x
    end
    return inc, get
end

local inc, get = foo()
inc()
inc()
print(get())  -- 2
```
> **第一反应**：`inc` 和 `get` 是两个 Closure，但引用的是**同一个** Upvalue 实体 `x`。不是各复制一份。`inc` 改、`get` 读，看到的是同一块存储。

```text
            x
            |
            v
         Upvalue
          /    \
        inc    get

inc() -> x = 1
inc() -> x = 2
get() -> 2
```

对比示例 1：同一次外层调用里诞生的多个闭包 **共享** Upvalue；**每一次**外层调用则产生 **新的** Upvalue。

---

#### 示例 8：中间件拦截与装饰器模式
```lua
local function timed(fn)
    return function(...)
        local t0 = os.clock()
        local res = fn(...)
        print("Elapsed:", os.clock() - t0)
        return res
    end
end
```
> **第一反应**：接收被包装函数作为 Upvalue，拦截调用前后行为，无侵入增强功能。

---

#### 示例 9：调试与反射上值（debug.getupvalue）
```lua
local function make_secret()
    local token = "top_secret_123"
    return function() return token end
end

local fn = make_secret()
local name, val = debug.getupvalue(fn, 1)
print(name, val) -- token  top_secret_123
```
> **第一反应**：虽然闭包在语法上实现了私有封装，但在运行时通过 `debug` 库仍然可以内省上值（常用于热更新状态迁移）。

---

#### 示例 10：避免不必要的闭包分配以减轻 GC
```lua
-- 低效写法：每次事件触发都重新生成闭包
emitter:on("event", function(e) handle(e) end)

-- 高效写法：直接传递已有可重用函数
emitter:on("event", handle)
```
> **第一反应**：频繁创建的短生命周期闭包会在堆上产生大量微小垃圾对象，游戏或高吞吐系统应尽量复用函数值。

### （4）运行过程（Execution Flow）

以计数器为例，走完从栈到堆、从调用到累加的全过程。

```lua
local function counter()
    local n = 0
    return function()
        n = n + 1
        return n
    end
end

local c = counter()
c()  -- 1
c()  -- 2
c()  -- 3
```

**第一步：`local c = counter()`**

```text
counter 的栈帧
+------------------+
| n = 0            | <---- Open Upvalue 仍指向栈槽
+------------------+
| inner function() |
+------------------+
```

如果没有 Closure：`counter` 返回 -> 栈销毁 -> `n` 消失。

但内层函数引用了 `n`，Lua 在外层返回前把 `n` **从栈提升出去**：

```text
Stack 销毁                Heap
                          +------------------+
                          | Upvalue: n = 0   | <--+
                          +------------------+    |
                                                  |
c  ---------------->  Closure                     |
                      +------------------+        |
                      | function()       |        |
                      | Upvalue ---------+--------+
                      +------------------+
```

栈可以销毁。`n` 仍然存在。

**第二步：每次 `c()` 改的是同一份 Upvalue**

```text
c()     n = n + 1     Upvalue.n : 0 -> 1
c()     n = n + 1     Upvalue.n : 1 -> 2
c()     n = n + 1     Upvalue.n : 2 -> 3
```

改的不是新变量，是那份被提升到堆上的 `n`。

**底层指令视角（与 Open/Closed 对应）**

```text
Phase 1: Outer still running
[VM Stack Slot] local n = 0
Inner Closure created: Upvalue[1] -> Slot (Open)

Phase 2: Outer exits
VM sees Slot is referenced by an open Upvalue
1. Allocates a small box on the GC heap
2. Copies n from Slot into heap box
3. Repoints Closure Upvalue[1] to the heap box (Closed)
4. Slot is reclaimed
```

**第三步：两次 `counter()` 产生两份独立 Upvalue**

```lua
local c1 = counter()
local c2 = counter()
print(c1())  -- 1
print(c1())  -- 2
print(c2())  -- 1   不是 3
```

```text
c1 --> Closure1 --> Upvalue n=0 --> 1 --> 2
c2 --> Closure2 --> Upvalue n=0 --> 1
```

> **第一反应**：同一次外层调用里的多个闭包共享一份 Upvalue；每一次外层调用创建一份新的 Upvalue。

### （5）与其它概念关系（Relationship）
- **Closure 与 Scope**：词法作用域决定谁能看见哪个 `local`；闭包让这些跨作用域访问在外层栈销毁之后仍然成立。
- **Closure 与 Table**：闭包提供行为 + 真正私有的内部状态；Table 提供对外接口。Lua 靠这套机制实现回调、迭代器、状态机和面向对象封装。
- **Closure 与 Function**：每个函数值在运行时都是闭包对象；没有捕获任何 Upvalue 时，它只是一个「空环境」的函数。

### （6）真实项目中最常见写法
游戏 AI 行为树节点与定时器任务封装：
```lua
-- 游戏引擎中延迟任务的标准写法
function spawn_delayed_task(delay, target_id)
    local timer = 0
    return function(dt)
        timer = timer + dt
        if timer >= delay then
            execute_action(target_id)
            return true -- 任务结束
        end
        return false -- 继续执行
    end
end
```

### （7）容易踩坑（Gotchas）
1. **把 Upvalue 想成「保存了当时的值」**：闭包捕获的是变量对象。后续对外层变量的写入，闭包下次调用立刻看见。
2. **误以为多次 `counter()` 共享同一份 `n`**：每次外层调用产生新的 Upvalue。`c1` 和 `c2` 互不影响。
3. **闭包生命周期延长导致隐蔽内存泄漏**：长期存活的闭包（挂在全局单例或事件中心上的监听器）若捕获了一个庞大 Table，该 Table 永远不会被 GC。

### （8）一句话总结
> **Upvalue 是被内层函数捕获的外层变量本身；Closure 是带着这份状态继续活下去的函数对象。**

---

## 核心概念 6：Metatable & Metamethod（元表与元方法）

### （1）为什么存在（Why）
C++ 拥有显式的运算符重载和虚表继承；Python 拥有魔术方法（`__getitem__`, `__add__`）。
Lua 只有 Table。默认情况下 Table 不能做加减法，不能当函数调用，访问不存在的 Key 直接返回 `nil`。
Lua 的解决方案是**元机制（Metamechanism）**：给 Table 绑一张 `Metatable`，在里面放约定好的元方法（`__index`, `__newindex`, `__add`……），在特定事件发生时劫持行为。

其中最重要、也最容易让新手卡死的一行是：

```lua
local Vector = {}
Vector.__index = Vector
```

只记住「`__index` 就是查找失败时继续查」还不够。真正要建立的心智是：

> **对象没有的方法，去类（那张 Table）里面找。**

### （2）核心思想（Mental Model）
> **Metatable 决定「找不到时怎么办」；`Class.__index = Class` 把类自己变成方法表，从而形成「对象 → 元表 → 类」的查找链。**

#### 2.1 普通 Table：找不到就结束

```lua
local t = { x = 10 }
print(t.x)  -- 10
print(t.y)  -- nil
```

```text
t.x                    t.y
 |
 v
t 里有 "x"?            t 里有 "y"?
 |
 YES                   NO
 |
 return 10             return nil   <-- 结束，没有第二层
```

#### 2.2 `__index`：找不到时不要立刻返回 nil，继续找

```lua
local t = {}
local mt = {
    __index = { x = 100 }
}
setmetatable(t, mt)
print(t.x)  -- 100
```

```text
t.x
 |
 t 有 x?  -- NO
 |
 有 metatable?  -- YES
 |
 有 __index?    -- YES
 |
 去 __index 里找 x  -- 100
```

```text
t --> {}          <-- 实例是空的
      |
      | 没找到 x
      v
   Metatable
      |
      v
   __index --> { x = 100 }
```

> **第一反应**：绑了元表，并不等于「去元表里找字段」。Lua 只在元表里找 **`__index` 这个钩子**；钩子指向哪里，才去哪里继续查。

#### 2.3 为什么必须写 `Vector.__index = Vector`

假设：

```lua
local Vector = {}
function Vector.length() end

local v = {}
setmetatable(v, Vector)
```

内存：

```text
v --> {}
      |
      | metatable
      v
   Vector
   +--------------------+
   | length -> function |
   +--------------------+
```

调用 `v:length()`：

1. `v` 里有没有 `length`？没有。
2. 去元表 `Vector` 里找 **`__index`**，不是直接找 `length`。
3. 如果**没有** `Vector.__index = Vector`：结束，`nil`，方法不存在。
4. 如果**有** `Vector.__index = Vector`：`__index` 指向 Vector 自己，再在 Vector 里找到 `length`。

这是大家最容易疑惑的地方：

```text
Vector
+---------------------------+
| __index ----------------+ |
| length  -> function     | |
| new     -> function     | |
+-------------------------|-+
                          |
                          +---> 指向自己
```

`__index` 就是这张 Table **自己**。对象找不到成员，就回 Vector 里继续找。对象 → 类，这条关系就建立起来了。

**Vector 同时承担两个角色：**

| 角色 | 靠什么成立 | 干什么 |
| :--- | :--- | :--- |
| 元表（Metatable） | `setmetatable(obj, Vector)` | 参与字段查找规则，提供 `__index` 钩子 |
| 类 / 方法表（Method Table） | `function Vector:length()` | 保存 `new`、`length` 等共享方法 |

正因为 `__index` 又指回 Vector 自己，才形成：

```text
对象  ->  元表(Vector)  ->  __index  ->  方法表(还是 Vector)
```

这是 Lua 最经典、最常见的对象模型。

#### 2.4 能不能写成 `Vector.__index = {}`？

可以，完全合法：

```lua
Vector.__index = {
    length = function(self) end
}
```

只是你要维护两张表：一张当元表，一张当方法集合。大家通常希望「类就是方法集合」，所以直接：

```lua
Vector.__index = Vector
```

只维护一个 Table。

#### 2.5 一张图：以后看到这行立刻浮现

```lua
local Vector = {}
Vector.__index = Vector
```

```text
             对象 v
          +-----------+
          | x = 3     |
          | y = 4     |
          +-----------+
                |
                | metatable
                v
          +----------------------+
          | Vector               |
          |----------------------|
          | __index ----------+  |
          | new()             |  |
          | length()          |  |
          +-------------------|--+
                              |
                              +-- 指向自己
```

访问 `v:length()` 的路径：

```text
v
 |
 没有 length
 |
 metatable = Vector
 |
 Vector.__index = Vector
 |
 Vector.length
 |
 找到，调用 Vector.length(v)
```

`v:length()` 就是 `v.length(v)`，找到方法后变成 `Vector.length(v)`。

```text
Table Query Flow (t[key]):

        t[key]
          |
    Is key present in t?
       /      \
     YES       NO
     /          \
Return val   Does t have a Metatable with __index?
                 /                     \
               YES                      NO
               /                         \
       Is __index a Table?             Return nil
           /        \
         YES         NO (It's a function)
         /            \
  Lookup in Table   Call __index(t, key)
```

### （3）典型代码（10个典型场景）

#### 示例 1：运算符重载（向量加法 `__add`）
```lua
local Vector = {}
Vector.__index = Vector

function Vector.new(x, y)
    return setmetatable({ x = x, y = y }, Vector)
end

function Vector.__add(v1, v2)
    return Vector.new(v1.x + v2.x, v1.y + v2.y)
end

local a = Vector.new(1, 2)
local b = Vector.new(3, 4)
local c = a + b
print(c.x, c.y)  -- 4  6
```
> **第一反应**：当执行 `a + b` 时，Lua 检查 `a` 或 `b` 的元表中是否有 `__add`，如果有，转为调用 `Vector.__add(a, b)`。

---

#### 示例 2：面向对象之经典原型类（`Class.__index = Class`）
```lua
local Vector = {}
Vector.__index = Vector

function Vector.new(x, y)
    return setmetatable({ x = x, y = y }, Vector)
end

function Vector:length()
    return math.sqrt(self.x * self.x + self.y * self.y)
end

local v = Vector.new(3, 4)
print(v:length())  -- 5
```
> **第一反应**：`v` 里只有 `{ x = 3, y = 4 }`，没有 `length`。`v:length()` 找不到后走元表 `Vector`，再走 `__index`（指向 Vector 自己），命中 `Vector.length`，最后变成 `Vector.length(v)`。

漏写 `Vector.__index = Vector` 时：`setmetatable(v, Vector)` 仍然成立，但 Lua 只会在元表里找 `__index` 钩子，不会自动把 `Vector.length` 当实例方法用。结果是 `attempt to call a nil value`。

同一模式的另一个常见写法（Person）完全同构：`Person.__index = Person`，`setmetatable({}, Person)`。

---

#### 示例 3：类继承（Inheritance）与派生链
```lua
local Person = {}
Person.__index = Person

function Person.new(name)
    return setmetatable({ name = name }, Person)
end

function Person:say_hello()
    print("Hi, I am " .. self.name)
end

local Employee = setmetatable({}, { __index = Person })
Employee.__index = Employee

function Employee.new(name, role)
    local self = setmetatable(Person.new(name), Employee)
    self.role = role
    return self
end

function Employee:work()
    print(self.name .. " is working as " .. self.role)
end

local e = Employee.new("Bob", "Developer")
e:say_hello()  -- 沿原型链回溯到 Person:say_hello
e:work()       -- 直接在 Employee 命中
```
> **第一反应**：`Employee` 自身也是一张普通 Table。它的 `__index` 指向自己（本类方法），它自己的元表再指向 `Person`（父类方法）。实例找不到 → Employee → Person。

---

#### 示例 4：`__index` 作为函数（动态属性拦截与懒加载）
```lua
local auto_init = setmetatable({}, {
    __index = function(tbl, key)
        print("Lazy creating key:", key)
        tbl[key] = {}  -- 动态生成并存回，防止下次再次触发
        return tbl[key]
    end
})

local sub = auto_init.users  -- 触发函数，动态创建 auto_init["users"] = {}
```
> **第一反应**：当 `__index` 是函数时，相当于 Python 的 `__getattr__`，用于动态计算属性、配置懒初始化与 RPC 存根。

---

#### 示例 5：`__newindex` 拦截未知属性赋值（创建严格表/防误写）
```lua
local strict = {}
setmetatable(strict, {
    __newindex = function(tbl, key, val)
        error("Attempt to write to undeclared key: " .. tostring(key), 2)
    end
})

-- strict.foo = 10 -- 抛出异常：Attempt to write to undeclared key: foo
```
> **第一反应**：当向 Table 写入一个**原本不存在**的 Key 时，触发 `__newindex`。这正是实现安全沙箱和只读配置表的利器。

---

#### 示例 6：创建真正的只读表（Read-only Table 代理模式）
```lua
local function make_readonly(t)
    local proxy = {}
    local mt = {
        __index = t,
        __newindex = function(tbl, key, val)
            error("Cannot modify read-only table!", 2)
        end
    }
    return setmetatable(proxy, mt)
end

local config = make_readonly({ host = "127.0.0.1", port = 6379 })
print(config.host)   -- 正常读取: 127.0.0.1
-- config.port = 80  -- 报错拦截
```
> **第一反应**：`proxy` 自身始终保持为空！所有读取被 `__index` 导向底层实体 `t`，所有写入被 `__newindex` 强行拦截报错。

---

#### 示例 7：将 Table 像函数一样调用（`__call`）
```lua
local Adder = {}
Adder.__index = Adder

function Adder.new(delta)
    return setmetatable({ delta = delta }, {
        __call = function(self, x)
            return x + self.delta
        end
    })
end

local add10 = Adder.new(10)
print(add10(5))  -- 15 (Table 像普通函数一样被直接调用)
```
> **第一反应**：定义了 `__call` 的 Table 称为仿函数（Functor），既能保存内部属性，又具备函数调用的形态。

---

#### 示例 8：自定义字符串转换（`__tostring`）
```lua
local User = { name = "Charlie", id = 1001 }
setmetatable(User, {
    __tostring = function(self)
        return string.format("User(id=%d, name=%s)", self.id, self.name)
    end
})
print(User)  -- "User(id=1001, name=Charlie)"
```
> **第一反应**：类似于 Python 的 `__repr__` 或 Java 的 `toString()`，统一格式化输出。

---

#### 示例 9：绕过元方法限制的 `rawget` 与 `rawset`
```lua
local t = setmetatable({}, {
    __index = function(tbl, key) return "mock" end,
    __newindex = function(tbl, key, val) error("locked") end
})

rawset(t, "real_key", 123)  -- 绕过 __newindex，强行写入哈希表
print(rawget(t, "real_key")) -- 绕过 __index，直接读取 -> 123
print(rawget(t, "unknown"))  -- 绕过 __index，直接返回原生 nil
```
> **第一反应**：`raw*` 系列操作直击底层数据，完全不触发元表钩子，防止元方法递归死循环或用于特权初始化。

---

#### 示例 10：垃圾回收拦截元方法（`__gc`）
```lua
local resource = setmetatable({ handle = 0x1234 }, {
    __gc = function(self)
        print("Cleaning up C handle:", self.handle)
    end
})
```
> **第一反应**：析构函数（Destructor）机制！当对象不再被引用且被 GC 扫描时，触发 `__gc` 释放宿主外部句柄资源。

### （4）运行过程（Execution Flow）

**主路径：`v:length()` 如何找到方法**

```lua
local v = Vector.new(3, 4)
v:length()
```

```text
创建后的内存:

v
 |
 +-----------+
 | x = 3     |
 | y = 4     |
 +-----------+
 | metatable --> Vector
                   +----------------+
                   | __index -> Vector (自己)
                   | new     -> function
                   | length  -> function
                   +----------------+

调用 v:length()  即  v.length(v):

1. v 里有 length?          NO
2. v 的 metatable?         Vector
3. Vector.__index?         Vector  (指向自己)
4. Vector.length?          FOUND
5. 调用 Vector.length(v)
```

**延伸路径：派生类沿 `__index` 链继续往上**

```text
User writes:
  e:say_hello()

1. Lookup "say_hello" in instance e     --> NOT FOUND
2. e's Metatable = Employee
   Employee.__index = Employee
   Lookup in Employee                   --> NOT FOUND
3. Employee's own Metatable has __index = Person
   Lookup in Person                     --> FOUND Person.say_hello
4. Execute Person.say_hello(e)
```

### （5）与其它概念关系（Relationship）
- **Metatable 与 Table**：Table 是白纸；Metatable 只提供钩子。绑了元表不等于去元表里找字段，等于去元表里找 `__index`。
- **Metatable 与 Function**：`:` 调用找到方法后，变成 `Class.method(self)`。冒号不是魔法，查找链才是。
- **Metatable 与 Module**：`_M.__index = _M` 让模块表同时当导出包和原型类，只维护一张 Table。

### （6）真实项目中最常见写法
OpenResty / LuaRocks 模块中最标准的类定义惯用法：
```lua
local _M = {}
_M.__index = _M

function _M.new(opts)
    local self = setmetatable({}, _M)
    self.timeout = opts.timeout or 1000
    return self
end

function _M.request(self)
    -- ...
end

return _M
```
> **第一反应**：这就是 `Vector.__index = Vector`。`_M` 既是模块，也是元表，也是方法表。

### （7）容易踩坑（Gotchas）
1. **只写 `setmetatable(obj, Class)`，忘了 `Class.__index = Class`**：元表挂上了，但 Lua 只在元表里找 `__index` 钩子，不会自动把 `Class.method` 当实例方法。调用直接 `nil`。
2. **误以为 `__index` 必须指向自己**：`Class.__index = {}` 合法，只是要维护两张表。指向自己是为了只维护一张。
3. **在 `__index` / `__newindex` 函数里写 `t[k]`**：会再次触发元方法，无限递归。必须用 `rawget` / `rawset`。

### （8）一句话总结
> **`Class.__index = Class` 的本质：对象找不到字段时，到 Class 这张 Table 里继续找。Class 同时是元表和方法表。**

---

## 核心概念 7：Module (require) 与工程组织

### （1）为什么存在（Why）
C 语言通过头文件与链接器管理代码组织；Go 拥有严格的 package 和模块系统；Python 拥有基于文件目录的 import 机制。
Lua 作为一个极简宿主语言，早期甚至没有正式的包管理。
现代 Lua（5.1+）设计了一套基于 `package.loaded` 单例缓存和 `require` 加载器的工程化规范，通过纯粹的 Table 映射实现高效、无重复加载的模块化体系。

### （2）核心思想（Mental Model）
> **`require("foo")` 的实质：优先查缓存；若未命中，执行该脚本并获取其返回的 Table，存入 `package.loaded["foo"]` 并返回该 Table。**

```text
require("module_name")
           |
           v
Is package.loaded["module_name"] existing?
       /               \
     YES                NO
     /                   \
Return cached table   Search file paths in package.path
                                  |
                      Load, parse and execute script Chunk
                                  |
                      Collect return value (Must be Table)
                                  |
                      Cache: package.loaded["module_name"] = result
                                  |
                      Return result
```

### （3）典型代码（10个典型场景）

#### 示例 1：标准模块的导出（Standard Module Pattern）
```lua
-- http_client.lua
local http_client = {}
http_client.__index = http_client

function http_client.new(base_url)
    return setmetatable({ url = base_url }, http_client)
end

function http_client:get(path)
    return "GET " .. self.url .. path
end

return http_client  -- 必须显式 return 导出的 Table！
```
> **第一反应**：这个文件是个自包含模块，只导出一个 Table，不污染任何外部环境。

---

#### 示例 2：标准模块的引入与消费
```lua
-- main.lua
local http = require("http_client")

local client = http.new("https://api.example.com")
print(client:get("/users"))
```
> **第一反应**：用局部变量 `http` 接收加载结果，避免全局符号泄露。

---

#### 示例 3：单例特性验证（Singleton by Default）
```lua
local mod1 = require("my_config")
local mod2 = require("my_config")

mod1.debug_level = 2
print(mod2.debug_level)  -- 2 (全局唯一单例)
print(mod1 == mod2)      -- true (同一 Table 指针)
```
> **第一反应**：`require` 第二次调用直接命中 `package.loaded` 缓存，不会重新执行脚本文件。

---

#### 示例 4：强制重新加载模块（热更新核心机制）
```lua
-- 清除缓存强制重载
package.loaded["my_config"] = nil
local mod_reloaded = require("my_config")
```
> **第一反应**：游戏服务器与 Nginx 热更新的秘密！直接将 `package.loaded[mod_name]` 置为 `nil`，下次 `require` 就会重新读取磁盘并执行新逻辑。

---

#### 示例 5：查看与扩展模块搜索路径（`package.path`）
```lua
-- 将自定义路径追加到模块搜索路径列表
package.path = package.path .. ";/opt/app/lua/?.lua;/opt/app/lua/?/init.lua"
```
> **第一反应**：`package.path` 是以分号分隔的模板字符串，`?` 会被替换为 `require` 传入的模块名。

---

#### 示例 6：子包组织（Sub-packages）
```lua
-- 文件位于 app/services/user_service.lua
-- 调用方：
local user_svc = require("app.services.user_service")
```
> **第一反应**：点号 `.` 会自动被搜索器转换为操作系统的路径分隔符 `/`。

---

#### 示例 7：C 原生动态链接库模块加载（`package.cpath`）
```lua
-- 加载编译好的 cjson.so / cjson.dll
local cjson = require("cjson")
local text = cjson.encode({ status = "ok" })
```
> **第一反应**：`require` 不仅能载入 `.lua` 文件，还能通过 `package.cpath` 载入 C 动态库，并调用约定的 `luaopen_cjson` 入口导出 Table。

---

#### 示例 8：安全加载模块（`pcall` 捕获依赖缺失）
```lua
local ok, lfs = pcall(require, "lfs")
if not ok then
    print("Warning: LuaFileSystem module not installed, fallback to basic IO")
end
```
> **第一反应**：`require` 失败会抛出致命错误，使用受保护调用 `pcall` 可以优雅降级。

---

#### 示例 9：模块私有函数与公有函数隔离
```lua
-- logger.lua
local _M = {}

-- 私有函数：不在外部 Table 暴露，纯 local，不可被外部调用
local function format_msg(level, msg)
    return string.format("[%s] %s", level, msg)
end

-- 公有函数：挂载在 _M 上
function _M.info(msg)
    print(format_msg("INFO", msg))
end

return _M
```
> **第一反应**：不需要 `public` / `private` 关键字，`local` 就是天然最高安全级别的私有函数。

---

#### 示例 10：废弃的旧式 `module(...)` 函数陷阱
```lua
-- 极其陈旧的 Lua 5.1 代码中可能看到：
-- module("mymodule", package.seeall)
```
> **第一反应**：历史遗毒！千万不要这样写。它会破坏全局环境并在 Lua 5.2+ 中被彻底移除，一律采用显式 `local M = {} ... return M`。

### （4）运行过程（Execution Flow）

`require` 的全局缓存与加载状态流转：

```text
Call: require("auth")

+-----------------------------------------------+
| Check package.loaded["auth"]                  |
+-----------------------------------------------+
    |                        |
(Hit Cache)              (Miss Cache)
    |                        |
    v                        v
Return Table <----+  +---------------------------------------+
                  |  | Traverse package.path                 |
                  |  | Find "path/to/auth.lua"               |
                  |  +---------------------------------------+
                  |          |
                  |          v
                  |  +---------------------------------------+
                  |  | LoadChunk & Execute File Body         |
                  |  | Collect return table: `local M`       |
                  |  +---------------------------------------+
                  |          |
                  |          v
                  |  +---------------------------------------+
                  +--| Write: package.loaded["auth"] = M     |
                     +---------------------------------------+
```

### （5）与其它概念关系（Relationship）
- **Module 与 Table**：模块由 Table 承载，导入模块本质上是获取一个全局共享的 Table 引用。
- **Module 与 Scope**：模块依靠文件级 `local` 作用域屏蔽实现细节，只输出洁净的接口 Table。

### （6）真实项目中最常见写法
OpenResty / Kong 微服务网关中的核心插件模块标准骨架：
```lua
-- kong/plugins/my_plugin/handler.lua
local BasePlugin = require("kong.plugins.base_plugin")

local CustomHandler = BasePlugin:extend()

CustomHandler.PRIORITY = 1000
CustomHandler.VERSION = "1.0.0"

function CustomHandler:new()
    CustomHandler.super.new(self, "my-custom-plugin")
end

function CustomHandler:access(conf)
    CustomHandler.super.access(self)
    -- 执行具体的网关过滤逻辑
end

return CustomHandler
```

### （7）容易踩坑（Gotchas）
1. **模块末尾忘记 `return M`**：如果脚本末尾没有 `return`，`require` 默认会将该模块的缓存值设为 `true`，导致后续调用 `mod.func()` 时报 `attempt to index a boolean value`。
2. **循环依赖（Circular Dependency）死锁或半初始化**：A 模块在加载过程中 require B，B 在加载中 require A，由于 A 此时尚未执行到 `return`，B 获取到的 A 将是一个未完成的空骨架。

### （8）一句话总结
> **模块化就是返回一个 Table，加载就是查表与单例缓存。**

---

# 4. 看到 Lua 代码应该如何阅读

当你接手一个全新的、庞大的 Lua 项目（无论是 OpenResty 网关、Redis 业务脚本、游戏逻辑还是 Neovim 插件）时，**绝对不要逐行翻译语法，不要把它当成 C/Go 试图寻找 `main` 入口或庞大的继承树**。

请严格遵循以下**五步心智解码顺序**：

```text
Step 1: Module Skeleton  (文件导出什么？输入依赖什么？)
          |
          v
Step 2: Table Structures (核心数据长什么样？私有还是公开？)
          |
          v
Step 3: Functions & Self (业务行为逻辑是什么？. 还是 : ？)
          |
          v
Step 4: Metatable Chains (是否有原型委托或操作符拦截？)
          |
          v
Step 5: Upvalue States   (闭包捕获了什么持久状态？)
```

---

### 实战演练：以一段典型的工程代码为例

```lua
local setmetatable = setmetatable
local ngx_log = ngx.log
local ERR = ngx.ERR

local _M = { _VERSION = '1.0' }
local mt = { __index = _M }

function _M.new(self, opts)
    opts = opts or {}
    local data = {
        name = opts.name or "anonymous",
        retry = opts.retry or 3,
        _active = false
    }
    return setmetatable(data, mt)
end

function _M:start()
    self._active = true
end

function _M:process(task)
    if not self._active then
        ngx_log(ERR, self.name, " is inactive")
        return nil, "inactive"
    end
    return true
end

return _M
```

当你第一眼看到这段代码，脑海中的解码流应当是：

#### 第一步：扫开头与结尾 —— 识别模块（Module）
- 看到开头的 `local _M = {}` 和最后的 `return _M`。
- **心智反应**：这是标准的单例模块模式。当前文件输出一个模块表 `_M`，外部调用方式必定是 `local worker = require("worker")`。

#### 第二步：看依赖与环境 —— 识别性能优化（Scope & Upvalue）
- 看到开头的 `local setmetatable = setmetatable` 和 `local ngx_log = ngx.log`。
- **心智反应**：作者在做性能加速（Upvalue Local Caching），把全局哈希查找打落到寄存器访问。

#### 第三步：看原型绑定 —— 识别对象模型（Metatable）
- 看到 `local mt = { __index = _M }` 和 `return setmetatable(data, mt)`。
- **心智反应**：经典的**原型类**构造。
  - `_M` 既是模块也是类（原型表）。
  - 返回的实例对象是 `data`（一个独立的普通 Table）。
  - 当实例找不到方法时，自动沿 `mt.__index` 向上去 `_M` 中找。

#### 第四步：看函数定义符号 —— 识别调用协议（Function Calling Convention）
- 看到 `function _M.new(self, opts)`（点号）与 `function _M:start()`（冒号）。
- **心智反应**：
  - `new` 是静态工厂函数（通过 `worker.new(...)` 调用）。
  - `start` 和 `process` 是实例方法（通过 `inst:start()` 调用，内部可安全使用隐式参数 `self` 操作实例内部的 `_active` 状态）。

#### 第五步：看数据与状态生命周期（Table & Field）
- 看到 `data` 中的 `_active = false`。
- **心智反应**：字段以下划线开头，表明开发者约定其为内部私有状态，不建议外部直接修改，应由 `:start()` 方法驱动。

---

# 5. 一眼识别结构速查表

在阅读和编写 Lua 项目时，养成看到代码模式瞬间完成心智映射的条件反射：

| 看到代码 | 脑海中的第一反应（Mental Model） | C / C++ / Go / Python 对应物 |
| :--- | :--- | :--- |
| `{}` | 创建一个新的堆上 Table 引用对象 | `malloc(struct)` / `make(map)` / `{}` |
| `{ 1, 2, 3 }` | 连续数组，下标从 **1** 开始，存放在 Array Part | C 连续数组 / Go Slice |
| `t.foo` | `t["foo"]` 的语法糖，哈希字段访问 | 结构体成员 `s.foo` / Python `d["foo"]` |
| `local x = ...` | 寄存器级局部变量，离开 block 立即释放 | C 栈局部变量 / Go `:=` |
| `x = ...` (无 local) | 危险！写入全局环境 `_ENV["x"]`，全局污染 | 全局变量 / Python `global` |
| `local sin = math.sin` | 性能优化：将全局表查找缓存为局部寄存器引用 | C 局部函数指针缓存 |
| `obj:method(...)` | 语法糖：等价于 `obj.method(obj, ...)` | C++ / Python 隐式传入 `this` / `self` |
| `function M:foo()` | 语法糖：等价于 `function M.foo(self)` | 成员函数声明，首参绑定为 `self` |
| `setmetatable(o, mt)` | 建立行为代理拦截，给 `o` 赋予灵魂 | 绑定 vtable / 继承基类 |
| `mt.__index = M` | 找不到就去 `M` 找；`M.__index = M` 表示类自己当方法表 | 原型链查找（JS prototype / Python MRO） |
| `mt.__newindex = fn` | 拦截未知属性的写入：只读保护或属性监听 | Python `__setattr__` |
| `ipairs(arr)` | 仅遍历连续整数索引 `1, 2, ...`，遇到 `nil` 即停 | Go `for i, v := range slice` |
| `pairs(dict)` | 遍历 Table 中所有的 Key-Value，顺序不可预测 | Python `for k, v in dict.items()` |
| `t[#t + 1] = v` | 数组末尾高速追加元素 | Go `append(slice, v)` / Python `list.append` |
| `x and y or z` | 短路三元运算符（当 y 不为 false/nil 时等价） | C `x ? y : z` / Python `y if x else z` |
| `return function()...` | Closure：带走外层 `local`（Upvalue 是变量本身，不是值拷贝） | C++ `[&]` 引用捕获 / Go 闭包 |
| `package.loaded[mod]` | 模块单例缓存表，置为 `nil` 即可实现热重载 | Python `sys.modules` |
| `...` | 变长参数列表，可通过 `{...}` 打包为数组 | Go `args ...interface{}` / C `va_list` |
| `pcall(fn, ...)` | 保护模式执行，捕获异常与运行时崩溃 | `try ... catch` / Go `recover()` |
