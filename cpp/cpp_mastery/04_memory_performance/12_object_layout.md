# Topic 12: Object Layout, Alignment, and ABI

## 1. Problem Statement

### What real engineering problem does this solve?

Understanding memory layout affects performance and interoperability:

```
POOR LAYOUT                          GOOD LAYOUT
struct Bad {                         struct Good {
    char a;     // 1 byte            double d;   // 8 bytes
    // 7 padding                      int i;     // 4 bytes
    double d;   // 8 bytes            short s;   // 2 bytes
    short s;    // 2 bytes            char a;    // 1 byte
    // 2 padding                      char b;    // 1 byte
    char b;     // 1 byte           };            // 16 bytes total
    // 3 padding
};              // 24 bytes total!

WASTED: 12 bytes padding            WASTED: 0 bytes padding
```

### What goes wrong if ignored?

- **Cache inefficiency**: Padding wastes cache lines
- **Memory bloat**: Arrays waste significant memory
- **ABI incompatibility**: Different compilers/options = different layout
- **Serialization bugs**: Direct struct copy fails across systems

**中文说明：**
C++ 对象在内存中的布局受对齐要求影响。每种类型有自己的对齐要求（如 double 通常需要 8 字节对齐），编译器会插入填充字节来满足对齐。不理解布局会导致内存浪费、缓存效率低下、跨编译器兼容问题。

---

## 2. Core Idea

### Alignment Rules

```
TYPE            SIZE    ALIGNMENT    NOTES
────────────────────────────────────────────────────────────
char            1       1            Always aligned
short           2       2            Must be 2-byte aligned
int             4       4            Must be 4-byte aligned
long            8       8            (on 64-bit)
float           4       4
double          8       8
void*           8       8            (on 64-bit)
struct/class    varies  max member   Struct alignment = largest member
```

### Padding Rules

```cpp
struct Example {
    char c;     // offset 0, size 1
    // 3 bytes padding to align int
    int i;      // offset 4, size 4
    char d;     // offset 8, size 1
    // 7 bytes padding to align double
    double x;   // offset 16, size 8
};              // Total: 24 bytes, alignment: 8

// Reordered for efficiency:
struct Better {
    double x;   // offset 0, size 8
    int i;      // offset 8, size 4
    char c;     // offset 12, size 1
    char d;     // offset 13, size 1
    // 2 bytes padding to make size multiple of alignment
};              // Total: 16 bytes, alignment: 8
```

**中文说明：**
编译器在成员之间插入填充以满足对齐：
1. 每个成员的起始地址必须是其对齐值的倍数
2. 结构体总大小必须是其对齐值的倍数
3. 结构体的对齐值是其最大成员的对齐值

优化：按对齐要求从大到小排列成员。

---

## 3. Idiomatic C++ Techniques

### Checking Layout

```cpp
#include <iostream>
#include <cstddef>

struct Example {
    char a;
    double b;
    int c;
};

int main() {
    std::cout << "Size: " << sizeof(Example) << "\n";
    std::cout << "Alignment: " << alignof(Example) << "\n";
    std::cout << "Offset a: " << offsetof(Example, a) << "\n";
    std::cout << "Offset b: " << offsetof(Example, b) << "\n";
    std::cout << "Offset c: " << offsetof(Example, c) << "\n";
}
```

### Controlling Alignment

```cpp
// alignas specifier
struct alignas(16) Vector4 {
    float x, y, z, w;  // 16 bytes, 16-byte aligned (for SIMD)
};

// Aligned storage
alignas(64) char cacheLineBuffer[64];  // Cache-line aligned

// std::aligned_storage (deprecated in C++23)
std::aligned_storage_t<sizeof(T), alignof(T)> storage;
```

### Packed Structures (Caution!)

```cpp
// Disable padding (compiler-specific)
#pragma pack(push, 1)
struct Packed {
    char a;
    int b;    // Unaligned access! May be slow or crash on some platforms
    double c;
};
#pragma pack(pop)

// Use with extreme caution - only for serialization/protocols
```

---

## 4. Complete C++ Example

```cpp
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <new>

// ============================================================
// Demonstrating padding and alignment
// ============================================================
struct BadLayout {
    char a;     // 1
    double b;   // 8 (but needs 7 padding before)
    char c;     // 1
    int d;      // 4 (but needs 3 padding before)
};

struct GoodLayout {
    double b;   // 8
    int d;      // 4
    char a;     // 1
    char c;     // 1
    // 2 bytes padding at end
};

// ============================================================
// Cache-line aware structure
// ============================================================
struct alignas(64) CacheLineAligned {
    int hotData[15];     // Frequently accessed
    int padding;         // Fill to 64 bytes
};

// Prevent false sharing in multithreading
struct alignas(64) PerThreadData {
    int counter;
    char padding[60];    // Ensure each thread has own cache line
};

// ============================================================
// ABI-stable structure for serialization
// ============================================================
#pragma pack(push, 1)
struct NetworkPacket {
    uint32_t magic;      // 4 bytes
    uint16_t version;    // 2 bytes
    uint16_t length;     // 2 bytes
    uint8_t data[1024];  // Variable length in practice
};
#pragma pack(pop)

static_assert(offsetof(NetworkPacket, magic) == 0);
static_assert(offsetof(NetworkPacket, version) == 4);
static_assert(offsetof(NetworkPacket, length) == 6);
static_assert(offsetof(NetworkPacket, data) == 8);

// ============================================================
// Placement new with alignment
// ============================================================
template<typename T>
class AlignedStorage {
    alignas(T) char buffer_[sizeof(T)];
    bool constructed_ = false;
    
public:
    template<typename... Args>
    void construct(Args&&... args) {
        new (buffer_) T(std::forward<Args>(args)...);
        constructed_ = true;
    }
    
    void destroy() {
        if (constructed_) {
            reinterpret_cast<T*>(buffer_)->~T();
            constructed_ = false;
        }
    }
    
    T& get() { return *reinterpret_cast<T*>(buffer_); }
    
    ~AlignedStorage() { destroy(); }
};

// ============================================================
// Bit fields for compact storage
// ============================================================
struct Flags {
    unsigned int enabled : 1;
    unsigned int mode : 3;      // 0-7
    unsigned int priority : 4;  // 0-15
    unsigned int reserved : 24;
};

static_assert(sizeof(Flags) == 4);

void printLayout() {
    std::cout << "=== Layout Analysis ===\n\n";
    
    std::cout << "BadLayout:\n";
    std::cout << "  sizeof: " << sizeof(BadLayout) << "\n";
    std::cout << "  alignof: " << alignof(BadLayout) << "\n";
    std::cout << "  offset a: " << offsetof(BadLayout, a) << "\n";
    std::cout << "  offset b: " << offsetof(BadLayout, b) << "\n";
    std::cout << "  offset c: " << offsetof(BadLayout, c) << "\n";
    std::cout << "  offset d: " << offsetof(BadLayout, d) << "\n";
    
    std::cout << "\nGoodLayout:\n";
    std::cout << "  sizeof: " << sizeof(GoodLayout) << "\n";
    std::cout << "  alignof: " << alignof(GoodLayout) << "\n";
    std::cout << "  offset b: " << offsetof(GoodLayout, b) << "\n";
    std::cout << "  offset d: " << offsetof(GoodLayout, d) << "\n";
    std::cout << "  offset a: " << offsetof(GoodLayout, a) << "\n";
    std::cout << "  offset c: " << offsetof(GoodLayout, c) << "\n";
    
    std::cout << "\nSaved: " << (sizeof(BadLayout) - sizeof(GoodLayout)) 
              << " bytes per object\n";
}

int main() {
    printLayout();
    
    std::cout << "\n=== Cache Line Alignment ===\n";
    std::cout << "CacheLineAligned size: " << sizeof(CacheLineAligned) << "\n";
    std::cout << "CacheLineAligned align: " << alignof(CacheLineAligned) << "\n";
    
    std::cout << "\n=== Packed Network Struct ===\n";
    std::cout << "NetworkPacket size: " << sizeof(NetworkPacket) << "\n";
    
    std::cout << "\n=== Bit Fields ===\n";
    Flags f{};
    f.enabled = 1;
    f.mode = 5;
    f.priority = 10;
    std::cout << "Flags size: " << sizeof(f) << " bytes\n";
    std::cout << "enabled=" << f.enabled 
              << ", mode=" << f.mode 
              << ", priority=" << f.priority << "\n";
    
    return 0;
}
```

---

## 5. Failure Modes

### Mistake 1: Assuming struct size

```cpp
// BAD: Assuming contiguous layout
struct Data { char a; int b; };
memcpy(buffer, &data, 5);  // Wrong! sizeof(Data) is 8, not 5

// GOOD: Use actual size
memcpy(buffer, &data, sizeof(Data));
```

### Mistake 2: Unaligned access

```cpp
// BAD: May crash on some architectures
char buffer[10];
int* p = (int*)(buffer + 1);  // Unaligned!
*p = 42;  // Undefined behavior on strict alignment platforms

// GOOD: Use memcpy for unaligned data
int value = 42;
memcpy(buffer + 1, &value, sizeof(int));
```

### Mistake 3: Pointer size assumptions

```cpp
// BAD: Assumes 32-bit
struct Message {
    int type;
    void* data;  // 4 bytes on 32-bit, 8 bytes on 64-bit!
};
// Size varies between platforms!
```

---

## 6. Summary

```
+------------------------------------------------------------------+
|              OBJECT LAYOUT BEST PRACTICES                         |
+------------------------------------------------------------------+
|                                                                  |
|  MINIMIZE PADDING:                                               |
|    □ Order members by alignment (largest first)                  |
|    □ Group same-sized members together                           |
|    □ Use static_assert to verify expected size                   |
|                                                                  |
|  CONTROL ALIGNMENT:                                              |
|    □ Use alignas for SIMD or cache-line alignment                |
|    □ Use #pragma pack only for wire protocols                    |
|    □ Avoid unaligned access                                      |
|                                                                  |
|  PORTABLE CODE:                                                  |
|    □ Use fixed-width types (uint32_t) for serialization          |
|    □ Don't assume pointer size                                   |
|    □ Use offsetof for field positions                            |
|                                                                  |
+------------------------------------------------------------------+
```

