#!/usr/bin/env bash
# =============================================================================
# build-initramfs.sh
#
# 目的（中文说明）：
#   为课程中的每一个 Layer / Experiment 构建一个最小化的 initramfs。
#   所有 Layer 共用这一份脚本，只是传入的 "extra files" 目录不同
#   （例如某个 Layer 编译出来的 .ko 内核模块、对应的测试脚本）。
#
#   这样我们就不需要为每个实验单独准备一套根文件系统——
#   initramfs 本质上只是一个打包进内存的 cpio 归档，
#   kernel 启动时会把它解压到 rootfs，然后执行其中的 /init。
#
# Usage:
#   build-initramfs.sh <extra-dir> <output.cpio.gz> [init-script]
#
#   <extra-dir>     : 一个目录，其内容会被原样拷贝进 initramfs 的 /root/extra/
#                      (典型内容：xxx.ko, test.sh)
#   <output.cpio.gz>: 生成的 initramfs 压缩包路径
#   [init-script]   : 可选，如果提供，会被安装为 initramfs 的 /init
#                      (否则使用本脚本自带的默认 /init，启动后掉入一个 shell)
# =============================================================================
set -euo pipefail

EXTRA_DIR="${1:?usage: build-initramfs.sh <extra-dir> <output.cpio.gz> [init-script]}"
OUT_FILE="${2:?usage: build-initramfs.sh <extra-dir> <output.cpio.gz> [init-script]}"
CUSTOM_INIT="${3:-}"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

echo "[build-initramfs] 工作目录: $WORK"

# -----------------------------------------------------------------------------
# 1. 基础目录骨架
#    这是一个最小 Linux 根文件系统应该有的标准目录。
# -----------------------------------------------------------------------------
echo "[build-initramfs] 基础目录骨架..."
mkdir -p "$WORK"/{bin,sbin,usr/bin,usr/sbin,etc,proc,sys,dev,root,root/extra,lib,lib/modules,tmp}

# -----------------------------------------------------------------------------
# 2. 安装 busybox 与常用调试工具 (lspci)
# -----------------------------------------------------------------------------
cp /usr/bin/busybox "$WORK/bin/busybox"
chmod 755 "$WORK/bin/busybox"

for app in $(/usr/bin/busybox --list); do
    if [ "$app" != "busybox" ]; then
        ln -sf busybox "$WORK/bin/$app"
        ln -sf ../bin/busybox "$WORK/sbin/$app"
    fi
done
ln -sf busybox "$WORK/bin/sh"

# 如果宿主机有 lspci，自动安装进 initramfs 方便实验调试
if command -v lspci >/dev/null 2>&1; then
    mkdir -p "$WORK/usr/bin" "$WORK/lib64" "$WORK/lib/x86_64-linux-gnu"
    cp "$(command -v lspci)" "$WORK/usr/bin/lspci"
    chmod 755 "$WORK/usr/bin/lspci"
    for lib in $(ldd "$(command -v lspci)" | grep -o '/lib[^ ]*'); do
        if [ -f "$lib" ]; then
            dest_dir="$WORK/$(dirname "$lib")"
            mkdir -p "$dest_dir"
            cp -L "$lib" "$dest_dir/"
        fi
    done
fi

# -----------------------------------------------------------------------------
# 3. 拷贝实验相关的额外文件（.ko 模块 / 测试脚本等）
# -----------------------------------------------------------------------------
if [ -d "$EXTRA_DIR" ]; then
    cp -a "$EXTRA_DIR"/. "$WORK/root/extra/"
    echo "[build-initramfs] 已拷贝 extra 文件: $(ls "$EXTRA_DIR")"
else
    echo "[build-initramfs] 警告: extra-dir '$EXTRA_DIR' 不存在，跳过"
fi

# -----------------------------------------------------------------------------
# 4. 生成 /init
#    /init 是 initramfs 的 PID 1 —— kernel 解压完 initramfs 后，
#    第一件事就是 exec 这个文件。它负责：
#      a) 挂载 /proc /sys /dev （否则 insmod/lspci/dmesg 等都无法工作）
#      b) 如果有自定义 init-script，就 source/exec 它
#      c) 否则执行 extra/test.sh（如果存在），再掉入交互 shell
# -----------------------------------------------------------------------------
if [ -n "$CUSTOM_INIT" ] && [ -f "$CUSTOM_INIT" ]; then
    cp "$CUSTOM_INIT" "$WORK/init"
    chmod +x "$WORK/init"
    echo "[build-initramfs] 使用自定义 init: $CUSTOM_INIT"
else
cat > "$WORK/init" << 'INIT_EOF'
#!/bin/sh
# 默认 /init：挂载基本文件系统，运行 extra/test.sh（如果存在），再进入 shell
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev 2>/dev/null || mdev -s
export PATH=/bin:/sbin:$PATH

echo ""
echo "=================================================="
echo " PCIe Driver Course - initramfs 已启动"
echo "=================================================="

if [ -x /root/extra/test.sh ]; then
    echo "[init] 运行 /root/extra/test.sh ..."
    /root/extra/test.sh
fi

echo "[init] 进入交互 shell (输入 'poweroff -f' 退出 QEMU)"
exec /bin/sh
INIT_EOF
    chmod +x "$WORK/init"
fi

# -----------------------------------------------------------------------------
# 5. 打包成 cpio + gzip
#    Linux kernel 认识的 initramfs 格式就是 "newc" cpio 格式再 gzip 压缩。
# -----------------------------------------------------------------------------
(cd "$WORK" && find . -print0 | cpio --null -o -H newc 2>/dev/null) | gzip -9 > "$OUT_FILE"

echo "[build-initramfs] 完成: $OUT_FILE ($(du -h "$OUT_FILE" | cut -f1))"
