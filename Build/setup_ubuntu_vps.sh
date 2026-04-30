#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_ROOT="${SCRIPT_DIR}/build-ubuntu"

PROJECTS=(Master GameServer ItemManager WorldRenderer)

log() {
    printf '\n[%s] %s\n' "$(date +'%H:%M:%S')" "$*"
}

usage() {
    cat <<'EOF'
Usage:
  ./setup_ubuntu_vps.sh setup
  ./setup_ubuntu_vps.sh build [Master|GameServer|ItemManager|WorldRenderer|all]
  ./setup_ubuntu_vps.sh run <Master|GameServer|ItemManager|WorldRenderer>
  ./setup_ubuntu_vps.sh all

Examples:
  ./setup_ubuntu_vps.sh setup
  ./setup_ubuntu_vps.sh all
  ./setup_ubuntu_vps.sh run Master
EOF
}

have_cmd() {
    command -v "$1" >/dev/null 2>&1
}

as_root() {
    if [[ ${EUID:-$(id -u)} -eq 0 ]]; then
        "$@"
    else
        sudo "$@"
    fi
}

install_deps() {
    if ! have_cmd apt-get; then
        echo "apt-get not found. This script is meant for Ubuntu/Debian VPS only."
        exit 1
    fi

    local mysql_pkg="default-libmysqlclient-dev"

    as_root apt-get update

    if ! apt-cache show "${mysql_pkg}" >/dev/null 2>&1; then
        mysql_pkg="libmysqlclient-dev"
    fi

    as_root apt-get install -y \
        build-essential \
        cmake \
        git \
        pkg-config \
        "${mysql_pkg}" \
        libssl-dev
}

build_project() {
    local project="$1"
    local build_dir="${BUILD_ROOT}/${project}"

    mkdir -p "${build_dir}"

    log "Configuring ${project}"
    cmake -S "${ROOT_DIR}" -B "${build_dir}" \
        -DPROJECT_TO_BUILD="${project}" \
        -DCMAKE_BUILD_TYPE=Release

    log "Building ${project}"
    cmake --build "${build_dir}" --parallel "$(nproc 2>/dev/null || echo 2)"
}

build_all() {
    for project in "${PROJECTS[@]}"; do
        build_project "${project}"
    done
}

run_project() {
    local project="$1"
    local binary="${ROOT_DIR}/Runtime/${project}"

    if [[ ! -x "${binary}" ]]; then
        echo "Binary not found: ${binary}"
        echo "Build it first with: ./setup_ubuntu_vps.sh build ${project}"
        exit 1
    fi

    log "Running ${project}"
    exec "${binary}"
}

case "${1:-all}" in
    setup)
        install_deps
        ;;
    build)
        install_deps
        if [[ "${2:-all}" == "all" ]]; then
            build_all
        else
            build_project "${2}"
        fi
        ;;
    run)
        if [[ -z "${2:-}" ]]; then
            usage
            exit 1
        fi
        install_deps
        build_project "${2}"
        run_project "${2}"
        ;;
    all)
        install_deps
        build_all
        ;;
    -h|--help|help)
        usage
        ;;
    *)
        usage
        exit 1
        ;;
esac