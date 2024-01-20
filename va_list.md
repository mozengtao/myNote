- `va_list` 是一个只包含单一结构体成员的数组，它包含了实现 `va_arg` 宏所必要的信息。定义如下
  ```
  typedef struct {
     unsigned int gp_offset;
     unsigned int fp_offset;
     void *overflow_arg_area;
     void *reg_save_area;
  } va_list[1];
  ```
- `va_start` 宏初始化 `va_list` 结构的结果如下
	- `reg_save_area` 指向**寄存器保存区域的开始位置**
	- `overflow_arg_area` 用来获取栈上保存的参数，它被初始化为**栈上第一个参数的地址**，之后每次更新指向栈上下一个参数的起始位置
	- `gp_offset` 以字节为单位保存了从 `reg_save_area` 到下一个可用的**通用参数寄存器**之间的偏移。为了防止所有的参数寄存器被耗尽，它的值被设置为 48(6*8)
	- `fp_offset` 以字节为单位保存了从 `reg_save_area` 到下一个可用的**浮点参数寄存器**之间的偏移。为了防止所有的参数寄存器被耗尽，它的值被设置为 304 (6 * 8 + 16 * 16).
- 关于寄存器使用可以参考[System V Application Binary Interface](https://docslib.org/doc/9478253/x86-64-psabi-1-0-pdf)
- C 标准库 API 可参考[Standard C Library Functions](https://www.ibm.com/docs/en/i/7.3?topic=extensions-standard-c-library-functions-table-by-name)
- `va_list` 应用举例
	- ```c
	  #include <stdio.h>
	  #include <stdarg.h>
	  #include <math.h>
	   
	  double sample_stddev(int count, ...) 
	  {
	      /* Compute the mean with args1. */
	      double sum = 0;
	      va_list args1;
	      va_start(args1, count);
	      va_list args2;
	      va_copy(args2, args1);   /* copy va_list object */
	      for (int i = 0; i < count; ++i) {
	          double num = va_arg(args1, double);
	          sum += num;
	      }
	      va_end(args1);
	      double mean = sum / count;
	   
	      /* Compute standard deviation with args2 and mean. */
	      double sum_sq_diff = 0;
	      for (int i = 0; i < count; ++i) {
	          double num = va_arg(args2, double);
	          sum_sq_diff += (num-mean) * (num-mean);
	      }
	      va_end(args2);
	      return sqrt(sum_sq_diff / count);
	  }
	   
	  int main(void) 
	  {
	      printf("%f\n", sample_stddev(4, 25.0, 27.3, 26.9, 25.7));
	  }
	  ```
-