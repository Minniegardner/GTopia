#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
SQL_FILE="${ROOT_DIR}/Configs/gtopia.sql"
RUNTIME_DIR="${ROOT_DIR}/Runtime"

DB_NAME="${DB_NAME:-gtopia}"
DB_USER="${DB_USER:-gtopia}"
DB_PASS="${DB_PASS:-gtopia123}"
DB_HOST="${DB_HOST:-127.0.0.1}"
DB_PORT="${DB_PORT:-3306}"

log() {
    printf '\n[%s] %s\n' "$(date +'%H:%M:%S')" "$*"
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

install_db_packages() {
    if ! have_cmd apt-get; then
        echo "apt-get not found. This script is meant for Ubuntu/Debian VPS only."
        exit 1
    fi

    as_root apt-get update
    as_root apt-get install -y mariadb-server mariadb-client
}

start_database_service() {
    if have_cmd systemctl; then
        as_root systemctl enable mariadb >/dev/null 2>&1 || true
        as_root systemctl start mariadb
        return
    fi

    if have_cmd service; then
        as_root service mysql start || as_root service mariadb start
    fi
}

prepare_runtime_dirs() {
    mkdir -p "${RUNTIME_DIR}/cache" \
        "${RUNTIME_DIR}/worlds" \
        "${RUNTIME_DIR}/renderer_output"

    cp "${ROOT_DIR}/Configs/config.txt" "${RUNTIME_DIR}/config.txt"
    cp "${ROOT_DIR}/Configs/servers.txt" "${RUNTIME_DIR}/servers.txt"

    if [[ -f "${ROOT_DIR}/Configs/telnet_config.txt" ]]; then
        cp "${ROOT_DIR}/Configs/telnet_config.txt" "${RUNTIME_DIR}/telnet_config.txt"
    fi
}

create_database_user() {
    as_root mysql <<SQL
CREATE DATABASE IF NOT EXISTS \`${DB_NAME}\` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER IF NOT EXISTS '${DB_USER}'@'localhost' IDENTIFIED BY '${DB_PASS}';
CREATE USER IF NOT EXISTS '${DB_USER}'@'127.0.0.1' IDENTIFIED BY '${DB_PASS}';
GRANT ALL PRIVILEGES ON \`${DB_NAME}\`.* TO '${DB_USER}'@'localhost';
GRANT ALL PRIVILEGES ON \`${DB_NAME}\`.* TO '${DB_USER}'@'127.0.0.1';
FLUSH PRIVILEGES;
SQL
}

import_sql() {
    if [[ ! -f "${SQL_FILE}" ]]; then
        echo "SQL file not found: ${SQL_FILE}"
        exit 1
    fi

    if mysql -h "${DB_HOST}" -P "${DB_PORT}" -u "${DB_USER}" -p"${DB_PASS}" "${DB_NAME}" -e "SHOW TABLES LIKE 'Players';" 2>/dev/null | grep -q Players; then
        log "Database already contains tables, skipping import"
        return
    fi

    log "Importing ${SQL_FILE} into ${DB_NAME}"
    mysql -h "${DB_HOST}" -P "${DB_PORT}" -u "${DB_USER}" -p"${DB_PASS}" "${DB_NAME}" < "${SQL_FILE}"
}

show_config() {
    cat <<EOF
Database setup complete.

Use these values in config.txt:
database_info|${DB_HOST}|${DB_USER}|${DB_PASS}|${DB_NAME}|${DB_PORT}|

Runtime folders prepared:
${RUNTIME_DIR}/cache
${RUNTIME_DIR}/worlds
${RUNTIME_DIR}/renderer_output
EOF
}

main() {
    if [[ ! -f "${SQL_FILE}" ]]; then
        echo "Missing SQL file: ${SQL_FILE}"
        exit 1
    fi

    log "Installing database packages"
    install_db_packages

    log "Starting database service"
    start_database_service

    log "Preparing runtime folders"
    prepare_runtime_dirs

    log "Creating database and user"
    create_database_user

    import_sql
    show_config
}

main "$@"