#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

pkill -f "$ROOT/build/fvh --serve" 2>/dev/null || true
pkill -f 'prometheus --config.file' 2>/dev/null || true
pkill -f 'grafana server --homepath' 2>/dev/null || true

echo "monitoring processes stopped"
