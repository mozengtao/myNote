# Makefile
# out-of-tree kernel module 标准写法：
#   当 make 不带 -C KERNELDIR 参数直接运行时（第一次调用），
#   会进入下面的 "else" 分支，反过来调用内核构建系统，
#   并通过 M=$(PWD) 告诉内核构建系统："到这个目录里找 obj-m"。
obj-m += hello.o

KERNELDIR ?= ../../kernel
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
