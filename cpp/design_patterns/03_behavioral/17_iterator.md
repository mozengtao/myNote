# Pattern 17: Iterator

## 1. Problem the Pattern Solves

### Design Pressure
- Need to traverse a collection without exposing internals
- Support multiple traversal methods
- Uniform interface across different collections

### What Goes Wrong Without It
```cpp
// Without iterator: exposing internals
for (int i = 0; i < list.size(); i++) {
    process(list.array_[i]);  // Knows internal structure
}
// Breaks if implementation changes (array to linked list)
```

---

## 2. Core Idea (C++-Specific)

**Iterator provides a way to access elements of a collection sequentially without exposing its underlying representation.**

STL defines iterator concepts:
- **Input**: Read forward once (`istream_iterator`)
- **Output**: Write forward once (`ostream_iterator`)
- **Forward**: Read/write forward, multi-pass
- **Bidirectional**: Forward + backward (`list`)
- **Random Access**: Jump to any position (`vector`)

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| `begin()/end()` | Range access | Standard interface |
| `operator++/--` | Increment/decrement | Traversal |
| `operator*` | Dereference | Access element |
| `std::iterator_traits` | Type info | Generic algorithms |
| Range-based for | Syntactic sugar | Clean iteration |

---

## 4. Canonical C++ Implementation

### Custom Container with Iterator

```cpp
#include <iterator>
#include <cstddef>

template<typename T>
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity)
        : data_(new T[capacity]), capacity_(capacity) {}
    
    ~RingBuffer() { delete[] data_; }
    
    void push(const T& value) {
        data_[tail_] = value;
        tail_ = (tail_ + 1) % capacity_;
        if (size_ < capacity_) ++size_;
        else head_ = (head_ + 1) % capacity_;
    }
    
    // Iterator class
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;
        
        iterator(RingBuffer& buf, size_t idx, size_t count)
            : buf_(buf), idx_(idx), count_(count) {}
        
        reference operator*() { return buf_.data_[idx_]; }
        
        iterator& operator++() {
            idx_ = (idx_ + 1) % buf_.capacity_;
            ++count_;
            return *this;
        }
        
        bool operator!=(const iterator& other) const {
            return count_ != other.count_;
        }
        
    private:
        RingBuffer& buf_;
        size_t idx_;
        size_t count_;
    };
    
    iterator begin() { return iterator(*this, head_, 0); }
    iterator end() { return iterator(*this, tail_, size_); }
    
private:
    T* data_;
    size_t capacity_;
    size_t head_ = 0, tail_ = 0, size_ = 0;
};

// Usage
int main() {
    RingBuffer<int> buf(5);
    for (int i = 0; i < 7; ++i) buf.push(i);
    
    for (int x : buf) {  // Range-based for works!
        std::cout << x << " ";
    }
    // Output: 2 3 4 5 6
    return 0;
}
```

### External Iterator Pattern

```cpp
#include <memory>
#include <vector>

template<typename T>
class TreeIterator {
public:
    virtual ~TreeIterator() = default;
    virtual bool hasNext() const = 0;
    virtual T& next() = 0;
};

template<typename T>
class InOrderIterator : public TreeIterator<T> {
public:
    explicit InOrderIterator(TreeNode<T>* root) {
        pushLeft(root);
    }
    
    bool hasNext() const override { return !stack_.empty(); }
    
    T& next() override {
        auto* node = stack_.back();
        stack_.pop_back();
        pushLeft(node->right);
        return node->value;
    }
    
private:
    void pushLeft(TreeNode<T>* node) {
        while (node) {
            stack_.push_back(node);
            node = node->left;
        }
    }
    
    std::vector<TreeNode<T>*> stack_;
};
```

---

## 5. Typical Usage

| Domain | Example |
|--------|---------|
| STL | All container iterators |
| Databases | Result set cursors |
| File I/O | `istream_iterator` |
| Tree/Graph | Various traversal orders |

---

## 6. Common Mistakes

### ❌ Iterator Invalidation

```cpp
std::vector<int> v = {1, 2, 3};
for (auto it = v.begin(); it != v.end(); ++it) {
    if (*it == 2) v.erase(it);  // BUG: iterator invalidated!
}
// FIX: it = v.erase(it);
```

---

## 7. C++20 Ranges

```cpp
#include <ranges>
#include <vector>

std::vector<int> v = {1, 2, 3, 4, 5};
auto result = v 
    | std::views::filter([](int x) { return x % 2 == 0; })
    | std::views::transform([](int x) { return x * 2; });
// Lazy iteration with composition
```

---

## 8. Mental Model Summary

**When Iterator "Clicks":**

Iterator is fundamental in C++. Every STL container provides iterators. Create custom iterators when you have a custom collection that should work with range-based for loops and STL algorithms.

---

## 中文说明

### 迭代器模式要点

1. **STL 迭代器类别**：
   - 输入/输出迭代器
   - 前向/双向迭代器
   - 随机访问迭代器

2. **实现要点**：
   - `begin()/end()` 返回迭代器
   - 定义 `operator++, *, !=`
   - 使用 `std::iterator_traits`

3. **常见错误**：迭代器失效

4. **C++20 改进**：Ranges 库

