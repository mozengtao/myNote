- module_init
  id:: 640eddd9-8726-4b79-a618-6de32ef68adf
	- ```c
	  include/linux/init.h
	  
	  /**
	   * module_init() - driver initialization entry point
	   * @x: function to be run at kernel boot time or module insertion
	   * 
	   * module_init() will either be called during do_initcalls() (if
	   * builtin) or at module insertion time (if a module).  There can only
	   * be one per module.
	   */
	  #define module_init(x)	__initcall(x);
	  	#define __initcall(fn) device_initcall(fn)
	      	#define device_initcall(fn)		__define_initcall(fn, 6)
	        	  #define __define_initcall(fn, id) \
	  				static initcall_t __initcall_##fn##id __used \
	  				__attribute__((__section__(".initcall" #id ".init"))) = fn; \
	  				LTO_REFERENCE_INITCALL(__initcall_##fn##id)
	  
	  
	  #ifdef CONFIG_LTO
	  /* Work around a LTO gcc problem: when there is no reference to a variable
	   * in a module it will be moved to the end of the program. This causes
	   * reordering of initcalls which the kernel does not like.
	   * Add a dummy reference function to avoid this. The function is
	   * deleted by the linker.
	   */
	  #define LTO_REFERENCE_INITCALL(x) \
	  	; /* yes this is needed */			\
	  	static __used __exit void *reference_##x(void)	\
	  	{						\
	  		return &x;				\
	  	}
	  #else
	  #define LTO_REFERENCE_INITCALL(x)
	  #endif
	  ```
	- ```c
	  # include/linux/compiler-gcc3.h
	  
	  #if GCC_VERSION >= 30300
	  # define __used                 __attribute__((__used__))
	  #else
	  # define __used                 __attribute__((__unused__))
	  #endif
	  
	  ```
	- ```c
	  include/linux/init.h
	  
	  /*
	   * Used for initialization calls..
	   */
	  typedef int (*initcall_t)(void);
	  typedef void (*exitcall_t)(void);
	  
	  /* initcalls are now grouped by functionality into separate 
	   * subsections. Ordering inside the subsections is determined
	   * by link order. 
	   * For backwards compatibility, initcall() puts the call in 
	   * the device init subsection.
	   *
	   * The `id' arg to __define_initcall() is needed so that multiple initcalls
	   * can point at the same handler without causing duplicate-symbol build errors.
	   */
	  
	  #define __define_initcall(fn, id) \
	  	static initcall_t __initcall_##fn##id __used \
	  	__attribute__((__section__(".initcall" #id ".init"))) = fn; \
	  	LTO_REFERENCE_INITCALL(__initcall_##fn##id)
	  
	  /*
	   * Early initcalls run before initializing SMP.
	   *
	   * Only for built-in code, not modules.
	   */
	  #define early_initcall(fn)		__define_initcall(fn, early)
	  
	  /*
	   * A "pure" initcall has no dependencies on anything else, and purely
	   * initializes variables that couldn't be statically initialized.
	   *
	   * This only exists for built-in code, not for modules.
	   * Keep main.c:initcall_level_names[] in sync.
	   */
	  #define pure_initcall(fn)		__define_initcall(fn, 0)
	  
	  #define core_initcall(fn)		__define_initcall(fn, 1)
	  #define core_initcall_sync(fn)		__define_initcall(fn, 1s)
	  #define postcore_initcall(fn)		__define_initcall(fn, 2)
	  #define postcore_initcall_sync(fn)	__define_initcall(fn, 2s)
	  #define arch_initcall(fn)		__define_initcall(fn, 3)
	  #define arch_initcall_sync(fn)		__define_initcall(fn, 3s)
	  #define subsys_initcall(fn)		__define_initcall(fn, 4)
	  #define subsys_initcall_sync(fn)	__define_initcall(fn, 4s)
	  #define fs_initcall(fn)			__define_initcall(fn, 5)
	  #define fs_initcall_sync(fn)		__define_initcall(fn, 5s)
	  #define rootfs_initcall(fn)		__define_initcall(fn, rootfs)
	  #define device_initcall(fn)		__define_initcall(fn, 6)
	  #define device_initcall_sync(fn)	__define_initcall(fn, 6s)
	  #define late_initcall(fn)		__define_initcall(fn, 7)
	  #define late_initcall_sync(fn)		__define_initcall(fn, 7s)
	  
	  #define __initcall(fn) device_initcall(fn)
	  ```
	- ```c
	  include/asm-generic/vmlinux.lds.h
	  
	  #define INIT_CALLS_LEVEL(level)						\
	  		VMLINUX_SYMBOL(__initcall##level##_start) = .;		\
	  		*(.initcall##level##.init)				\
	  		*(.initcall##level##s.init)				\
	  
	  #define INIT_CALLS							\
	  		VMLINUX_SYMBOL(__initcall_start) = .;			\
	  		*(.initcallearly.init)					\
	  		INIT_CALLS_LEVEL(0)					\
	  		INIT_CALLS_LEVEL(1)					\
	  		INIT_CALLS_LEVEL(2)					\
	  		INIT_CALLS_LEVEL(3)					\
	  		INIT_CALLS_LEVEL(4)					\
	  		INIT_CALLS_LEVEL(5)					\
	  		INIT_CALLS_LEVEL(rootfs)				\
	  		INIT_CALLS_LEVEL(6)					\
	  		INIT_CALLS_LEVEL(7)					\
	  		VMLINUX_SYMBOL(__initcall_end) = .;
	  
	  ......
	  #define INIT_DATA_SECTION(initsetup_align)				\
	  	.init.data : AT(ADDR(.init.data) - LOAD_OFFSET) {		\
	  		INIT_DATA						\
	  		INIT_SETUP(initsetup_align)				\
	  		INIT_CALLS						\
	  		CON_INITCALL						\
	  		SECURITY_INITCALL					\
	  		INIT_RAM_FS						\
	  	}
	  ```
	- ```c
	  arch/arm/kernel/vmlinux.lds.S
	  
	  ......
	    	.init.data : {
	  #ifndef CONFIG_XIP_KERNEL
	  		INIT_DATA
	  #endif
	  		INIT_SETUP(16)
	  		INIT_CALLS
	  		CON_INITCALL
	  		SECURITY_INITCALL
	  		INIT_RAM_FS
	  	}
	  ```
- 示例
	- ```c
	  # module_init_test.c
	  
	  #ifdef CONFIG_LTO
	  #define LTO_REFERENCE_INITCALL(x) \
	          ; /* yes this is needed */                      \
	          static __used __exit void *reference_##x(void)  \
	          {                                               \
	                  return &x;                              \
	          }
	  #else
	  #define LTO_REFERENCE_INITCALL(x)
	  #endif
	  
	  
	  #define  __used  __attribute__((__used__))
	  #define __define_initcall(fn, id) \
	          static initcall_t __initcall_##fn##id __used \
	          __attribute__((__section__(".initcall" #id ".init"))) = fn; \
	          LTO_REFERENCE_INITCALL(__initcall_##fn##id)
	  #define device_initcall(fn)     __define_initcall(fn, 6)
	  #define __initcall(fn) device_initcall(fn)
	  #define module_init(x)  __initcall(x);
	  
	  module_init(my_test_fun)
	  ------------------------------------------------------
	  
	  
	  # gcc -E module_init_test.c -o module_init_test.i
	  
	  
	  ------------------------------------------------------
	  # module_init_test.i
	  # 1 "module_init_test.c"
	  # 1 "<built-in>"
	  # 1 "<command-line>"
	  # 31 "<command-line>"
	  # 1 "/usr/include/stdc-predef.h" 1 3 4
	  # 32 "<command-line>" 2
	  # 1 "module_init_test.c"
	  # 22 "module_init_test.c"
	  static initcall_t __initcall_my_test_fun6 __attribute__((__used__)) __attribute__((__section__(".initcall" "6" ".init"))) = my_test_fun; ;
	  ```
- 参考文档
	- [Common-Function-Attributes](https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#Common-Function-Attributes)
	- [An introduction to Linux kernel initcalls](https://www.collabora.com/news-and-blog/blog/2020/07/14/introduction-to-linux-kernel-initcalls/)
	- [Linux Kernel module_init执行过程](https://www.cnblogs.com/qiynet/p/15398731.html)
	- [Linux 内核：initcall机制与module_init](https://www.cnblogs.com/schips/p/linux_kernel_initcall_and_module_init.html)