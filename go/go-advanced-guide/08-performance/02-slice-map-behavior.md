# Slice and Map Internals

## 1. Engineering Problem

**Understanding slice and map internals prevents subtle bugs and enables optimization.**

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    SLICE STRUCTURE                                      │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Slice Header (24 bytes):                                              │
│   ┌─────────┬─────────┬─────────┐                                       │
│   │  ptr    │  len    │  cap    │                                       │
│   └────┬────┴─────────┴─────────┘                                       │
│        │                                                                │
│        ▼                                                                │
│   ┌─────┬─────┬─────┬─────┬─────┐                                       │
│   │  0  │  1  │  2  │  3  │  4  │  Backing array                       │
│   └─────┴─────┴─────┴─────┴─────┘                                       │
│                                                                         │
│   Append behavior:                                                      │
│   • len < cap: append in place                                          │
│   • len == cap: allocate new array (2x size)                           │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

## 2. Slice Gotchas

```go
// Sharing backing array
a := []int{1, 2, 3, 4, 5}
b := a[1:3]  // b shares array with a
b[0] = 99    // Modifies a[1] too!

// Append may or may not share
a := make([]int, 3, 5)
b := append(a, 4)  // Shares if cap allows
```

## 3. Map Internals

```go
// Maps never shrink
m := make(map[string]int)
// Add 1 million entries...
// Delete all entries...
// Memory is NOT freed!

// Solution: create new map
m = make(map[string]int)
```

## 4. Best Practices

```go
// Pre-allocate slices
s := make([]Route, 0, 1000)

// Pre-allocate maps
m := make(map[string]Route, 1000)

// Copy to break reference
copy := append([]int{}, original...)
```

---

## Chinese Explanation (中文解释)

### Slice 结构

- 24 字节头：指针 + 长度 + 容量
- 共享底层数组
- 超容量时 2 倍扩容

### Map 特点

- 永不收缩
- 删除元素不释放内存
- 创建新 map 来释放

