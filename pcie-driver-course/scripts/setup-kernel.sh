#!/usr/bin/env bash
# =============================================================================
# setup-kernel.sh
#
# 目的：
#   按需下载 Linux 内核源码 (v6.12 LTS)，配置并编译 bzImage 与模块构建环境。
#   本工程无需在日常笔记归档中保留数 GB 的内核源码树，需要运行实验时执行本脚本即可。
#
# Usage:
#   ./scripts/setup-kernel.sh
#
# 环境变量（可覆盖）：
#   KERNEL_DIR      内核源码存放路径（默认：工程根目录下的 kernel）
#   KERNEL_VERSION  内核版本分支/标签（默认：v6.12）
#   KERNEL_REPO     git 仓库地址（默认官方源，也可指定 GitHub 等镜像源）
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

KERNEL_DIR="${KERNEL_DIR:-$REPO_ROOT/kernel}"
KERNEL_VERSION="${KERNEL_VERSION:-v6.12}"
KERNEL_REPO="${KERNEL_REPO:-https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git}"
# 备用镜像源示例: https://github.com/torvalds/linux.git

echo "=================================================="
echo " [setup-kernel] 开始准备 Linux 内核构建环境"
echo " 目标目录: $KERNEL_DIR"
echo " 内核版本: $KERNEL_VERSION"
echo " 仓库地址: $KERNEL_REPO"
echo "=================================================="

# 1. 检查构建依赖提示
MISSING_PKGS=()
for cmd in gcc make flex bison bc git; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        MISSING_PKGS+=("$cmd")
    fi
done

if [ ${#MISSING_PKGS[@]} -ne 0 ]; then
    echo "错误: 缺少必要的构建工具: ${MISSING_PKGS[*]}" >&2
    echo "请在宿主机上运行: sudo apt-get update && sudo apt-get install -y build-essential libncurses-dev bison flex libssl-dev libelf-dev bc git" >&2
    exit 1
fi

# 2. 浅克隆内核源码 (若不存在)
if [ ! -d "$KERNEL_DIR/.git" ]; then
    echo "[setup-kernel] 正在浅克隆 Linux 内核源码 ($KERNEL_VERSION)..."
    git clone --depth 1 --branch "$KERNEL_VERSION" "$KERNEL_REPO" "$KERNEL_DIR"
else
    echo "[setup-kernel] 内核目录已存在: $KERNEL_DIR，跳过 clone。"
fi

# 3. 生成 x86_64 默认配置
echo "[setup-kernel] 生成 x86_64_defconfig..."
make -C "$KERNEL_DIR" x86_64_defconfig

# 4. 并行编译 bzImage 与准备模块构建环境
NPROC="$(nproc 2>/dev/null || echo 4)"
echo "[setup-kernel] 开始并行编译 (CPU 线程数: $NPROC)..."
make -C "$KERNEL_DIR" -j"$NPROC" bzImage modules_prepare

BZIMAGE="$KERNEL_DIR/arch/x86/boot/bzImage"
if [ -f "$BZIMAGE" ]; then
    echo "=================================================="
    echo " [setup-kernel] 内核准备完成！"
    echo " bzImage 路径: $BZIMAGE"
    echo " 现在可直接进入各 layer 目录执行 make 编译驱动模块。"
    echo "=================================================="
else
    echo "错误: 未找到生成的 bzImage: $BZIMAGE" >&2
    exit 1
fi
