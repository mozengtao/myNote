#!/usr/bin/env bash
#
# mini-docker.sh - a minimal, educational re-implementation of `docker run`
#
# Combines everything learned in docker/mini-docker-lab/:
#   - PID / UTS / Mount / Network Namespaces   (chapters 2, 3, 4, 5)
#   - chroot into a prepared rootfs             (chapter 8, combined in chapter 9)
#   - cgroups v2: cpu / memory / pids limits    (chapter 10)
#
# Usage:
#   sudo ./mini-docker.sh run <rootfs-dir> [command [args...]]
#
# Example:
#   mkdir -p /tmp/mini-rootfs/bin
#   cp $(which busybox) /tmp/mini-rootfs/bin/busybox
#   for c in sh ls ps hostname mount echo cat mkdir sleep; do
#     ln -sf busybox /tmp/mini-rootfs/bin/$c
#   done
#   sudo ./mini-docker.sh run /tmp/mini-rootfs /bin/sh
#
# Tunable via environment variables:
#   MINI_DOCKER_CPU_MAX     default "50000 100000" (~0.5 CPU core, cgroup v2 cpu.max syntax)
#   MINI_DOCKER_MEM_MAX     default "256M"
#   MINI_DOCKER_PIDS_MAX    default "64"
#   MINI_DOCKER_HOSTNAME    default "mini-box"
#
# What this script deliberately does NOT do (see the corresponding chapter instead):
#   - no image format / registry pull -> you must supply a ready-made rootfs directory
#   - no OverlayFS layer wiring       -> chapter 7 (you can point rootfs at an overlay "merged" dir yourself)
#   - no real networking              -> chapter 6 (net namespace here starts empty/isolated)
#   - no User Namespace / seccomp / capability dropping -> out of scope for this course

set -euo pipefail

CGROUP_ROOT="/sys/fs/cgroup"
CGROUP_NAME="mini-docker-$$"
CGROUP_PATH="${CGROUP_ROOT}/${CGROUP_NAME}"

CPU_MAX="${MINI_DOCKER_CPU_MAX:-50000 100000}"
MEM_MAX="${MINI_DOCKER_MEM_MAX:-256M}"
PIDS_MAX="${MINI_DOCKER_PIDS_MAX:-64}"
HOSTNAME_IN_CONTAINER="${MINI_DOCKER_HOSTNAME:-mini-box}"

usage() {
  echo "Usage: sudo $0 run <rootfs-dir> [command [args...]]" >&2
  exit 1
}

require_root() {
  if [ "$(id -u)" -ne 0 ]; then
    echo "[mini-docker] must be run as root (it creates namespaces and cgroups)" >&2
    exit 1
  fi
}

setup_cgroup() {
  # Best-effort: parent cgroup may already have these controllers enabled.
  echo "+cpu +memory +pids" > "${CGROUP_ROOT}/cgroup.subtree_control" 2>/dev/null || true

  mkdir -p "${CGROUP_PATH}"
  echo "${CPU_MAX}"  > "${CGROUP_PATH}/cpu.max"
  echo "${MEM_MAX}"  > "${CGROUP_PATH}/memory.max"
  echo "${PIDS_MAX}" > "${CGROUP_PATH}/pids.max"

  # Put THIS script's own PID into the cgroup. Every process it forks from
  # here on (unshare -> chroot -> the container command) automatically
  # inherits cgroup membership, exactly like chapter 10's manual experiments.
  echo $$ > "${CGROUP_PATH}/cgroup.procs"

  echo "[mini-docker] cgroup ready: ${CGROUP_PATH} (cpu.max='${CPU_MAX}', memory.max=${MEM_MAX}, pids.max=${PIDS_MAX})" >&2
}

cleanup_cgroup() {
  # Move ourselves back to the root cgroup so the child cgroup becomes
  # empty and can be removed (cgroup v2 refuses to rmdir a non-empty group).
  echo $$ > "${CGROUP_ROOT}/cgroup.procs" 2>/dev/null || true
  rmdir "${CGROUP_PATH}" 2>/dev/null || true
}

main() {
  require_root

  [ $# -ge 1 ] || usage
  local action="$1"; shift
  [ "$action" = "run" ] || usage

  [ $# -ge 1 ] || usage
  local rootfs="$1"; shift
  [ -d "$rootfs" ] || { echo "[mini-docker] rootfs directory not found: $rootfs" >&2; exit 1; }

  local cmd=("$@")
  if [ ${#cmd[@]} -eq 0 ]; then
    cmd=("/bin/sh")
  fi

  mkdir -p "${rootfs}/proc"

  trap cleanup_cgroup EXIT

  setup_cgroup

  echo "[mini-docker] starting '${cmd[*]}' in ${rootfs} as hostname '${HOSTNAME_IN_CONTAINER}'" >&2
  echo "[mini-docker] (PID/UTS/Mount/Network namespaces + chroot + cgroups all active)" >&2

  set +e
  unshare --pid --uts --mount --net --fork \
    --mount-proc="${rootfs}/proc" \
    chroot "${rootfs}" /bin/sh -c "
      /bin/hostname '${HOSTNAME_IN_CONTAINER}' 2>/dev/null
      exec ${cmd[*]}
    "
  local status=$?
  set -e

  echo "[mini-docker] container exited with status ${status}" >&2
  exit "${status}"
}

main "$@"
