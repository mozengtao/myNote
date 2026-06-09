#!/usr/bin/env bash

set -Eeuo pipefail

################################################################################
# Global Variables
################################################################################

readonly SCRIPT_NAME="$(basename "$0")"

TASK="evc"
JOB="evc-dentist"
USER="admin"

################################################################################
# Logging
################################################################################

log_info() {
    printf '[INFO ] %s\n' "$*" >&2
}

log_warn() {
    printf '[WARN ] %s\n' "$*" >&2
}

log_error() {
    printf '[ERROR] %s\n' "$*" >&2
}

################################################################################
# Usage
################################################################################

usage() {
    cat <<EOF
Usage:
    $SCRIPT_NAME [OPTIONS] <command>

Options:
    -t, --task      Nomad task name
    -j, --job       Nomad job name
    -u, --user      NCS username
    -h, --help      Show help

Examples:

    $SCRIPT_NAME \
        'show cable modem brief | tab | nomore'

    $SCRIPT_NAME \
        --task evc \
        --job evc-dentist \
        'show running-config'

EOF
}

################################################################################
# Error Handler
################################################################################

cleanup() {
    :
}

on_error() {
    local exit_code=$?

    log_error "Command failed (exit=${exit_code})"
    exit "$exit_code"
}

trap on_error ERR

################################################################################
# CLI
################################################################################

parse_args() {

    while [[ $# -gt 0 ]]; do
        case "$1" in

            -j|--job)
                JOB="$2"
                shift 2
                ;;

            -t|--task)
                TASK="$2"
                shift 2
                ;;

            -u|--user)
                USER="$2"
                shift 2
                ;;

            -h|--help)
                usage
                exit 0
                ;;

            -*)
                log_error "Unknown option: $1"
                usage
                exit 1
                ;;

            *)
                COMMAND="$1"
                shift
                ;;
        esac
    done

    if [[ -z "${COMMAND:-}" ]]; then
        usage
        exit 1
    fi
}

################################################################################
# NCS
################################################################################

ncs_cli_exec() {

    nomad alloc exec \
        -task "$TASK" \
        -job "$JOB" \
        ncs_cli -u "$USER"
}

: <<'COMMENT'
ncs_cli_exec() {
    local cmd=(
        nomad
        alloc
        exec
        -task "$TASK"
        -job "$JOB"
        ncs_cli
        -u "$USER"
    )

    "${cmd[@]}"
}
COMMENT

run_ncs_command() {

    local cmd="$1"

    log_info "job=$JOB task=$TASK"
    log_info "Executing: $cmd"

    {
        printf '%s\n' "unhide debug"
        printf '%s\n' "$cmd"
    } | ncs_cli_exec
}

################################################################################
# Main
################################################################################

main() {

    parse_args "$@"

    run_ncs_command "$COMMAND"
}

main "$@"
