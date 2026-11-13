# Metrics Exporter

`metrics_server.cpp` exposes the latest validation snapshot on `/metrics` using
Prometheus' text exposition format. The exporter is intentionally small: the
harness owns scenario execution and the HTTP layer only publishes the current
counters and gauges.

The default listener is `127.0.0.1:9105`. It is suitable for local dashboards
and development checks, not for direct exposure to an untrusted network.
