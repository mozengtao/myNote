#!/bin/sh
# test.sh - 观察 sysfs 与 /proc/iomem 中的 PCI 资源分布

echo "=================================================="
echo " Experiment 2.2: sysfs 与 /proc/iomem 资源观察"
echo "=================================================="

BDF="0000:00:04.0"
SYS_DEV="/sys/bus/pci/devices/$BDF"

echo "[1] 查看设备 sysfs 目录中的关键属性文件:"
ls -F "$SYS_DEV"

echo ""
echo "[2] 查看设备 Vendor ID 与 Device ID 文件内容:"
echo -n "Vendor: "; cat "$SYS_DEV/vendor"
echo -n "Device: "; cat "$SYS_DEV/device"

echo ""
echo "[3] 查看设备导出的 resource 列表 (起始 结束 标志位):"
cat "$SYS_DEV/resource" | head -n 3

echo ""
echo "[4] 查看 /proc/iomem 中该物理地址区间的归属记录:"
cat /proc/iomem | grep -B 1 -A 2 -i "00:04"

echo ""
echo "=== 观察完成，关机 ==="
poweroff -f
