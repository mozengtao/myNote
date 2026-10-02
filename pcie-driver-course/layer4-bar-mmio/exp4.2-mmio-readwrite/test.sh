#!/bin/sh
# test.sh - 验证 MMIO 读写交互与寄存器翻转

echo "=================================================="
echo " Experiment 4.2: MMIO 读写与寄存器协议验证"
echo "=================================================="

insmod /root/extra/edu_mmio.ko
dmesg | grep -A 5 "edu_mmio: read IDENT"
rmmod edu_mmio
poweroff -f
