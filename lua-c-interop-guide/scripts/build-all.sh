#!/usr/bin/env bash
#
# build-all.sh
#
# 统一编译回归：递归找到 chapters/*/code/Makefile 和 projects/*/Makefile，
# 逐个 `make clean && make run`，汇总成功/失败列表。
#
# 用法：
#   ./scripts/build-all.sh            # 全部执行
#   ./scripts/build-all.sh --quiet    # 只打印每个目录的 PASS/FAIL，不打印完整构建日志
#
set -uo pipefail
ulimit -c 0 2>/dev/null || true   # 部分 Failure 反例会故意崩溃，禁止落地 core dump 文件

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GUIDE_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

QUIET=0
if [ "${1:-}" = "--quiet" ]; then
  QUIET=1
fi

# 先确保 vendor lua 已经构建好
"${SCRIPT_DIR}/build-vendor-lua.sh"

PASS=()
FAIL=()

run_one() {
  local dir="$1"
  local name="${dir#${GUIDE_ROOT}/}"
  echo "==================================================================="
  echo ">>> $name"
  echo "==================================================================="
  if [ "$QUIET" = "1" ]; then
    if (cd "$dir" && make clean >/dev/null 2>&1; make run) >/tmp/build-all-one.log 2>&1; then
      echo "PASS: $name"
      PASS+=("$name")
    else
      echo "FAIL: $name"
      FAIL+=("$name")
      tail -n 30 /tmp/build-all-one.log
    fi
  else
    if (cd "$dir" && make clean >/dev/null 2>&1; make run); then
      PASS+=("$name")
    else
      FAIL+=("$name")
    fi
  fi
}

# chapters/*/code/Makefile
while IFS= read -r mk; do
  run_one "$(dirname "$mk")"
done < <(find "${GUIDE_ROOT}/chapters" -mindepth 3 -maxdepth 3 -type f -name Makefile | sort)

# projects/*/Makefile
while IFS= read -r mk; do
  run_one "$(dirname "$mk")"
done < <(find "${GUIDE_ROOT}/projects" -mindepth 2 -maxdepth 2 -type f -name Makefile | sort)

echo ""
echo "==================================================================="
echo "汇总：${#PASS[@]} 通过 / ${#FAIL[@]} 失败（共 $((${#PASS[@]} + ${#FAIL[@]})) 个）"
echo "==================================================================="
if [ "${#FAIL[@]}" -gt 0 ]; then
  echo "失败列表："
  for f in "${FAIL[@]}"; do
    echo "  - $f"
  done
  exit 1
fi
exit 0
