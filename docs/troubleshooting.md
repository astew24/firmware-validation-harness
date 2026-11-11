# Troubleshooting

## The regression gate fails

Run the failing scenario with `--verbose`, then compare its detection and
recovery values with the matching row in `baseline/baseline.csv`. Do not update
the baseline until the firmware change and the new result are understood.

## Telemetry frames are rejected

Check the CRC failure count first. A few rejected frames are expected in the
wire-corruption scenario; a rising count in nominal traffic usually indicates
a framing, buffer, or checksum change.

## The dashboard is empty

Confirm the harness is listening on port 9105 and that Prometheus can reach
`127.0.0.1:9105`. The orchestration script writes component logs under `.run/`.
