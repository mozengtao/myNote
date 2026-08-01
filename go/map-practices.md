# Map 最佳实践与典型示例

> **前置阅读：** [make.md](make.md) — 理解 hmap descriptor 与 `make(map[K]V, ...)` 的 runtime 含义
>
> **一句话心智模型：**
>
> **Map = 指向 runtime.hmap 的 descriptor。写入前必须 `make`；读取 nil map 安全返回零值。**

**相关文档：** [Slice 最佳实践](slice-practices.md) · [Channel 最佳实践](chan-practices.md)

---

## 核心心智模型

```
Map 变量（descriptor）           runtime.hmap
+--------+                       +------------------+
| ptr    | ------------------->  | buckets          |
+--------+                       | overflow buckets |
                                 | hash seed        |
                                 +------------------+
```

**五条原则：**

1. Map 变量只是指针；**写入前必须 `make`**，否则 panic: `assignment to entry in nil map`。
2. **读取 nil map 安全**，返回 value 类型的零值，不会 panic。
3. Map **没有 cap()**，`make(map[K]V, n)` 的 `n` 只是 hint，不是固定容量。
4. Map **永不收缩**；大量 delete 后内存不释放，需重建 map。
5. Map **并发不安全**；并发读写必须加锁或使用 `sync.Map`。

---

## 何时用 make / 何时不用

| 场景 | 推荐写法 | 原因 |
|------|----------|------|
| 需要写入 | `make(map[K]V)` 或 `make(map[K]V, hint)` | nil map 不能写 |
| 只读 lookup，可能无 map | `var m map[K]V` + `v, ok := m[k]` | nil map 读安全 |
| 已知大约元素个数 | `make(map[K]V, n)` | 减少 rehash |
| 释放 map 占用的内存 | `m = make(map[K]V)` 或 `m = nil` | delete 不收缩 |
| 需要有序容器 | 不用 map | 用 slice + sort 或有序 map 库 |

---

## 10+ 典型示例

### 示例 1：写入前必须 `make`

**场景：** 向 map 插入第一个 key。

```go
package main

import "fmt"

func main() {
	var m map[string]int // nil map

	// m["a"] = 1 // panic: assignment to entry in nil map

	m = make(map[string]int)
	m["a"] = 1
	fmt.Println(m) // map[a:1]
}
```

**最佳实践：** 任何写入操作前，确保 map 已通过 `make` 或字面量 `{}` 初始化。

---

### 示例 2：nil map 读 vs 写

**场景：** 理解 nil map 的读写不对称性。

```go
package main

import "fmt"

func main() {
	var m map[string]int

	// 读：安全，返回零值
	fmt.Println(m["missing"]) // 0

	// 写：panic
	// m["key"] = 1
}
```

**最佳实践：** nil map 像"空哈希表视图"——可以查，不能写。函数返回 map 时，返回 nil 表示"无数据"是常见惯例。

---

### 示例 3：comma-ok 判断 key 是否存在

**场景：** 区分"key 不存在"与"值恰好是零值"。

```go
package main

import "fmt"

func main() {
	m := map[string]int{
		"exists_zero": 0,
		"exists_one":  1,
	}

	v := m["missing"]
	fmt.Println(v) // 0 — 无法区分不存在还是值为 0

	v, ok := m["missing"]
	fmt.Println(v, ok) // 0 false

	v, ok = m["exists_zero"]
	fmt.Println(v, ok) // 0 true — key 存在，值就是 0
}
```

**最佳实践：** 需要判断存在性时，始终用 `v, ok := m[k]`；仅需要默认值时用 `m[k]` 即可。

---

### 示例 4：容量 hint 减少 rehash

**场景：** 已知大约要插入多少元素。

```go
package main

import "fmt"

func main() {
	// hint=1000：runtime 估算 bucket 数量，减少扩容 rehash
	m := make(map[int]string, 1000)

	for i := 0; i < 1000; i++ {
		m[i] = fmt.Sprintf("val-%d", i)
	}
	fmt.Println(len(m)) // 1000
}
```

**最佳实践：** `make(map[K]V, n)` 的 `n` 是**预估元素个数**，不是上限；map 仍会按需 grow。

**常见陷阱：** 以为 hint 是固定容量——map 没有 `cap()`，超出 hint 仍可插入。

---

### 示例 5：`delete()` 语义

**场景：** 删除 key 的行为。

```go
package main

import "fmt"

func main() {
	m := map[string]int{"a": 1, "b": 2, "c": 3}

	delete(m, "b")
	fmt.Println(m) // map[a:1 c:3]

	// 删除不存在的 key 不会 panic
	delete(m, "missing")

	// delete(nil, "key") 也安全，无操作
	var nilMap map[string]int
	delete(nilMap, "key")
}
```

**最佳实践：** `delete(m, k)` 对 nil map 和不存在的 key 都安全；但 delete **不会收缩**底层 bucket 内存。

---

### 示例 6：重建 map 释放内存

**场景：** 大量 delete 后内存仍被占用。

```go
package main

import "fmt"

func main() {
	m := make(map[int]int, 100000)
	for i := 0; i < 100000; i++ {
		m[i] = i
	}
	fmt.Println("filled:", len(m))

	for k := range m {
		delete(m, k)
	}
	fmt.Println("after delete all:", len(m)) // 0，但内存未释放

	// 重建 map 让 GC 回收旧 hmap
	m = make(map[int]int)
	fmt.Println("rebuilt:", len(m))
}
```

**最佳实践：** 短期大量 delete 后若 map 不再使用，赋新 `make(map[K]V)` 或 `nil` 释放内存。

---

### 示例 7：set 模式 `map[T]struct{}`

**场景：** 高效 membership 测试，零额外内存。

```go
package main

import "fmt"

func main() {
	seen := make(map[int]struct{})

	for _, n := range []int{1, 2, 2, 3, 3, 3} {
		if _, ok := seen[n]; !ok {
			seen[n] = struct{}{}
			fmt.Println("new:", n)
		}
	}
	fmt.Println(len(seen)) // 3
}
```

**最佳实践：** `struct{}` 不占额外空间，是 Go 实现 set 的标准惯用法；判断存在用 `_, ok := set[k]`。

---

### 示例 8：分组聚合 `map[string][]T`

**场景：** 按 key 分组收集元素。

```go
package main

import "fmt"

func groupByParity(nums []int) map[string][]int {
	groups := make(map[string][]int)
	for _, n := range nums {
		key := "even"
		if n%2 != 0 {
			key = "odd"
		}
		groups[key] = append(groups[key], n)
	}
	return groups
}

func main() {
	nums := []int{1, 2, 2, 3, 4}
	fmt.Println(groupByParity(nums)) // map[even:[2 2 4] odd:[1 3]]
}
```

**最佳实践：** value 是 slice 时，直接 `append(m[k], v)`；Go 会自动处理 nil slice 的 append。

---

### 示例 9：并发写保护 — `sync.Mutex`

**场景：** 多个 goroutine 同时写 map。

```go
package main

import (
	"fmt"
	"sync"
)

func main() {
	m := make(map[int]int)
	var mu sync.Mutex
	var wg sync.WaitGroup

	for i := 0; i < 100; i++ {
		wg.Add(1)
		go func(n int) {
			defer wg.Done()
			mu.Lock()
			m[n] = n * n // 无锁会 panic: concurrent map writes
			mu.Unlock()
		}(i)
	}

	wg.Wait()
	fmt.Println(len(m)) // 100
}
```

**最佳实践：** 普通 map 并发写必须加锁；读+写也需要保护（或使用 `sync.RWMutex` 读多写少）。

---

### 示例 10：`sync.Map` 适用场景

**场景：** 读多写少、key 集合稳定的缓存。

```go
package main

import (
	"fmt"
	"sync"
)

func main() {
	var cache sync.Map

	// Store / Load
	cache.Store("user:1", "Alice")
	cache.Store("user:2", "Bob")

	if v, ok := cache.Load("user:1"); ok {
		fmt.Println(v) // Alice
	}

	// Range 遍历
	cache.Range(func(key, value any) bool {
		fmt.Println(key, value)
		return true
	})
}
```

**最佳实践：** `sync.Map` 适合读远多于写、key 相对稳定；一般业务 map 用 `map + Mutex` 更简单清晰。

**常见陷阱：** 不要把 `sync.Map` 当万能并发 map——类型是 `any`，API 不如普通 map 直观。

---

### 示例 11：map 值不可寻址

**场景：** 修改 struct 值的字段。

```go
package main

import "fmt"

type User struct {
	Name string
	Age  int
}

func main() {
	m := map[string]User{
		"alice": {Name: "Alice", Age: 30},
	}

	// m["alice"].Age = 31 // 编译错误：cannot assign to struct field in map

	u := m["alice"]
	u.Age = 31
	m["alice"] = u // 必须整体写回
	fmt.Println(m["alice"]) // {Alice 31}
}
```

**最佳实践：** map 的 value 不可寻址；改 struct 字段需取出 → 修改 → 写回。value 是指针时可直接改：`m[k].Field = x`（若 m[k] 返回 *T）。

---

### 示例 12：遍历顺序随机

**场景：** 不要依赖 `for range` 的顺序。

```go
package main

import "fmt"

func main() {
	m := map[string]int{"a": 1, "b": 2, "c": 3, "d": 4}

	fmt.Println("run 1:")
	for k, v := range m {
		fmt.Println(k, v)
	}

	fmt.Println("run 2:")
	for k, v := range m {
		fmt.Println(k, v)
	}
	// 两次输出顺序可能不同，且与插入顺序无关
}
```

**最佳实践：** 需要有序输出时，收集 key 到 slice → `sort.Strings(keys)` → 按序访问 map。

---

### 示例 13：lazy init 模式

**场景：** 函数或结构体中延迟创建 map。

```go
package main

import "fmt"

type Registry struct {
	items map[string]int
}

func (r *Registry) Set(key string, val int) {
	if r.items == nil {
		r.items = make(map[string]int)
	}
	r.items[key] = val
}

func (r *Registry) Get(key string) (int, bool) {
	if r.items == nil {
		return 0, false
	}
	v, ok := r.items[key]
	return v, ok
}

func main() {
	var reg Registry
	reg.Set("a", 1)
	fmt.Println(reg.Get("a")) // 1 true
}
```

**最佳实践：** 零值 struct 中 map 字段为 nil；首次写入前 lazy init 是常见模式。

---

## 反模式速查表

| 反模式 | 问题 | 正确做法 |
|--------|------|----------|
| nil map 写入 | panic | `make(map[K]V)` 或 `{}` |
| 并发裸写 map | panic: concurrent map writes | `Mutex` / `sync.Map` |
| 用 `m[k]` 判断存在且值为零值 | 误判 | `v, ok := m[k]` |
| delete 后期望内存释放 | map 不收缩 | 重建 map |
| 依赖 range 顺序 | 结果不稳定 | key 排序后访问 |
| 直接改 map 中 struct 字段 | 编译错误 | 取出 → 改 → 写回 |
| 用 map 当有序容器 | 无序 | slice + sort |

---

## 与 make.md 的关联

| 主题 | make.md 章节 |
|------|-------------|
| `make(map[K]V)` 基本用法 | [第七节 Map 的 make](make.md#七map-的-make) |
| 容量 hint 含义 | [第七节 指定 hint](make.md#七map-的-make) |
| nil map 可读不可写 | [第八节 为什么 nil map 可以读取](make.md#八为什么-nil-map-可以读取) |
| 已知大小时提供 hint | [第十三节最佳实践 #2](make.md#十三make-的最佳实践) |

---

## 快速上手小结

> **写入前必须 make；读取 nil map 安全。用 comma-ok 区分不存在与零值。并发一律加锁。delete 不释放内存——重建 map 才能回收。遍历无序，需要顺序就 sort key。**
