## C语言可执行程序的数据段大小是如何确定的
- c program's memory layout
```
+-------------------------+
| Stack (grows downward)  |
+-------------------------+
| Heap (grows upward)     |
+-------------------------+
| BSS Segment             | ← uninitialized globals/static vars
+-------------------------+
| Data Segment            | ← initialized globals/static vars
+-------------------------+
| Text Segment            | ← compiled code (functions)
+-------------------------+
```
- 编译和链接共同决定了数据段的大小
```
| 段名        | 作用                                      | 举例变量                           | 是否占用文件空间   |
| ---------   | ------------------------                 | ------------------------------     | ---------- |
| `.text`     | 程序指令（机器码）                         | 函数体                             | ✅          |
| `.data`     | **已初始化的全局/静态变量**                 | `int a = 5; static int b = 6;`    | ✅          |
| `.bss`      | **未初始化（或初始化为0）的全局/静态变量**   | `int c; static int d;`             | ❌（只在运行时分配） |
| `.rodata`   | 只读数据（常量字符串、`const`变量）          | `"Hello"` / `const int x = 1;`    | ✅          |

编译阶段(.c -> .o)
	编译器（如 gcc）会为每个源文件生成一个目标文件 .o
	编译器根据变量的初始化情况，把符号放入不同的段：
		已初始化的 → .data
		未初始化或初始化为0的 → .bss
		字符串常量或 const → .rodata
	编译器会生成这些段的“符号表项”，记录变量大小、对齐要求
👉 每个 .o 文件自己有独立的 .data、.bss 段

汇编与对齐
	编译器会在 .o 中指定每个段的对齐要求（alignment）：
		通常 .data 段按 8 或 16 字节对齐
		如果结构体中有大字段（例如 double），会增加对齐填充

链接阶段（.o → ELF 可执行文件）
链接器（ld）会：
	1. 把所有 .data 段拼接起来
	2. 计算总对齐、偏移
	3. 记录最终的段大小到 ELF header
	4. .bss 段不会实际写入文件，而只记录大小（加载时由内核分配零填充内存）
因此最终 .data 段的大小 = 所有已初始化静态/全局变量的大小 + 对齐填充


加载阶段
当程序运行时：
	Linux ELF loader（动态链接器）会：
		从可执行文件中 映射 .data 段内容到内存；
		为 .bss 段 分配相应大小的零填充空间；
		在进程虚拟地址空间中分别创建这两个内存区域
可以通过 /proc/<pid>/maps 看到它们的位置（通常相邻）
```

```c
#include <stdio.h>

int global_var = 10;
int global_var_uninitialized;

int main(void)
{
    static int static_var = 20;
    static int static_var_uninitialized;
    printf("global_var: %d\n", global_var);
    printf("static_var: %d\n", static_var);
    printf("global_var_uninitialized: %d\n", global_var_uninitialized);
    printf("static_var_uninitialized: %d\n", static_var_uninitialized);
    return 0;
}

gcc -o main main.c && size main
   text    data     bss     dec     hex filename
   1553     608      16    2177     881 main
```