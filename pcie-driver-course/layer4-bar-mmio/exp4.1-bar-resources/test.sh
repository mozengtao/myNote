#!/bin/sh
# test.sh - 运行 BAR 资源打印实验

echo "=================================================="
echo " Experiment 4.1: BAR 物理资源解析验证"
echo "=================================================="

insmod /root/extra/edu_bar_info.ko
dmesg | grep "edu_bar_info: BAR"
rmmod edu_bar_info
poweroff -f
