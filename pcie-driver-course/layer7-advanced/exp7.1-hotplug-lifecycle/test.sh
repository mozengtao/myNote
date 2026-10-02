#!/bin/sh
# test.sh - 热插拔自动测试脚本（利用 sysfs 的 rescan / remove 机制模拟热插拔操作）

echo "=================================================="
echo " Experiment 7.1: PCIe 运行时热插拔生命周期验证"
echo "=================================================="

insmod /root/extra/edu_hotplug.ko

echo ""
echo "[1] 初始状态：驱动已就绪并绑定静态设备"
dmesg | grep "edu_hotplug: >>> HOTPLUG"

echo ""
echo "[2] 模拟热拔：向设备的 remove 接口写入 1"
BDF=$(lspci -d 1234:11e8 | awk '{print $1}')
echo 1 > "/sys/bus/pci/devices/0000:${BDF}/remove"
sleep 1
dmesg | tail -n 3

echo ""
echo "[3] 模拟热插：触发 PCI 总线全盘重新扫描 (rescan)"
echo 1 > /sys/bus/pci/rescan
sleep 1
dmesg | tail -n 4

echo ""
echo "[4] 卸载驱动"
rmmod edu_hotplug
poweroff -f
