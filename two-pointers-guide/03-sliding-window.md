# 滑动窗口（Sliding Window）

滑动窗口不是「一个窗口在数组上滑」。

它是：在以 `(left, right)` 为坐标的上三角网格里，
利用**窗口性质随扩张/收缩单向变化**，让 left 永远不回头。

---

# 第一部分：它解决什么问题

适用于：

- 答案是一段**连续**子数组 / 子串
- 窗口满足某种约束（无重复、覆盖某集合、和 >= s、种类 <= k）
- 固定右端时，合法左端形成一个前缀或后缀区间

共同特点：

- 候选是所有 `0 <= L <= R < n` 的区间 `[L, R]`
- 右端扩张：约束变紧（和变大、种类变多、重复可能出现、覆盖更完整）
- 左端收缩：约束变松

为什么暴力一般是 O(n^2)：

每个区间都是一个格子。没有单调性时，必须检查全部 `n(n+1)/2` 段。

真正需要搜索的是：

> 哪些 `[L, R]` 合法，以及其中最长 / 最短 / 和最大的那一段。

---

# 第二部分：二维搜索空间

```
              right -->
         0    1    2    3    4
      +----+----+----+----+----+
   0  |  * |  * |  * |  * |  * |
      +----+----+----+----+----+
   1  |    |  * |  * |  * |  * |
left  +----+----+----+----+----+
   2  |    |    |  * |  * |  * |
      +----+----+----+----+----+
   3  |    |    |    |  * |  * |
      +----+----+----+----+----+
   4  |    |    |    |    |  * |
      +----+----+----+----+----+

格子 (L, R) = 子数组 a[L..R]
```

暴力为什么要遍历整个空间：

每个子数组的统计量（是否重复、是否覆盖、和）看起来彼此独立。

真正哪些区域不用搜索：

若约束对扩张单调，则对固定 R，合法的 L 往往是：

```
L 太小: 窗口太大 / 太脏 / 种类过多  -> 非法
L 合适: 恰好合法
L 太大: 仍然合法，但更短（最短类问题里更差；最长类问题里不必看）
```

于是每一列（固定 R）不必扫全部 L，只需维护「这一列里最左或最右的临界 L」。

```
固定 right=3

L=0  非法 (太长或已脏)
L=1  非法
L=2  临界合法     <-- 只需知道这个边界
L=3  合法但更短
```

---

# 第三部分：单调性

这是滑动窗口的心脏。

真正单调的不是数组有序（多数窗口题数组无序），而是：

```
固定左端 L，右端 R 增大:
    元素只进不出
    和增大（正数数组）
    字符种类不减
    覆盖程度不减
    「无重复」只可能从真变假，不会从假变真

固定右端 R，左端 L 增大:
    元素只出不进
    上述量反向变化
```

图（正数数组，求最短子数组和 >= s）：

```
a = 2 3 1 2 4 3     s = 7

窗口 [0, 3] = 2+3+1+2 = 8 >= 7  合法

因为全是正数:
    再往右扩，和更大，仍合法，但更长 -> 最短问题里不必扩了再看更左
    往左缩，和变小，可能变成非法

    [0,3] 合法  => 所有 [0, R] (R>3) 都合法且更长
                  对「最短」这一目标，整行 left=0、right>3 死亡
```

图（无重复最长子串）：

```
一旦窗口里出现重复:
    包含这个重复的所有更左起点都脏
    必须把 left 推过「上一次出现该字符的位置」
    推过之后，更左的那些 [L, R] 对当前 R 全部死亡
    而且对未来更大的 R 也死亡: 它们仍然包含那段重复
```

哪些状态可以直接排除：

- 已经非法的左端，右端再增大只会更非法（对「收缩才能变合法」的约束）
- 已经合法且足够长的窗口，再扩张对「最短」无用
- 已经丢掉的左端字符，以后不会因为右端增长而重新变得必要（对无重复 / 种类上限）

例外要清醒：

若数组有负数，「和随窗口扩大而增大」为假，不能滑窗求最小和子数组。
单调性破了，就必须回退或换算法（前缀和 + 单调队列等）。

---

# 第四部分：指针移动规则

标准骨架：

```
right 从 0 走到 n-1: 只负责扩张，把 a[right] 纳入统计
当窗口不满足约束: left 向右收缩，直到约束恢复
在「窗口合法」时更新答案
```

为什么只能这样移动：

`right` 是时间箭头。每个右端只出现一次。
`left` 是「当前右端下，最左还能活的起点」。
约束被破坏时，所有 `L <= left` 对这个 R 都已死，left 必须前进。

如果反方向移动会发生什么：

```
right 回退:
    把已经纳入的元素吐出去
    这些元素对未来更大右端通常仍然需要被考虑
    变成 O(n^2) 的反复进出

left 回退:
    把已经证明脏的前缀重新放回窗口
    脏前缀对更大的 right 更脏
    破坏「已排除区域永不复活」
```

为什么不会漏掉答案：

对每个 R，算法停在「这个 R 对应的临界 L」上。

- 最长类：临界 L 是仍合法的最左起点，`[L, R]` 就是以 R 结尾的最长合法段
- 最短类：先扩到合法，再缩到刚合法，`[L, R]` 就是以 R 结尾的最短合法段
- 覆盖类：缩到「再缩就覆盖失败」的最左或记录最小长度

所有区间要么被这个临界代表，要么比它更差（更短/更长/不合法）。

为什么以后永远不用回来：

```
不变量:
    [0, left) 里的每个起点
    对当前 right 以及所有更大的 right
    都已经不合法（或对目标更差）

证明靠单调性:
    更大的 right 只往窗口里加东西
    不会让一个已经过脏 / 过长的左端重新变好
```

---

# 第五部分：复杂度为什么下降

```
right 从 0 到 n-1，只增，n 步
left  从 0 到 n-1，只增，最多 n 步
每个元素至多「进入窗口一次 + 离开窗口一次」
总移动 <= 2n
若进出时更新哈希/计数为 O(1)，总时间 O(n)
```

从二维空间看：

```
暴力: 每个 (L,R) 都算一遍统计量，O(n^2) 或更差

滑窗: 只沿着一条「right 递增、left 不减」的前沿走

    left
      ^
      |         已死 (太左, 已脏)
      |    +----+----+
      |    |死  |前沿|
      |    +----+----+
      |         |还活| 更短的合法段
      +-----------------> right

前沿长度 O(n)
每个格子的统计量从相邻格子 O(1) 继承
```

---

# 第六部分：统一心智模型

滑动窗口真正利用的是：

> **窗口性质对「加入右端 / 删除左端」单调变化，因此过期的左端永远过期。**

为什么成立：

```
元素只从右进、从左出
统计量存在可增量维护的状态 S
S 随扩张单向变紧，随收缩单向变松

于是:
    一旦 S 对某个 left 过紧
    对更大的 right，同一个 left 只会更紧
    left 必须永久右移
```

窗口可以一直滑，是因为时间只向前：
right 是扫描时钟，left 是过期时钟。
两个时钟都不倒拨。

left 必须移动的时刻：

> 当前窗口已经违反约束，且再留着这个左端，未来只会继续违反。

---

# 第七部分：完整例子

---

## 例子 1：最长无重复子串（LeetCode 3）

### 1）题目

给定字符串，找不含重复字符的最长子串长度。

示例：`"abcabcbb"`，答案 3（`"abc"`）

### 2）为什么想到双指针

暴力：枚举所有 `[L, R]`，再用集合判重，O(n^2) 或 O(n^3)。

子串连续。固定 R，若 `[L, R]` 有重复，则所有 `L' <= L` 配这个 R 都有重复。
无重复对扩张单调：只能从真变假。

### 3）画出搜索过程

```
s = a b c a b c b b

R=0  a
[a] 无重复  len=1
```

```
R=1  a b
[a b] 无重复  len=2
```

```
R=2  a b c
[a b c] 无重复  len=3
```

```
R=3  a b c a
         L     R
a 进入时窗口已有 a
left 必须推过上一个 a
窗口变成 [b c a]  len=3
```

```
R=4  b c a b
left 推过上一个 b
窗口 [c a b]
```

```
R=6  ... c b
窗口缩到 [c b] 再 [b]
R=7  第二个 b
窗口缩到 [b]
最长仍是 3
```

逐步指针：

```
L
a b c a b c b b
    R
无重复, R++
```

```
L
a b c a b c b b
      R
s[R]=a 重复 => L++ 直到丢掉旧 a
```

```
    L
a b c a b c b b
      R
恢复无重复
```

### 4）为什么移动这个指针

出现重复时必须动 left，不能动 right 回退。

为什么不能只把 right 停下换别的：
right 是「以该位置结尾」的所有子串的共同右端。
以 R 结尾的最长无重复串，左端必须严格在「每个字符上次出现位置」的右侧。

不会漏解：以每个 R 结尾的最长合法串都被算过。全局最长是这些值的 max。

### 5）状态剪枝

R=3 时 left 从 0 移到 1，剪掉：

```
(0, 3) (0, 4) (0, 5) ... (0, n-1)

以及任何以 index0 的 a 为左端、且包含新 a 的区间

它们都含两个 a，永远非法
```

```
        R=3
L=0  XXXXXXXX  整行死亡
L=1  .*****    还活
```

### 6）最终代码

Python:

```python
def length_of_longest_substring(s):
    last_index = {}
    left = 0
    best = 0

    for right, ch in enumerate(s):
        if ch in last_index and last_index[ch] >= left:
            left = last_index[ch] + 1
        last_index[ch] = right
        best = max(best, right - left + 1)

    return best
```

C++:

```cpp
#include <string>
#include <vector>
#include <algorithm>

int lengthOfLongestSubstring(const std::string& s)
{
    std::vector<int> last_index(256, -1);
    int left = 0;
    int best = 0;

    for (int right = 0; right < static_cast<int>(s.size()); ++right) {
        unsigned char ch = static_cast<unsigned char>(s[right]);
        if (last_index[ch] >= left)
            left = last_index[ch] + 1;
        last_index[ch] = right;
        best = std::max(best, right - left + 1);
    }
    return best;
}
```

### 7）复杂度分析

`right` 走 n 步。`left` 只跳不减，累计移动 <= n。
哈希/数组更新 O(1)。时间 O(n)，空间 O(字符集)。

### 8）这一题真正训练了什么

> 固定右端，脏左端对未来所有更大右端继续脏。
> left 一次跳过整段死亡起点。

---

## 例子 2：最小覆盖子串（LeetCode 76）

### 1）题目

在 `s` 中找覆盖 `t` 全部字符（含重复次数）的最短子串。没有则返回空。

示例：`s = "ADOBECODEBANC"`, `t = "ABC"`，答案 `"BANC"`

### 2）为什么想到双指针

暴力：所有子串检查是否覆盖 t，O(n^2 * |sigma|)。

覆盖对扩张单调：窗口变大，覆盖只可能从不完整变完整，不会相反。
因此：right 负责凑齐，left 负责丢掉多余。
对每个使窗口刚覆盖的 R，存在最短左端。更左的更长，更右的可能不再覆盖。

### 3）画出搜索过程

```
t 需要: A:1 B:1 C:1

s: A D O B E C O D E B A N C
   L
   R  纳入 A  还缺 B C
```

```
A D O B E C ...
L         R
纳入 C 后 missing=0  第一次覆盖 "ADOBEC"
```

```
尝试 L++:
D O B E C  丢掉 A，missing>0 停止回退? 
不回退 L。继续? 不，先记录再收缩到刚好破坏，然后 R 继续。
```

正确过程：

```
覆盖后收缩:
A D O B E C
L         R  记录长度 6
L++ 丢掉 A，不再覆盖，停止收缩
```

```
          继续 R 走到第二个 B, A, N, C
... B A N C
    L     R  再次覆盖
收缩: 丢掉 B 左边的无用字符，得到 B A N C
再缩丢掉 B -> A N C 仍覆盖? A N C 含 A,N,C 缺 B，停止
记录 "BANC" 长度 4
```

### 4）为什么移动这个指针

未覆盖时只能 `right++`。动 left 会丢掉已有字符，覆盖更差。

已覆盖时必须尝试 `left++`：左边多出来的字符不贡献覆盖，只会让答案更长。

不能 left 回退：被丢掉的左端在「当时已经覆盖」的前提下是多余的；
以后 right 增大，那个左端仍然多余，或即使需要也会被新的同样字符替代。
（计数窗口保证：只有当某个必需字符计数降到需求以下，才停止收缩。）

### 5）状态剪枝

第一次覆盖后 `left` 离开 A：

```
所有以 index0 为左端、right >= 当前的区间
都覆盖且比更短的覆盖串更长
对「最短覆盖」整行死亡
```

```
(0, 所有 R)
在最短目标下全部更差
```

### 6）最终代码

Python:

```python
from collections import Counter

def min_window(s, t):
    if not t or not s:
        return ""

    need = Counter(t)
    missing = len(t)
    left = 0
    best_left = 0
    best_len = float("inf")

    for right, ch in enumerate(s):
        if need[ch] > 0:
            missing -= 1
        need[ch] -= 1

        while missing == 0:
            if right - left + 1 < best_len:
                best_len = right - left + 1
                best_left = left
            leaving = s[left]
            need[leaving] += 1
            if need[leaving] > 0:
                missing += 1
            left += 1

    if best_len == float("inf"):
        return ""
    return s[best_left:best_left + best_len]
```

C++:

```cpp
#include <string>
#include <vector>
#include <limits>

std::string minWindow(const std::string& s, const std::string& t)
{
    if (s.empty() || t.empty())
        return "";

    std::vector<int> need(256, 0);
    for (unsigned char ch : t)
        ++need[ch];

    int missing = static_cast<int>(t.size());
    int left = 0;
    int best_left = 0;
    int best_len = std::numeric_limits<int>::max();

    for (int right = 0; right < static_cast<int>(s.size()); ++right) {
        unsigned char ch = static_cast<unsigned char>(s[right]);
        if (need[ch] > 0)
            --missing;
        --need[ch];

        while (missing == 0) {
            if (right - left + 1 < best_len) {
                best_len = right - left + 1;
                best_left = left;
            }
            unsigned char leaving = static_cast<unsigned char>(s[left]);
            ++need[leaving];
            if (need[leaving] > 0)
                ++missing;
            ++left;
        }
    }

    if (best_len == std::numeric_limits<int>::max())
        return "";
    return s.substr(best_left, best_len);
}
```

### 7）复杂度分析

每个下标：right 纳入一次，left 最多弹出一次。时间 O(|s| + |t|)，空间 O(字符集)。

### 8）这一题真正训练了什么

> 先扩到「约束满足」，再缩到「再缩就破坏」。
> 两个阶段对应二维网格里先向下吃一列，再把过长的上端砍掉。

---

## 例子 3：长度最小子数组（LeetCode 209）

### 1）题目

正整数数组 `nums` 和 `target`，找和 >= target 的最短连续子数组长度。不存在返回 0。

示例：`target = 7`, `nums = [2,3,1,2,4,3]`，答案 2（`[4,3]`）

### 2）为什么想到双指针

暴力：所有子数组求和，O(n^2)。

全是正数：固定 L，R 增大则和增大；固定 R，L 增大则和减小。
这是窗口和的单调性。

### 3）画出搜索过程

```
2 3 1 2 4 3    target=7

L
2 3 1 2 4 3
      R
和=8 >=7  记录长度 4
```

```
L++ 丢掉 2，和=6 <7  停止收缩
  L
2 3 1 2 4 3
      R
```

```
R++ 纳入 4，和=10
  L
2 3 1 2 4 3
        R
收缩: 丢掉 3，和=7  长度 3
再丢 1，和=6 <7 停
```

```
      L
2 3 1 2 4 3
          R 纳入 3，和=9
收缩丢掉 2，和=7  长度 2  最优
```

### 4）为什么移动这个指针

和 < target：只能 right++。left++ 让和更小。

和 >= target：必须 left++。右边继续扩只会更长，对最短无益；
先把当前右端下的最短合法段结算掉。

正数保证：丢掉左端后，若和仍 >= target，更左的那些更长段不必保留。

### 5）状态剪枝

第一次合法后丢掉开头的 2：

```
(0, 3) 已记录
(0, 4) (0, 5) 更长，对最短目标死亡
整行 left=0 死亡
```

```
        0 1 2 3 4 5
L=0     . . . * X X   *已看 X更差
L=1         . . . .
```

### 6）最终代码

Python:

```python
def min_sub_array_len(target, nums):
    left = 0
    window_sum = 0
    best = float("inf")

    for right, value in enumerate(nums):
        window_sum += value
        while window_sum >= target:
            best = min(best, right - left + 1)
            window_sum -= nums[left]
            left += 1

    return 0 if best == float("inf") else best
```

C++:

```cpp
#include <vector>
#include <algorithm>
#include <limits>

int minSubArrayLen(int target, const std::vector<int>& nums)
{
    int left = 0;
    long long window_sum = 0;
    int best = std::numeric_limits<int>::max();

    for (int right = 0; right < static_cast<int>(nums.size()); ++right) {
        window_sum += nums[right];
        while (window_sum >= target) {
            best = std::min(best, right - left + 1);
            window_sum -= nums[left];
            ++left;
        }
    }
    return best == std::numeric_limits<int>::max() ? 0 : best;
}
```

### 7）复杂度分析

每个元素进入 `window_sum` 一次、被减去一次。时间 O(n)，空间 O(1)。
若有负数，while 收缩会推错 left，O(n) 保证立刻失效。

### 8）这一题真正训练了什么

> 正数使「和」成为窗口上的单调势函数。
> 最短合法段 = 每个右端上，能收缩到的最右 left。

---

## 例子 4：水果成篮（LeetCode 904）

### 1）题目

数组 `fruits[i]` 是树的种类。从任意位置向右走，篮子只能装 2 种水果。
求最多能摘的连续棵数。

示例：`[1,2,1,2,3]`，答案 4（`[1,2,1,2]`）

### 2）为什么想到双指针

本质：最长子数组，最多包含 2 个不同值。

暴力：枚举区间数种类，O(n^2)。

种类数随扩张不减、随收缩不增。种类 > 2 时，过左的起点对更大右端种类更多，必须推进 left。

### 3）画出搜索过程

```
1 2 1 2 3

L
1
种类={1}
```

```
L
1 2
种类={1,2}  合法  len=2
```

```
L
1 2 1 2
种类仍 2  len=4
```

```
L
1 2 1 2 3
        R
种类=3  非法
left 右移直到丢掉种类 1 的全部
窗口变成 [2,3] 或更精确: 从最后一个使种类回到 2 的位置
```

```
        L R
1 2 1 2 3
种类={2,3} len=2
最优 4
```

### 4）为什么移动这个指针

种类 <= 2：只扩 right，更新最长。

种类 > 2：必须 left++。
不能 right--：那是放弃「以当前 R 结尾」的候选。
以 R 结尾的合法段一定存在（至少单元素），需要的是把左端推到只剩 2 种。

不会漏：每个 R 都得到以它结尾的最长「至多 2 种」段。

### 5）状态剪枝

纳入 3 后 left 推过前面的 1：

```
(0, 4) (1, 4) ... 所有仍包含两种旧水果又加上 3 的区间
种类 >= 3，对当前及未来死亡
```

```
整段起点还停在「第一种水果」的那些格子全部 X
```

### 6）最终代码

Python:

```python
from collections import defaultdict

def total_fruit(fruits):
    count = defaultdict(int)
    kinds = 0
    left = 0
    best = 0

    for right, fruit in enumerate(fruits):
        if count[fruit] == 0:
            kinds += 1
        count[fruit] += 1

        while kinds > 2:
            count[fruits[left]] -= 1
            if count[fruits[left]] == 0:
                kinds -= 1
            left += 1

        best = max(best, right - left + 1)

    return best
```

C++:

```cpp
#include <vector>
#include <unordered_map>
#include <algorithm>

int totalFruit(const std::vector<int>& fruits)
{
    std::unordered_map<int, int> count;
    int left = 0;
    int best = 0;

    for (int right = 0; right < static_cast<int>(fruits.size()); ++right) {
        ++count[fruits[right]];
        while (static_cast<int>(count.size()) > 2) {
            --count[fruits[left]];
            if (count[fruits[left]] == 0)
                count.erase(fruits[left]);
            ++left;
        }
        best = std::max(best, right - left + 1);
    }
    return best;
}
```

### 7）复杂度分析

每个下标进出哈希各一次。平均 O(n)，最坏视哈希。种类上限是 2，也可两变量替代哈希，严格 O(n)。

### 8）这一题真正训练了什么

> 「至多 K 种」是覆盖问题的对偶：扩张使种类上升，超标就永久淘汰左端。
> 水果成篮只是 K=2 的最长合法窗口。

---

## 例子 5：固定长度最大和（LeetCode 643 最大平均的内核）

### 1）题目

求长度为 k 的连续子数组的最大和。（643 是最大平均，和最大即可。）

示例：`nums = [1,12,-5,-6,50,3]`, `k = 4`
窗口和：`1+12-5-6=2`，`12-5-6+50=51`，`-5-6+50+3=42`，最大 51。

### 2）为什么想到双指针

暴力：每个起点求和 k 个，O(nk)。

窗口长度固定：右端进一步，左端必须同步进一步。
这是滑动窗口的退化：约束不是「合法/非法」，而是「长度必须 = k」。
长度对扩张/收缩显然单调。

### 3）画出搜索过程

```
k=4
1  12  -5  -6  50  3

L          R
和=2  记录 2
```

```
   L           R
1  12  -5  -6  50  3
丢掉 1，加入 50，和=2-1+50=51
```

```
       L           R
1  12  -5  -6  50  3
丢掉 12，加入 3，和=51-12+3=42
最大 51
```

### 4）为什么移动这个指针

长度已经是 k 时，若只 `right++`，长度变成 k+1，离开可行集。
必须同时 `left++`，才能留在长度 = k 的对角线上。

```
可行集不是上三角全部，而是一条对角线:

    R - L + 1 = k
    即 R = L + k - 1
```

不能 left 不动：那是在搜变长窗口。
不能 left 回退：更早的窗口已经算过。

### 5）状态剪枝

每次平移剪掉：

```
旧窗口 [L, R] 整个格子已经结算
下一个只看 [L+1, R+1]

所有长度 != k 的格子本来就不在本题搜索空间里
所有已滑过的长度 = k 格子不再访问
```

```
        right
      0 1 2 3 4 5
    +------------
  0 |       *
  1 |         *
  2 |           *
left
只走这一条对角，不看其余 O(n^2) 格子
```

### 6）最终代码

Python:

```python
def max_sum_fixed_length(nums, k):
    window_sum = sum(nums[:k])
    best = window_sum

    for right in range(k, len(nums)):
        window_sum += nums[right]
        window_sum -= nums[right - k]
        if window_sum > best:
            best = window_sum

    return best
```

C++:

```cpp
#include <vector>
#include <algorithm>

long long maxSumFixedLength(const std::vector<int>& nums, int k)
{
    long long window_sum = 0;
    for (int i = 0; i < k; ++i)
        window_sum += nums[i];

    long long best = window_sum;
    for (int right = k; right < static_cast<int>(nums.size()); ++right) {
        window_sum += nums[right];
        window_sum -= nums[right - k];
        best = std::max(best, window_sum);
    }
    return best;
}
```

### 7）复杂度分析

先算第一个窗口 O(k)，之后每次 O(1) 平移 n-k 次。时间 O(n)，空间 O(1)。
两个端点始终锁定 `right = left + k - 1`，各走 n-k 步，总移动 < 2n。

### 8）这一题真正训练了什么

> 固定长度 = 搜索空间从三角形塌成一条对角线。
> 仍然是双指针：一端进、一端出，统计量增量更新，绝不回退。

---

# 滑动窗口总结

窗口为什么可以一直滑：

> 右端是时间。时间不可逆。左端是「对当前时间已经过期的起点」。
> 过期对未来时间仍然过期。

什么时候 left 必须移动：

> 窗口性质已经违反约束（重复、未覆盖却要缩最短、和已够、种类超标、长度超过 k）。
> 并且单调性保证：不移 left，未来只会更违反，或对目标更差。

```
连续区间
    +
性质对扩张/收缩单调
    +
left 过期永不复活
    =
滑动窗口
```
