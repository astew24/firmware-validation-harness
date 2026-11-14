# Monitoring Stack

The stack is configured as code:

- `prometheus.yml` scrapes the local exporter every five seconds.
- `alerts.yml` evaluates validation, latency, corruption, and liveness rules.
- Grafana provisioning loads the dashboard and Prometheus datasource without
  manual setup.

Use `scripts/run_monitoring.sh` to start all three processes. Logs and the
temporary Prometheus database are written to the ignored `.run/` directory.
