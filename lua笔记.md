Lua 由 clean C（标准 C 和 C++ 间共通的子集） 实现

Lua 是一门动态类型语言。 意味着变量没有类型；只有值才有类型。 语言中不设类型定义。 所有的值携带自己的类型。

Lua 中所有的值都是 一等公民。 这意味着所有的值均可保存在变量中、 当作参数传递给其它函数、以及作为返回值

Lua 中有八种基本类型： nil、boolean、number、string、function、userdata、 thread 和 table
Boolean 是 false 与 true 两个值的类型。 nil 和 false 都会导致条件判断为假； 而其它任何值都表示为真

table 是一个关联数组， 也就是说，这个数组不仅仅以数字做索引，除了 nil 和 NaN 之外的所有 Lua 值 都可以做索引
表是 Lua 中唯一的数据结构， 它可被用于表示普通数组、序列、符号表、集合、记录、图、树等等

我们使用 序列 这个术语来表示一个用 {1..n} 的正整数集做索引的表。 这里的非负整数 n 被称为该序列的长度

表、函数、线程、以及完全用户数据在 Lua 中被称为 对象： 变量并不真的 持有 它们的值，而仅保存了对这些对象的 引用。 赋值、参数传递、函数返回，都是针对引用而不是针对值的操作， 这些操作均不会做任何形式的隐式拷贝

库函数 type 用于以字符串形式返回给定值的类型

Lua 中的每个值都可以有一个 元表.这个 元表 就是一个普通的 Lua 表， 它用于定义原始值在特定操作下的行为。 如果你想改变一个值在特定操作下的行为，你可以在它的元表中设置对应域。 例如，当你对非数字值做加操作时， Lua 会检查该值的元表中的 "__add" 域下的函数。 如果能找到，Lua 则调用这个函数来完成加这个操作
在元表中事件的键值是一个双下划线（__）加事件名的字符串； 键关联的那些值被称为 元方法。 在上一个例子中，__add 就是键值， 对应的元方法是执行加操作的函数。

表和完全用户数据有独立的元表 （当然，多个表和用户数据可以共享同一个元表）。 其它类型的值按类型共享元表； 也就是说所有的数字都共享同一个元表， 所有的字符串共享另一个元表等等

元表决定了一个对象在数学运算、位运算、比较、连接、 取长度、调用、索引时的行为。 元表还可以定义一个函数，当表对象或用户数据对象在垃圾回收 （参见§2.5）时调用它


元表可以控制的事件的详细列表，每个操作都用对应的事件名来区分。 每个事件的键名用加有 '__' 前缀的字符串来表示； 例如 "add" 操作的键名为字符串 "__add"
__add: + 操作。 如果任何不是数字的值（包括不能转换为数字的字符串）做加法， Lua 就会尝试调用元方法。 首先、Lua 检查第一个操作数（即使它是合法的）， 如果这个操作数没有为 "__add" 事件定义元方法， Lua 就会接着检查第二个操作数。 一旦 Lua 找到了元方法， 它将把两个操作数作为参数传入元方法， 元方法的结果（调整为单个值）作为这个操作的结果。 如果找不到元方法，将抛出一个错误。
__sub: - 操作。 行为和 "add" 操作类似。
__mul: * 操作。 行为和 "add" 操作类似。
__div: / 操作。 行为和 "add" 操作类似。
__mod: % 操作。 行为和 "add" 操作类似。
......



Lua 采用了自动内存管理。 这意味着你不用操心新创建的对象需要的内存如何分配出来， 也不用考虑在对象不再被使用后怎样释放它们所占用的内存。

下面五种方式描述了完全相同的字符串：
a = 'alo\n123"'
a = "alo\n123\""
a = '\97lo\10\04923"'
a = [[alo
123"]]
a = [==[
alo
123"]==]

在字符串外的任何地方出现以双横线 (--) 开头的部分是 注释

Lua 中有三种变量： 全局变量、局部变量和表的域
所有没有显式声明为局部变量的变量名都被当做全局变量
在变量的首次赋值之前，变量的值均为 nil
对全局变量以及表的域之访问的含义可以通过元表来改变

以索引方式访问一个变量 t[i] 等价于 调用 gettable_event(t,i)
var.Name 这种语法只是一个语法糖，用来表示 var["Name"]
对全局变量 x 的操作等价于操作 _ENV.x。 由于代码块编译的方式， _ENV 永远也不可能是一个全局名字 


Lua 支持所有与 Pascal 或是 C 类似的常见形式的语句， 这个集合包括赋值，控制结构，函数调用，还有变量声明。
Lua 的一个编译单元被称为一个 代码块。 从句法构成上讲，一个代码块就是一个语句块。
Lua 把一个代码块当作一个拥有不定参数的匿名函数 （参见§3.4.11）来处理。 正是这样，代码块内可以定义局部变量，它可以接收参数，返回若干值


Lua 允许多重赋值
赋值语句首先让所有的表达式完成运算， 之后再做赋值操作
对全局变量以及表的域的赋值操作的含义可以通过元表来改变。 对 t[i] = val 这样的变量索引赋值， 等价于 settable_event(t,i,val)

函数调用和可变参数表达式都可以放在多重返回值中
     f()                -- 调整为 0 个结果
     g(f(), x)          -- f() 会被调整为一个结果
     g(x, f())          -- g 收到 x 以及 f() 返回的所有结果
     a,b,c = f(), x     -- f() 被调整为 1 个结果 （c 收到 nil）
     a,b = ...          -- a 收到可变参数列表的第一个参数，
                        -- b 收到第二个参数（如果可变参数列表中
                        -- 没有实际的参数，a 和 b 都会收到 nil）
     
     a,b,c = x, f()     -- f() 被调整为 2 个结果
     a,b,c = f()        -- f() 被调整为 3 个结果
     return f()         -- 返回 f() 的所有返回结果
     return ...         -- 返回从可变参数列表中接收到的所有参数parameters
     return x,y,f()     -- 返回 x, y, 以及 f() 的所有返回值
     {f()}              -- 用 f() 的所有返回值创建一个列表
     {...}              -- 用可变参数中的所有值创建一个列表
     {f(), nil}         -- f() 被调整为一个结果

Lua 中字符串的连接操作符写作两个点（'..'）
取长度操作符写作一元前置符 #

表构建
     a = { [f(1)] = g; "x", "y"; x = 1, f(x), [30] = 23; 45 }
等价于

     do
       local t = {}
       t[f(1)] = g
       t[1] = "x"         -- 1st exp
       t[2] = "y"         -- 2nd exp
       t.x = 1            -- t["x"] = 1
       t[3] = f(x)        -- 3rd exp
       t[30] = 23
       t[4] = 45          -- 4th exp
       a = t
     end

函数定义
一个函数定义是一个可执行的表达式， 执行结果是一个类型为 function 的值
一些语法糖简化函数定义的写法

     function f () body end
被转译成
     f = function () body end

     function t.a.b.c.f () body end
被转译成
     t.a.b.c.f = function () body end

     local function f () body end
被转译成
     local f; f = function () body end
而不是
     local f = function () body end


当一个函数被调用， 如果函数并非一个 可变参数函数，那么实参列表就会被调整到形参列表的长度。 
变长参数函数不会调整实参列表； 它将把所有额外的参数放在一起通过 变长参数表达式传递给函数， 其写法依旧是三个点。 
如果一个变长参数表达式放在另一个表达式中使用， 或是放在另一串表达式的中间， 那么它的返回值就会被调整为单个值。 若这个表达式放在了一系列表达式的最后一个， 就不会做调整了 （除非这最后一个参数被括号给括了起来）。
     function f(a, b) end
     function g(a, b, ...) end
     function r() return 1,2,3 end
实参到形参数以及可变长参数的映射关系：
     CALL            PARAMETERS
     
     f(3)             a=3, b=nil
     f(3, 4)          a=3, b=4
     f(3, 4, 5)       a=3, b=4
     f(r(), 10)       a=1, b=10
     f(r())           a=1, b=2
     
     g(3)             a=3, b=nil, ... -->  (nothing)
     g(3, 4)          a=3, b=4,   ... -->  (nothing)
     g(3, 4, 5, 8)    a=3, b=4,   ... -->  5  8
     g(5, r())        a=5, b=1,   ... -->  2  3


Lua 语言有词法作用范围。 变量的作用范围开始于声明它们之后的第一个语句段， 结束于包含这个声明的最内层语句块的最后一个非空语句
     x = 10                -- 全局变量
     do                    -- 新的语句块
       local x = x         -- 新的一个 'x', 它的值现在是 10
       print(x)            --> 10
       x = x+1
       do                  -- 另一个语句块
         local x = x+1     -- 又一个 'x'
         print(x)          --> 12
       end
       print(x)            --> 11
     end
     print(x)              --> 10 （取到的是全局的那一个）


加载一个模块
require (modname)


字符串处理
当在 Lua 中对字符串做索引时，第一个字符从 1 开始计算

```lua
-- 1
--[[
pairs(t)
如果 t 有元方法 __pairs， 以 t 为参数调用它，并返回其返回的前三个值。
否则，返回三个值：next 函数， 表 t，以及 nil。 因此以下代码
     for k,v in pairs(t) do body end
能迭代表 t 中的所有键值对。
--]]
tab1 = {
    key1 = "value1",
    key2 = "value2",
}

for k, v in pairs(tab1) do
    print(k .. " - " .. v)  -- .. 用来连接字符串
end

-- 2
-- 可以用 2 个方括号 "[[]]" 来表示"一块"字符串
HTML = [[
<html>
<head></head>
<body>
    <a href="//www.dba.cn/">w3cschoolW3Cschool教程</a>
</body>
</html>
]]
print(html)

-- 3
print("2" + 8)  -- 10
print("2.0" + 8)  -- 10.0
print("a" .. 'b')   -- ab
print(123 .. 456)   -- 123456
print(#"hello")     -- 5

-- 4
tb1 = {}
tb1[1] = "apple"
tb1[2] = "pear"

tb2 = {"apple", "pear"}

for k, v in pairs(tb1) do
    print(k .. ":" .. v)
end

for k, v in pairs(tb2) do
    print(k .. ":" .. v)
end

-- 5
function factorial1(n)
    if n == 0 then
        return 1
    else
        return n * factorial1(n-1)
    end
end

factorial2 = factorial1
print(factorial1(5))
print(factorial2(5))

-- 6 匿名函数作为函数参数
function anonymous(tab, func)
    for k, v in pairs(tab) do
        print(func(k, v))
    end
end

tab = { key1 = "value1", key2 = "value2" }
anonymous(tab, function(key, val)
    return key .. " = " .. val
end)


-- 7 全局变量 与 局部变量
i1 = 1          -- global
local i2 = 2    -- local

function joke()
    i3 = 3          -- global
    local i4 = 4    -- local
end

joke()
print(i3, i4)       -- output: 3 nil

do
    local i1 = 5    -- local
    i2 = 6          -- global
    print(i1, i2)   -- output: 1 6
end

print(i1, i2)       -- output: 1 6


-- 8 变量赋值
--[[
    当变量个数和值的个数不一致时，Lua会一直以变量个数为基础采取以下策略：
    变量个数 > 值的个数             按变量个数补足nil
    变量个数 < 值的个数             多余的值会被忽略 
--]]
a, b, c = 0, 1
print(a, b, c)      -- output: 0 1 nil

a, b = 0, 1, 2
print(a, b)         -- output: 0 1

a, b, c = 0
print(a, b, c)      -- output: 0 nil nil


-- 9 table 的索引
--[[
对 table 的索引方式
    t[i]
    t.i                 -- 当索引为字符串类型时的一种简化写法
    gettable_event(t,i) -- 采用索引访问本质上是一个类似这样的函数调用
--]]
site = { key = "value", }
print(site["key"])
print(site.key)

-- 10 循环
-- while
i = 1
while i < 10 do
    if i == 5 then
        break
    end

    print(i)
    i = i + 1
end

-- for
for i=10, 1, -1 do
    if i == 5 then
        break
    end

    print(i)
end

i = 1

-- repeat .. until
repeat
    if i == 5 then
        break
    end

    print(i)
    
    i = i + 1
until i >= 10

-- if
i = 1

if i == 1 then
    print(i .. " equals to 1")
else
    print(i .. " not equals to 1")
end

--11 函数
-- 函数定义
function max1(num1, num2)
    if num1 > num2 then
        return num1
    end
    return num2
end

max2 = function(num1, num2)
    if num1 > num2 then
        return num1
    end
    return num2
end

print(max1(1, 2))
print(max2(1, 2))

-- 多个返回值
function maximum(a)
    local mi = 1
    local m = a[mi]
    for i, val in ipairs(a) do
        if val > m then
            mi = i
            m = val
        end
    end
    return m, mi
end

print(maximum({2, 3, 1}))

-- 可变参数
function average(...)
    result = 0
    local arg = {...}

    for i, v in ipairs(arg) do
        result = result + v
    end

    return result / #arg
end

print(average(1, 2, 3))
print(average(1, 2, 3, 4, 5))

-- 12 运算符
print(2 + 3)
print(2 - 3)
print(2 * 3)
print(2 / 3)
print(2 % 3)
print(2 ^ 3)
print(-3)

print(2 == 3)
print(2 ~= 3)
print(2 > 3)
print(2 < 3)
print(2 >= 3)
print(2 <= 3)

print(1 and nil)
print(1 and false)
print(1 or nil)
print(1 or false)
print(not true)

print(12 .. 34)
print(#"hello")

-- 13 字符串
mystr = "abcde"
print(string.upper(mystr))
print(string.lower(mystr))
print(string.gsub(mystr, "l", "x"))
print(string.gsub(mystr, "l", "x", 1))
print(string.gsub(mystr, "l", "x", 2))
print(string.find(mystr, "l"))
print(string.find(mystr, "l", 4))
print(string.find(mystr, "ll"))
print(string.reverse(mystr))
print(string.format("the string is: %s", mystr))
print(string.byte(mystr))
print(string.byte(mystr, 2))
print(string.byte(mystr, -1))
print(string.byte(mystr, -2))
print(string.char(97, 98))
print(string.len(mystr))
print(string.rep(mystr, 2))
print(mystr .. "fg")


-- 14 数组
-- 一维数组
arr1 = {"element 1", "element 2"}
for i = 0, 2 do
    print(arr1[i])
end

arr2 = {}
for i = -2, 2 do
    arr2[i] = i * 2
    print(arr2[i])
end

-- 多维数组
arr3 = {}
for i = 1, 3 do
    arr3[i] = {}
    for j = 1, 3 do
        arr3[i][j] = i * j
    end
end

for i = 1, 3 do
    for j = 1, 3 do
        print(arr3[i][j])
    end
end

-- 多维数组
arr4 = {}
maxRows = 3
maxCols = 3

for row = 1, maxRows do
    for col = 1, maxCols do
        arr4[row*maxCols + col] = row * col
    end
end

for row = 1, maxRows do
    for col = 1, maxCols do
        print(arr4[row*maxCols + col])
    end
end

-- 15


-- 16


-- 17


-- 18


-- 19


```