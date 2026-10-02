#!/bin/sh
# test.sh - 验证 edu_minimal 驱动的加载、匹配、probe 与卸载

echo "=================================================="
echo " Experiment 3.1: 最小 PCI 驱动加载与匹配验证"
echo "=================================================="

echo "[1] 加载驱动模块:"
insmod /root/extra/edu_minimal.ko

echo ""
echo "[2] 检查 dmesg 是否输出 probe 成功信息:"
dmesg | tail -n 5

echo ""
echo "[3] 验证 sysfs 中设备与驱动的绑定关系:"
ls -l /sys/bus/pci/drivers/edu_minimal/

echo ""
echo "[4] 卸载驱动模块:"
rmmod edu_minimal

echo ""
echo "[5] 检查 dmesg 是否输出 remove 成功信息:"
dmesg | tail -n 3

echo ""
echo "=== 实验完成，关机 ==="
poweroff -f
