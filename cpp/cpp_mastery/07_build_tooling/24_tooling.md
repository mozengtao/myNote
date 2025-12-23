# Topic 24: Sanitizers and Static Analysis

## 1. Problem Statement

### What real engineering problem does this solve?

C++ bugs are hard to find:

```
BUG TYPES:                          TOOLS:
┌─────────────────────────────┐     ┌─────────────────────────────┐
│ Memory errors (UB)          │ ←── │ AddressSanitizer (ASan)     │
│ - Use after free            │     │ MemorySanitizer (MSan)      │
│ - Buffer overflow           │     └─────────────────────────────┘
│ - Double free               │     
├─────────────────────────────┤     ┌─────────────────────────────┐
│ Data races                  │ ←── │ ThreadSanitizer (TSan)      │
│ - Concurrent access         │     └─────────────────────────────┘
│ - Missing synchronization   │     
├─────────────────────────────┤     ┌─────────────────────────────┐
│ Undefined behavior          │ ←── │ UBSanitizer (UBSan)         │
│ - Signed overflow           │     └─────────────────────────────┘
│ - Null dereference          │     
├─────────────────────────────┤     ┌─────────────────────────────┐
│ Logic errors, code smells   │ ←── │ Static analyzers            │
│ - Unused variables          │     │ clang-tidy, cppcheck        │
│ - Missing return            │     └─────────────────────────────┘
└─────────────────────────────┘
```

**中文说明：**
C++ 的未定义行为和内存错误可能表现为随机崩溃、数据损坏或"恰好能用"。Sanitizer 是运行时检测工具，在问题发生时立即报告，而非让错误默默传播。静态分析在编译时检查代码，找出潜在问题。

---

## 2. Core Tools

### AddressSanitizer (ASan)

```bash
# Compile with ASan
g++ -fsanitize=address -g program.cpp -o program
clang++ -fsanitize=address -g program.cpp -o program

# Detects:
# - Heap/stack buffer overflow
# - Use after free
# - Use after return
# - Double free
# - Memory leaks
```

### ThreadSanitizer (TSan)

```bash
# Compile with TSan
g++ -fsanitize=thread -g program.cpp -o program

# Detects:
# - Data races
# - Deadlocks (limited)
```

### UndefinedBehaviorSanitizer (UBSan)

```bash
# Compile with UBSan
g++ -fsanitize=undefined -g program.cpp -o program

# Detects:
# - Signed integer overflow
# - Null pointer dereference
# - Invalid shift
# - Out-of-bounds array access
# - Misaligned access
```

### Combine Sanitizers

```bash
# ASan + UBSan (common combination)
g++ -fsanitize=address,undefined -g program.cpp

# Note: TSan cannot be combined with ASan
```

---

## 3. Static Analysis

### clang-tidy

```bash
# Run clang-tidy
clang-tidy source.cpp -- -std=c++20

# With config file (.clang-tidy)
clang-tidy source.cpp

# Fix issues automatically
clang-tidy -fix source.cpp
```

```yaml
# .clang-tidy example
Checks: >
  bugprone-*,
  cppcoreguidelines-*,
  modernize-*,
  performance-*,
  readability-*,
  -modernize-use-trailing-return-type

WarningsAsErrors: ''
HeaderFilterRegex: '.*'
```

### cppcheck

```bash
# Run cppcheck
cppcheck --enable=all --std=c++20 source.cpp

# Suppress false positives
cppcheck --suppress=unusedFunction source.cpp
```

---

## 4. Complete Example

```cpp
// bugs.cpp - Example code with various bugs
#include <iostream>
#include <thread>
#include <vector>

// ============================================================
// Bug 1: Buffer overflow (detected by ASan)
// ============================================================
void bufferOverflow() {
    int arr[10];
    arr[10] = 42;  // Out of bounds write!
}

// ============================================================
// Bug 2: Use after free (detected by ASan)
// ============================================================
void useAfterFree() {
    int* p = new int(42);
    delete p;
    *p = 100;  // Use after free!
}

// ============================================================
// Bug 3: Data race (detected by TSan)
// ============================================================
void dataRace() {
    int counter = 0;
    
    std::thread t1([&]() {
        for (int i = 0; i < 1000; ++i) {
            ++counter;  // Data race!
        }
    });
    
    std::thread t2([&]() {
        for (int i = 0; i < 1000; ++i) {
            ++counter;  // Data race!
        }
    });
    
    t1.join();
    t2.join();
    std::cout << counter << "\n";
}

// ============================================================
// Bug 4: Signed overflow (detected by UBSan)
// ============================================================
void signedOverflow() {
    int x = INT_MAX;
    x = x + 1;  // Signed overflow is UB!
}

// ============================================================
// Bug 5: Null dereference (detected by UBSan)
// ============================================================
void nullDeref() {
    int* p = nullptr;
    *p = 42;  // Null dereference!
}

// ============================================================
// Bug 6: Memory leak (detected by ASan with leak detection)
// ============================================================
void memoryLeak() {
    int* p = new int(42);
    // Missing delete!
}

// ============================================================
// Test with specific sanitizer
// ============================================================
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <test_number>\n";
        std::cout << "  1 - Buffer overflow\n";
        std::cout << "  2 - Use after free\n";
        std::cout << "  3 - Data race\n";
        std::cout << "  4 - Signed overflow\n";
        std::cout << "  5 - Null dereference\n";
        std::cout << "  6 - Memory leak\n";
        return 1;
    }
    
    int test = std::stoi(argv[1]);
    
    switch (test) {
        case 1: bufferOverflow(); break;
        case 2: useAfterFree(); break;
        case 3: dataRace(); break;
        case 4: signedOverflow(); break;
        case 5: nullDeref(); break;
        case 6: memoryLeak(); break;
        default: std::cout << "Unknown test\n";
    }
    
    return 0;
}
```

### Build and Test

```bash
# Test ASan
g++ -fsanitize=address -g bugs.cpp -o bugs_asan
./bugs_asan 1  # Buffer overflow
./bugs_asan 2  # Use after free

# Test TSan
g++ -fsanitize=thread -g bugs.cpp -o bugs_tsan -pthread
./bugs_tsan 3  # Data race

# Test UBSan
g++ -fsanitize=undefined -g bugs.cpp -o bugs_ubsan
./bugs_ubsan 4  # Signed overflow
./bugs_ubsan 5  # Null dereference
```

---

## 5. Best Practices

### CI Integration

```yaml
# .github/workflows/sanitizers.yml
jobs:
  sanitizers:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        sanitizer: [address, thread, undefined]
    steps:
      - uses: actions/checkout@v2
      - name: Build with sanitizer
        run: |
          g++ -fsanitize=${{ matrix.sanitizer }} -g \
              tests/*.cpp -o test_${{ matrix.sanitizer }}
      - name: Run tests
        run: ./test_${{ matrix.sanitizer }}
```

### CMake Integration

```cmake
# CMakeLists.txt
option(ENABLE_SANITIZERS "Enable sanitizers" OFF)

if(ENABLE_SANITIZERS)
    set(SANITIZER_FLAGS "-fsanitize=address,undefined -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SANITIZER_FLAGS}")
endif()
```

---

## 6. Summary

```
+------------------------------------------------------------------+
|              SANITIZER & ANALYSIS QUICK REFERENCE                 |
+------------------------------------------------------------------+
|                                                                  |
|  RUNTIME SANITIZERS (use during testing):                        |
|                                                                  |
|  ASan (-fsanitize=address):                                      |
|    • Buffer overflow, use-after-free, double-free, leaks         |
|    • ~2x slowdown                                                |
|                                                                  |
|  TSan (-fsanitize=thread):                                       |
|    • Data races, some deadlocks                                  |
|    • ~5-10x slowdown, high memory usage                          |
|    • Cannot combine with ASan                                    |
|                                                                  |
|  UBSan (-fsanitize=undefined):                                   |
|    • Signed overflow, null deref, invalid shift                  |
|    • Minimal overhead, combine with ASan                         |
|                                                                  |
|  STATIC ANALYSIS (use in CI):                                    |
|    • clang-tidy: Comprehensive C++ linting                       |
|    • cppcheck: Fast, low false positive rate                     |
|    • Compiler warnings: -Wall -Wextra -Werror                    |
|                                                                  |
|  BEST PRACTICE:                                                  |
|    • Run ASan+UBSan on all tests in CI                           |
|    • Run TSan on multithreaded tests separately                  |
|    • Enable all compiler warnings as errors                      |
|    • Run clang-tidy on code changes                              |
|                                                                  |
+------------------------------------------------------------------+
```

