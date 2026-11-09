# Validation Matrix

The harness exercises the firmware at three boundaries: sensor input, state
transitions, and telemetry on the wire. Every scenario is deterministic when
the seed is held constant, which makes a failure suitable for local replay and
CI triage.

| Scenario | Injected condition | Expected result |
|---|---|---|
| nominal | clean sensor samples | RUN with no faults |
| sensor dropout | missing samples for 20 ms | timeout detected within 30 ms |
| noise spike | short outlier burst | fault and recovery without a false alarm |
| out_of_range | ADC value outside limits | range fault with clean recovery |
| wire_corruption | flipped telemetry bits | bad frames rejected and stream resynchronized |
| combined | dropout plus wire corruption | all applicable checks remain within baseline |

The saved CSV baseline is the release gate. A change is acceptable only when
all scenarios pass and detection, recovery, tracking, and packet-integrity
metrics do not regress.
