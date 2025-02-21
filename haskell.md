[Learn You a Haskell for Great Good!](https://learnyouahaskell.com/chapters)  
[Real World Haskell](https://book.realworldhaskell.org/read/)  
[The Craft of Functional Programming](https://simonjohnthompson.github.io/craft3e/craft3e.pdf)  
[Haskell Hierarchical Libraries](https://downloads.haskell.org/ghc/latest/docs/libraries/)  
[Haskell 趣学指南](https://www.w3cschool.cn/hsriti/)  

## haskell 是一门纯函数式编程语言
函数式编程语言中的函数能做的唯一事情就是求值，因而没有副作用
haskell 是惰性的。也就是说若非特殊指明，函数在真正需要结果以前不会被求值
haskell 是静态类型的
haskell中的函数并没有顺序

```haskell
# 函数调用拥有最高的优先级
succ 9 + max 5 4 + 1
等价于
(succ 9) + (max 5 4) + 1


# infix call
div 92 10
等价于
92 `div` 10

# 函数定义
函数名 参数1 参数2 ... = 函数实现
doubleMe x = x + x
```

### List
```
List是一种单类型的数据结构，可以用来存储多个类型相同的元素
let lostNumbers = [4,8,15,16,23,48]   
[1,2,3,4] ++ [9,10,11,12]    
"hello" ++ " " ++ "world"
['w','o'] ++ ['o','t']
'A':" SMALL CAT"

[1,2,3]实际上是 1:2:3:[] 的语法糖
[],[[]],[[],[],[]]是不同的

[3,2,1] > [2,1,0]  
[3,4,2] > [3,4]

head [5,4,3,2,1]  
tail [5,4,3,2,1]
last [5,4,3,2,1]
init [5,4,3,2,1]   
head []   // Exception
length [5,4,3,2,1]
null [1,2,3]
null []
reverse [5,4,3,2,1]
take 3 [5,4,3,2,1]
take 1 [3,9,3]  
take 5 [1,2]
take 0 [6,6,6]
drop 3 [8,4,2,1,5,6]   
drop 0 [1,2,3,4]
drop 100 [1,2,3,4]
minimum [8,4,2,1,5,6] 
maximum [1,9,2,3,4]
sum [5,2,1,6,3,2,5,7]
product [6,2,1,2]
product [1,2,5,6,7,9,2,0]
4 `elem` [3,4,5,6]   
10 `elem` [3,4,5,6]  

[1..20]
['a'..'z']
[2,4..20]

cycle 接受一个 List 做参数并返回一个无限List
take 10 (cycle [1,2,3]) 
take 12 (cycle "LOL ")

repeat 接受一个值作参数，并返回一个仅包含该值的无限 List
take 10 (repeat 5)

list comprehension

[x*2 | x <- [1..10]]
[x*2 | x <- [1..10], x*2 >= 12] 
[ x | x <- [50..100], x `mod` 7 == 3] 

boomBangs xs = [ if x < 10 then "BOOM!" else "BANG!" | x <- xs, odd x]  
boomBangs [7..13] 

[ x | x <- [10..20], x /= 13, x /= 15, x /= 19]

[ x*y | x <- [2,5,10], y <- [8,10,11]]

[ x*y | x <- [2,5,10], y <- [8,10,11], x*y > 50] 

let nouns = ["hobo","frog","pope"] 
let adjectives = ["lazy","grouchy","scheming"]
[adjective ++ " " ++ noun | adjective <- adjectives, noun <- nouns] 

length' xs = sum [1 | _ <- xs] 

removeNonUppercase st = [ c | c <- st, c `elem` ['A'..'Z']]  
removeNonUppercase "Hahaha! Ahahaha!"
```

### Tuple
```
Tuple 里可以存入多类型项的组合

fst (8,11)
fst ("Wow", False)

snd (8,11)
snd ("Wow", False)

zip [1,2,3,4,5] [5,5,5,5,5]
zip [5,3,2,6,2,7,2,5,4,6,6] ["im","a","turtle"]
zip [1..] ["apple", "orange", "cherry", "mango"]

let rightTriangles' = [ (a,b,c) | c <- [1..10], b <- [1..c], a <- [1..b], a^2 + b^2 == c^2, a+b+c == 24]
rightTriangles'
```

### 类型
```
Haskell是静态类型的语言，在编译时每个表达式的类型都是确定的，凡是类型其首字母必大写
类型定义行为的接口，如果一个类型属于某类型类，那它必实现了该类型类所描述的行为
:t 'a'
:t True
:t "Hello"
:t (True, 'a')
:t 4 == 5

removeNonUppercase :: [Char] -> [Char]   
removeNonUppercase st = [ c | c <- st, c `elem` ['A'..'Z']]

addThree :: Int -> Int -> Int -> Int   
addThree x y z = x + y + z

Int表示整数，它有上限和下限
Integer是无界的，可以用来存放非常非常大的数，但效率不如Int高

:t head
head :: [a] -> a    // a 是个类型变量，意味着a可以是任意的类型

函数的类型声明

:t (==)   
(==) :: (Eq a) => a -> a -> Bool
// 相等函数取两个相同类型的值作为参数并返回一个布尔值，而这两个参数的类型同在Eq类之中（即类型约束）

Eq这一类型类提供了判断相等性的接口，凡是可比较相等性的类型必属于Eq类
5 == 5
5 /= 5
'a' == 'a'
"Ho Ho" == "Ho Ho"
3.432 == 3.432

elem函数的类型为:(Eq a)=>a->[a]->Bool

Ord包含可比较大小的类型。除了函数以外，我们目前所谈到的所有类型都属于Ord类

:t (>)   
(>) :: (Ord a) => a -> a -> Bool

"Abrakadabra" < "Zebra"
"Abrakadabra" `compare` "Zebra"
5 >= 2

Show的成员为可用字符串表示的类型。目前为止，除函数以外的所有类型都是Show的成员 (取任一Show的成员类型并将其转为字符串)
show 3
show 5.334

Read是与Show相反的类型类。read函数可以将一个字符串转为Read的某成员类型
read "True" || False
read "[1,2,3,4]" ++ [3]

read "4" // error:类型不明确

:t read   
read :: (Read a) => String -> a

read "5" :: Int
read "5" :: Float
(read "5" :: Float) * 4 
read "[1,2,3,4]" :: [Int] 

Enum的成员都是连续的类型--也就是可枚举，该类型类包含的类型有
(),Bool,Char,Ordering,Int,Integer,Float和Double

['a'..'e']   
[LT .. GT]
[3 .. 5]
succ 'B'

Bounded的成员都有一个上限和下限
minBound :: Int
maxBound :: Char
maxBound :: Bool 
minBound :: Bool

maxBound :: (Bool, Int, Char)

Num是表示数字的类型类，它的成员类型都具有数字的特征

:t 20

:t (*)   
(*) :: (Num a) => a -> a -> a

Integral同样是表示数字的类型类。Num包含所有的数字：实数和整数。而Intgral仅包含整数，其中的成员类型有Int和Integer
Floating仅包含浮点类型：Float和Double

fromIntegral :: (Num b, Integral a) => a -> b   // 取一个整数做参数并返回一个更加通用的数字，同时处理整数和浮点时会尤为有用

```

### 函数
```haskell
# 1
lucky :: (Integral a) => a -> String   
lucky 7 = "LUCKY NUMBER SEVEN!"   
lucky x = "Sorry, you're out of luck, pal!"   

# 2
addVectors :: (Num a) => (a, a) -> (a, a) -> (a, a)   
addVectors (x1, y1) (x2, y2) = (x1 + x2, y1 + y2) 

# 3
third :: (a, b, c) -> c   
third (_, _, z) = z  

# 4
tell :: (Show a) => [a] -> String   
tell [] = "The list is empty"   
tell (x:[]) = "The list has one element: " ++ show x   
tell (x:y:[]) = "The list has two elements: " ++ show x ++ " and " ++ show y   
tell (x:y:_) = "This list is long. The first two elements are: " ++ show x ++ " and " ++ show y  

# 5
length' :: (Num b) => [a] -> b   
length' [] = 0   
length' (_:xs) = 1 + length' xs 

# 6
模式用来检查一个值是否合适并从中取值
sum' :: (Num a) => [a] -> a   
sum' [] = 0                 // 模式1
sum' (x:xs) = x + sum' xs   // 模式2


-- as模式，将一个名字和@置于模式前，可以在按模式分割什么东西时仍保留对其整体的引用
如这个模式xs@(x:y:ys)，它会匹配出与x:y:ys对应的东西，同时你也可以方便地通过xs得到整个list，而不必在函数体中重复x:y:ys

capital :: String -> String   
capital "" = "Empty string, whoops!"   
capital all@(x:xs) = "The first letter of " ++ all ++ " is " ++ [x]  

capital "Dracula"

使用as模式通常就是为了在较大的模式中保留对整体的引用，从而减少重复性的工作


-- guard 用来检查一个值的某项属性是否为真

bmiTell :: (RealFloat a) => a -> a -> String  
bmiTell weight height  
    | weight / height ^ 2 <= 18.5 = "You're underweight, you emo, you!"  
    | weight / height ^ 2 <= 25.0 = "You're supposedly normal. Pffft, I bet you're ugly!"  
    | weight / height ^ 2 <= 30.0 = "You're fat! Lose some weight, fatty!"  
    | otherwise                 = "You're a whale, congratulations!"

guard 由跟在函数名及参数后面的竖线标志，通常他们都是靠右一个缩进排成一列。一个 guard 就是一个布尔表达式，如果为真，就使用其对应的函数体。如果为假，就送去见下一个 guard，如之继续

-- e.g
myCompare :: (Ord a) => a -> a -> Ordering
a `myCompare` b
    | a > b     = GT
    | a = b     = EQ
    | otherwise = LT

-- 关键字 where
where 绑定是在函数底部定义名字，对包括所有 guard 在内的整个函数可见
where 绑定中定义的名字只对本函数可见，因此不会污染其他函数的命名空间
bmiTell :: (RealFloat a) => a -> a -> String
bmiTell weight height
    | bmi <= skinny = "You're underweight, you emo, you!"
    | bmi <= normal = "You're supposedly normal. Pffft, I bet you're ugly!"
    | bmi <= fat    = "You're fat! Lose some weight, fatty!" 
    | otherwise     = "You're a whale, congratulations!"
    where bmi = weight / height ^ 2
          skinny = 18.5
          normal = 25.0
          fat = 30.0

where 绑定也可以使用模式匹配
    where bmi = weight / height ^ 2
          (skinny, normal, fat) = (18.5, 25.0, 30.0)

where 绑定也可以定义函数
calcBmis :: (RealFloat a) => [(a, a)] -> [a]
calcBmis xs = [bmi w h | (w, h) <- xs]
    where bmi weight height = weight / height ^ 2

-- 关键字 let
let 绑定则是个表达式，允许你在任何位置定义局部变量，而对不同的 guard 不可见
let 绑定也可以使用模式匹配
let 的格式为 let [bindings] in [expressions]。在 let 中绑定的名字仅对 in 部分可见
let 绑定本身是个表达式，而 where 绑定则是个语法结构

if语句时提到它是个表达式，可以出现在几乎任何位置
[if 5 > 3 then "Woo" else "Boo", if 'a' > 'b' then "Foo" else "Bar"]
4 * (if 10 > 5 then 10 else 0) + 2

let 绑定是个表达式
4 * (let a = 9 in a + 1) + 2  
[let square x = x * x in (square 5, square 3, square 2)]
(let a = 100; b = 200; c = 300 in a*b*c, let foo="Hey "; bar = "there!" in foo ++ bar)  
(let (a,b,c) = (1,2,3) in a+b+c) * 100 

可以把 let 绑定放到 List Comprehension 中
在 List Comprehension 中我们忽略了 let 绑定的 in 部分，因为名字的可见性已经预先定义好了
把一个 let...in 放到限制条件中也是可以的，这样名字只对这个限制条件可见
在 ghci 中 in 部分也可以省略，名字的定义就在整个交互中可见
calcBmis :: (RealFloat a) => [(a, a)] -> [a]  
calcBmis xs = [bmi | (w, h) <- xs, let bmi = w / h ^ 2]

let 中绑定的名字在输出函数及限制条件中都可见。这一来我们就可以让我们的函数只返回胖子的 bmi 值
calcBmis :: (RealFloat a) => [(a, a)] -> [a]  
calcBmis xs = [bmi | (w, h) <- xs, let bmi = w / h ^ 2, bmi >= 25.0]
在 (w, h) <- xs 这里无法使用 bmi 这名字，因为它在 let 绑定的前面

let zoot x y z = x * y + z  
zoot 3 9 2

ghci> let boot x y z = x * y + z in boot 3 4 2
ghci> boot  -- Error: Not in scope: `boot'

let 是个表达式，定义域限制的相当小，因此不能在多个 guard 中使用
where 是跟在函数体后面，把主函数体距离型别声明近一些会更易读


-- case 表达式
模式匹配本质上是 case 语句的语法糖

head' :: [a] -> a  
head' [] = error "No head for empty lists!"  
head' (x:_) = x

等价于

head' :: [a] -> a  
head' xs = case xs of [] -> error "No head for empty lists!"  
                      (x:_) -> x

-- case表达式的语法
case expression of pattern -> result  
                   pattern -> result  
                   pattern -> result  
                   ...

函数参数的模式匹配只能在定义函数时使用，而 ​case ​表达式可以用在任何地方

describeList :: [a] -> String  
describeList xs = "The list is " ++ case xs of [] -> "empty."  
                                               [x] -> "a singleton list."   
                                               xs -> "a longer list."

等价于

describeList :: [a] -> String  
describeList xs = "The list is " ++ what xs  
    where what [] = "empty."  
          what [x] = "a singleton list."  
          what xs = "a longer list."

-- 递归函数
maximum' :: (Ord a) => [a] -> a
maximum' [] = error "maximum of empty list"
maximum' [x] = x
maximum' [x:xs]
    | x > maxTail = x
    | otherwise = maxTail
    where maxTail = maximum xs

用max函数重写的maximun'

maximum' :: (Ord a) => [a] -> a
maximum' [] = error "maximum of empty list"
maximum' [x] = x
maximum' (x:xs) = max x (maximum' xs)

-- 1
replicate' :: (Num i, Ord i) => i -> a -> [a]
replicate' n x
    | n <= 0    = []
    | otherwise = x:replicate' (n -1) x

-- 2
take' :: (Num i, Ord i) => i -> [a] -> [a]
take' n _
    | n <= 0 = []
take' _ [] = []
take' n (x:xs) = x:take' (n-1) xs


```