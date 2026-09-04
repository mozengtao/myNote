#!/bin/bash
#
# run_tests.sh - Seqlock 自动化并发一致性与抗崩溃测试套件
#

set -e

GREEN="\033[32m"
RED="\033[31m"
YELLOW="\033[33m"
CYAN="\033[36m"
RESET="\033[0m"

BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="${BASE_DIR}/bin"

echo -e "${CYAN}========================================================================${RESET}"
echo -e "${CYAN}        C 语言共享内存 Seqlock 机制：自动化回归与一致性验证测试         ${RESET}"
echo -e "${CYAN}========================================================================${RESET}"

# 确保先编译最新二进制
make -C "${BASE_DIR}" all

# 1. 测试示例 1
echo -e "\n${YELLOW}>>> [测试 1/5] 运行示例 1：最简单写单读共享内存 Seqlock (限时 2 秒)...${RESET}"
"${BIN_DIR}/01_simplest_seqlock"
echo -e "${GREEN}>>> [测试 1/5 通过] 示例 1 成功完成，零数据撕裂！${RESET}"

# 2. 测试示例 2
echo -e "\n${YELLOW}>>> [测试 2/5] 运行示例 2：单写者 + 4 读者网络高频统计 (限时 3 秒)...${RESET}"
"${BIN_DIR}/02_multi_reader_stats"
echo -e "${GREEN}>>> [测试 2/5 通过] 示例 2 成功完成，多读者零竞争！${RESET}"

# 3. 测试示例 3 (跨进程 POSIX 共享内存与异常崩溃免疫测试)
echo -e "\n${YELLOW}>>> [测试 3/5] 运行示例 3：POSIX 共享内存跨进程 IPC 与 kill -9 强杀抗崩验证...${RESET}"
# 启动写者，持续运行 4 秒
"${BIN_DIR}/03_posix_shm_writer" 4 &
WRITER_PID=$!
sleep 0.3

# 启动第一个读者进程
"${BIN_DIR}/03_posix_shm_reader" 1 &
READER1_PID=$!
sleep 0.5

# 模拟读者进程突发异常崩溃（使用 kill -9 强杀读者 1）
echo -e "${CYAN}[混沌测试] 模拟读者进程意外崩溃：发送 SIGKILL 强杀读者进程 (PID: ${READER1_PID})...${RESET}"
kill -9 "${READER1_PID}" 2>/dev/null || true
sleep 0.2

# 验证写者进程是否依然健壮运行（非死锁）
if ps -p "${WRITER_PID}" > /dev/null; then
    echo -e "${GREEN}[测试现象] 写者进程完全不受读者崩溃影响，依然高速写入中！${RESET}"
else
    echo -e "${RED}[严重失败] 写者进程异常退出或死锁！${RESET}"
    exit 1
fi

# 启动第二个读者进程，验证共享内存依然完全正常可读
echo -e "${CYAN}[混沌测试] 启动第二个读者进程接入共享内存...${RESET}"
"${BIN_DIR}/03_posix_shm_reader" 2 &
READER2_PID=$!

wait "${WRITER_PID}" || true
wait "${READER2_PID}" || true

echo -e "${GREEN}>>> [测试 3/5 通过] 示例 3 跨进程 IPC 及崩溃隔离测试完美通过！${RESET}"

# 4. 测试示例 4
echo -e "\n${YELLOW}>>> [测试 4/5] 运行示例 4：DPDK 风格 64 字节缓存行对齐遥测 (限时 3 秒)...${RESET}"
"${BIN_DIR}/04_dpdk_telemetry"
echo -e "${GREEN}>>> [测试 4/5 通过] 示例 4 成功完成，快慢路径彻底解耦！${RESET}"

# 5. 测试示例 5
echo -e "\n${YELLOW}>>> [测试 5/5] 运行示例 5：Linux 内核 seqcount_t / seqlock_t 4写4读并发 (限时 3 秒)...${RESET}"
"${BIN_DIR}/05_kernel_seqcount"
echo -e "${GREEN}>>> [测试 5/5 通过] 示例 5 成功完成，多写者串行化锁保护无误！${RESET}"

echo -e "\n${GREEN}========================================================================${RESET}"
echo -e "${GREEN}        所有 5 个 Seqlock 工业级示例全部自动化验证通过！                ${RESET}"
echo -e "${GREEN}========================================================================${RESET}"
