#!/usr/bin/env bash
# Builds the harness and starts the live stack:
#   fvh (metrics on :9105) -> Prometheus (:9090, alert rules) -> Grafana (:3000)
# Needs: cmake, a C++17 compiler, prometheus, grafana (brew install prometheus grafana).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RUN="$ROOT/.run"
mkdir -p "$RUN/grafana"

cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$ROOT/build" -j >/dev/null

PIDS=()
cleanup() { kill "${PIDS[@]}" 2>/dev/null || true; }
trap cleanup EXIT INT TERM

# 4 of every 15 cycles run a deliberately slowed firmware build so the dashboard and alerts have something to catch.
"$ROOT/build/fvh" --serve 9105 --interval-sec 2 --bad-build-every 15 >"$RUN/fvh.log" 2>&1 &
PIDS+=($!)

prometheus --config.file="$ROOT/monitoring/prometheus.yml" \
  --storage.tsdb.path="$RUN/prometheus" \
  --web.listen-address=127.0.0.1:9090 >"$RUN/prometheus.log" 2>&1 &
PIDS+=($!)

GRAFANA_HOME="${GRAFANA_HOME:-$(brew --prefix grafana 2>/dev/null)/share/grafana}"
FVH_DASHBOARDS="$ROOT/monitoring/grafana/dashboards" \
GF_PATHS_PROVISIONING="$ROOT/monitoring/grafana/provisioning" \
GF_PATHS_DATA="$RUN/grafana" \
GF_PATHS_LOGS="$RUN/grafana/log" \
GF_AUTH_ANONYMOUS_ENABLED=true \
GF_AUTH_ANONYMOUS_ORG_ROLE=Viewer \
GF_SERVER_HTTP_ADDR=127.0.0.1 \
GF_SERVER_HTTP_PORT=3000 \
  grafana server --homepath "$GRAFANA_HOME" >"$RUN/grafana.log" 2>&1 &
PIDS+=($!)

echo "metrics:    http://127.0.0.1:9105/metrics"
echo "prometheus: http://127.0.0.1:9090/alerts"
echo "grafana:    http://127.0.0.1:3000/d/fvh-overview"
echo "Ctrl-C to stop."
wait
