#!/bin/sh
# test.sh - 深入查看 QEMU edu 设备的 PCI 配置空间

echo "=================================================="
echo " Experiment 1.2: PCI 配置空间深度观察"
echo "=================================================="

DEV="1234:11e8"

echo "[1] 查看设备的详细解码信息 (lspci -vv):"
lspci -d "$DEV" -vv

echo ""
echo "[2] 查看配置空间的原始十六进制字节转储 (lspci -xxx):"
lspci -d "$DEV" -xxx

echo ""
echo "[3] 验证 sysfs 中直接读取的 64 字节 Header:"
BDF=$(lspci -d "$DEV" | awk '{print $1}')
hexdump -C "/sys/bus/pci/devices/0000:${BDF}/config" | head -n 4

echo ""
echo "=== 观察完成，5秒后自动关机 ==="
poweroff -f
