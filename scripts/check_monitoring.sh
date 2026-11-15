#!/usr/bin/env bash
set -euo pipefail

check() {
  local name="$1"
  local url="$2"
  if curl --fail --silent --show-error --max-time 3 "$url" >/dev/null; then
    echo "ok: $name"
  else
    echo "not ready: $name" >&2
    return 1
  fi
}

check metrics http://127.0.0.1:9105/metrics
check prometheus http://127.0.0.1:9090/-/ready
check grafana http://127.0.0.1:3000/api/health
