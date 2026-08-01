# Go 核心知识点代码示例 —— ⭐⭐☆☆☆ reflect、unsafe、cgo

> 本文档拆分自 [core_concepts.md](./core_concepts.md) 中"80/20 学习优先级"表格的第四梯队（最低优先级）。
>
> 这一梯队是**特殊场景才需要**的工具：`reflect` 用于运行时检查/操作类型信息（框架、序列化库常用）；`unsafe` 绕过类型系统直接操作内存（性能极限场景）；`cgo` 用于调用 C 代码（复用 C 生态或系统调用）。日常业务代码应尽量避免直接使用它们，按需学习即可。

---

# 1. reflect、unsafe、cgo

## 核心思想

```
reflect = 运行时检查/操作类型信息
unsafe  = 绕过类型系统，直接操作内存
cgo     = Go 与 C 代码互相调用的桥梁
```

三者都是"逃离 Go 正常类型系统/内存模型"的手段，代价是性能、可读性、可移植性或类型安全，因此只在确实需要的场景使用。

## 为什么重要

- 理解 `reflect` 有助于读懂 `encoding/json`、ORM、依赖注入等框架的实现原理
- `unsafe` 常出现在高性能库（自定义序列化、内存池）的源码中，读懂即可，不代表要常写
- `cgo` 是 Go 与 C 生态（某些 SDK、驱动、系统调用）桥接的标准方式
- 这三者都有明显代价，应该"按需学习，谨慎使用"，而不是主动在业务代码里追求

## 代码示例

### 示例 1：reflect.TypeOf / reflect.ValueOf——运行时获取类型信息

```go
package main

import (
	"fmt"
	"reflect"
)

func main() {
	x := 42
	t := reflect.TypeOf(x)
	v := reflect.ValueOf(x)

	fmt.Println("type:", t)        // int
	fmt.Println("kind:", t.Kind()) // int
	fmt.Println("value:", v)       // 42
}
```

### 示例 2：遍历 struct 字段（序列化库/ORM 的实现原理）

```go
package main

import (
	"fmt"
	"reflect"
)

type User struct {
	Name string `json:"name"`
	Age  int    `json:"age"`
}

func main() {
	u := User{Name: "Alice", Age: 30}
	t := reflect.TypeOf(u)
	v := reflect.ValueOf(u)

	for i := 0; i < t.NumField(); i++ {
		field := t.Field(i)
		value := v.Field(i)
		tag := field.Tag.Get("json")
		fmt.Printf("field=%s tag=%s value=%v\n", field.Name, tag, value)
	}
}
```

### 示例 3：通过 reflect 动态修改字段值（需要传指针）

```go
package main

import (
	"fmt"
	"reflect"
)

type Config struct {
	Name string
}

func setField(obj any, fieldName string, newValue string) {
	v := reflect.ValueOf(obj).Elem() // 必须是指针才能修改
	field := v.FieldByName(fieldName)
	if field.IsValid() && field.CanSet() {
		field.SetString(newValue)
	}
}

func main() {
	c := &Config{Name: "old"}
	setField(c, "Name", "new")
	fmt.Println(c.Name) // new
}
```

### 示例 4：unsafe.Sizeof / unsafe.Alignof——查看内存布局

```go
package main

import (
	"fmt"
	"unsafe"
)

type Point struct {
	X int32
	Y int32
}

func main() {
	var p Point
	fmt.Println("size:", unsafe.Sizeof(p))            // 8
	fmt.Println("align:", unsafe.Alignof(p))           // 4
	fmt.Println("offset of Y:", unsafe.Offsetof(p.Y)) // 4
}
```

### 示例 5：unsafe.Pointer——绕过类型系统的指针转换（高风险，谨慎使用）

```go
package main

import (
	"fmt"
	"unsafe"
)

func main() {
	var f float64 = 3.14
	// 把 *float64 强行当作 *uint64 来读取它的原始位模式
	bits := *(*uint64)(unsafe.Pointer(&f))
	fmt.Printf("float64 %v raw bits: %x\n", f, bits)
	// 正常业务代码应使用 math.Float64bits(f)，而不是手写 unsafe 转换
}
```

### 示例 6：cgo——调用 C 标准库函数

```go
package main

/*
#include <stdio.h>
#include <math.h>
*/
import "C"
import "fmt"

func main() {
	C.puts(C.CString("hello from C\n")) // 调用C标准库的puts函数
	result := C.sqrt(C.double(16.0))    // 调用C标准库的sqrt函数
	fmt.Println("sqrt(16) from C:", float64(result))
}
```

```bash
# cgo需要CGO_ENABLED=1（默认开启，交叉编译时常需要关闭）
CGO_ENABLED=1 go run main.go
```

## 快速上手小结

先问"标准库/普通 Go 代码能不能实现"，答案是"能"就不要用这三者；只有在写框架底层、极致性能优化、或必须复用 C 代码时才引入 `reflect`/`unsafe`/`cgo`，并且尽量把它们封装在一个很小的内部模块里。

---

# 回到总览

本文档覆盖了 [core_concepts.md](./core_concepts.md) 优先级表格中 ⭐⭐☆☆☆ 的知识点，也是四个优先级文档的最后一篇：

- [priority-p0-core.md](./priority-p0-core.md) —— ⭐⭐⭐⭐⭐ 核心数据模型与工程组织
- [priority-p1-concurrency.md](./priority-p1-concurrency.md) —— ⭐⭐⭐⭐☆ 并发与标准库
- [priority-p2-runtime.md](./priority-p2-runtime.md) —— ⭐⭐⭐☆☆ Runtime、GC
