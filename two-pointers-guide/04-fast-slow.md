# 快慢指针（Fast & Slow）

快慢指针不是「一个走两步、一个走一步」的口诀。

它是：在同一条轨迹上放两个位置 `(slow, fast)`，
利用**相对速度**让「距离 / 相位差」单调变化，从而在线性时间内读出中点、环、倒数第 k 个位置。

---

# 第一部分：它解决什么问题

适用于：

- 结构是链表、函数迭代链、隐式序列，只能 `next`，不能按下标随机访问
- 关心的是两个位置的**相对关系**：是否相遇、距离是否为 k、是否到中点
- 需要 O(1) 额外空间（不能先装进数组）

共同特点：

- 状态是一对位置 `(s, f)`
- 每一步 `f` 比 `s` 多走固定步数
- 相对距离随时间单调增加（无环），或在模环长意义下单调逼近相遇（有环）

为什么暴力一般是 O(n^2) 或 O(n) 空间：

- 找中点 / 倒数第 n：先数长度再走第二遍，或把节点放进数组
- 判环：用哈希记访问过的节点，空间 O(n)
- 暴力「对每个节点向后扫」会变成 O(n^2)

真正需要搜索的是：

> 轨迹上哪一对 `(s, f)` 满足几何关系（相遇、f 到尾且 s 在中、f 比 s 先行 n 步）。

---

# 第二部分：二维搜索空间

把节点按首次到达顺序编号 0..n-1（有环时轨迹是「一段链 + 一个环」）。

```
              fast -->
         0    1    2    3    4
      +----+----+----+----+----+
   0  |  * |  * |  * |  * |  * |
      +----+----+----+----+----+
   1  |    |  * |  * |  * |  * |
slow  +----+----+----+----+----+
   2  |    |    |  * |  * |  * |
      +----+----+----+----+----+

无环时 fast 永远在 slow 前面
合法区域是 f >= s 的上三角
```

暴力为什么要遍历：

若随意组合两个位置，有 O(n^2) 对。
例如「枚举每个节点当慢针，再找快针」完全没必要。

真正哪些区域不用搜索：

快慢指针只走一条**时间参数化的对角线**：

```
t = 0: (0, 0) 或 (0, 1)
t = 1: (1, 2)
t = 2: (2, 4)
t = 3: (3, 6)  有环则下标模环

不是扫整个三角形
只扫这条「相对速度为 1」的射线
```

```
slow
  ^
  |  空白 = 从不访问
  |      *
  |    *
  |  *
  +-----------> fast   (或时间 t)

只访问射线上 O(n) 个点
```

---

# 第三部分：单调性

真正单调的是**相对距离**（或环上相位差）。

无环链表：

```
每一步:
    slow += 1
    fast += 2
    distance = fast - slow  增加 1

距离只增不减。
因此:
    一旦 fast 到尾，distance 达到最大
    此时 slow 走过 floor(n/2)，即中点
    中间那些 (s, f) 不必单独枚举
```

```
节点:  A -> B -> C -> D -> E

t=0   s=A  f=A     dist=0
t=1   s=B  f=C     dist=1
t=2   s=C  f=E     dist=2   fast 到尾，s 在中点 C
```

有环：

```
进入环后，相位差 d = (fast - slow) mod cycle
每一步 d 增加 1 (因为相对速度 1)
环长 C，则至多 C 步后 d == 0，相遇

相位差在模 C 下单向扫过所有剩余
不会跳过相遇点
```

```
环:  0-1-2-3-0   C=4

设相遇前相位差:
    0 -> 1 -> 2 -> 3 -> 0(相遇)

每个剩余恰好出现一次
```

删除倒数第 n 个：

```
fast 先走 n 步
之后同步 +1
gap = n 保持不变（单调，常数）
fast 到尾时，slow 恰好在待删节点前
```

快乐数：

```
next(x) = 各位平方和
轨迹是隐式链表
有环 <=> 不是快乐数（除了环 {1} 或最终到 1）
相对距离/相位同样单调
```

哪些状态可以直接排除：

- 无环：所有 `fast - slow != 当前时间差` 的格子不在射线上，不看
- 有环：相遇前的相位都不是 0，不必用哈希记每一个节点
- 倒数第 n：gap != n 的同步对全部无用

---

# 第四部分：指针移动规则

典型规则：

```
slow 每次 1 步
fast 每次 2 步   (中点 / 环)
或
fast 先走 n 步，然后一起 1 步  (倒数第 n)
```

为什么只能这样移动：

相对速度必须是常数，距离/相位才单调且可计算。
找中点要求 `v_f = 2 v_s`，这样当 f 走完 n，s 走完 n/2。
判环要求相对速度与环长互素或至少能扫到 0；速度差为 1 最干净。

如果反方向移动会发生什么：

链表没有 `prev`（单向），「反方向」通常意味着：

```
让 fast 变慢、slow 变快:
    相对距离不再单调增加
    中点公式崩溃
    环上可能永远相位对不上，或对上的位置没有意义

让某一指针回头:
    单向链表做不到
    若用哈希回头，空间不再是 O(1)
```

为什么不会漏掉答案：

- 中点：数学恒等式 `steps_slow = floor(n/2)` 当 `fast` 耗尽
- 环：相对速度 1 在有限环上必扫到相位 0
- 环入口：相遇点到入口的距离 = 头到入口的距离（Floyd 标准等式）
- 倒数第 n：不变量 `fast` 比 `slow` 恰好超前 n

为什么以后永远不用回来：

轨迹是函数 `next` 决定的，时间向前。
已经走过的节点对「未来的相对几何」只作为历史，不需要退回去重测。
环检测里，没相遇的历史相位已经证明当时没环或还没套上。

---

# 第五部分：复杂度为什么下降

```
无环:
    fast 最多走 n 次 next
    slow 最多走 n/2 次
    总 next 次数 <= 1.5n

有环，环长 C，入环前长度 mu:
    相遇发生在 O(mu + C) 步内
    最坏仍是 O(n)

倒数第 n:
    fast 走 n + (L-n) = L
    slow 走 L-n
    合计 < 2L
```

对比：

```
哈希判环: 每个节点存一次，O(n) 空间
快慢指针: 只保留两个引用，O(1) 空间
         时间仍是线性，因为只沿一条射线走

暴力对每个节点扫后面: O(n^2) 对
快慢: 只访问射线上 O(n) 对
```

二维解释：

```
O(n^2) 个位置对
取一条斜率 2 的射线
射线上点的个数 = 时间步数 = O(n)
每个点 O(1) 推进
O(n)
```

---

# 第六部分：统一心智模型

快慢指针真正利用的是**相对速度**。

为什么成立：

```
同一条 next 轨迹
两个时钟速率不同
相对位置 = (v_f - v_s) * t

无环: 相对位置单调增加，直到碰到边界，读出比例位置
有环: 相对位置模 C 单调扫一圈，零点即可检测
固定间距: 相对位置保持常数，边界对齐给出倒数位置
```

不是技巧，是运动学：

> 你不需要看见整条链，只需要一个以已知相对速度运动的探针。

---

# 第七部分：完整例子

---

## 例子 1：找链表中点（LeetCode 876）

### 1）题目

给定单链表头，返回中间节点。偶数个时返回第二个中点。

示例：`1 -> 2 -> 3 -> 4 -> 5`，返回 3。
`1 -> 2 -> 3 -> 4 -> 5 -> 6`，返回 4。

### 2）为什么想到双指针

暴力：先走一遍数 n，再走 n/2 步。正确，但是两遍。

二维：所有 `(i, j)` 中我们只需要 `j == n-1` 且 `i == n/2`。
用相对速度 2:1，一遍同时完成「量长度」和「走到一半」。

单调性：`fast - slow` 每步 +1，fast 触尾时比例锁死。

### 3）画出搜索过程

奇数：

```
1 -> 2 -> 3 -> 4 -> 5

t=0  s=1  f=1
t=1  s=2  f=3
t=2  s=3  f=5   f.next==null 停，s=3 中点
```

偶数：

```
1 -> 2 -> 3 -> 4 -> 5 -> 6

t=0  s=1  f=1
t=1  s=2  f=3
t=2  s=3  f=5
t=3  s=4  f=null 的前一步: f=5 还能走两步到 null
循环条件用 f 与 f.next，停在 s=4
```

```
s     f
1  2  3  4  5  6     之后
   s     f
      s        f
         s           f=null
```

### 4）为什么移动这个指针

fast 必须两步：一步的话两人始终重合，读不出中点。
slow 必须一步：否则比例不是 1:2。

不能让 fast 回头：尾部已经给出长度信息，回头浪费且破坏「距离单调」。

不会漏：对任意 n，循环不变量是 `pos(fast) == min(n, 2*pos(slow))`（边界处停）。

### 5）状态剪枝

```
不检查 (1,2)(1,4)(2,3)... 等任意对
只检查射线上:
    (1,1) (2,3) (3,5) ...

其余 O(n^2) 格子全部 X
```

### 6）最终代码

Python:

```python
class ListNode:
    def __init__(self, val=0, next=None):
        self.val = val
        self.next = next


def middle_node(head):
    slow = head
    fast = head

    while fast and fast.next:
        slow = slow.next
        fast = fast.next.next

    return slow
```

C++:

```cpp
struct ListNode {
    int val;
    ListNode* next;
    ListNode(int x) : val(x), next(nullptr) {}
};

ListNode* middleNode(ListNode* head)
{
    ListNode* slow = head;
    ListNode* fast = head;

    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
    }
    return slow;
}
```

### 7）复杂度分析

fast 每次跳 2，走到尾约 n/2 次循环。循环内常数次指针赋值。
时间 O(n)，空间 O(1)。总指针移动约 1.5n 次 next。

### 8）这一题真正训练了什么

> 边界当尺子。相对速度把「长度」翻译成「位置」。
> 不必先知道 n。

---

## 例子 2：判断链表是否有环（LeetCode 141）

### 1）题目

链表是否存在环。

### 2）为什么想到双指针

暴力/哈希：访问过的节点放进 set，再次遇到则有环。空间 O(n)。

轨迹要么进 null，要么永远在环里转。
两个速度不同的探针：无环则 fast 先到 null；有环则相位差单调扫到 0。

### 3）画出搜索过程

无环：

```
1 -> 2 -> 3 -> null

s=1 f=1
s=2 f=3
f.next==null  返回 false
```

有环：`1 -> 2 -> 3 -> 4 -> 2`

```
1 -> 2 -> 3 -> 4
     ^         |
     +---------+

t=0  s=1  f=1
t=1  s=2  f=3
t=2  s=3  f=2
t=3  s=4  f=4   相遇
```

```
相遇在环上某点即可
不必是入口；入口留给下一题
```

### 4）为什么移动这个指针

fast 一步：永远跟在 slow 后重合或同速，进环后相位差不变，可能永远不相遇。

fast 三步：相对速度 2。若 `gcd(2, C) != 1` 且初始相位差不被 2 整除，可能错过。
速度差 1 最安全：任何环长都能遇上。

不会漏：有环必相遇；无环 fast 必撞 null。二分法，没有第三种终点。

### 5）状态剪枝

```
哈希方案访问的「节点集合」不必存
每个历史节点对应的 (s,f) 只要当时没相遇就可以丢

剪掉的是: 所有非射线上的位置对
以及「用空间记住走过的点」这一整维
```

### 6）最终代码

Python:

```python
def has_cycle(head):
    slow = head
    fast = head

    while fast and fast.next:
        slow = slow.next
        fast = fast.next.next
        if slow is fast:
            return True

    return False
```

C++:

```cpp
bool hasCycle(ListNode* head)
{
    ListNode* slow = head;
    ListNode* fast = head;

    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
        if (slow == fast)
            return true;
    }
    return false;
}
```

### 7）复杂度分析

无环 O(n)。有环 O(mu + C)。空间 O(1)。
相对 fast 指针，next 调用次数仍线性。

### 8）这一题真正训练了什么

> 环是周期。相对速度 1 是在周期上做一次完整扫描。
> 相遇 = 相位差归零，不是运气。

---

## 例子 3：环入口（LeetCode 142）

### 1）题目

有环则返回入环第一个节点，无环返回 null。

### 2）为什么想到双指针

哈希：第一次重复访问的节点即入口。O(n) 空间。

Floyd：先用快慢相遇证明有环，再把相对位置重置成「一头从 head、一头从相遇点，同速」。
等式：

```
mu = 入环前长度
C  = 环长
相遇时 slow 走了 mu + a
fast 走了 mu + a + kC
fast = 2 * slow  =>  mu + a = kC  =>  mu = kC - a

从 head 走 mu，从相遇点再走 C-a，会在入口碰头
```

单调性：第二阶段两人同速，距离入口的剩余步数同时减 1，直到 0。

### 3）画出搜索过程

具体链：`1 -> 2 -> 3 -> 4 -> 5 -> 3`（入口是 3，环长 3）

```
阶段1 快慢 (1 步 / 2 步)

t=0  s=1  f=1
t=1  s=2  f=3
t=2  s=3  f=5
t=3  s=4  f=4   相遇在 4

mu=2, C=3, a=1 (入口 3 走到相遇 4)
C-a = 2 = mu (mod C)
```

```
阶段2 同速

p 从 head    q 从相遇点 4
p=1          q=4
p=2          q=5
p=3          q=3   入口
```

```
抽象几何:

head -----mu----> entry --a--> meet
                    ^            |
                    +----(C-a)---+

p 从 head 走 mu
q 从 meet 走 C-a
在入口对齐
```

### 4）为什么移动这个指针

阶段 2 必须同速。若仍 2:1，走的步数不再等于 mu。

不能从相遇点倒走：单向链表没有倒。
公式已经把「倒走 a 步」翻译成「从 head 正走 mu 步」。

不会漏：代数对任意 k、C、mu 成立。无环在阶段 1 就返回。

### 5）状态剪枝

```
不枚举每个节点当候选入口再转一圈 (那是 O(n^2))
阶段1 剪掉「无环」或给出环上一点
阶段2 剪掉环上除入口外的所有点: 它们对不齐 head 出发的指针
```

```
候选入口 = 链上 mu 个点 + 环上 C 个点
只验证一条同步轨迹上的 O(mu) 个对齐点
最终只在入口相等
```

### 6）最终代码

Python:

```python
def detect_cycle(head):
    slow = head
    fast = head

    while fast and fast.next:
        slow = slow.next
        fast = fast.next.next
        if slow is fast:
            finder = head
            while finder is not slow:
                finder = finder.next
                slow = slow.next
            return finder

    return None
```

C++:

```cpp
ListNode* detectCycle(ListNode* head)
{
    ListNode* slow = head;
    ListNode* fast = head;

    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
        if (slow == fast) {
            ListNode* finder = head;
            while (finder != slow) {
                finder = finder->next;
                slow = slow->next;
            }
            return finder;
        }
    }
    return nullptr;
}
```

### 7）复杂度分析

阶段 1 O(n)，阶段 2 O(mu) <= O(n)。合计 O(n)，空间 O(1)。
两指针在两阶段中各自前进次数仍不超过 O(n)。

### 8）这一题真正训练了什么

> 相对速度先用来「发现周期」，再换成同速用来「对齐起点」。
> 环入口是运动学等式，不是再扫一遍哈希。

---

## 例子 4：删除倒数第 N 个节点（LeetCode 19）

### 1）题目

删除单链表倒数第 n 个节点，返回新头。n 保证合法。

示例：`1->2->3->4->5`, n=2，删 4，得到 `1->2->3->5`。

### 2）为什么想到双指针

暴力：数长度 L，再走 L-n。两遍。

要搜的位置对是：`(slow, fast)` 且 `fast` 超前 n，`fast` 到尾。
固定间距 n 是单调的（常数）。一遍即可。

### 3）画出搜索过程

用 dummy 处理删头。

```
d -> 1 -> 2 -> 3 -> 4 -> 5    n=2

fast 先走 2 步:
s=d
f=2
```

```
一起走:
s=1  f=3
s=2  f=4
s=3  f=5
s=4? f=null 停在 f 到 null 前: 当 f.next==null，s=3
s.next 是 4，删除
```

```
d  1  2  3  4  5
s        f         先拉开 n
      s        f
         s        f=5
s 停在 3，删 4
```

### 4）为什么移动这个指针

先拉开 n：否则间距不是 n。
之后必须同速：间距才能保持。
若 fast 继续两步，间距增大，停在尾时 slow 不在倒数 n+1。

不会漏：不变量 `fast` 比 `slow` 多 n 个节点（或到尾的距离差为 n）。
dummy 保证删头时 slow 仍是「前驱」。

### 5）状态剪枝

```
不枚举每个节点数后面还有几个 (每个 O(n) 共 O(n^2))
只保留间距恰好为 n 的对角线

    fast = slow + n
    这条对角线上走到 fast 越界
```

```
其它间距的 (s,f) 全部 X
```

### 6）最终代码

Python:

```python
def remove_nth_from_end(head, n):
    dummy = ListNode(0, head)
    slow = dummy
    fast = dummy

    for _ in range(n):
        fast = fast.next

    while fast.next:
        slow = slow.next
        fast = fast.next

    slow.next = slow.next.next
    return dummy.next
```

C++:

```cpp
ListNode* removeNthFromEnd(ListNode* head, int n)
{
    ListNode dummy(0);
    dummy.next = head;
    ListNode* slow = &dummy;
    ListNode* fast = &dummy;

    for (int i = 0; i < n; ++i)
        fast = fast->next;

    while (fast->next) {
        slow = slow->next;
        fast = fast->next;
    }

    ListNode* doomed = slow->next;
    slow->next = doomed->next;
    return dummy.next;
}
```

### 7）复杂度分析

fast 从 dummy 走到尾，约 n + (L-n+1) 步；slow 走 L-n+1 步。
总移动 < 2(L+1)。时间 O(L)，空间 O(1)。

### 8）这一题真正训练了什么

> 固定相对距离，用边界对齐读出绝对位置。
> 「倒数」被翻译成「领先 n 步的那个探针先撞墙」。

---

## 例子 5：快乐数（LeetCode 202）

### 1）题目

重复把各位数字平方求和。若最终到 1 则为快乐数。否则会循环。

示例：19 -> 82 -> 68 -> 100 -> 1，是。

### 2）为什么想到双指针

每个数的下一步唯一，构成隐式链表。
值域有限（例如任意数很快掉进有限平方和集合），故必进环。
环是 `{1}` 或某个不含 1 的环。
判环问题，不必哈希存全部中间值。

单调性：与链表判环相同，相位差在环上单向扫。

### 3）画出搜索过程

19：

```
s=19  f=19
s=82  f=68
s=68  f=1
s=100 f=1
s=1   f=1  相遇在 1  快乐
```

非快乐，例如 2：

```
2 -> 4 -> 16 -> 37 -> 58 -> 89 -> 145 -> 42 -> 20 -> 4 -> ...
快慢会在 4 或环上某点相遇，相遇值 != 1
```

```
slow 每次一步 next
fast 每次两步 next
next(x) = 各位平方和
```

### 4）为什么移动这个指针

与判环完全相同。
不能只跑一个指针等看到 1：非快乐会无限转，必须用相遇或哈希终止。
快慢用相遇终止，且不存集合。

不会漏：有限图上必进环；1 的环是自环 1->1。
相遇值是 1 则为快乐，否则不是。

### 5）状态剪枝

```
不存所有出现过的数字 (哈希集那一维)
不枚举所有 (a,b) 数对看是否互相到达

只沿 next 的 2 倍速射线
碰到相等就停
```

### 6）最终代码

Python:

```python
def next_square_sum(n):
    total = 0
    while n:
        digit = n % 10
        total += digit * digit
        n //= 10
    return total


def is_happy(n):
    slow = n
    fast = n

    while True:
        slow = next_square_sum(slow)
        fast = next_square_sum(next_square_sum(fast))
        if slow == fast:
            return slow == 1
```

C++:

```cpp
int nextSquareSum(int n)
{
    int total = 0;
    while (n) {
        int digit = n % 10;
        total += digit * digit;
        n /= 10;
    }
    return total;
}

bool isHappy(int n)
{
    int slow = n;
    int fast = n;

    do {
        slow = nextSquareSum(slow);
        fast = nextSquareSum(nextSquareSum(fast));
    } while (slow != fast);

    return slow == 1;
}
```

### 7）复杂度分析

数字很快降到 3 位数以内，状态数常数级。
形式上对「状态图大小 S」是 O(S)，与哈希同阶时间，空间 O(1)。
两指针在图上各走 O(S) 步。

### 8）这一题真正训练了什么

> 快慢指针不依赖 ListNode。
> 任何「单后继函数」都是隐式链表。相对速度照样判环。

---

# 快慢指针总结

快慢指针真正利用的是**相对速度**。

```
同一条 next 轨迹
        |
        v
两个速率
        |
        v
相对距离或相位单调变化
        |
        v
边界或零相位给出答案
        |
        v
只走一条射线，O(n) 时间 O(1) 空间
```

什么情况下应该想到这一类：

1. 只能沿后继走，不能按下标跳
2. 问题是中点、环、固定间距、周期
3. 不想用 O(n) 表记住历史
4. 能写出相对速度与答案的等式
