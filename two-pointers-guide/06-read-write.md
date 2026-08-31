# 读写指针（Read / Write）

读写指针不是「一个读、一个写」的命名游戏。

它是：在「源下标 × 已完成前缀长度」这个空间里，
利用**已处理区间已经定型**，让 write 永远停在答案前缀的右端，read 只向前供货。

---

# 第一部分：它解决什么问题

适用于：

- 原地改写数组 / 字符串
- 过滤、压缩、去重、分区（把满足谓词的元素稳定地收到前面）
- 额外空间要求 O(1)，不能另开数组再拷回去

共同特点：

- 两个角色：`read` 扫描全集，`write` 指向下一个该写入的位置
- 不变量：`[0, write)` 已经是最终答案的前缀
- `read >= write`，未读区域与已写区域不相交（或只在当前格子重叠）

为什么暴力一般是 O(n^2)：

每删一个元素就 `memmove` 后面整段，共删 k 次则 O(nk)。
或对每个位置向左扫「该放谁」，反复回看。

真正需要搜索的是：

> 哪些源位置的值应该被保留，按原序写到哪些目标位置。
> 映射是单调的：第 k 个保留值写到下标 k。

---

# 第二部分：二维搜索空间

```
              read -->
         0    1    2    3    4
      +----+----+----+----+----+
   0  |  * |  * |  * |  * |  * |
      +----+----+----+----+----+
   1  |    |  * |  * |  * |  * |
write +----+----+----+----+----+
   2  |    |    |  * |  * |  * |
      +----+----+----+----+----+

格子 (w, r) = 「已经写好 w 个，正在看 r」
合法区: r >= w
```

暴力为什么要遍历：

若每次删除都整体搬移，相当于对很多 `(w, r)` 重复拷贝。
若没有「前缀已完成」不变量，可能回头改已经写好的区。

真正哪些区域不用搜索：

算法只走一条 `read` 每次 +1、`write` 偶尔 +1 的路径。

```
write 永不减
read  永不减
路径在上三角里向右、偶尔向上

    w
    ^    *
    |   *
    |  * *
    | *
    +-------> r
```

已写前缀对应的所有「再回头改 [0,w)」格子全部死亡。

---

# 第三部分：单调性

真正单调的是：

```
已处理前缀的正确性只增不减。

一旦 nums[0..write) 满足:
    - 都是该保留的
    - 相对顺序正确
    - 是目前能确定的最长答案前缀

那么未来任何 read:
    不会发现一个「应该插进前缀中间」的值
    （过滤/去重/压缩都保持稳定相对序）
    因此 write 只需在尾部追加，绝不回写
```

图（Move Zeroes）：

```
0 1 0 3 12

非零必须保持相对序，零压到末尾。
第 k 个非零的最终下标就是 k。
这个赋值是单调的: k 只增。
```

```
read 看到 1，它是第 0 个非零 -> 写到 write=0
read 看到 3，它是第 1 个非零 -> 写到 write=1
不会出现「3 其实该在 1 前面」
因为原相对序已经决定了名次
```

哪些状态可以直接排除：

- `[0, write)` 里每一个位置的最终值（或最终「非零前缀」）已冻结
- 被 read 跳过的值（该删的、该压缩掉的重复）对任何更小的 write 都不必再考虑
- `write > read` 的格子非法，根本不进

为什么：稳定过滤是顺序同态。第 k 个幸存者的名次不依赖后面还没看见的幸存者内容，只依赖「到目前为止数了几个幸存者」。看见新幸存者时名次已经确定。

---

# 第四部分：指针移动规则

```
每一轮 read += 1
若 a[read] 应该保留:
    a[write] = a[read]
    write += 1
否则:
    write 不动
```

为什么只能这样移动：

`read` 必须覆盖每一个源元素，否则漏保留或漏删除。
`write` 只有在确认一个幸存者时才前进，否则答案前缀会掺进垃圾。

如果反方向移动会发生什么：

```
write--:
    撕掉已经正确的答案前缀
    后面还得重写，复杂度回到反复 memmove

read--:
    重复处理已判定元素
    若再写入，会打乱 write 名次
    可能死循环

read 跳着走但不按谓词:
    漏掉幸存者，或把不该留的留下
```

为什么不会漏掉答案：

每个源下标恰好被 read 访问一次。
谓词为真则按出现顺序获得写名次。
稳定性和「数到第 k 个」一一对应。

为什么以后永远不用回来：

```
不变量 I:
    [0, write)  = 已扫描前缀 [0, read) 中全部幸存者，且顺序正确
    [write, read) = 已扫描但被丢弃的空洞（或待填的垃圾）

read 前进时，新元素要么追加到 write，要么丢弃。
两种都保持 I。
没有任何操作需要破坏 [0, write)。
```

---

# 第五部分：复杂度为什么下降

```
read 从 0 到 n-1，恰好 n 次
write 从 0 到最多 n，最多 n 次
总移动 <= 2n
每次判定 O(1)
时间 O(n)
对比: 每次删除 memmove 是 O(n)，最坏 O(n^2)
```

从二维空间看：

```
暴力删除:
    每删一次，右边每个元素都搬家
    等于反复访问很多 (目标位置, 源位置)

读写指针:
    每个源位置只读一次
    每个目标位置只写一次
    路径长度 O(n)
```

```
O(n^2) 次搬动
        |
        |  前缀冻结，不再搬已经对的东西
        v
每个元素至多搬一次
        |
        v
O(n)
```

---

# 第六部分：统一心智模型

读写指针真正利用的是：

> **处理区间已经完成。名次一旦颁发就不收回。**

为什么成立：

```
问题可写成稳定过滤 / 顺序压缩
幸存者的目标下标 = 它是第几个幸存者
这个函数对时间单调
所以 write 是单调计数器
read 是单调扫描器
两者都不回头
```

write 永远不会回退：回退等于撤销已经数过的幸存者名次，但名次由已经看见的历史唯一决定，历史不会改。

read 永远向前：每个源位置的谓词只依赖自身（或与「上一个保留值」——而那个值已经冻在 write-1），不必重看更早的源。

---

# 第七部分：完整例子

---

## 例子 1：Move Zeroes（LeetCode 283）

### 1）题目

原地把所有 0 移到末尾，非零相对顺序不变。

示例：`[0,1,0,3,12]` -> `[1,3,12,0,0]`

### 2）为什么想到双指针

暴力：遇到 0 就删除并 push 到末尾，每次删除 O(n)，最坏 O(n^2)。

真正映射：第 k 个非零应放到下标 k。这是单调名次。
零不需要一个个搬，最后把 `[write, n)` 填 0 即可。

### 3）画出搜索过程

```
w
0 1 0 3 12
r
nums[r]==0  不写  r++
```

```
w
0 1 0 3 12
  r
1 非零  写到 w  -> [1,1,0,3,12]  w++ r++
```

```
  w
1 1 0 3 12
    r
0 跳过
```

```
  w
1 1 0 3 12
      r
3 写到 w -> [1,3,0,3,12]  w++
```

```
    w
1 3 0 3 12
         r
12 写到 w -> [1,3,12,3,12]
最后 [w,n) 填 0 -> [1,3,12,0,0]
```

### 4）为什么移动这个指针

读到 0 不能 `write++`：会把 0 留在非零前缀里。
读到非零必须写到 `write` 再 `write++`：这是它的最终名次。

不能 write 回退去找 0 对调而不扫完：也能做（另一种双指针分区），但若每次从头部重扫就是 O(n^2)。
标准写法 write 只追加。

不会漏：每个非零被 read 碰到恰好一次，按序领取名次。

### 5）状态剪枝

写完 1 之后：

```
[0, 1) = [1] 冻结
任何以后的操作都不再改下标 0
整列「回头改位置 0」死亡
```

跳过第一个 0：

```
源下标 0 的 0 不会再被当作幸存者
(它, 所有 write 位置) 的「是否该写」已决: 否
```

### 6）最终代码

Python:

```python
def move_zeroes(nums):
    write = 0
    for read in range(len(nums)):
        if nums[read] != 0:
            nums[write] = nums[read]
            write += 1
    for i in range(write, len(nums)):
        nums[i] = 0
```

C++:

```cpp
#include <vector>

void moveZeroes(std::vector<int>& nums)
{
    int write = 0;
    for (int read = 0; read < static_cast<int>(nums.size()); ++read) {
        if (nums[read] != 0)
            nums[write++] = nums[read];
    }
    for (int i = write; i < static_cast<int>(nums.size()); ++i)
        nums[i] = 0;
}
```

### 7）复杂度分析

read 走 n，write 最多 n，最后填 0 最多 n。时间 O(n)，空间 O(1)。
每个非零写一次，每个零最后填一次，没有二次搬移。

### 8）这一题真正训练了什么

> 分区问题的名次函数是单调的。
> 先稳定收集幸存者，再填充被淘汰区。

---

## 例子 2：Remove Element（LeetCode 27）

### 1）题目

原地移除所有等于 `val` 的元素，返回剩余个数 k。前 k 个是结果，顺序可保持相对序。

示例：`nums = [3,2,2,3], val = 3` -> k=2，前两个是 2,2

### 2）为什么想到双指针

暴力：每遇到 val 就 memmove，O(n^2)。

谓词：`x != val` 的元素是幸存者。第 k 个幸存者写到 k。

### 3）画出搜索过程

```
val=3
3 2 2 3
w
r       3==val 跳过
```

```
w
3 2 2 3
  r     2 写入 -> [2,2,2,3] w++
```

```
  w
2 2 2 3
    r   2 写入 -> [2,2,2,3] w++
```

```
    w
2 2 2 3
      r 3 跳过
k = w = 2
```

### 4）为什么移动这个指针

等于 val：write 不动。若前进，垃圾进入答案前缀。
不等于 val：必须写到 write。若只动 read 不动 write，名次空洞，后面的幸存者会覆盖错误位置或漏写。

不会漏：每个非 val 领到连续名次 0..k-1。

### 5）状态剪枝

跳过第一个 3：

```
源位置 0 对所有目标位置都不再是候选
整列 read=0 在「保留」意义下死亡
```

写完两个 2：

```
[0,2) 冻结
不必再把后面的 3 往前 memmove
```

### 6）最终代码

Python:

```python
def remove_element(nums, val):
    write = 0
    for read in range(len(nums)):
        if nums[read] != val:
            nums[write] = nums[read]
            write += 1
    return write
```

C++:

```cpp
#include <vector>

int removeElement(std::vector<int>& nums, int val)
{
    int write = 0;
    for (int read = 0; read < static_cast<int>(nums.size()); ++read) {
        if (nums[read] != val)
            nums[write++] = nums[read];
    }
    return write;
}
```

### 7）复杂度分析

每个元素读一次，幸存者写一次。时间 O(n)，空间 O(1)。总指针前进 <= 2n。

### 8）这一题真正训练了什么

> Remove Element 是最干净的稳定过滤。
> write 就是计数器「到目前收下几个」。

---

## 例子 3：Remove Duplicates（LeetCode 26）

### 1）题目

有序数组原地去重，每个值留一次，返回新长度。

示例：`[1,1,2]` -> `[1,2,...]`，k=2

### 2）为什么想到双指针

暴力：发现重复就删，O(n^2)。

有序 => 重复只在连续段。谓词变成：`nums[read] != nums[write-1]`。
与「上一个已保留值」比较，而那个值冻在前缀尾，read 仍不必回头扫全部历史。

### 3）画出搜索过程

```
1 1 2
w=1
  r    1==1 跳过
```

```
1 1 2
w=1
    r  2!=1 写到 w -> [1,2,2] w=2
k=2
```

更长：

```
0 0 1 1 1 2
w
  r r r ... 见第二章例子 4 的图
```

### 4）为什么移动这个指针

与已保留尾相同：必须 `read++` only。
不同：必须写入再 `write++`，否则新值丢失。

write 不能回退去覆盖更早的值：更早的值是更小的、已完成的去重前缀。
read 不能回头找「这个值以前出现过没有」：有序保证只须看邻居。

### 5）状态剪枝

跳过第二、第三个 1：

```
值 1 的连续段除首个外全部死亡
(1_重复, 任何新 write 槽) 不写
```

```
已保留 [0, write) 对未来只通过最后一个元素说话
更早元素不必再比较
```

### 6）最终代码

Python:

```python
def remove_duplicates(nums):
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

int removeDuplicates(std::vector<int>& nums)
{
    if (nums.empty())
        return 0;

    int write = 1;
    for (int read = 1; read < static_cast<int>(nums.size()); ++read) {
        if (nums[read] != nums[write - 1])
            nums[write++] = nums[read];
    }
    return write;
}
```

### 7）复杂度分析

read 走 n-1，write 最多 n。时间 O(n)，空间 O(1)。
有序把「对全部历史去重」的潜在 O(n) 查询降成 O(1) 邻接比较。

### 8）这一题真正训练了什么

> 读写指针 + 有序邻接。
> 前缀尾是历史的充分统计量。

---

## 例子 4：字符串压缩（LeetCode 443）

### 1）题目

原地压缩：连续重复改成字符 + 次数（次数=1 则只留字符）。返回新长度。

示例：`["a","a","b","b","c","c","c"]` -> `["a","2","b","2","c","3"]`，长度 6

### 2）为什么想到双指针

暴力：另开字符串再拷回，或每段删除多余再插入数字，插入是 O(n)。

从左到右每段只写一次：字符，以及可能的位数。
write 是压缩结果的尾，read 用来量一段 run 的长度。
run 量完就冻结写入，read 跳到下一段。

单调性：更左的 run 先写完，压缩串前缀定型。后面的 run 更不可能插到前面（原顺序保持）。

### 3）画出搜索过程

```
a a b b c c c
w
r
量到 run=2 的 a
写 a, 写 2
w 停在原 b 的位置
r 停在第一个 b
```

```
写完: a 2 | b b c c c
          w
            r
量 b 的 run=2
写 b, 写 2
```

```
a 2 b 2 | c c c
        w
          r
量 c 的 run=3
写 c, 写 3
结束 长度 6
```

### 4）为什么移动这个指针

read 必须先走完整个 run 才知道数字怎么写。
中途不能 write：次数还没定。

write 在 run 结算后一次写完。回退会擦掉上一个 run 的数字。

不会漏：run 划分覆盖每个字符恰好一次。每个 run 对答案贡献 O(1) 或 O(log count) 个字符，按序追加。

### 5）状态剪枝

第一个 run 写完：

```
[0, write) = "a2" 冻结
源 [0, 2) 的两个 a 不再回头
```

```
所有「把后面字符插入 a2 中间」的状态死亡
```

### 6）最终代码

Python:

```python
def compress(chars):
    write = 0
    read = 0
    n = len(chars)

    while read < n:
        group_char = chars[read]
        count = 0
        while read < n and chars[read] == group_char:
            read += 1
            count += 1

        chars[write] = group_char
        write += 1

        if count > 1:
            for digit in str(count):
                chars[write] = digit
                write += 1

    return write
```

C++:

```cpp
#include <vector>
#include <string>

int compress(std::vector<char>& chars)
{
    int write = 0;
    int read = 0;
    int n = static_cast<int>(chars.size());

    while (read < n) {
        char group_char = chars[read];
        int count = 0;
        while (read < n && chars[read] == group_char) {
            ++read;
            ++count;
        }

        chars[write++] = group_char;
        if (count > 1) {
            std::string digits = std::to_string(count);
            for (char d : digits)
                chars[write++] = d;
        }
    }
    return write;
}
```

### 7）复杂度分析

read 每个字符访问一次。write 每个输出字符写一次，输出长度 <= n。
时间 O(n)，空间 O(1)（除 to_string 的极短缓冲）。总移动 <= 2n。

### 8）这一题真正训练了什么

> 压缩是「按 run 批量颁发名次」。
> read 负责量测，write 负责冻结一段输出。两段职责分开，都不回头。

---

## 例子 5：原地过滤数组

### 1）题目

经典：给定谓词 `keep(x)`，原地稳定保留满足谓词的元素，返回新长度。
这是 Remove Element / 过滤的一般形式。

示例：`[4, -1, 0, 5, -3, 2]`，保留非负 -> `[4, 0, 5, 2, ...]`，k=4

### 2）为什么想到双指针

暴力：新数组收集再拷回，O(n) 但 O(n) 空间；或删除不满足的，O(n^2)。

一般事实：稳定过滤的目标下标 = 幸存者计数。与 val、与 0 无关。
一旦看见这个，所有原地 filter 都是同一条读写路径。

### 3）画出搜索过程

```
keep = x >= 0
4  -1  0  5  -3  2
w
r     4 keep  写到 0（原地重叠）w++
```

```
  w
4 -1  0  5  -3  2
    r   -1 丢弃
```

```
  w
4 -1  0  5  -3  2
      r  0 写入 -> [4,0,0,5,-3,2] w++
```

```
    w
4 0 0 5 -3 2
        r  5 写入
```

```
      w
4 0 5 5 -3 2
           r  -3 丢
             2 写入
k=4  [4,0,5,2]
```

### 4）为什么移动这个指针

谓词假：write 冻结。真：追加。
理由与 Remove Element 相同，只是谓词换成任意 `keep`。

若谓词依赖「未来元素」，名次不再由已见历史决定，write 可能需要回写，读写指针失败。
例如「保留所有大于后面最小值的元素」——这不是稳定前缀过滤，需要从右扫或额外结构。

本模型成立的前提：

> `keep(x)`（或 `keep(x, last_kept)`）只依赖已冻结信息。

### 5）状态剪枝

每收下或丢掉一个源元素：

```
该源下标对应的整列「它该写到哪个更早的槽」已决
已写前缀整行「被回头修改」已决: 否
```

```
        read
      0 1 2 3 4 5
w=0   *
w=1     . *
w=2         *
w=3             *
只走这条阶梯
```

### 6）最终代码

Python:

```python
def filter_in_place(nums, keep):
    write = 0
    for read in range(len(nums)):
        if keep(nums[read]):
            nums[write] = nums[read]
            write += 1
    return write


# 例:
# k = filter_in_place(nums, lambda x: x >= 0)
```

C++:

```cpp
#include <vector>
#include <functional>

int filterInPlace(std::vector<int>& nums, const std::function<bool(int)>& keep)
{
    int write = 0;
    for (int read = 0; read < static_cast<int>(nums.size()); ++read) {
        if (keep(nums[read]))
            nums[write++] = nums[read];
    }
    return write;
}
```

### 7）复杂度分析

谓词若 O(1)，总时间 O(n)。write/read 各最多 n 步，合计 <= 2n。
这是把 O(n^2) 删除变成 O(n) 覆盖写的通用降阶。

### 8）这一题真正训练了什么

> 读写指针的一般型：稳定过滤。
> 前四题都是这个型的谓词特化（非零 / 非 val / 不同于前一个 / run 头部）。

---

# 读写指针总结

为什么 write 永远不会回退：

> 它记录的是已经颁发的名次。名次由已扫描历史唯一确定。
> 历史只增长，名次只追加。

为什么 read 永远向前：

> 每个源位置只需要被判定一次。
> 判定所需的上下文（上一个保留值、当前 run、谓词）都在 O(1) 状态里，不在已走过的源下标里。

```
稳定过滤 / 顺序压缩
        |
        v
幸存者名次单调
        |
        v
[0, write) 冻结
        |
        v
read 供货，write 追加
        |
        v
每个元素至多搬一次
```

什么情况下应该想到这一类：

1. 原地
2. 相对顺序要保持
3. 每个元素独立地「留或丢」，或只依赖已冻结前缀
4. 不能接受每次删除都搬后面整段
