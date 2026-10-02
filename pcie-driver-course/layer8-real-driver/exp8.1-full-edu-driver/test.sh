#!/bin/sh
# test.sh - 观察完整驱动的状态机跃迁与自检

echo "=================================================="
echo " Experiment 8.1: 整合驱动与状态机跃迁观察"
echo "=================================================="

insmod /root/extra/edu_full_driver.ko
echo ""
echo "[1] 查看 dmesg 中连续的状态机跃迁记录:"
dmesg | grep "State Transition"

echo ""
echo "[2] 卸载模块，观察 STOPPING -> STOPPED -> RESET 的安全注销链路:"
rmmod edu_full_driver
dmesg | tail -n 5

poweroff -f
