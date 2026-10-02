#!/usr/bin/env bash
# =============================================================================
# setup-qemu-mypcie.sh
#
# 目的：
#   仅在 Capstone 阶段按需下载 QEMU 源码 (v8.2.2)，集成自定义 mypcie 硬件设备，
#   并编译产出支持 -device mypcie 的专用 QEMU 二进制。
#
# Usage:
#   ./scripts/setup-qemu-mypcie.sh
#
# 环境变量（可覆盖）：
#   QEMU_DIR        QEMU 源码存放路径（默认：工程根目录下的 qemu-src）
#   QEMU_VERSION    QEMU 版本标签（默认：v8.2.2）
#   QEMU_REPO       git 仓库地址（默认官方 GitLab 源，也可指定 GitHub 镜像）
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

QEMU_DIR="${QEMU_DIR:-$REPO_ROOT/qemu-src}"
QEMU_VERSION="${QEMU_VERSION:-v8.2.2}"
QEMU_REPO="${QEMU_REPO:-https://gitlab.com/qemu-project/qemu.git}"
# 备用镜像源示例: https://github.com/qemu/qemu.git

DEVICE_SRC="$REPO_ROOT/capstone-dma-ring/qemu-device/mypcie.c"
PATCH_FILE="$REPO_ROOT/capstone-dma-ring/qemu-device/mypcie.patch"

echo "=================================================="
echo " [setup-qemu] 开始准备 Capstone QEMU 虚拟设备环境"
echo " 目标目录: $QEMU_DIR"
echo " QEMU 版本: $QEMU_VERSION"
echo " 自定义设备: $DEVICE_SRC"
echo "=================================================="

# 1. 检查构建依赖提示
MISSING_PKGS=()
for cmd in ninja git python3 pkg-config; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        MISSING_PKGS+=("$cmd")
    fi
done

if [ ${#MISSING_PKGS[@]} -ne 0 ]; then
    echo "错误: 缺少必要的构建工具: ${MISSING_PKGS[*]}" >&2
    echo "请在宿主机上运行: sudo apt-get update && sudo apt-get install -y ninja-build python3 pkg-config libglib2.0-dev libpixman-1-dev" >&2
    exit 1
fi

if [ ! -f "$DEVICE_SRC" ]; then
    echo "错误: 未找到自定义设备源文件: $DEVICE_SRC" >&2
    exit 1
fi

# 2. 浅克隆 QEMU 源码 (若不存在)
if [ ! -d "$QEMU_DIR/.git" ]; then
    echo "[setup-qemu] 正在浅克隆 QEMU 源码 ($QEMU_VERSION)..."
    git clone --depth 1 --branch "$QEMU_VERSION" "$QEMU_REPO" "$QEMU_DIR"
else
    echo "[setup-qemu] QEMU 目录已存在: $QEMU_DIR，跳过 clone。"
fi

# 3. 集成 mypcie.c 设备源码
echo "[setup-qemu] 拷贝 mypcie.c 到 hw/misc/..."
cp "$DEVICE_SRC" "$QEMU_DIR/hw/misc/mypcie.c"

# 4. 应用 meson.build 补丁
MESON_BUILD="$QEMU_DIR/hw/misc/meson.build"
if grep -q "mypcie.c" "$MESON_BUILD"; then
    echo "[setup-qemu] hw/misc/meson.build 中已包含 mypcie.c，跳过补丁应用。"
else
    echo "[setup-qemu] 应用 meson.build 补丁..."
    if [ -f "$PATCH_FILE" ]; then
        patch -d "$QEMU_DIR" -p1 < "$PATCH_FILE"
    else
        sed -i "/system_ss.add(when: 'CONFIG_EDU'/a system_ss.add(when: 'CONFIG_PCI', if_true: files('mypcie.c'))" "$MESON_BUILD"
    fi
fi

# 5. 配置并编译 QEMU x86_64 二进制
echo "[setup-qemu] 开始构建 QEMU x86_64 target..."
mkdir -p "$QEMU_DIR/build"
cd "$QEMU_DIR/build"

if [ ! -f "build.ninja" ]; then
    ../configure --target-list=x86_64-softmmu --disable-docs --disable-gtk --disable-sdl
fi

ninja qemu-system-x86_64

TARGET_BIN="$QEMU_DIR/build/qemu-system-x86_64"
if [ -f "$TARGET_BIN" ]; then
    echo "=================================================="
    echo " [setup-qemu] Capstone QEMU 构建成功！"
    echo " 可执行文件: $TARGET_BIN"
    echo " 可通过 -device mypcie 启动自定义设备。"
    echo "=================================================="
else
    echo "错误: 未能生成 $TARGET_BIN" >&2
    exit 1
fi
