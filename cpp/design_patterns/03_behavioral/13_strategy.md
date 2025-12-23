# Pattern 13: Strategy

## 1. Problem the Pattern Solves

### Design Pressure
- Algorithm varies independently from the class that uses it
- Multiple related algorithms exist, clients choose at runtime
- Avoid conditional statements for algorithm selection

### What Goes Wrong Without It
```cpp
void sort(Data& data, SortType type) {
    if (type == QUICK) { /* quicksort */ }
    else if (type == MERGE) { /* mergesort */ }
    else if (type == HEAP) { /* heapsort */ }
    // Adding new sort = modify this function
}
```

---

## 2. Core Idea (C++-Specific)

**Strategy defines a family of algorithms, encapsulates each one, and makes them interchangeable.**

```
+----------+       +-----------+
| Context  |------>| Strategy  |
| setStrat |       | execute() |
+----------+       +-----------+
                        ^
              +---------+---------+
              |                   |
        +-----------+       +-----------+
        | StrategyA |       | StrategyB |
        +-----------+       +-----------+
```

**C++ offers two approaches:**
1. **Runtime Strategy**: `std::function` or virtual interface
2. **Compile-Time Strategy**: Template parameter (Policy)

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| `std::function<>` | Store callable | Flexible, runtime |
| Lambda | Inline strategy | Concise |
| Virtual interface | Classic OOP | Type-erased |
| Template parameter | Policy-based | Zero overhead |

---

## 4. Canonical C++ Implementation

### Modern: `std::function` + Lambda

```cpp
#include <functional>
#include <vector>
#include <iostream>
#include <algorithm>

class DataProcessor {
public:
    using FilterStrategy = std::function<bool(int)>;
    using TransformStrategy = std::function<int(int)>;
    
    void setFilter(FilterStrategy f) { filter_ = std::move(f); }
    void setTransform(TransformStrategy t) { transform_ = std::move(t); }
    
    std::vector<int> process(const std::vector<int>& data) {
        std::vector<int> result;
        for (int x : data) {
            if (filter_(x)) {
                result.push_back(transform_(x));
            }
        }
        return result;
    }
    
private:
    FilterStrategy filter_ = [](int) { return true; };
    TransformStrategy transform_ = [](int x) { return x; };
};

int main() {
    DataProcessor proc;
    
    proc.setFilter([](int x) { return x > 0; });
    proc.setTransform([](int x) { return x * 2; });
    
    auto result = proc.process({-1, 2, 3, -4, 5});
    // result = {4, 6, 10}
    
    return 0;
}
```

### Classic: Virtual Interface

```cpp
#include <memory>

class CompressionStrategy {
public:
    virtual ~CompressionStrategy() = default;
    virtual std::vector<uint8_t> compress(const std::vector<uint8_t>&) = 0;
};

class ZipCompression : public CompressionStrategy {
public:
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) override {
        // ZIP algorithm
        return data;
    }
};

class GzipCompression : public CompressionStrategy {
public:
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) override {
        // GZIP algorithm
        return data;
    }
};

class Archiver {
public:
    void setStrategy(std::unique_ptr<CompressionStrategy> s) {
        strategy_ = std::move(s);
    }
    
    void archive(const std::vector<uint8_t>& data) {
        auto compressed = strategy_->compress(data);
        // Write to archive
    }
    
private:
    std::unique_ptr<CompressionStrategy> strategy_;
};
```

### Policy-Based (Compile-Time)

```cpp
template<typename SortPolicy>
class DataContainer {
public:
    void sort() {
        SortPolicy::sort(data_.begin(), data_.end());
    }
private:
    std::vector<int> data_;
};

struct QuickSortPolicy {
    template<typename Iter>
    static void sort(Iter begin, Iter end) {
        std::sort(begin, end);  // Quick sort
    }
};

struct StableSortPolicy {
    template<typename Iter>
    static void sort(Iter begin, Iter end) {
        std::stable_sort(begin, end);
    }
};

// Usage
DataContainer<QuickSortPolicy> container;
```

---

## 5. Typical Usage in Real Projects

| Domain | Example |
|--------|---------|
| Sorting | STL comparators |
| Validation | Input validators |
| Authentication | Auth strategies |
| Compression | Compression algorithms |

### STL Examples
- `std::sort(begin, end, comparator)` - comparator is strategy
- `std::transform(begin, end, out, func)` - func is strategy

---

## 6. Common Mistakes

### ❌ Over-Engineering Simple Cases

```cpp
// BAD: Strategy for single implementation
class OnlyOneStrategy : public Strategy { };
// Just use the algorithm directly
```

---

## 7. When NOT to Use

| Situation | Alternative |
|-----------|-------------|
| Fixed algorithm | Direct implementation |
| Few simple variations | Lambda parameter |

---

## 8. Runtime vs Compile-Time

| Aspect | Runtime (`std::function`) | Compile-Time (Template) |
|--------|--------------------------|-------------------------|
| Flexibility | Change at runtime | Fixed at compile |
| Performance | Indirect call | Inlined |
| Binary size | Smaller | Larger (per instantiation) |

---

## 9. Mental Model Summary

**When Strategy "Clicks":**

Use Strategy when you have **interchangeable algorithms** and want to select one at runtime. In C++, prefer `std::function` + lambdas for flexibility, or templates for performance.

---

## 中文说明

### 策略模式要点

1. **两种实现**：
   - 运行时：`std::function` + lambda
   - 编译时：模板参数（策略类）

2. **STL 中的策略**：
   - `std::sort` 的比较器
   - `std::transform` 的转换函数

3. **选择指南**：
   - 需要运行时切换 → `std::function`
   - 追求性能 → 模板参数

