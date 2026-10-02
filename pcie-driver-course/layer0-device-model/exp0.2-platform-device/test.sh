#!/bin/sh
# test.sh - 验证 platform bus 上的 device/driver matching

echo "=== Experiment 0.2: insmod platform_demo.ko ==="
insmod /root/extra/platform_demo.ko
echo "=== dmesg: 应该看到 my_probe() called -> MATCHED! ==="
dmesg | tail -5
echo "=== sysfs: device 的 driver 符号链接 (matching 的直接证据) ==="
ls -la /sys/bus/platform/devices/my_pdev/driver 2>&1
echo "=== rmmod platform_demo.ko ==="
rmmod platform_demo
echo "=== dmesg: 应该看到 my_remove() called ==="
dmesg | tail -3
echo "=== Experiment 0.2 done, powering off ==="
poweroff -f
