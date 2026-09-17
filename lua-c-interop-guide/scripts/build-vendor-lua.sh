#!/usr/bin/env bash
#
# build-vendor-lua.sh
#
# 在不需要 sudo / 不依赖系统 liblua-dev 的前提下，下载 Lua 5.4.7 官方源码，
# 本地编译出 liblua.a + 头文件，供本教程所有章节 / 项目的 Makefile 链接使用。
#
# 用法：
#   ./scripts/build-vendor-lua.sh
#
# 产物：
#   vendor/lua-5.4.7/include/*.h
#   vendor/lua-5.4.7/lib/liblua.a
#
set -euo pipefail

LUA_VERSION="5.4.7"
LUA_TARBALL="lua-${LUA_VERSION}.tar.gz"
LUA_URL="https://www.lua.org/ftp/${LUA_TARBALL}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GUIDE_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
VENDOR_DIR="${GUIDE_ROOT}/vendor"
SRC_DIR="${VENDOR_DIR}/lua-${LUA_VERSION}-src"
INSTALL_DIR="${VENDOR_DIR}/lua-${LUA_VERSION}"

if [ -f "${INSTALL_DIR}/lib/liblua.a" ] && [ -f "${INSTALL_DIR}/include/lua.h" ]; then
  echo "[build-vendor-lua] 已存在 ${INSTALL_DIR}，跳过构建（如需重建请先删除该目录）"
  exit 0
fi

mkdir -p "${VENDOR_DIR}"
cd "${VENDOR_DIR}"

if [ ! -f "${LUA_TARBALL}" ]; then
  echo "[build-vendor-lua] 下载 ${LUA_URL}"
  curl -fsSL -o "${LUA_TARBALL}" "${LUA_URL}"
fi

echo "[build-vendor-lua] 解压 ${LUA_TARBALL}"
rm -rf "${SRC_DIR}"
mkdir -p "${SRC_DIR}"
tar -xzf "${LUA_TARBALL}" -C "${SRC_DIR}" --strip-components=1

echo "[build-vendor-lua] 编译 Lua ${LUA_VERSION}（posix 目标，-fPIC 方便以后按需做动态库）"
make -C "${SRC_DIR}" posix CC="gcc -std=gnu99" MYCFLAGS="-fPIC -O2" -j"$(nproc)"

echo "[build-vendor-lua] 安装到 ${INSTALL_DIR}"
mkdir -p "${INSTALL_DIR}/include" "${INSTALL_DIR}/lib" "${INSTALL_DIR}/bin"
cp "${SRC_DIR}"/src/lua.h "${SRC_DIR}"/src/luaconf.h "${SRC_DIR}"/src/lualib.h \
   "${SRC_DIR}"/src/lauxlib.h "${SRC_DIR}"/src/lua.hpp "${INSTALL_DIR}/include/"
cp "${SRC_DIR}/src/liblua.a" "${INSTALL_DIR}/lib/"
cp "${SRC_DIR}/src/lua" "${SRC_DIR}/src/luac" "${INSTALL_DIR}/bin/"

echo "[build-vendor-lua] 完成："
echo "  头文件: ${INSTALL_DIR}/include"
echo "  静态库: ${INSTALL_DIR}/lib/liblua.a"
echo "  解释器: ${INSTALL_DIR}/bin/lua"
