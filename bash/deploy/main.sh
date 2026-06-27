#!/usr/bin/env bash
#
# 程序入口（Execution 的发起点）。
# 职责：加载各层 -> 按参数分发到对应 Workflow。
#
# 用法：
#     bash main.sh deploy      # 发布全流程（缺省）
#     bash main.sh rollback    # 回滚全流程
#
# 注意：本 demo 中所有 tar/scp/ssh/systemctl/curl 都用 echo 代替，
# 因此可以安全地直接运行，用来直观理解大型 Bash 项目的分层组织方式。

set -euo pipefail

# 用脚本自身所在目录定位各层，保证在任意目录下都能运行。
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

# Data 层
# shellcheck source=config/config.sh
source "$ROOT/config/config.sh"

# Primitive 层
for f in "$ROOT"/lib/*.sh
do
    # shellcheck source=/dev/null
    source "$f"
done

# Workflow 层
for f in "$ROOT"/workflow/*.sh
do
    # shellcheck source=/dev/null
    source "$f"
done

usage()
{
    cat <<EOF
用法: $(basename "$0") [deploy|rollback]

  deploy    发布全流程: archive -> upload -> stop -> deploy -> start -> health
  rollback  回滚全流程: stop -> 切回上一版本 -> start -> health

(本 demo 命令均以 echo 模拟，不会真正执行)
EOF
}

main()
{
    local action=${1:-deploy}

    case "$action" in
        deploy)
            deploy_cluster
            ;;
        rollback)
            rollback_cluster
            ;;
        -h|--help|help)
            usage
            ;;
        *)
            echo "未知动作: $action" >&2
            usage >&2
            exit 1
            ;;
    esac
}

main "$@"
