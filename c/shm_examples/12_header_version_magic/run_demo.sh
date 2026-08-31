#!/usr/bin/env bash
# 演示: Header/Version/Magic/CRC 校验器在四种场景下的行为
#   1) 正常写入 + 正常读取 -> 应该成功
#   2) 篡改 magic -> reader 应该报 MAGIC_MISMATCH 并拒绝
#   3) 翻转数据字节（不动 crc 字段）-> reader 应该报 CRC_MISMATCH 并拒绝
#   4) 用不兼容的新 major 版本写入 -> reader 应该报 VERSION_INCOMPATIBLE 并拒绝
set -uo pipefail

cd "$(dirname "$0")"

rm -f /dev/shm/shm_examples_header_magic

status=0

run_case() {
	local desc="$1"
	local expect_substr="$2"
	shift 2
	local out
	local ec

	out=$("$@" 2>&1)
	ec=$?

	if echo "$out" | grep -qF "$expect_substr"; then
		echo "[PASS] $desc"
		echo "$out" | sed 's/^/       /'
	else
		echo "[FAIL] $desc (expect to see: $expect_substr)"
		echo "$out" | sed 's/^/       /'
		status=1
	fi

	return $ec
}

echo "=== 场景 1: 正常写入 + 正常读取 ==="
./writer > /dev/null
run_case "reader 应该成功读取" "check result: OK" ./reader

echo
echo "=== 场景 2: 篡改 magic ==="
./writer > /dev/null
./corrupt_tool magic > /dev/null
run_case "reader 应该拒绝（MAGIC_MISMATCH）" "MAGIC_MISMATCH" ./reader

echo
echo "=== 场景 3: 翻转数据字节（模拟内存损坏） ==="
./writer > /dev/null
./corrupt_tool data > /dev/null
run_case "reader 应该拒绝（CRC_MISMATCH）" "CRC_MISMATCH" ./reader

echo
echo "=== 场景 4: 用不兼容的新 major 版本写入 ==="
./writer 2 0 > /dev/null
run_case "reader（期望 major=1）应该拒绝（VERSION_INCOMPATIBLE）" \
	"VERSION_INCOMPATIBLE" ./reader 1

rm -f /dev/shm/shm_examples_header_magic

echo
if [ "$status" -eq 0 ]; then
	echo "全部场景行为符合预期"
else
	echo "存在与预期不符的场景，见上方 [FAIL]"
fi

exit $status
