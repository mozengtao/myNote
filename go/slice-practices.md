# Slice 最佳实践与典型示例

> **前置阅读：** [make.md](make.md) — 理解 slice descriptor 与 `make([]T, ...)` 的 runtime 含义
>
> **一句话心智模型：**
>
> **Slice = 底层数组的窗口（ptr + len + cap）。`make` 创建的是背后的数组，不是 slice 本身。**

**相关文档：** [Map 最佳实践](map-practices.md) · [Channel 最佳实践](chan-practices.md)

---

## 核心心智模型

```
Slice 变量（descriptor）          底层数组（runtime 分配）
+-------+                        +---+---+---+---+---+
| ptr   | ---------------------> | 0 | 1 | 2 | ? | ? |
| len=3 |                        +---+---+---+---+---+
| cap=5 |                        ^^^^^^^^^ 有效元素
+-------+                        ^^^^^^^^^^^^^^^ 已分配容量
```

**五条原则：**

1. Slice 不是动态数组，是指向数组的一个**视图**（Window of Array）。
2. `make([]T, len, cap)` 预分配底层数组，减少 append 时的 realloc 与 GC 压力。
3. 多个 slice 可以**共享**同一底层数组；子切片修改会影响原 slice。
4. `append` 在 `len == cap` 时会分配**新数组**，旧 slice 不再与新 slice 共享数据。
5. `nil slice` 可以 `append`；与空 slice 在 JSON 等场景语义不同。

---

## 何时用 make / 何时不用

| 场景 | 推荐写法 | 原因 |
|------|----------|------|
| 已知最终元素个数 | `make([]T, n)` | 直接初始化，无需 append |
| 已知上限、个数不定 | `make([]T, 0, cap)` | 预分配 cap，append 不扩容 |
| 元素个数很少或未知 | `var s []T` 或 `[]T{}` | nil/空 slice 均可 append |
| 字面量已知全部元素 | `[]T{a, b, c}` | 无需 make |
| 需要独立副本 | `copy(dst, src)` 或 `slices.Clone` | make 只分配新数组，不复制旧数据 |

---

## 10+ 典型示例

### 示例 1：`make([]T, n)` vs `make([]T, 0, cap)`

**场景：** 已知最终长度 vs 仅知上限。

```go
package main

import "fmt"

func main() {
	// 已知正好 5 个元素，且初始值为零值即可
	fixed := make([]int, 5)
	fmt.Println(fixed, len(fixed), cap(fixed)) // [0 0 0 0 0] 5 5

	// 只知道最多 100 个，逐个 append
	growing := make([]int, 0, 100)
	for i := 0; i < 5; i++ {
		growing = append(growing, i)
	}
	fmt.Println(growing, len(growing), cap(growing)) // [0 1 2 3 4] 5 100
}
```

**最佳实践：** 能确定 `len` 用 `make([]T, len)`；只能估上限用 `make([]T, 0, cap)`。

---

### 示例 2：预分配避免扩容

**场景：** 循环 append 大量元素。

```go
package main

import "fmt"

func buildSlow(n int) []int {
	var s []int
	for i := 0; i < n; i++ {
		s = append(s, i) // cap 不足时反复 2x 扩容 + 复制
	}
	return s
}

func buildFast(n int) []int {
	s := make([]int, 0, n)
	for i := 0; i < n; i++ {
		s = append(s, i) // cap 足够，零次扩容
	}
	return s
}

func main() {
	n := 1000
	fmt.Println(len(buildSlow(n)), cap(buildSlow(n)))
	fmt.Println(len(buildFast(n)), cap(buildFast(n))) // cap == 1000
}
```

**最佳实践：** 提前知道规模时，`make([]T, 0, n)` 是 Go 最重要的性能优化之一。

**常见陷阱：** 写成 `make([]T, n)` 但后续仍 append——会超出预分配 len，行为与预期不同。

---

### 示例 3：nil slice vs 空 slice

**场景：** JSON 序列化、API 响应语义。

```go
package main

import (
	"encoding/json"
	"fmt"
)

func main() {
	var nilSlice []int          // ptr=nil, len=0, cap=0
	emptySlice := make([]int, 0) // ptr!=nil, len=0, cap=0

	fmt.Println(nilSlice == nil)   // true
	fmt.Println(emptySlice == nil) // false

	b1, _ := json.Marshal(nilSlice)
	b2, _ := json.Marshal(emptySlice)
	fmt.Println(string(b1)) // null
	fmt.Println(string(b2)) // []

	// 两者都可以 append
	nilSlice = append(nilSlice, 1)
	emptySlice = append(emptySlice, 1)
	fmt.Println(nilSlice, emptySlice) // [1] [1]
}
```

**最佳实践：** 需要 JSON 输出 `[]` 而非 `null` 时，用 `make([]T, 0)` 或 `[]T{}`；需要表达"未初始化"语义时用 `nil`。

---

### 示例 4：子切片共享底层数组

**场景：** 截取部分数据后修改。

```go
package main

import "fmt"

func main() {
	original := []int{1, 2, 3, 4, 5}
	sub := original[1:3] // [2 3]，与 original 共享底层数组

	sub[0] = 100
	fmt.Println(original) // [1 100 3 4 5] — 原数组被改了
	fmt.Println(sub)      // [100 3]
}
```

**最佳实践：** 传递 slice 默认是共享数据；需要隔离时才 `copy` 或 `slices.Clone`。

**常见陷阱：** 以为 `sub := original[1:3]` 会复制数据——实际上只复制了 descriptor。

---

### 示例 5：`copy()` 断开引用

**场景：** 需要独立副本，修改不影响原 slice。

```go
package main

import "fmt"

func main() {
	original := []int{1, 2, 3, 4, 5}
	sub := original[1:3]

	// 显式 copy 到新数组
	safeCopy := make([]int, len(sub))
	copy(safeCopy, sub)

	safeCopy[0] = 999
	fmt.Println(original)  // [1 2 3 4 5]，不受影响
	fmt.Println(safeCopy)  // [999 3]
}
```

**最佳实践：** `copy(dst, src)` 返回实际复制元素个数；`dst` 长度不够时只复制部分。

---

### 示例 6：append 触发 realloc

**场景：** `len == cap` 时 append 分配新数组。

```go
package main

import "fmt"

func main() {
	s := make([]int, 3, 3) // len == cap，无剩余容量
	s2 := append(s, 4)     // 触发扩容，s2 指向新数组

	s2[0] = 999
	fmt.Println(s)  // [0 0 0]，s 未变
	fmt.Println(s2) // [999 0 0 4]
}
```

**最佳实践：** 判断两个 slice 是否还会互相影响，关键看 append 后 cap 是否够用，而非当前 len。

---

### 示例 7：三索引切片 `s[low:high:max]`

**场景：** 限制子 slice 的 cap，防止 append 污染原数组。

```go
package main

import "fmt"

func main() {
	original := []int{1, 2, 3, 4, 5}

	// 普通切片：sub 的 cap 延伸到 original 末尾
	sub := original[1:3]
	sub = append(sub, 99)
	fmt.Println(original) // [1 2 3 99 5] — 被污染了！

	original = []int{1, 2, 3, 4, 5}
	// 三索引：cap 被限制在 high，append 必定 realloc
	safe := original[1:3:3]
	safe = append(safe, 99)
	fmt.Println(original) // [1 2 3 4 5]，不受影响
	fmt.Println(safe)   // [2 3 99]
}
```

**最佳实践：** 从大块 buffer 切出子 slice 并 append 时，用 `s[low:high:high]` 限制 cap。

---

### 示例 8：`[:0]` 复用 buffer

**场景：** 高性能循环中反复构建 slice，避免重复分配。

```go
package main

import "fmt"

func main() {
	buf := make([]int, 0, 8)

	for round := 0; round < 3; round++ {
		buf = buf[:0] // 重置 len 为 0，保留 cap
		for i := 0; i < 3; i++ {
			buf = append(buf, round*10+i)
		}
		fmt.Println(buf, len(buf), cap(buf))
	}
	// [0 1 2] 3 8
	// [10 11 12] 3 8
	// [20 21 22] 3 8
}
```

**最佳实践：** `buf = buf[:0]` 清空元素但保留已分配容量，适合热路径复用。

---

### 示例 9：函数传参语义

**场景：** slice 作为函数参数时的拷贝行为。

```go
package main

import "fmt"

func mutate(s []int) {
	s[0] = 999 // 修改共享底层数组
	s = append(s, 4) // append 可能 realloc，只改变局部 s
}

func main() {
	data := []int{1, 2, 3}
	mutate(data)
	fmt.Println(data) // [999 2 3]，首元素被改，但 append 的 4 不会出现
}
```

**最佳实践：** 传 slice 进函数会拷贝 descriptor（ptr/len/cap），底层数组仍共享；函数内 append 若 realloc，不影响调用者的 slice。

**常见陷阱：** 想在函数内 append 并回传，应 `return append(s, x)` 或使用指针 `*[]T`。

---

### 示例 10：删除元素

**场景：** 按索引删除，保留顺序 vs 不要求顺序。

```go
package main

import "fmt"

func removeOrdered(s []int, i int) []int {
	return append(s[:i], s[i+1:]...)
}

func removeUnordered(s []int, i int) []int {
	last := len(s) - 1
	s[i] = s[last]
	return s[:last]
}

func main() {
	s1 := []int{1, 2, 3, 4, 5}
	s1 = removeOrdered(s1, 2)
	fmt.Println(s1) // [1 2 4 5]

	s2 := []int{1, 2, 3, 4, 5}
	s2 = removeUnordered(s2, 2)
	fmt.Println(s2) // [1 2 5 4]，顺序变了但 O(1)
}
```

**最佳实践：** 有序删除用 `append(s[:i], s[i+1:]...)`；不要求顺序时用 swap-with-last 更高效。

---

### 示例 11：`strings.Split` 子串共享大 backing array

**场景：** 子 slice 持有大数组引用导致内存无法 GC。

```go
package main

import (
	"fmt"
	"strings"
)

func main() {
	big := strings.Repeat("x", 1<<20) // 1MB 字符串
	parts := strings.Split(big, "y")
	first := parts[0] // 即使 first 很短，也引用整个 1MB backing array

	_ = first
	// big = ""  // 即使 big 不再使用，first 仍阻止 GC 释放 1MB

	// 正确做法：复制需要的部分
	safe := strings.Clone(first)
	_ = safe
}
```

**最佳实践：** 从大 buffer 切出小子串并长期保存时，用 `strings.Clone`（Go 1.18+）或 `copy` 到新 slice。

---

### 示例 12：Go 1.21+ `slices` 包

**场景：** 标准库提供的 slice 操作，替代手写逻辑。

```go
package main

import (
	"fmt"
	"slices"
)

func main() {
	src := []int{1, 2, 3, 4, 5}

	// Clone：独立副本
	cloned := slices.Clone(src)
	cloned[0] = 999
	fmt.Println(src)    // [1 2 3 4 5]
	fmt.Println(cloned) // [999 2 3 4 5]

	// Grow：确保 cap 足够，减少 append 扩容
	s := make([]int, 0, 2)
	s = slices.Grow(s, 10)
	fmt.Println(cap(s)) // >= 10

	// Compact：原地去除零值（常用于过滤后压缩）
	s2 := []int{1, 0, 2, 0, 3}
	s2 = slices.Compact(s2)
	fmt.Println(s2) // [1 2 3]
}
```

**最佳实践：** 优先用 `slices` 包（Go 1.21+）的 `Clone`、`Grow`、`Compact`、`Contains` 等，语义清晰且经过优化。

---

## 反模式速查表

| 反模式 | 问题 | 正确做法 |
|--------|------|----------|
| 大量 append 不预分配 | 反复 2x 扩容 + 复制 + GC | `make([]T, 0, n)` |
| 子切片 + append 不设 cap 上限 | 污染原数组 | `s[low:high:high]` |
| 用 slice 当 set 做 membership | O(n) 查找 | `map[T]struct{}` |
| 长期持有大 buffer 的小子串 | 内存泄漏 | `strings.Clone` / `copy` |
| 函数内 append 期望调用者可见 | descriptor 局部拷贝 | `return append(...)` 或 `*[]T` |
| 混淆 nil slice 与空 slice | JSON/API 语义错误 | 按场景选择 `nil` 或 `make([]T,0)` |

---

## 与 make.md 的关联

| 主题 | make.md 章节 |
|------|-------------|
| `make([]T, len)` / `make([]T, len, cap)` | [第五节 Slice 的 make](make.md#五slice-的-make) |
| 为什么要指定 capacity | [第六节 为什么要指定 capacity](make.md#六为什么要指定-capacity) |
| nil slice 可以 append | [第十一节 make 与 nil 的区别](make.md#十一make-与-nil-的区别) |
| 预分配 slice 容量 | [第十三节最佳实践 #1](make.md#十三make-的最佳实践) |

---

## 快速上手小结

> **传递 slice 默认共享底层数组；需要隔离时 `copy` 或 `slices.Clone`。提前知道规模就 `make` 预分配 cap。append 在 cap 用尽时会换新数组——用三索引切片或预分配 cap 避免意外污染。**
