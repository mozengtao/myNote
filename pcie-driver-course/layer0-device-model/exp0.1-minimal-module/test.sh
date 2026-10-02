#!/bin/sh
# test.sh - 在 initramfs 启动后自动执行，验证 module_init/exit 生命周期
mount -t proc proc /proc 2>/dev/null
mount -t sysfs sysfs /sys 2>/dev/null
export PATH=/bin:/sbin:/usr/bin:/usr/sbin:$PATH

echo "=== Experiment 0.1: insmod hello.ko ==="
insmod /root/extra/hello.ko
echo "=== dmesg 应该看到 'module_init() called' ==="
dmesg | tail -5
echo "=== lsmod 应该看到 hello 模块 ==="
lsmod
echo "=== cat /sys/module/hello/... ==="
ls /sys/module/hello/
echo "=== rmmod hello.ko ==="
rmmod hello
echo "=== dmesg 应该看到 'module_exit() called' ==="
dmesg | tail -3
echo "=== Experiment 0.1 done, powering off ==="
poweroff -f
