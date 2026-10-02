#!/bin/sh
# test.sh - 自动执行 lspci 并过滤出教育设备

echo "=================================================="
echo " Experiment 1.1: lspci 基础观察"
echo "=================================================="

echo "[1] 列出虚拟机中识别到的所有 PCI 设备:"
lspci

echo ""
echo "[2] 专门检索 QEMU 教育设备 (Vendor 1234: Device 11e8):"
lspci -d 1234:11e8

echo ""
echo "[3] 检查 /sys/bus/pci/devices/ 下对应的目录:"
ls -l /sys/bus/pci/devices/

echo ""
echo "=== 观察完成，5秒后自动关机 ==="
poweroff -f
