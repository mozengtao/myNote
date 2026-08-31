# 左右夹逼（Opposite Direction）

左右夹逼不是「一个从左、一个从右」的动作记忆。

它是：在以 `(left, right)` 为坐标的上三角网格里，
利用**有序性或宽度单调性**，从右上角开始，每次砍掉一行或一列。

---

# 第一部分：它解决什么问题

适用于：

- 在**有序**数组上找两个位置，使某个二元函数等于 / 最大 / 最小
- 两个端点共同决定答案，且丢掉较短边或较小端后，该端点再也没有价值
- 从两端同时收缩的对称判定（回文）

共同特点：

- 候选是一对下标 `(i, j)`，且 `i < j`
- 固定一端，另一端沿数组方向移动时，目标量单调变化
- 当前这对一旦被判定「过大 / 过小 / 不可能更优」，其中一侧可以永久扔掉

为什么暴力一般是 O(n^2)：

真正要搜的是所有合法对。对数是 `n(n-1)/2`。
没有单调性时，每一对都可能是唯一答案。

真正需要搜索的是：

> 上三角网格里，哪一个 `(i, j)` 满足约束，或使目标最优。

---

# 第二部分：二维搜索空间

n = 5 时，暴力空间：

```
              right -->
         0    1    2    3    4
      +----+----+----+----+----+
   0  |    |  * |  * |  * |  * |
      +----+----+----+----+----+
   1  |    |    |  * |  * |  * |
left  +----+----+----+----+----+
   2  |    |    |    |  * |  * |
      +----+----+----+----+----+
   3  |    |    |    |    |  * |
      +----+----+----+----+----+
   4  |    |    |    |    |    |
      +----+----+----+----+----+

* = 必须检查的 (left, right)
空 = left >= right，非法
```

暴力为什么要遍历整个空间：

每个 * 都是一个独立候选。若数组无序，或目标对左右没有单调关系，
知道 `(0, 4)` 的和偏大，推不出 `(0, 3)` 或 `(1, 4)` 哪个该扔。

真正哪些区域不用搜索：

常识容易写反，必须对着格子说清楚：

```
sum(L, R) > target 时

左端再往右 (L' > L): 数更大，和更大，更超标
所以死亡的是整列 right=R，不是整行 left=L
动作是 R--

sum(L, R) < target 时

右端再往左 (R' < R): 数更小，和更小，更不够
所以死亡的是整行 left=L
动作是 L++
```

推导见下一部分。

---

# 第三部分：单调性

左右夹逼真正单调的，是下面这句话（以有序非降数组、求和为例）：

```
a[0] <= a[1] <= ... <= a[n-1]

固定 right = R:
    left 增大  =>  a[left] 不减  =>  sum 不减

固定 left = L:
    right 减小 =>  a[right] 不增 =>  sum 不增
```

图：

```
数组:  1  2  4  7  11     target = 9

当前:  L=0, R=4, sum = 1+11 = 12 > 9

因为数组有序:
    任何 left' >= 0 配上 right=4
    sum = a[left'] + 11 >= 12 > 9

所以整列 right=4 全部死亡:

              R=0  1  2  3  4
            +--+--+--+--+--+
        L=0 |  |  |  |  | X|
            +--+--+--+--+--+
        L=1 |  |  |  |  | X|
            +--+--+--+--+--+
        L=2 |  |  |  |  | X|
            +--+--+--+--+--+
        L=3 |  |  |  |  | X|
            +--+--+--+--+--+

X 全部不用再看。唯一合法动作: R--
```

若 sum 偏小：

```
当前: L=0, R=3, sum = 1+7 = 8 < 9

任何 right' <= 3 配上 left=0
sum = 1 + a[right'] <= 8 < 9

所以整行 left=0 全部死亡。唯一合法动作: L++
```

哪些状态可以直接排除：

- sum > target：排除当前右端与所有更大左端的配对（整列）
- sum < target：排除当前左端与所有更小右端的配对（整行）

为什么：有序性让「再往该方向走，只会更离谱」。

对「盛水容器」，单调的不是和，而是：

> 容量 = min(h[L], h[R]) * (R - L)
> 宽度 (R-L) 每次收缩都变小。
> 若移动较长的那根柱子，新高度上限 <= 旧的较短柱，宽度还更小，容量必不增。
> 所以必须移动较短的那根——只有它才有机会抬高 min。

对回文：

> 两端不等，整串立刻失败。
> 两端相等，这对字符已经对上，以后不必再看，向内收缩。

---

# 第四部分：指针移动规则

不要记 `left++` / `right--`。记证明。

为什么只能这样移动：

当前格子给出一个信号（太大 / 太小 / 矮边是瓶颈 / 两端不对称）。
信号指向**必须被处死的那一侧**。
只能把那一侧的指针沿收缩方向推走。

如果反方向移动会发生什么：

```
sum 太大时如果 left++:
    和变得更大，离 target 更远
    同时 right 这个已经过大的端点还留着
    既没有剪掉死列，又可能跨过唯一能配对的左端
    => 漏解

sum 太小时如果 right--:
    和变得更小，死行没剪掉
    可能把唯一能补足差额的右端扔掉
    => 漏解
```

为什么不会漏掉答案：

被扔掉的那一行或那一列，已经用单调性证明**全部不满足**。
答案若存在，只能还活在剩余子矩形里。

为什么以后永远不用回来：

死掉的行/列对任何未来的另一端都仍然死。
有序性不会在扫描过程中反转。所以不需要回退。

---

# 第五部分：复杂度为什么下降

```
初始: left = 0, right = n-1
每一轮恰好把 left 加 1，或把 right 减 1
left 最多加 n 次
right 最多减 n 次
循环次数 <= 2n
每个格子的计算是 O(1)
因此时间 O(n)
```

从二维空间看：

```
一开始要看的 * 有 O(n^2) 个

每一次移动:
    不是丢掉 1 个 *
    而是丢掉当前一行或一列里所有还活着的 *

n 行 + n 列被陆续处死
实际计算过的格子只有路径上的 O(n) 个点
```

```
路径像从右上角沿着「砍行或砍列」走到对角线:

    start (0, n-1)
        |
        |  每次砍掉一行或一列
        v
    停在 left >= right

走过的是一条从外向内的折线，长度 O(n)
折线左侧/右侧的大块矩形从未访问
```

---

# 第六部分：统一心智模型

左右夹逼真正利用的是：

> **有序性（或宽度收缩 + 短板决定上界）让当前端点对整行或整列同时宣判。**

不是技巧，是这条命题成立：

```
若当前 (L, R) 已经过大:
    对所有 L' >= L, (L', R) 更大或同样大
    整列 R 死亡
    R 必须永久左移

若当前 (L, R) 已经过小:
    对所有 R' <= R, (L, R') 更小或同样小
    整行 L 死亡
    L 必须永久右移
```

「有序」是为了让「过大 / 过小」对整行整列同时成立。
无序数组上这句话是假的，左右夹逼会漏解。那时该哈希，不该夹逼。

---

# 第七部分：完整例子

---

## 例子 1：Two Sum II（LeetCode 167）

### 1）题目

有序非降数组 `numbers`，找两个下标（1-based）使和为 `target`。
恰好一组解。必须使用常量额外空间。

示例：`numbers = [1, 2, 4, 7, 11, 15]`, `target = 13`

### 2）为什么想到双指针

暴力：枚举所有 `(i, j)`，O(n^2)。

可以优化：数组有序。固定右端，左端越大和越大；固定左端，右端越小和越小。

单调性：和沿 left++ 不减，沿 right-- 不增。

### 3）画出搜索过程

```
left                     right
1    2    4    7    11   15
sum = 1+15 = 16 > 13
整列 right=15 死亡 => right--
```

```
left                right
1    2    4    7    11   15
sum = 1+11 = 12 < 13
整行 left=1 死亡 => left++
```

```
     left           right
1    2    4    7    11   15
sum = 2+11 = 13 == target
停止
```

### 4）为什么移动这个指针

第一步 sum 太大。

不能 `left++`：1 配 15 已经太大，2 配 15 更大，更不可能。必须扔掉 15。

第二步 sum 太小。

不能 `right--`：1 配 11 已经不够，1 配 7 更不够。必须扔掉 1。

不会漏解：唯一解若存在，一定还在剩余子数组的两端夹逼路径上。
因为被砍掉的行/列已证明全部偏离 target 且方向错误。

### 5）状态剪枝

第一步 `right--` 剪掉：

```
(15, 所有左边) 即
(1,15) (2,15) (4,15) (7,15) (11,15)

全部不用检查。
```

```
         2  4  7  11 15
      +--+--+--+--+--+
   1  |  |  |  |  | X|
      +--+--+--+--+--+
   2  |  |  |  |  | X|
      +--+--+--+--+--+
   4  |  |  |  |  | X|
      +--+--+--+--+--+
   7  |  |  |  |  | X|
      +--+--+--+--+--+
  11  |  |  |  |  | X|
      +--+--+--+--+--+
```

第二步 `left++` 剪掉：

```
(1, 所有还活着的右边) 即 (1,11) (1,7) (1,4) (1,2)
全部 < 13，不用检查。
```

### 6）最终代码

Python:

```python
def two_sum_sorted(numbers, target):
    left = 0
    right = len(numbers) - 1

    while left < right:
        current_sum = numbers[left] + numbers[right]
        if current_sum == target:
            return [left + 1, right + 1]
        if current_sum > target:
            right -= 1
        else:
            left += 1

    return []
```

C++:

```cpp
#include <vector>

std::vector<int> twoSumSorted(const std::vector<int>& numbers, int target)
{
    int left = 0;
    int right = static_cast<int>(numbers.size()) - 1;

    while (left < right) {
        int current_sum = numbers[left] + numbers[right];
        if (current_sum == target)
            return {left + 1, right + 1};
        if (current_sum > target)
            --right;
        else
            ++left;
    }
    return {};
}
```

### 7）复杂度分析

`left` 只增，最多 n-1 次；`right` 只减，最多 n-1 次。
循环至多 2n 步，每步 O(1)，时间 O(n)，额外空间 O(1)。

### 8）这一题真正训练了什么

不是「两数之和模板」。

而是：

> 有序数组上，当前和对整行或整列同时宣判。
> 夹逼路径是上三角网格上的一条砍行/砍列折线。

---

## 例子 2：三数之和（LeetCode 15）

### 1）题目

无序整数数组，找所有不重复三元组，使 `a + b + c = 0`。

示例：`[-1, 0, 1, 2, -1, -4]`
一组答案：`[-1, -1, 2]`, `[-1, 0, 1]`

### 2）为什么想到双指针

暴力：三重循环，O(n^3)，搜的是 `(i, j, k)`。

先排序。固定最小下标 i，剩下变成有序数组上的 Two Sum：
找 `(L, R)` 使 `a[L] + a[R] = -a[i]`。

三维空间被切成 n 个二维切片；每个切片用左右夹逼从 O(n^2) 降到 O(n)。
总时间 O(n^2)。

单调性仍是：有序，和随 L 增而增，随 R 减而减。

### 3）画出搜索过程

排序后：

```
-4  -1  -1  0  1  2
```

固定 i = -4，target = 4：

```
 i   L              R
-4  -1  -1  0  1    2
sum = -4 + -1 + 2 = -3 < 0  => L++  (整行对当前 i 过小)
```

```
 i       L          R
-4  -1  -1  0  1    2
继续过小，L 一路右移，直到 L >= R，本切片无解
```

固定 i = -1（第一个 -1），target = 1：

```
     i   L          R
-4  -1  -1  0  1    2
-1 + -1 + 2 = 0  记录 [-1,-1,2]
为去重: L++ 且跳过相同值, R-- 且跳过相同值
```

```
     i       L   R
-4  -1  -1   0   1  2
-1 + 0 + 1 = 0  记录 [-1,0,1]
```

### 4）为什么移动这个指针

固定 i 之后，规则与 Two Sum 完全相同。

为什么不能移动 i 来代替 L/R：
i 是切片选择器。移动 i 是换一张二维切片，不是在当前切片里剪枝。

去重时跳过相同值：相同值会生成同一三元组，属于重复状态，不是漏解。
跳过它们是把「值相同的重复格子」一并处死。

不会漏解：每个 i 的切片被完全夹逼；更小的 i 已经作为第一元处理过。
要求 `i < L < R`，避免同一组数的排列被重复搜。

### 5）状态剪枝

固定 i=-4 时，第一次 L++ 剪掉：

```
(-4, -1, 所有 <= 2 的右端)
和都 < 0
```

固定 i=-1 命中 `[-1,-1,2]` 后同时收缩，剪掉：

```
以这对端点为边界、值相同的所有重复三元组
```

三维直觉：

```
k
 ^
 |   每个固定 i 是一张 (L, R) 平面
 |   平面内按 Two Sum 砍行砍列
 +--------> j
```

### 6）最终代码

Python:

```python
def three_sum(nums):
    nums.sort()
    n = len(nums)
    result = []

    for i in range(n):
        if i > 0 and nums[i] == nums[i - 1]:
            continue
        left = i + 1
        right = n - 1
        while left < right:
            current_sum = nums[i] + nums[left] + nums[right]
            if current_sum == 0:
                result.append([nums[i], nums[left], nums[right]])
                left += 1
                right -= 1
                while left < right and nums[left] == nums[left - 1]:
                    left += 1
                while left < right and nums[right] == nums[right + 1]:
                    right -= 1
            elif current_sum < 0:
                left += 1
            else:
                right -= 1

    return result
```

C++:

```cpp
#include <vector>
#include <algorithm>

std::vector<std::vector<int>> threeSum(std::vector<int> nums)
{
    std::sort(nums.begin(), nums.end());
    int n = static_cast<int>(nums.size());
    std::vector<std::vector<int>> result;

    for (int i = 0; i < n; ++i) {
        if (i > 0 && nums[i] == nums[i - 1])
            continue;
        int left = i + 1;
        int right = n - 1;
        while (left < right) {
            int current_sum = nums[i] + nums[left] + nums[right];
            if (current_sum == 0) {
                result.push_back({nums[i], nums[left], nums[right]});
                ++left;
                --right;
                while (left < right && nums[left] == nums[left - 1])
                    ++left;
                while (left < right && nums[right] == nums[right + 1])
                    --right;
            } else if (current_sum < 0) {
                ++left;
            } else {
                --right;
            }
        }
    }
    return result;
}
```

### 7）复杂度分析

排序 O(n log n)。外层 i 走 n 次，内层 left/right 合计最多 O(n)。
总时间 O(n^2)。额外空间视排序实现，答案本身除外为 O(1)。

内层仍然满足「两指针各走一遍」，所以每个切片是 O(n) 不是 O(n^2)。

### 8）这一题真正训练了什么

> 高维枚举可以切成许多二维切片；
> 每张切片只要有序，就可以左右夹逼。
> 去重是在处死「值相同的重复格子」，不是额外技巧。

---

## 例子 3：盛最多水的容器（LeetCode 11）

### 1）题目

`height[i]` 是竖线高度。选 `i < j`，容量为 `min(height[i], height[j]) * (j - i)`，求最大容量。

示例：`[1, 8, 6, 2, 5, 4, 8, 3, 7]`，答案 49（下标 1 与 8：min(8,7)*8=49）

### 2）为什么想到双指针

暴力：所有 `(i, j)`，O(n^2)。

需要搜的仍是上三角。

单调性不在数组是否有序，而在：

> 宽度每次强制减 1。
> 容量被较短边锁死。
> 移动长边：新 min <= 旧短边，宽度变小，容量不可能变好。
> 所以长边所在的那一行/列对「超过当前最优」这件事已经死亡。

### 3）画出搜索过程

```
L                               R
1  8  6  2  5  4  8  3  7
min=1, width=8, area=8
矮的是左边 1。移动长边 7 只会更差。必须 L++
```

```
   L                            R
1  8  6  2  5  4  8  3  7
min=7, width=7, area=49   更新最优
矮的是右边 7。必须 R--
```

```
   L                         R
1  8  6  2  5  4  8  3  7
min=3, width=6, area=18
矮的是 3。R--
```

```
   L                      R
1  8  6  2  5  4  8  3  7
min=8, width=5, area=40
两边一样高，移任一侧（证明见下）
```

继续直到相遇。最优保持 49。

### 4）为什么移动这个指针

当前 `h[L] < h[R]` 时必须 `L++`。

为什么不能 `R--`：

```
容量上限被 h[L] 锁死
任何 (L, R') 其中 R' < R:
    width 更小
    min(h[L], h[R']) <= h[L]
    area <= h[L] * (R' - L) < h[L] * (R - L)
所以整列「左端冻结为这个短板」全部更差
```

两边相等时移哪一侧：
两侧都是短板。`(L, R')` 和 `(L', R)` 都以这个高度为上限且宽度更小，
两边整行整列都不可能超过当前这对。移任一侧都不漏更优解。
实现里通常任意选一侧，或两侧都收。

### 5）状态剪枝

第一步 `L++` 剪掉：

```
左端 = 高度1 的柱子，配上所有右边
宽度更小的那些容量都 <= 8，且已被当前 8 覆盖
更精确: 配 right=最后一根 7 的所有更左? 不。
剪掉的是: 固定短板 index=0，所有 right' < last
即 (0, 1)(0,2)...(0,7) 以及我们已算过的 (0,8)

(0, 所有还活着的右边) 再也不会比当前更好
```

```
短板 1 锁死的整行死亡:

    (index0, 所有 j > 0)
```

### 6）最终代码

Python:

```python
def max_area(height):
    left = 0
    right = len(height) - 1
    best = 0

    while left < right:
        width = right - left
        if height[left] < height[right]:
            area = height[left] * width
            left += 1
        else:
            area = height[right] * width
            right -= 1
        if area > best:
            best = area

    return best
```

C++:

```cpp
#include <vector>
#include <algorithm>

int maxArea(const std::vector<int>& height)
{
    int left = 0;
    int right = static_cast<int>(height.size()) - 1;
    int best = 0;

    while (left < right) {
        int width = right - left;
        int area;
        if (height[left] < height[right]) {
            area = height[left] * width;
            ++left;
        } else {
            area = height[right] * width;
            --right;
        }
        best = std::max(best, area);
    }
    return best;
}
```

### 7）复杂度分析

每轮恰好移动一个端点，总共最多 n-1 次移动。时间 O(n)，空间 O(1)。

二维上：每次处死「当前短板配所有更窄宽度」这一行或一列。
n 行 n 列杀完，O(n) 次判决。

### 8）这一题真正训练了什么

> 数组不必有序。
> 单调性可以来自「宽度强制变小 + 短板锁死上界」。
> 左右夹逼利用的是**判决对整行/列同时成立**，不限于两数之和。

---

## 例子 4：有序数组删除重复元素（LeetCode 26，用左右逼近思想看）

### 1）题目

有序数组原地删除重复，使每个元素出现一次，返回新长度。
前 k 个位置是去重结果。

示例：`[0, 0, 1, 1, 1, 2, 2, 3, 3, 4]` -> `[0, 1, 2, 3, 4, ...]`，k=5

说明：
这题实现上是读写指针（见第 6 章）。
这里用左右逼近思想解释**为什么扫描一遍就够**：
有序让「相等只发生在连续一段」，从左已经结算的值，右边再出现的相同值都属于已死区间。

### 2）为什么想到双指针

暴力：对每个位置向右扫，看是否与前面某值相同，O(n^2)。

有序之后：相同值必须粘在一起。
一旦当前值与「已保留的最后一个值」不同，它就是新值；
与之相等的整段都可以一次性跳过。

单调性：值沿下标不减。已出现过的值，更右边只可能相等或更大，
不可能变回一个更早被丢掉的旧值。

### 3）画出搜索过程

把「已保留区的右端」叫 settled，把「正在考察的右端」叫 scan。

```
settled                scan
0  0  1  1  1  2  2  3  3  4
```

更清楚的逐步：

```
write 读到的新值要写到的位置
read  正在看的位置

[0] 0  1  1  1  2 ...
 w  r
nums[r]==nums[w-1]  这段重复已死  r++
```

```
[0] 0  1  1  1  2 ...
 w     r
1 != 0  新值存活  写入 w，w++，r++
```

```
[0  1] 1  1  1  2 ...
    w        r
后面三个 1 整段死亡
```

### 4）为什么移动这个指针

`read` 遇到与已保留尾相同的值，必须前进。
回头看已经保留的更早位置没有意义：有序保证它们 <= 当前值，且已经是去重后的前缀。

不能把 `write` 往左撤：左撤会毁掉已经正确的答案前缀。

不会漏解：每个值的第一次出现一定在该值连续段的最左端，会被 write 收下。

### 5）状态剪枝

```
值 1 的连续段:

    (1, 下标 2 之后的所有 1)

与「已保留的 1」配对的所有重复格子全部死亡。
```

```
已结算前缀 [0, write)
    对任意未来 read:
    前缀里的值都 <= nums[read]
    且不会再被改写
```

### 6）最终代码

Python:

```python
def remove_duplicates_sorted(nums):
    if not nums:
        return 0

    write = 1
    for read in range(1, len(nums)):
        if nums[read] != nums[write - 1]:
            nums[write] = nums[read]
            write += 1
    return write
```

C++:

```cpp
#include <vector>

int removeDuplicatesSorted(std::vector<int>& nums)
{
    if (nums.empty())
        return 0;

    int write = 1;
    for (int read = 1; read < static_cast<int>(nums.size()); ++read) {
        if (nums[read] != nums[write - 1]) {
            nums[write] = nums[read];
            ++write;
        }
    }
    return write;
}
```

### 7）复杂度分析

`read` 从 1 走到 n-1，只增一次遍历。`write` 只增且 write <= read。
总移动 <= 2n。时间 O(n)，空间 O(1)。

### 8）这一题真正训练了什么

> 有序把「重复」从二维任意对，塌缩成一维上的连续段。
> 左右（已结算端 / 扫描端）逼近：左边冻结，右边只负责宣判整段重复死亡。
> 它是夹逼思想与读写指针的交界题。实现落在读写，判决落在有序性。

---

## 例子 5：判断回文字符串（LeetCode 125 / 经典）

### 1）题目

判断字符串是否回文。这里用纯字母数字、忽略大小写的版本（125），
核心与「两端字符是否对称」相同。

示例：`"A man, a plan, a canal: Panama"` 为真。

### 2）为什么想到双指针

暴力：把清洗后的字符串再反转比较，或枚举所有 `(i, n-1-i)`。
本质仍是成对检查。

真正要验证的对是：`(0, n-1), (1, n-2), ...`
它们不是任意 `(i, j)`，而是对角线上的镜像对。

单调性：

> 若最外层已经不相等，整串失败，内层不必看。
> 若最外层相等，这对外层对答案不再提供新信息，可以永久扔掉，向内走。

### 3）画出搜索过程

清洗后：`amanaplanacanalpanama`

```
L                              R
a m a n a p l a n a c a n a l p a n a m a
两端 a==a  外层对死亡（已验证） L++ R--
```

```
  L                          R
a m a n a p l a n a c a n a l p a n a m a
m==m  L++ R--
```

```
    L                      R
a m a n a p l a n a c a n a l p a n a m a
继续直到相遇，全部相等
```

失败例子：`race a car` -> `raceacar`

```
L          R
r a c e a c a r
r==r  L++ R--
```

```
  L      R
r a c e a c a r
a==a
```

```
    L  R
r a c e a c a r
c==c
```

```
      LR
r a c e a c a r
e!=a  整串死亡
```

### 4）为什么移动这个指针

两端相等时必须同时向内。

只移左边：右边那个已匹配字符还占着位置，比较的不再是镜像对，会把合法回文判错。

两端不等时必须立刻失败，不能继续移：
继续移等于假装外层不存在，会把非回文判成回文。

不会漏解：回文的定义就是所有镜像对都相等。
从外往内检查是充分必要的；每一对只需要看一次。

### 5）状态剪枝

两端相等：

```
(最外层这一对) 已经验证
所有「还包含这对外层、但内层未查」的状态
被收缩成更小的开区间 (L+1, R-1)
外层永远不回
```

两端不等：

```
整个 (0..n-1) 字符串状态死亡
内层所有格子全部不用看
```

```
        镜像对
    (0,n-1) 失败 => 整个上三角不必再证回文
```

### 6）最终代码

Python:

```python
def is_palindrome(s):
    left = 0
    right = len(s) - 1

    while left < right:
        while left < right and not s[left].isalnum():
            left += 1
        while left < right and not s[right].isalnum():
            right -= 1
        if s[left].lower() != s[right].lower():
            return False
        left += 1
        right -= 1

    return True
```

C++:

```cpp
#include <string>
#include <cctype>

bool isPalindrome(const std::string& s)
{
    int left = 0;
    int right = static_cast<int>(s.size()) - 1;

    while (left < right) {
        while (left < right && !std::isalnum(static_cast<unsigned char>(s[left])))
            ++left;
        while (left < right && !std::isalnum(static_cast<unsigned char>(s[right])))
            --right;
        if (std::tolower(static_cast<unsigned char>(s[left])) !=
            std::tolower(static_cast<unsigned char>(s[right])))
            return false;
        ++left;
        --right;
    }
    return true;
}
```

### 7）复杂度分析

每个字符最多被 left 或 right 看一次（跳过非字母数字也是前进）。
总移动 <= 2n。时间 O(n)，空间 O(1)。

### 8）这一题真正训练了什么

> 夹逼也可以发生在对称结构上。
> 单调性是：外层已决，内层成为唯一剩余搜索空间；外层失败则全域死亡。

---

# 左右夹逼总结

左右夹逼真正利用的是**有序性**（或与有序同构的「短板 + 宽度收缩」）。

```
有序 / 短板锁死
        |
        v
当前 (L, R) 对整行或整列宣判
        |
        v
被宣判的一侧永久收缩
        |
        v
不回头
        |
        v
O(n) 条判决吃掉 O(n^2) 个格子
```

什么情况下应该想到这一类：

1. 候选是一对端点 `(i, j)`
2. 数组有序，或目标被较短边/较外层锁死
3. 当前比较能同时否定一整行或一整列
4. 两端可以把自己的一侧扔掉，且以后不必回来
