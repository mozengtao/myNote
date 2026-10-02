#!/bin/sh
# trace_scan.sh - 在 QEMU 启动日志中寻找内核枚举 PCI 设备的证据

echo "=================================================="
echo " Experiment 2.1: 内核 PCI 枚举日志检查"
echo "=================================================="

echo "[1] 提取内核启动阶段关于 PCI bus scan 的 dmesg 日志:"
dmesg | grep -iE "pci 0000:|pci_bus" | head -n 20

echo ""
echo "[2] 查看 QEMU edu 设备的枚举记录:"
dmesg | grep -i "1234:11e8"

echo ""
echo "=== 检查完成，关机 ==="
poweroff -f
