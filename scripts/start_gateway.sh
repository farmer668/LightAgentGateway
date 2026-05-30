#!/usr/bin/env bash
set -euo pipefail

PORT="${PORT:-80}"
LOG_PATH="${LOG_PATH:-/tmp/WebServer.log}"
BIN_PATH="${BIN_PATH:-./WebServer}"

# Port 80 usually requires root privileges. Use sudo or set PORT=8080 for
# local development, for example: PORT=8080 ./scripts/start_gateway.sh
exec "${BIN_PATH}" -p "${PORT}" -l "${LOG_PATH}"
