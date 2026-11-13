# Release Checklist

Before publishing a firmware or harness change:

1. Configure a clean Release build with CMake.
2. Run `ctest --test-dir build --output-on-failure`.
3. Run the intentionally slowed firmware case and confirm the regression gate
   fails for the expected reason.
4. Compare any baseline update against the validation matrix and record why the
   expected-good behavior changed.
5. Run the monitoring stack locally and confirm `/metrics`, Prometheus alerts,
   and the Grafana dashboard are reachable on loopback only.
6. Review the staged diff and confirm generated `build/` and `.run/` contents
   are absent.
