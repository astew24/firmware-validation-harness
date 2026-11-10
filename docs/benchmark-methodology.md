# Benchmark Methodology

Fault-detection latency is measured from the simulated hardware event to the
firmware event frame observed by the harness decoder. Recovery latency begins
with the first good sample after the injected fault and ends after the
controller has accepted the configured run of consecutive good readings.

Each benchmark uses the fake clock rather than wall time. This keeps results
stable across machines and lets the regression gate compare integer
millisecond measurements against `baseline/baseline.csv`.
