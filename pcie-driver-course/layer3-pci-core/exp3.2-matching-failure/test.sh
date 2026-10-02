#!/bin/sh
# test.sh - 观察匹配失败时的系统静默状态

echo "=================================================="
echo " Experiment 3.2: ID 不匹配故障注入观察"
echo "=================================================="

echo "[1] 加载包含错误 Device ID 的驱动:"
insmod /root/extra/edu_mismatch.ko
echo "insmod 退出码: $?"  # 必定为 0

echo ""
echo "[2] 检查 dmesg (绝不应出现 probe 信息):"
dmesg | tail -n 5

echo ""
echo "[3] 检查 driver 目录下的绑定的设备:"
ls -l /sys/bus/pci/drivers/edu_mismatch/

echo ""
echo "[4] 卸载模块:"
rmmod edu_mismatch

echo ""
echo "[5] 检查 dmesg (绝不应出现 remove 信息):"
dmesg | tail -n 3

echo ""
echo "=== 观察完成，关机 ==="
poweroff -f
