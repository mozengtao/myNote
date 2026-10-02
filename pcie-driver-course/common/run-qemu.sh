#!/usr/bin/env bash
# =============================================================================
# run-qemu.sh
#
# 目的（中文说明）：
#   统一的 QEMU 启动脚本，所有 Layer 的实验都通过它来启动虚拟机，
#   区别只是传入的 KERNEL / INITRAMFS / 额外设备参数不同。
#
#   这样我们保证：
#     - 所有实验使用同一个 kernel 镜像 (bzImage)，共用一次编译结果
#     - 所有实验使用同样的 QEMU 启动约定（串口 console，nographic，退出方式）
#     - Layer 0-6 使用发行版自带 qemu-system-x86_64 (带内置 edu device)
#     - Capstone 阶段只需把 QEMU_BIN 换成自己编译出的二进制，
#       并把 EXTRA_DEVICE_ARGS 换成 "-device mypcie"
#
# Usage:
#   run-qemu.sh <initramfs.cpio.gz> [extra qemu device args...]
#
# 环境变量（可覆盖默认值）：
#   KERNEL_BZIMAGE   默认: ../kernel/arch/x86/boot/bzImage
#   QEMU_BIN         默认: qemu-system-x86_64 (发行版自带)
#   QEMU_MEM         默认: 512M
#   QEMU_SMP         默认: 2
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

INITRAMFS="${1:?usage: run-qemu.sh <initramfs.cpio.gz> [extra qemu args...]}"
shift

KERNEL_BZIMAGE="${KERNEL_BZIMAGE:-$SCRIPT_DIR/../kernel/arch/x86/boot/bzImage}"
QEMU_BIN="${QEMU_BIN:-qemu-system-x86_64}"
QEMU_MEM="${QEMU_MEM:-512M}"
QEMU_SMP="${QEMU_SMP:-2}"

if [ ! -f "$KERNEL_BZIMAGE" ]; then
    echo "错误: 找不到 kernel bzImage: $KERNEL_BZIMAGE" >&2
    echo "请在工程根目录下执行 ./scripts/setup-kernel.sh 自动下载并编译内核" >&2
    exit 1
fi

echo "[run-qemu] KERNEL     = $KERNEL_BZIMAGE"
echo "[run-qemu] INITRAMFS  = $INITRAMFS"
echo "[run-qemu] QEMU_BIN   = $QEMU_BIN"
echo "[run-qemu] EXTRA ARGS = $*"
echo "---------------------------------------------------------"

# -----------------------------------------------------------------------------
# 关键参数解释：
#   -nographic           : 不开图形窗口，所有输出走串口/stdio
#   -append "console=..."  : 告诉 kernel 把 console 定向到第一个串口 (ttyS0)，
#                           这样 dmesg / printk 才能在我们的终端里看到
#   -no-reboot            : panic 时不自动重启，方便观察错误
#   -device <extra>        : 由调用者传入，比如 "-device edu" 或自定义 PCIe 设备
# -----------------------------------------------------------------------------
exec "$QEMU_BIN" \
    -kernel "$KERNEL_BZIMAGE" \
    -initrd "$INITRAMFS" \
    -append "console=ttyS0 panic=1 rdinit=/init" \
    -m "$QEMU_MEM" \
    -smp "$QEMU_SMP" \
    -nographic \
    -no-reboot \
    "$@"
