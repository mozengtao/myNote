#!/usr/bin/bash
#
# 通用「多 subcommand」shell 脚本模板
# ----------------------------------------------------------------------------
# 适用场景：一个脚本对外暴露多个子命令（如 start / show / clean / help），
# 每个子命令可再带一个 target 参数。
#
# 分层心智模型（依赖单向向下，禁止向上回调）：
#   入口/分发层 main        ——  解析第一个动作（cmd），分发到对应 handler
#   命令处理层 cmd_*         ——  只校验本动作的 target 参数是否合法
#   领域操作层 do_*          ——  一个函数 = 一件业务事，不解析命令行、不写死配置
#   通用原语层 log/die/...   ——  与业务无关的积木：日志、报错、依赖检查、命令封装
#   配置/数据层 config 文件  ——  所有可变值（路径、token、前缀…），不含逻辑
#
# 用法：复制本文件 -> 改名 -> 替换 do_* 业务函数和 case 分发表即可。

# set -e: 命令失败立即退出；-u: 引用未定义变量报错；-o pipefail: 管道中任一段失败即失败
set -euo pipefail

# ----------------------------------------------------------------------------
# 配置/数据层：脚本自身位置 + 外部配置文件路径（允许用环境变量覆盖）
# ----------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_FILE="${MYTOOL_CONFIG:-$SCRIPT_DIR/mytool.conf}"

# ----------------------------------------------------------------------------
# 通用原语层：与业务无关，可整段复制到任何脚本
# ----------------------------------------------------------------------------

# 普通日志：写到 stdout
log() { printf '%s\n' "$*"; }

# 错误日志并退出：写到 stderr，约定退出码非 0
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

# 依赖检查：确保所需外部命令存在，缺失则提前失败而非运行到一半报错
require() {
    local cmd
    for cmd in "$@"; do
        command -v "$cmd" >/dev/null 2>&1 || die "missing required command: $cmd"
    done
}

# 帮助信息：用 heredoc 集中维护，子命令一行一条，方便对齐
usage() {
    cat <<EOF
usage: $(basename "$0") <command> [target]

commands:
  start <foo|bar>   start a service (foo, or bar)
  show <status>     show current status
  clean <all>      clean up resources
  help              show this help

configuration: $CONFIG_FILE
EOF
}

# 配置加载 + 校验：source 外部配置，并用 \${VAR:?} 断言关键变量必须存在
load_config() {
    [[ -f "$CONFIG_FILE" ]] || die "config file not found: $CONFIG_FILE (set MYTOOL_CONFIG or copy mytool.conf.example)"
    # shellcheck source=/dev/null
    source "$CONFIG_FILE"
    # 关键配置缺失即报错退出，把"配置错误"挡在业务逻辑之前
    : "${SERVICE_PREFIX:?SERVICE_PREFIX not set in $CONFIG_FILE}"
    : "${WORK_DIR:?WORK_DIR not set in $CONFIG_FILE}"
}

# ----------------------------------------------------------------------------
# 领域操作层：每个函数只做一件业务事；配置值从全局变量读取，不在此硬编码
# 这里用占位实现演示，真实脚本替换为具体命令即可。
# ----------------------------------------------------------------------------

do_start_foo() {
    log "starting foo under $WORK_DIR (prefix=$SERVICE_PREFIX)"
    # 例：(cd "$WORK_DIR" && some_tool run foo)
}

do_start_bar() {
    log "starting bar under $WORK_DIR (prefix=$SERVICE_PREFIX)"
    # 例：(cd "$WORK_DIR" && some_tool run bar)
}

do_show_status() {
    log "status of $SERVICE_PREFIX services:"
    # 例：some_tool status | awk -v pat="$SERVICE_PREFIX" '$0 ~ pat'
}

do_clean_all() {
    log "cleaning all $SERVICE_PREFIX resources"
    # 例：some_tool stop --all --prefix "$SERVICE_PREFIX"
}

# ----------------------------------------------------------------------------
# 命令处理层 cmd_*：只负责校验本动作的 target，分发到领域函数
# 约定：target 缺省为空串，非法 target 给出"该命令的用法"提示
# ----------------------------------------------------------------------------

cmd_start() {
    local target=${1:-}
    case $target in
        foo) do_start_foo ;;
        bar) do_start_bar ;;
        *)   die "usage: start <foo|bar>" ;;
    esac
}

cmd_show() {
    local target=${1:-}
    case $target in
        status) do_show_status ;;
        *)      die "usage: show <status>" ;;
    esac
}

cmd_clean() {
    local target=${1:-}
    case $target in
        all) do_clean_all ;;
        *)   die "usage: clean <all>" ;;
    esac
}

# ----------------------------------------------------------------------------
# 入口/分发层 main：解析第一个动作，分发到对应 handler
# ----------------------------------------------------------------------------
main() {
    # 取第一个参数作为命令；缺省为 help。shift 把它从 $@ 移除，余下参数即 target
    local cmd=${1:-help}
    shift || true

    # help 走在 load_config 之前：没有配置也能看用法
    case $cmd in
        help | -h | --help)
            usage
            return 0
            ;;
    esac

    # 业务命令统一在执行前加载配置 + 检查依赖
    require awk
    load_config

    # 分发表：命令名 -> cmd_* handler；支持别名（如 list/ls）
    case $cmd in
        start)       cmd_start "$@" ;;
        show)        cmd_show "$@" ;;
        clean)       cmd_clean "$@" ;;
        *)
            usage >&2
            die "unknown command: $cmd"
            ;;
    esac
}

# 把所有命令行参数原样传入 main，作为脚本唯一入口
main "$@"
