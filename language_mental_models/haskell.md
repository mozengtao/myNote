# Haskell —— 变换（Transformation）

> **核心驱动力：程序就是纯函数之间的组合。**
> Haskell 程序员不问"下一步要修改哪个变量"，而问"如何把输入值变换成输出值"。

---

## 心智模型图解

```
Input（输入值）
     │
     ▼
filter（保留符合条件的部分）
     │
     ▼
map（逐个变换）
     │
     ▼
reduce（聚合成最终结果）
```

Haskell 里没有"修改变量"这件事——所有值一旦创建就不可变（immutable）。
程序的运行过程，就是把一个输入值，通过一系列纯函数（pure function）的组合，
一步步"变换"成另一个值，中间没有隐藏的状态、没有副作用。

---

## 核心驱动力详解

- **纯函数**：给定相同输入，永远返回相同输出，且不产生任何副作用（不修改外部状态、不做 IO）。
- **不可变性**：值一旦绑定，不能被"就地修改"，需要新值时就创建一个新值，而不是修改旧值。
- **函数组合**：复杂逻辑不是靠一长串语句堆出来的，而是靠把小函数拼接（`.`）成大函数。
- **惰性求值**：表达式默认不会立刻计算，只有真正需要结果时才会被求值——这让"无限列表"之类的抽象成为可能。
- **用类型系统表达"可能没有结果/可能失败"**：`Maybe`、`Either` 把"空值/异常"变成了普通的值变换。

---

## 典型代码片段

### 1. 纯函数 —— 无副作用，行为完全由输入决定

```haskell
square :: Int -> Int
square x = x * x

-- 调用 square 100 次，只要输入是 5，结果永远是 25
-- 不依赖任何全局状态，也不产生任何副作用
main :: IO ()
main = print (square 5)
```

**心智模型解读**：`square` 的整个行为完全由它的输入决定，你甚至可以把某次调用的结果
提前算好、缓存下来（引用透明性，referential transparency），这是"变换"思维的最小单元。

### 2. `map` —— 对列表中的每个值做同一种变换

```haskell
double :: [Int] -> [Int]
double xs = map (\x -> x * 2) xs

main :: IO ()
main = print (double [1, 2, 3, 4]) -- [2, 4, 6, 8]
```

**心智模型解读**：没有 `for` 循环、没有下标索引，`map` 表达的是"把某个变换函数应用到每一个元素上"，
生成一个全新的列表——原列表 `xs` 完全没有被修改。

### 3. `filter` —— 保留满足条件的部分，本身也是一种变换

```haskell
evens :: [Int] -> [Int]
evens = filter even

main :: IO ()
main = print (evens [1..10]) -- [2,4,6,8,10]
```

**心智模型解读**：`filter` 把"输入列表"变换成"只包含符合谓词的元素的新列表"，
这和 `map`（逐一变形）是同一大类思维的不同变体：都是"从一个值得到另一个值"。

### 4. `foldr` —— 把整个列表"折叠"成一个值

```haskell
sumList :: [Int] -> Int
sumList = foldr (+) 0

productList :: [Int] -> Int
productList = foldr (*) 1

main :: IO ()
main = do
    print (sumList [1, 2, 3, 4])     -- 10
    print (productList [1, 2, 3, 4]) -- 24
```

**心智模型解读**：`foldr` 是"变换"思维的终极形式之一：把一整个结构逐步"收敛"成单个值，
`sum`、`product`、`length`、甚至 `map`/`filter` 本身都可以用 `foldr` 表达出来。

### 5. 函数组合 `.` —— 把小变换拼接成大变换

```haskell
processNames :: [String] -> [String]
processNames = map (take 3) . filter (not . null) . map trim
  where trim = dropWhile (== ' ')

main :: IO ()
main = print (processNames ["  Alice", "", "  Bob", "   "])
-- ["Ali", "Bob"]
```

**心智模型解读**：`f . g` 表示"先做 g 的变换，再对结果做 f 的变换"，
整条处理流程读起来就像一条数据变换的流水线——这和 Shell 管道的心智模型高度相似，只是换成了纯函数世界。

### 6. 不可变性 —— 需要"新状态"时创建新值，而不是修改旧值

```haskell
data Counter = Counter { count :: Int }

increment :: Counter -> Counter
increment c = Counter (count c + 1) -- 返回一个新的 Counter，旧的 c 完全不变

main :: IO ()
main = do
    let c0 = Counter 0
        c1 = increment c0
        c2 = increment c1
    print (count c0, count c1, count c2) -- (0, 1, 2)，c0 全程保持为 0
```

**心智模型解读**：`increment` 不会"修改" `c`，它只是根据旧值变换出一个新值。
`c0` 在整个程序运行期间永远是 `Counter 0`，这消除了"某处代码悄悄改了我的数据"这类 bug 的可能性。

### 7. 模式匹配 —— 用"结构分解"代替 if/else 分支

```haskell
describe :: [Int] -> String
describe []       = "empty list"
describe [x]      = "single element: " ++ show x
describe (x:y:_)  = "starts with " ++ show x ++ " and " ++ show y

main :: IO ()
main = do
    putStrLn (describe [])
    putStrLn (describe [42])
    putStrLn (describe [1, 2, 3])
```

**心智模型解读**：每个等式左边的模式（`[]`、`[x]`、`(x:y:_)`）描述的是"输入值长什么样",
右边描述"这种形状的输入该变换成什么输出"——本质上仍然是"输入到输出的映射规则"。

### 8. `Maybe` —— 把"可能没有值"变成一种可以被变换的值

```haskell
safeDivide :: Int -> Int -> Maybe Int
safeDivide _ 0 = Nothing
safeDivide x y = Just (x `div` y)

describeResult :: Maybe Int -> String
describeResult Nothing  = "cannot divide by zero"
describeResult (Just n) = "result is " ++ show n

main :: IO ()
main = do
    putStrLn (describeResult (safeDivide 10 2)) -- result is 5
    putStrLn (describeResult (safeDivide 10 0)) -- cannot divide by zero
```

**心智模型解读**：Haskell 没有 `null`，"可能失败"这件事被显式编码进类型（`Maybe Int`），
调用者被类型系统强制要求处理 `Nothing` 分支——"空值"从一个隐藏的运行期风险，变成了一个显式的变换分支。

### 9. 惰性求值 —— 表达式不是"立刻算"，而是"需要时才算"

```haskell
naturals :: [Integer]
naturals = [1..] -- 无限列表，但 Haskell 不会真的去"生成无穷多个元素"

main :: IO ()
main = print (take 5 (map (* 2) naturals)) -- [2,4,6,8,10]
```

**心智模型解读**：`naturals` 是一个无限列表，`map (*2) naturals` 也是"无限的"，
但因为 Haskell 是惰性求值，只有 `take 5` 真正需要多少个元素，才会去计算多少个——变换在这里是"按需"发生的。

### 10. 高阶函数 —— 函数本身也是"可以被变换、被传递"的值

```haskell
twice :: (a -> a) -> a -> a
twice f x = f (f x)

main :: IO ()
main = do
    print (twice (+3) 10)     -- 16, 即 10+3+3
    print (twice reverse "ab") -- "ab"，reverse 两次等于没变
```

**心智模型解读**：`twice` 接受一个函数作为参数，返回"把这个函数应用两次"的新行为——
函数在 Haskell 里和数字、字符串一样，是可以被传递、组合、变换的"一等公民"（first-class value）。

### 11. 列表推导式 —— 用声明式语法表达"变换 + 过滤"

```haskell
pythagoreanTriples :: Int -> [(Int, Int, Int)]
pythagoreanTriples n =
    [ (a, b, c) | c <- [1..n], b <- [1..c], a <- [1..b], a*a + b*b == c*c ]

main :: IO ()
main = print (pythagoreanTriples 20)
```

**心智模型解读**：列表推导式把"从哪些值出发（生成器）、要满足什么条件（谓词）、
最终变换成什么形式（表达式）"三件事放进一行里声明清楚，是"变换+过滤"思维最紧凑的表达方式。

### 12. Functor —— 用 `fmap` 统一"在容器里做变换"这件事

```haskell
main :: IO ()
main = do
    print (fmap (+1) (Just 5))     -- Just 6
    print (fmap (+1) Nothing)      -- Nothing
    print (fmap (*2) [1, 2, 3])    -- [2,4,6]
```

**心智模型解读**：不管是 `Maybe`、列表还是其他"带着值的容器"，`fmap` 都表达同一件事——
"把变换函数应用到容器内部的值上，容器的结构本身保持不变"。这是"变换"这一核心驱动力在类型层面的抽象。

---

## 黄金法则

> **不要想着修改变量，而要想着构造新的值。**

遇到"需要更新状态"的场景，先问自己："能不能根据旧值，计算出一个新值？"
而不是想"怎么原地改掉这个变量"。

---

## 常见误区对比

### 误区一：试图用命令式的"循环+累加变量"思维硬翻译成 Haskell

```haskell
-- 错误心智模型：模拟"可变累加器"，写法笨拙且不符合语言习惯
sumListImperative :: [Int] -> Int
sumListImperative xs = go xs 0
  where
    go [] acc = acc
    go (x:rest) acc = go rest (acc + x) -- acc 看似在"累加"，实际每次都是传入新值
```

```haskell
-- Haskell 习惯写法：直接用现成的折叠/内置函数表达"变换"
sumListIdiomatic :: [Int] -> Int
sumListIdiomatic = sum -- 或者 foldr (+) 0
```

**为什么后者更好**：手写递归+累加参数虽然能工作（本质上也是"传入旧值算出新值"），
但更地道的写法是直接使用 `sum`/`foldl'`/`foldr` 等标准变换函数，代码更短，也更容易看出"这是一次归约"。

### 误区二：在纯函数里"藏"副作用，误以为 Haskell 允许隐式的可变状态

```haskell
-- 错误心智模型：以为可以像其他语言一样悄悄修改一个"全局变量"
-- （Haskell 中这种写法根本无法通过类型检查，只是用来说明思维误区）
-- counter = 0
-- incrementAndGet = counter += 1; return counter  -- 这种写法在 Haskell 里不存在
```

```haskell
-- Haskell 习惯写法：需要状态时，显式通过类型（如 State monad 或显式传参）表达
import Control.Monad.State

incrementAndGet :: State Int Int
incrementAndGet = do
    n <- get
    put (n + 1)
    return (n + 1)

main :: IO ()
main = print (runState incrementAndGet 0) -- (1, 1)
```

**为什么后者更好**：Haskell 的类型系统不允许"隐藏的可变状态"存在，任何"有状态的计算"
都必须显式地在类型里体现出来（比如 `State` monad），这保证了"看函数签名就知道它是否纯"。

---

## 快速上手 Checklist

- [ ] 看到一个函数，能确认它是否是"纯函数"（相同输入永远给相同输出，无副作用）吗？
- [ ] 遇到"需要更新的状态"，第一反应是"构造一个新值"而不是"原地修改"吗？
- [ ] 能看懂 `f . g` 这种函数组合写法，并说出执行顺序吗？
- [ ] 理解为什么 Haskell 用 `Maybe`/`Either` 而不是 `null`/异常来表达"可能失败"吗？
- [ ] 知道惰性求值意味着"表达式不写等号右边就不一定会被计算"这件事吗？

---

上一篇：[JavaScript —— 事件](javascript.md) ・ 下一篇：[SQL —— 声明](sql.md)
