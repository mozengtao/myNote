# Topic 10: Exceptions vs std::optional / std::expected

## 1. Problem Statement

### What real engineering problem does this solve?

Operations can fail. How do you communicate failure?

```
THREE STRATEGIES:
┌──────────────────┬──────────────────┬──────────────────┐
│   EXCEPTIONS     │   OPTIONAL       │   EXPECTED       │
├──────────────────┼──────────────────┼──────────────────┤
│ throw error;     │ return {};       │ return error;    │
│ catch (e) {...}  │ if (val) {...}   │ if (val) {...}   │
├──────────────────┼──────────────────┼──────────────────┤
│ Exceptional case │ "Not found" OK   │ Expected failure │
│ Unwind stack     │ Local handling   │ Rich error info  │
│ Non-local jump   │ Caller decides   │ Caller decides   │
└──────────────────┴──────────────────┴──────────────────┘
```

### What goes wrong with wrong choice?

```cpp
// WRONG: Exception for expected case
int parse(const std::string& s) {
    try {
        return std::stoi(s);
    } catch (...) {
        return 0;  // Slow path for common case!
    }
}

// WRONG: Optional loses error information
std::optional<User> findUser(int id) {
    // Why did it fail? Network? Not found? Permission?
    return std::nullopt;
}

// WRONG: Error code ignored
int result = riskyOperation();  // Did it fail?
```

**中文说明：**
选择错误的错误处理策略会导致：异常用于预期情况（性能差）、optional 丢失错误原因、返回码被忽略。关键是匹配策略和场景：异常用于真正的异常情况，optional 用于"无结果"可接受的情况，expected 用于需要错误信息的预期失败。

---

## 2. Core Idea

### When to Use Each

```
┌─────────────────────────────────────────────────────────────────┐
│  EXCEPTIONS                                                     │
├─────────────────────────────────────────────────────────────────┤
│ • Truly exceptional (rare, unexpected)                          │
│ • Cannot be handled locally                                     │
│ • Need to unwind across many stack frames                       │
│ • Construction/destruction failures                             │
│ • Programmer errors (contract violations)                       │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│  std::optional<T>                                               │
├─────────────────────────────────────────────────────────────────┤
│ • "Not found" is valid outcome                                  │
│ • Absence doesn't need explanation                              │
│ • Nullable-like semantics                                       │
│ • Examples: find(), lookup(), parse()                           │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│  std::expected<T, E> (C++23) / Result<T, E>                     │
├─────────────────────────────────────────────────────────────────┤
│ • Failure is expected/common                                    │
│ • Error details matter                                          │
│ • Caller should handle error                                    │
│ • Examples: I/O, network, validation                            │
└─────────────────────────────────────────────────────────────────┘
```

### Comparison

```cpp
// Exception: File not found is exceptional
void loadConfig() {
    std::ifstream f("config.txt");
    if (!f) throw std::runtime_error("Config missing");
}

// Optional: Not finding is normal
std::optional<User> findUser(int id) {
    auto it = users_.find(id);
    if (it == users_.end()) return std::nullopt;
    return it->second;
}

// Expected: Parse failure with reason
std::expected<int, ParseError> parseInt(std::string_view s) {
    if (s.empty()) return std::unexpected(ParseError::Empty);
    // ...
    return value;
}
```

**中文说明：**
- **异常**：跨越多个调用层级传播错误，调用者无法（或不需要）立即处理
- **optional**：返回"有值或无值"，无值是正常情况，不需要解释原因
- **expected**：返回"值或错误"，错误是预期的，需要携带错误信息

---

## 3. Idiomatic C++ Techniques

### std::optional Usage

```cpp
#include <optional>

std::optional<int> divide(int a, int b) {
    if (b == 0) return std::nullopt;
    return a / b;
}

// Usage patterns
if (auto result = divide(10, 2)) {
    std::cout << *result << "\n";
}

// With default
int value = divide(10, 0).value_or(-1);

// Chaining with transform (C++23) or and_then
auto result = divide(10, 2)
    .transform([](int x) { return x * 2; })
    .value_or(0);
```

### std::expected (C++23)

```cpp
#include <expected>

enum class ParseError { Empty, Invalid, Overflow };

std::expected<int, ParseError> parseInt(std::string_view s) {
    if (s.empty()) return std::unexpected(ParseError::Empty);
    
    int result = 0;
    for (char c : s) {
        if (c < '0' || c > '9') 
            return std::unexpected(ParseError::Invalid);
        result = result * 10 + (c - '0');
    }
    return result;
}

// Usage
auto result = parseInt("123");
if (result) {
    std::cout << *result << "\n";
} else {
    switch (result.error()) {
        case ParseError::Empty: /* ... */ break;
        case ParseError::Invalid: /* ... */ break;
    }
}
```

### Pre-C++23 Expected Implementation

```cpp
template<typename T, typename E>
class Result {
    std::variant<T, E> data_;
    
public:
    Result(T value) : data_(std::move(value)) {}
    Result(E error) : data_(std::move(error)) {}
    
    bool has_value() const { return std::holds_alternative<T>(data_); }
    explicit operator bool() const { return has_value(); }
    
    T& value() { return std::get<T>(data_); }
    const T& value() const { return std::get<T>(data_); }
    
    E& error() { return std::get<E>(data_); }
    const E& error() const { return std::get<E>(data_); }
    
    T value_or(T default_val) const {
        return has_value() ? value() : default_val;
    }
};
```

---

## 4. Complete C++ Example

```cpp
#include <charconv>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// ============================================================
// Result type (pre-C++23 expected)
// ============================================================
template<typename T, typename E>
class Result {
    std::variant<T, E> data_;
    
public:
    Result(T value) : data_(std::in_place_index<0>, std::move(value)) {}
    
    template<typename U = E>
    static Result error(U&& e) {
        Result r;
        r.data_ = std::forward<U>(e);
        return r;
    }
    
    bool ok() const { return data_.index() == 0; }
    explicit operator bool() const { return ok(); }
    
    T& value() { return std::get<0>(data_); }
    const T& value() const { return std::get<0>(data_); }
    
    E& err() { return std::get<1>(data_); }
    const E& err() const { return std::get<1>(data_); }
    
    T value_or(T default_val) const {
        return ok() ? value() : std::move(default_val);
    }
    
private:
    Result() = default;
};

// ============================================================
// Error types
// ============================================================
enum class ParseError {
    Empty,
    InvalidCharacter,
    Overflow,
    Underflow
};

std::string to_string(ParseError e) {
    switch (e) {
        case ParseError::Empty: return "empty input";
        case ParseError::InvalidCharacter: return "invalid character";
        case ParseError::Overflow: return "overflow";
        case ParseError::Underflow: return "underflow";
    }
    return "unknown";
}

struct FileError {
    std::string filename;
    std::string reason;
};

// ============================================================
// Functions using different strategies
// ============================================================

// 1. Optional: "not found" is normal
std::optional<int> findIndex(const std::vector<int>& vec, int target) {
    for (size_t i = 0; i < vec.size(); ++i) {
        if (vec[i] == target) return static_cast<int>(i);
    }
    return std::nullopt;
}

// 2. Result: parsing can fail with reason
Result<int, ParseError> parseInt(std::string_view s) {
    if (s.empty()) return Result<int, ParseError>::error(ParseError::Empty);
    
    bool negative = false;
    size_t start = 0;
    
    if (s[0] == '-') {
        negative = true;
        start = 1;
    } else if (s[0] == '+') {
        start = 1;
    }
    
    if (start >= s.size()) {
        return Result<int, ParseError>::error(ParseError::InvalidCharacter);
    }
    
    long long result = 0;
    for (size_t i = start; i < s.size(); ++i) {
        if (s[i] < '0' || s[i] > '9') {
            return Result<int, ParseError>::error(ParseError::InvalidCharacter);
        }
        result = result * 10 + (s[i] - '0');
        
        if (result > INT_MAX) {
            return Result<int, ParseError>::error(
                negative ? ParseError::Underflow : ParseError::Overflow);
        }
    }
    
    return static_cast<int>(negative ? -result : result);
}

// 3. Result: file operations can fail
Result<std::string, FileError> readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        return Result<std::string, FileError>::error(
            FileError{path, "cannot open"});
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    
    if (file.bad()) {
        return Result<std::string, FileError>::error(
            FileError{path, "read error"});
    }
    
    return content;
}

// 4. Exception: truly exceptional case
void processConfig(const std::string& path) {
    auto result = readFile(path);
    if (!result) {
        // Config file is REQUIRED - exceptional if missing
        throw std::runtime_error("Config not found: " + result.err().filename);
    }
    // Process content...
}

// ============================================================
// Chaining operations
// ============================================================
Result<int, std::string> pipeline(const std::string& input) {
    // Parse string to int
    auto parsed = parseInt(input);
    if (!parsed) {
        return Result<int, std::string>::error(
            "Parse error: " + to_string(parsed.err()));
    }
    
    int value = parsed.value();
    
    // Validate range
    if (value < 0 || value > 100) {
        return Result<int, std::string>::error("Value out of range [0, 100]");
    }
    
    // Transform
    return value * 2;
}

int main() {
    std::cout << "=== Optional: Find Index ===\n";
    std::vector<int> vec = {10, 20, 30, 40, 50};
    
    if (auto idx = findIndex(vec, 30)) {
        std::cout << "Found at index: " << *idx << "\n";
    } else {
        std::cout << "Not found\n";
    }
    
    if (auto idx = findIndex(vec, 99)) {
        std::cout << "Found at index: " << *idx << "\n";
    } else {
        std::cout << "Not found (expected)\n";
    }
    
    std::cout << "\n=== Result: Parse Int ===\n";
    for (const auto& s : {"123", "-456", "12.34", "", "99999999999"}) {
        auto result = parseInt(s);
        if (result) {
            std::cout << "'" << s << "' -> " << result.value() << "\n";
        } else {
            std::cout << "'" << s << "' -> ERROR: " << to_string(result.err()) << "\n";
        }
    }
    
    std::cout << "\n=== Result: File Read ===\n";
    auto content = readFile("/etc/hostname");
    if (content) {
        std::cout << "Read " << content.value().size() << " bytes\n";
    } else {
        std::cout << "Error: " << content.err().reason << "\n";
    }
    
    std::cout << "\n=== Chaining ===\n";
    for (const auto& input : {"42", "150", "abc", "-10"}) {
        auto result = pipeline(input);
        if (result) {
            std::cout << input << " -> " << result.value() << "\n";
        } else {
            std::cout << input << " -> ERROR: " << result.err() << "\n";
        }
    }
    
    return 0;
}
```

---

## 5. Failure Modes

### Mistake 1: Exception for common case

```cpp
// BAD: Parsing user input throws often
int getUserAge() {
    try {
        return std::stoi(input);  // Throws on invalid!
    } catch (...) {
        return 0;  // Slow, common path
    }
}

// GOOD: Use optional or expected
std::optional<int> getUserAge() {
    int result;
    auto [ptr, ec] = std::from_chars(input.data(), 
                                      input.data() + input.size(), 
                                      result);
    if (ec != std::errc()) return std::nullopt;
    return result;
}
```

### Mistake 2: Ignoring optional

```cpp
std::optional<int> getValue();

// BAD: Assumes value exists
int x = *getValue();  // UB if nullopt!

// GOOD: Check first
if (auto val = getValue()) {
    int x = *val;
}
```

### Mistake 3: Losing error information

```cpp
// BAD: Lost why it failed
std::optional<Config> loadConfig() {
    if (/*file missing*/) return std::nullopt;
    if (/*parse error*/) return std::nullopt;
    if (/*validation failed*/) return std::nullopt;
    // Caller can't distinguish these!
}

// GOOD: Return error info
Result<Config, ConfigError> loadConfig();
```

---

## 6. When to Use Each

### Decision Matrix

| Scenario | Mechanism |
|----------|-----------|
| Lookup not found | `optional` |
| Parse with reason | `expected` |
| Out of memory | Exception |
| Contract violation | Exception/assert |
| Network failure | `expected` |
| Constructor failure | Exception |
| User input validation | `expected` |

### Performance Considerations

```
┌─────────────────────────────────────────────────────────────────┐
│ MECHANISM        │ SUCCESS COST │ FAILURE COST                  │
├─────────────────────────────────────────────────────────────────┤
│ Return code      │ Zero         │ Zero                          │
│ optional         │ Bool check   │ Bool check                    │
│ expected         │ Bool check   │ Bool check + error copy       │
│ Exception        │ Zero         │ Stack unwind (expensive!)     │
└─────────────────────────────────────────────────────────────────┘

Rule: Use exceptions when failure is rare (<1%)
      Use optional/expected when failure is common
```

**中文说明：**
- **异常**：成功路径零开销，失败时代价高（栈展开）
- **optional/expected**：成功和失败代价相近（分支检查）

因此：失败罕见用异常，失败常见用 optional/expected。

---

## Summary

```
+------------------------------------------------------------------+
|              ERROR HANDLING DECISION GUIDE                        |
+------------------------------------------------------------------+
|                                                                  |
|  Q: Can the function fail?                                       |
|     NO  → Return T directly                                      |
|     YES ↓                                                        |
|                                                                  |
|  Q: Is failure common/expected?                                  |
|     NO  → Use exception                                          |
|     YES ↓                                                        |
|                                                                  |
|  Q: Does caller need to know WHY it failed?                      |
|     NO  → Use std::optional<T>                                   |
|     YES → Use std::expected<T, E> or Result<T, E>                |
|                                                                  |
+------------------------------------------------------------------+
```

