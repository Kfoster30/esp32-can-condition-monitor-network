# Test Plan

## Automated tests

| Test | Expected result |
|---|---|
| Heartbeat golden vector | Exact expected 8-byte payload |
| Condition golden vector | Exact expected 8-byte payload |
| Motion/power golden vector | Exact expected 8-byte payload |
| Encode/decode round trips | Decoded object equals source object |
| Invalid payload length | Decoder rejects frame |
| Invalid system state | Decoder rejects frame |
| Sensor firmware build | Successful PlatformIO build |
| Dashboard firmware build | Successful PlatformIO build |

## Bench tests

| ID | Procedure | Pass condition |
|---|---|---|
| BT-01 | Measure unpowered CANH-to-CANL resistance. | Approximately 60 ohms |
| BT-02 | Run both nodes for 10 minutes. | No resets or timeout events |
| BT-03 | Analyze received frame timing. | Periods meet FR-02 through FR-04 |
| BT-04 | Inspect dashboard sequences. | No gaps during healthy run |
| BT-05 | Disconnect one CAN wire for at least 3 seconds. | Timeout events assert |
| BT-06 | Reconnect the CAN wire. | Traffic resumes and timeout events clear |
| BT-07 | Reset only the sensor ECU. | Dashboard detects interruption and recovery |
| BT-08 | Reverse CANH/CANL while unpowered, then test. | No valid traffic; restore before continuing |

Record results and references to logs/photographs in `evidence/bench_results.md`.
Record the firmware commit, board role, date, test ID, and result for every run.
Large local captures remain in the ignored `data/can-soak-*` directories unless
explicitly reviewed and selected for publication.

### Recorded acceptance status

See [bench results](../evidence/bench_results.md) for evidence and qualifications.
BT-01 is verified on the replacement bus at 59.2 ohms. BT-02/BT-03 and decoded
telemetry BT-04 passed the 0.3.0 mixed-source 600-second capture. BT-07 reset,
session rebaselining, and temperature recovery were observed in a separate
intentional-interruption capture whose automated result remains REVIEW.
BT-05/BT-06 have only a CANH-open-at-startup / power-off-restoration variant;
continuous live-wire interruption/reconnection is not verified. BT-08 remains
unperformed. No powered wire changes are authorized by these recorded results.

## Ten-minute telemetry capture

Keep both nodes powered and connected, close all serial monitors, and do not
press EN/BOOT or change wiring while capturing. The saved soak captures used
dashboard COM12, but the later manual check identified dashboard COM8 from its
role banner with the sensor USB unplugged. Re-identify the physical dashboard
before each test; COM numbers are not permanent role assignments.

```powershell
py tools/capture_can.py --port COM12 --seconds 600
```

Run from the project directory. The tool sets DTR/RTS inactive before opening the
port and does not flash, erase, or intentionally reset either board. It prints
progress every 30 seconds and stops automatically. Ctrl+C stops early and saves
an incomplete summary. Port failures save a REVIEW result, not a false pass.

Each unique `data/can-soak-*` folder contains:

- `raw.log`: original serial bytes, including startup and fault messages.
- `telemetry.csv`: parsed records with host elapsed time and dashboard millis.
- `summary.json`: completion status, counts/rates, largest observed intervals,
  timeout transitions, restart indicators, parsing errors, and review reasons.

Healthy targets are approximately 600 heartbeat, 3,000 condition, and 6,000
motion/power records in 600 seconds, with boundary variation. The screening
requires all three streams, at least 90% of target counts, mean periods within
10% of 1000/200/100 ms, no intervals above 1.5 times their target, traffic near
both capture boundaries, and no timeouts, error lines, observed restarts, or
malformed records. Startup banners during the run require review too.

These are telemetry-level screening tolerances, not hard real-time guarantees.
Firmware 0.1/0.2 does not export sequence/session metadata, so those captures
cannot verify BT-04 even when timing passes. The separate 0.3.0 run below
verifies decoded-telemetry continuity under its documented assumptions, not
zero raw CAN frame loss. Uptime alone may miss quick unobserved sensor resets;
0.3.0 adds boot-session attribution with its own bounded-observation limitations.

## DHT11 hardware extension (0.2.0)

See [DHT11 integration](DHT11.md) for deployment and pending bench acceptance.
New host vectors cover missing-temperature sentinels, stale values, conflicting
quality bits, and mixed-source heartbeat flags. The capture tool records labeled
`nan` values rather than discarding them and flags stale/missing readings for
review. `--require-hardware-temperature` additionally rejects a simulated-only
run. Source flags indicate firmware mode, not independently measured sensor
accuracy. Existing 0.1.0 simulated PASS evidence is not automatically a 0.2.0 PASS.

## Sequence observability extension (0.3.0)

The healthy 600.031-second run `can-soak-20261020T040511Z-8b0668` passed overall
and sequence screening. All three streams had zero inferred missing sequences,
duplicates, late records, ambiguity, unattributed records, and session changes.
The separate 60.062-second sensor-reset run
`can-soak-20261020T042303Z-f9d904` correctly remained REVIEW: one sensor restart,
one session change per stream, temporary missing temperature, and longer
intervals were observed. Dashboard time continued, and valid temperature
returned. Neither run is a continuous live CAN-wire fault-injection test.

See [sequence observability](SEQUENCES.md) for the boot-session control frame,
appended serial fields, exact classification rules, limitations, and deployment
requirements. New host tests cover skipped publication sequences, duplicates,
late records, normal counter wraps, session transitions, unknown attribution,
half-range ambiguity, and long silence. This is automated software evidence,
not a completed physical CAN fault-injection test.

For BT-04, after successful builds and uploads of both updated roles, run a new
healthy 600-second capture with both `--require-hardware-temperature` and
`--require-sequences`. Require overall PASS and sequence PASS with metadata on
all telemetry records and no observed anomalies. Older logs should report
sequence UNVERIFIED, never retrospectively PASS. Repeat a deliberate sensor
reset separately to verify session rebaselining; that intentional interruption
is not a healthy soak run.

## Hardware-power extension (0.4.0; healthy soak and SDA-open/restoration observed)

See [combined DHT11/INA219 integration](INA219_CAN.md). The standalone INA219
meter comparison, no-load behavior, SDA failure NaNs, and reported recovery are
manual evidence; they do not establish timing or fault behavior when sharing
the ECU with DHT/CAN tasks. All four application builds subsequently passed,
dashboard-first/sensor uploads were reported successful, and a fresh 0.4.0
600-second combined run passed BT-02/BT-03/BT-04. That capture retained 600
heartbeats, 2,999 condition records, and 5,999 motion/power records, with flags 7,
zero unavailable hardware values, and sequence PASS. See bench evidence for
hashes and bounds. A separate 30-second startup SDA-open capture retained
30/150/300 records with numeric temperature, power NaNs/warning 32, sequence
PASS, and no observed timeout/reset. Overall REVIEW is expected solely for
unavailable power. A separate 30-second capture after power-off SDA restoration
and reboot passed overall/sequences, with 30/150/300 records, RUN/warnings 0,
and no unavailable hardware readings or observed reset/timeout. The subsequent
combined deliberate sensor-reset capture is expected REVIEW: one sensor reset
and session change per stream, a data-timeout assertion/clearing, initial
missing power/temperature, then RUN/warnings 0 with numeric hardware readings.
Dashboard did not restart. See bench results for the 59/296/591 records and
timing boundaries. Live loss-detection/retry latency is not established by
these power-off wire tests or the separate EN-reset test.

A combined DHT OUT-open-at-startup capture also retained 30/150/300 records:
all conditions warning 64/NaN temperature, numeric INA219 power throughout,
sequence PASS, no observed timeout/reset. Overall REVIEW is expected solely
for missing temperature. After power-off OUT restoration and reboot, a separate
30-second capture passed overall/sequences with 30/150/300 records, RUN/warnings
0, and no unavailable hardware readings or observed reset/timeout. This does
not establish an intermediate STALE state or live loss/reconnection latency.

Run the healthy capture with `--require-hardware-temperature`,
`--require-hardware-power`, and `--require-sequences` together. Source flags
must remain 7; neither missing power samples nor power warning-32 conditions
are allowed in a healthy PASS. Any intentional fault remains REVIEW even after
recovery. Unexpected non-comment serial lines also require review, including
garbled ASCII that would formerly only have incremented other_lines.

Repeat BT-02/BT-03/BT-04 with both hardware channels active, plus separately
documented INA219-loss/restoration and sensor-reset cases. No continuous powered
wire change is requested by this plan. BT-05/BT-06's earlier startup-open variant
and the unresolved checksum/serial-text observations keep their original limits.

## MPU6050 motion over CAN (0.5.0)

The regular firmware passed the 600-second still soak on November 9,
2026. The rocking response and deliberate MPU loss and restoration were
verified with the serialized bench variant. See the [bench results](../evidence/bench_results.md).

Use `esp32dev_sensor_dht11_ina219_mpu6050` with the 0.5.0
`esp32dev_dashboard`. Older sensor builds retain their prior wire layouts
and source flags. The combined sensor announces flags 15, with the condition
percentage still simulated. Motion is the RMS change in the acceleration
vector over a nominal one-second window, in whole mg. Changing tilt
contributes to this measurement. CAN publishes the latest window every
100 ms while it is fresh, so adjacent values can repeat.

The regular combined sensor build serializes CAN transmit bursts with
shared I²C transactions and publishes without the bench startup delay.
Power the dashboard first for acceptance captures. The sensor startup
marker is `#can_i2c_serialization=enabled`. If CAN transmission stalls,
the sensor releases I²C after the bounded wait and resumes publishing
once the pending transmit queue drains. Record any drain warnings.

- For a healthy capture, allow sensor startup to settle, then run with
  `--require-hardware-motion`, `--require-hardware-temperature`,
  `--require-hardware-power`, and `--require-sequences`. Verify the motion
  value rises during gentle rocking and returns near its still baseline.
  Repeat the ten-minute capture to check timing and availability.
- For loss isolation, power both nodes off and open only the MPU6050's SDA
  branch. Leave the INA219 connected to the shared SDA and SCL lines.
  After startup settles, expect motion quality 0 and `nan` (wire value
  65535), condition warning mask 0x10, and an overall REVIEW due to unavailable
  motion. Temperature and power should remain valid and sequences should pass.
- Restore the MPU6050 branch with both nodes off and repeat a healthy
  capture. Record the recovery as a separate run. This procedure establishes
  startup fault behavior and restoration after reboot.

