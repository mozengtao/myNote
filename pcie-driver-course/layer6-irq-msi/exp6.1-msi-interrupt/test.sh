#!/bin/sh
# test.sh - 验证基于 MSI 中断的异步 DMA 唤醒与传输正确性

echo "=================================================="
echo " Experiment 6.1: MSI 中断驱动 DMA 验证"
echo "=================================================="

insmod /root/extra/edu_msi.ko
echo ""
echo "[1] 查看 /proc/interrupts 中挂载的 edu_msi 中断计数:"
cat /proc/interrupts | grep -E "edu_msi|CPU"

echo ""
echo "[2] 查看内核日志确认中断通知与回环测试结果:"
dmesg | grep -A 7 "edu_msi: MSI vector allocated"

rmmod edu_msi
poweroff -f
