# C —— 内存（Memory）

> **核心驱动力：程序就是对内存和地址的操作。**
> C 程序员不问"这是什么对象"，而问"这块数据存放在哪个地址，占多少字节"。

---

## 心智模型图解

```
Memory（一整块地址空间）
     │
     ▼
Pointer（地址，指向内存中的某个位置）
     │
     ▼
Bytes（该位置往后 N 个字节，如何解读取决于类型）
```

C 没有"对象"这个运行期概念，只有"内存 + 如何解释这段内存"。
指针只是一个数字（地址），类型只是告诉编译器"从这个地址开始，按多少字节、什么方式去读写"。

---

## 核心驱动力详解

- **变量就是内存的别名**：`int x` 不是"一个整数对象"，而是"给某块 4 字节内存起了个名字 x"。
- **指针是地址，不是对象**：`*p` 不是"箭头指向的东西"，而是"对地址 p 处内存的一次访问"。
- **类型决定步长和解释方式**：`int *p` 里 `p+1` 跳过的是 `sizeof(int)` 字节，不是"下一个元素"这种抽象概念。
- **没有自动回收，内存的生命周期完全由程序员管理**：`malloc` 申请，`free` 释放，中间的每一步都要自己负责。

理解这一点后，C 的很多"坑"（悬垂指针、内存泄漏、越界）本质上都是"对内存生命周期/边界的估计出错"。

---

## 典型代码片段

### 1. 指针即地址，`*`/`&` 是"解引用"和"取地址"

```c
int
main(void)
{
	int x = 10;
	int *p = &x; /* p 存的是 x 所在内存的地址 */

	*p = 20;     /* 通过地址修改内存内容 */
	printf("x=%d\n", x); /* x=20，因为 p 和 x 指向同一块内存 */

	return 0;
}
```

**心智模型解读**：`p` 不是"x 的别名对象"，`p` 本身是一个存着地址值的变量；
`*p` 是"去这个地址取值/写值"的动作，这就是为什么改 `*p` 会影响 `x`。

### 2. 指针运算 —— 按类型大小移动地址

```c
int
main(void)
{
	int arr[4] = {10, 20, 30, 40};
	int *p = arr;

	printf("%d\n", *(p + 2)); /* 30，p+2 跳过了 2*sizeof(int) 字节 */
	printf("%d\n", *(p + 2) == arr[2]); /* 1，p[i] 和 arr[i] 是同一件事 */

	return 0;
}
```

**心智模型解读**：`p + 2` 不是"地址数值加 2"，而是"加 2 个 `int` 的大小"（通常是 8 字节）。
`arr[i]` 在编译器眼里就是 `*(arr + i)` 的语法糖——数组下标本质上是指针运算。

### 3. `malloc`/`free` —— 手动管理堆内存的生命周期

```c
int *
make_buffer(size_t n)
{
	int *buf;

	buf = malloc(n * sizeof(int)); /* 申请一块堆内存，返回起始地址 */
	if(!buf)
		return NULL;

	memset(buf, 0, n * sizeof(int));
	return buf;
}

int
main(void)
{
	int *buf = make_buffer(100);

	if(!buf)
		return 1;

	/* ... 使用 buf ... */

	free(buf);   /* 必须由程序员显式释放，否则内存泄漏 */
	buf = NULL;  /* 避免之后误用悬垂指针 */
	return 0;
}
```

**心智模型解读**：`malloc` 返回的这块内存"不属于"任何自动管理机制，
它的生命周期完全靠"谁申请、谁负责 free"这条纪律来维持——这正是 C 的内存心智模型的核心。

### 4. struct 内存布局与对齐

```c
struct point {
	char  tag;   /* 1 字节 */
	int   x;     /* 4 字节，但会因对齐在 tag 后插入 3 字节 padding */
	short y;     /* 2 字节 */
};

int
main(void)
{
	printf("sizeof(struct point) = %zu\n", sizeof(struct point));
	/* 常见输出 12，而不是 1+4+2=7，因为编译器插入了对齐填充 */

	return 0;
}
```

**心智模型解读**：struct 不是"字段的抽象集合"，而是"一段连续内存，按字段声明顺序、
按对齐规则排布"。理解内存布局，才能理解为什么 `sizeof` 经常比字段直接相加要大。

### 5. 数组名在表达式中"退化"为指针

```c
void
print_first(int arr[]) /* 形参写成数组，实际上退化成 int * */
{
	printf("%d\n", arr[0]);
	printf("sizeof(arr) in function = %zu\n", sizeof(arr)); /* 8，指针的大小，不是数组的大小! */
}

int
main(void)
{
	int nums[10] = {1};

	printf("sizeof(nums) in main = %zu\n", sizeof(nums)); /* 40，真正数组的大小 */
	print_first(nums);

	return 0;
}
```

**心智模型解读**：数组作为函数参数传递时，会"退化"成一个指向首元素的指针，
函数内部已经无法用 `sizeof` 得知原数组的元素个数——这是新手最容易踩的内存认知坑。

### 6. 二级指针 —— 通过地址修改"另一个地址"

```c
void
allocate(int **out, size_t n)
{
	*out = malloc(n * sizeof(int)); /* 修改调用者那个指针变量本身指向的地址 */
}

int
main(void)
{
	int *buf = NULL;

	allocate(&buf, 10); /* 传入 buf 的地址，让函数能修改 buf 本身 */
	buf[0] = 42;
	printf("%d\n", buf[0]);

	free(buf);
	return 0;
}
```

**心智模型解读**：普通指针参数只能让函数改"指针指向的内容"，改不了"指针变量本身"；
要修改指针变量本身（让它指向新分配的内存），必须再传一层地址，即"指向指针的指针"。

### 7. 函数指针 —— 把"一段代码的地址"当成数据

```c
typedef int (*cmp_fn)(int, int);

int
less_than(int a, int b)
{
	return a < b;
}

void
apply(int a, int b, cmp_fn fn)
{
	printf("%d\n", fn(a, b));
}

int
main(void)
{
	apply(3, 5, less_than); /* 把函数的地址当作参数传递 */
	return 0;
}
```

**心智模型解读**：函数在内存里也是一段"代码字节"，函数名同样会退化成"指向这段代码的地址"，
函数指针只是"把地址存起来，之后可以跳过去执行"——这是 C 里"回调"能实现的底层原因。

### 8. union —— 让多个字段共享同一块内存

```c
union value {
	int   i;
	float f;
	char  bytes[4];
};

int
main(void)
{
	union value v;

	v.i = 1;
	printf("%d %d %d %d\n", v.bytes[0], v.bytes[1], v.bytes[2], v.bytes[3]);
	/* 在小端机器上输出 1 0 0 0，因为 v.i 和 v.bytes 是同一块 4 字节内存 */

	return 0;
}
```

**心智模型解读**：`union` 里所有字段的起始地址相同，`sizeof(union)` 等于最大字段的大小，
写一个字段会直接影响其他字段"看到"的字节——这直观展示了"类型只是解释内存的方式"这一核心思想。

### 9. 栈内存 vs 堆内存的生命周期差异

```c
int *
bad_pointer(void)
{
	int local = 42;

	return &local; /* 危险：local 在栈上，函数返回后这块内存已失效 */
}

int *
good_pointer(void)
{
	int *p = malloc(sizeof(int));

	*p = 42;
	return p; /* 安全：堆内存的生命周期不受函数返回影响，但调用者要记得 free */
}
```

**心智模型解读**：栈内存的生命周期跟着"作用域/函数调用"走，函数返回后局部变量占用的栈空间
就可能被后续调用覆写；堆内存的生命周期完全由程序员通过 `malloc`/`free` 决定，与调用栈无关。

### 10. 指针类型转换 / Type Punning —— 换一种方式解读同一块内存

```c
int
main(void)
{
	float f = 3.14f;
	unsigned char *bytes = (unsigned char *)&f;
	int i;

	for(i = 0; i < (int)sizeof(float); i++)
		printf("%02x ", bytes[i]); /* 逐字节打印 float 的原始位模式 */
	printf("\n");

	return 0;
}
```

**心智模型解读**：把 `float *` 强转成 `unsigned char *`，并没有改变内存里的任何一个字节，
只是改变了"从这块内存读取时该按什么规则解释"——这再次印证"类型是解释方式，不是内存本身"。

### 11. 越界访问 —— 内存心智模型崩塌的典型后果

```c
int
main(void)
{
	int arr[3] = {1, 2, 3};

	arr[3] = 99; /* 越界写入，arr[3] 已经不属于这个数组，行为未定义 */
	printf("%d\n", arr[3]); /* 可能"看起来正常"，也可能崩溃或破坏其他数据 */

	return 0;
}
```

**心智模型解读**：C 不会在运行期检查数组边界，`arr[3]` 只是"`arr` 起始地址往后跳 3 个 int"
这样一个纯粹的地址计算，跳到了数组之外，写坏的可能是别的变量、返回地址，甚至更危险的东西。

---

## 黄金法则

> **不要想着对象，而要想着内存布局。**

看到一个变量/指针/struct，先问自己：它占多少字节？存放在栈上还是堆上？
它的地址是什么，生命周期到什么时候结束？谁负责回收它？

---

## 常见误区对比

### 误区一：返回指向局部变量的指针

```c
/* 错误心智模型：以为返回的指针会"带走"它指向的数据 */
char *
build_message(const char *name)
{
	char buf[64];

	snprintf(buf, sizeof(buf), "Hello, %s!", name);
	return buf; /* 危险：buf 是栈上局部数组，函数返回后失效 */
}
```

```c
/* C 习惯写法：调用者提供缓冲区，或者用 malloc 转移所有权到堆 */
char *
build_message(const char *name)
{
	char *buf = malloc(64);

	if(!buf)
		return NULL;
	snprintf(buf, 64, "Hello, %s!", name);
	return buf; /* 调用者负责之后 free(buf) */
}
```

**为什么后者更好**：栈内存在函数返回后就可能被后续调用覆写，
只有堆内存（或调用者提供的缓冲区）的生命周期能跨越函数调用边界。

### 误区二：`free` 之后继续使用指针（Use-After-Free）

```c
/* 错误心智模型：以为 free 只是"标记"，指针依然指向"有效"的旧数据 */
int *p = malloc(sizeof(int));
*p = 5;
free(p);
printf("%d\n", *p); /* 未定义行为：这块内存可能已被其他 malloc 复用 */
```

```c
/* C 习惯写法：free 之后立刻置空，避免悬垂指针被误用 */
int *p = malloc(sizeof(int));
*p = 5;
free(p);
p = NULL;
if(p)
	printf("%d\n", *p); /* 永远不会执行，从源头避免 use-after-free */
```

**为什么后者更好**：`free(p)` 之后，`p` 所指的内存随时可能被系统重新分配给别的用途，
继续解引用是未定义行为；养成"free 后立即置 NULL"的习惯，能大幅降低悬垂指针风险。

---

## 快速上手 Checklist

- [ ] 看到一个变量，能说出它占多少字节、存放在栈还是堆上吗？
- [ ] 能分清"数组下标"和"指针运算"其实是同一件事（`arr[i]` == `*(arr+i)`）吗？
- [ ] 每一次 `malloc`，能立刻想到"谁负责、在什么时机 `free`"吗？
- [ ] 理解为什么数组作为函数参数传递后会"退化"成指针、丢失长度信息吗？
- [ ] 遇到 struct，能大致估算出它的内存布局和对齐产生的 padding 吗？

---

上一篇：[Rust —— 所有权](rust.md) ・ 下一篇：[C++ —— 生命周期](cpp.md)
