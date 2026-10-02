#!/bin/sh
# test.sh - 验证 DMA 内存传输与回环校验

echo "=================================================="
echo " Experiment 5.1: Coherent DMA 轮询传输验证"
echo "=================================================="

insmod /root/extra/edu_dma.ko
dmesg | grep -A 8 "edu_dma: Coherent DMA buffer"
rmmod edu_dma
poweroff -f
