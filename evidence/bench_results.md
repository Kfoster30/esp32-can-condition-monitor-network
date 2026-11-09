# Bench Results

## Current acceptance summary

| Check | Verified result and boundary |
|---|---|
| BT-01 | PASS: current replacement bus measured 59.2 ohms unpowered |
| BT-02 / BT-03 | PASS: latest 0.4.0 DHT11/INA219 600-second stability/timing capture |
| BT-04 | PASS: 0.4.0 decoded telemetry sequence/session coverage; not a raw CAN loss guarantee |
| BT-05 / BT-06 | Startup-open / power-off restoration variant observed; live CAN-wire interruption/reconnection remains unverified |
| BT-07 | 0.3.0 and combined 0.4.0 deliberate sensor reset/session recovery observed; intentional captures remain REVIEW |
| BT-08 | Not performed |
| DHT11 | Valid reception, combined 0.4.0 startup OUT-open with INA/CAN continuing, power-off OUT restoration PASS, and post-reset qualification observed; accuracy/live STALE timing unverified |
| INA219 | Standalone checks, 0.4.0 healthy CAN soak, startup SDA-open expected REVIEW, and power-off SDA restoration PASS observed; live retry latency unverified |
| MPU6050-compatible | Shared-bus identity read observed; standalone acceleration responds to orientation, but stationary norm about 0.87/1.19 g remains under review; no calibrated motion/CAN acceptance |

Historical results below retain their original firmware-specific limitations.
In the latest combined target, temperature, bus voltage, and LED-branch current
are hardware-based; condition percentage and motion remain simulated. The
startup core-dump checksum warning remains
unresolved; these tests do not establish safety-rated operation.

## BT-01 — CAN termination inspection

| Field | Result |
|---|---|
| Date | 2026-09-19 |
| Equipment | Digital multimeter |
| Module 1, CANH to CANL | 123 ohms |
| Module 2, CANH to CANL | 123 ohms |
| Interpretation | Each module has onboard termination |
| Predicted assembled-bus resistance | 61.5 ohms |
| Measured assembled-bus resistance | 61.4 ohms |
| External termination required | No |
| Status | PASS |

The measured assembled-bus resistance is within the expected 60–62 ohm range.
BT-01 is complete.

## 2026-10-19 — Replacement hardware and first live CAN link

Evidence below was reported by the operator and shown in photographs/terminal
screenshots in the project chat. These are manual observations, not an automated
soak-test report. Firmware source baseline: `04059025b9d96b98805ee44b7d3d29c7181dc079`;
reported firmware version: `0.1.0`.

- Replaced the original transceivers with two assembled Waveshare SN65HVD230 boards.
- Current assembled bus, both nodes unpowered: **59.2 ohms** between CANH and CANL.
- Each module is supplied by its own ESP32 3V3 pin; nodes share ground, not 3V3.
- Sensor power LED stayed off when only the dashboard USB was connected.
- Both standard ESP32s successfully flashed and passed upload data-hash checks.
- Startup banners confirmed `SENSOR,simulation=1` and `DASHBOARD`; both reported
  CAN controller ready at 500 kbit/s, TX GPIO17, RX GPIO16.
- Connection-time port mapping: sensor **COM8**, dashboard **COM12**. Re-identify
  ports if USB connections change.
- Dashboard displayed live `heartbeat`, `condition`, and `motion_power` records.
  Values are simulated, not measurements from physical sensors.

### Sensor power-loss/recovery test — PASS (manual observation)

Procedure: leave dashboard powered and monitored; disconnect only sensor USB,
then reconnect sensor USB without changing CAN wiring.

Screenshot evidence showed, in order:

1. Valid condition/motion telemetry, including dashboard timestamp 114750 ms.
2. `#event,data_timeout,1` and `#event,heartbeat_timeout,1`.
3. Condition/motion telemetry resumed at dashboard timestamp 122756 ms.
4. `#event,data_timeout,0`.
5. Heartbeat at dashboard timestamp 123238 ms with sensor uptime 1 second.
6. `#event,heartbeat_timeout,0` and continued telemetry.

This verifies the observed assertion and clearing of both timeout states and
continued dashboard operation across a sensor power cycle. The events themselves
do not contain timestamps, so exact assertion latency is **not measured** here.
This is a power-loss variant, not evidence that a CAN-wire-disconnection test was
performed. This screenshot alone does not establish ten-minute stability, exact
frame timing, sequence-gap counts, or reverse-polarity fault injection.

### BT-02 / BT-03 — Ten-minute telemetry stability and timing — PASS

| Field | Result |
|---|---|
| Local date/time | 2026-10-19, approximately 21:14:30–21:24:30 America/Chicago |
| Start time UTC | 2026-10-20T02:14:30.634812+00:00 |
| Capture duration | 600.078 seconds (600 seconds requested), complete |
| Dashboard serial interface | COM12, 115200 baud |
| Firmware/source baseline | 0.1.0 / `04059025b9d96b98805ee44b7d3d29c7181dc079` |
| Sensor mode | Simulated data |
| Capture ID | `can-soak-20261020T021430Z-d8ee30` |
| Result | PASS, no review reasons |

| Stream | Captured records | Mean interval | Min–max interval | Observed rate |
|---|---:|---:|---:|---:|
| Heartbeat | 600 | 999.998 ms | 999–1000 ms | 1 Hz |
| Condition | 3,000 | 200.000 ms | 199–201 ms | 5 Hz |
| Motion/power | 6,000 | 100.000 ms | 99–101 ms | 10 Hz |

All **9,600** expected decoded telemetry records were captured. Summary reported
zero timeout events, observed dashboard/sensor restarts, startup banners,
warning/error lines, malformed records, UTF-8 decode errors, and late intervals
under the documented 1.5-times-target screening threshold.

The saved CSV was independently grouped and its dashboard-timestamp intervals
recomputed, confirming the counts and interval ranges above. The raw log was
also searched for timeout, warning/error, startup, and reset messages; none were
found. This supports BT-02 and telemetry-level BT-03 under the documented bench
tolerances. It does **not** prove zero raw CAN frame loss: firmware 0.1.0 omits
sequence numbers from its serial output, so **BT-04 remains unverified**. Restart
counts are observed/inferred indicators, not proof that every possible brief
sensor restart would be detected. Electrical timing and reverse-polarity tests
remain outside this result.

Local evidence files (ignored by Git by default) are under
`data/can-soak-20261020T021430Z-d8ee30/`:

| File | SHA-256 |
|---|---|
| `raw.log` | `230d936dfbad797ecaff3747887e080c5b4eb659316f2558def0ca845296e871` |
| `telemetry.csv` | `e3fcb6350e9c5436e907114f1e9faf71101ff01acf3d9d1b5075738cc4f8a80e` |
| `summary.json` | `902768eba1a1fb69bc3f3cdfc6ca8dc947397e0465629eab4f2c77f5302a7011` |

## Firmware 0.2.0 — DHT11 first hardware reading

Operator terminal screenshots confirmed:

- All three environments (`esp32dev_sensor_sim`, `esp32dev_sensor_dht11`, and
  `esp32dev_dashboard`) compiled successfully in the operator's PowerShell.
  This also evaluates the C++ temperature-state compile-time assertions.
- Dashboard and DHT11 sensor uploads completed successfully, with written-data
  hash verification. The earlier host-session compiler DLL launch error did not
  reproduce in the operator's build.
- Dashboard startup reported firmware `0.2.0`, role `DASHBOARD`, and CAN ready.
- Received source annotation:
  `#sources,condition=SIM,temperature=DHT11,motion=SIM,power=SIM`.
- `#temperature_quality,HW_VALID` appeared; heartbeats carried flags `3`.
- Visible condition records showed `RUN`, warnings `0`, faults `0`, and
  approximately **24.5–24.7 C** temperature. These are the DHT11 channel values;
  condition percentage, motion, and power remain simulated.

**First hardware-temperature receipt: PASS (manual screenshot observation).**
This is not a thermometer-reference comparison, controlled thermal-response
test, DHT loss/recovery test, or a new ten-minute stability result; those remain
pending. Startup showed the previously observed core-dump checksum warning,
followed by continued application startup and valid telemetry. Its underlying
stored-record issue has not been cleared or conclusively diagnosed.

### Informal hand-proximity observation

After holding a hand near the blue sensor housing, the operator reported
**24.6 C to 24.9 C**. A dashboard screenshot showed the corresponding values
in the final condition column, heartbeat source flags 3, continued telemetry,
state `RUN`, warnings 0, and faults 0.

The observed change is +0.3 C. It is recorded as an informal response observation,
not a calibrated thermal-response or accuracy PASS: no reference thermometer,
controlled heat input, repeatability, or cooling-back-to-baseline evidence was
provided, and a change this small may include normal sensor/ambient variation.
The following fresh capture tests timing with DHT sampling active; the earlier
simulated soak alone could not establish timing performance of the new DHT task.

### BT-02 / BT-03 — DHT11 mixed-source stability and timing — PASS

| Field | Result |
|---|---|
| Local date/time | 2026-10-19, approximately 21:58–22:08 America/Chicago |
| Start time UTC | 2026-10-20T02:58:00.873111+00:00 |
| Capture ID | `can-soak-20261020T025800Z-e5aae7` |
| Duration | 600.031 seconds, complete |
| Mode enforcement | `--require-hardware-temperature`, true |
| Dashboard serial interface | COM12, 115200 baud |
| Firmware attribution | 0.2.0 based on preceding build/upload/startup observations |
| Result | PASS, no review reasons |

| Stream | Captured complete records | Mean interval | Min–max interval |
|---|---:|---:|---:|
| Heartbeat | 600 | 999.998 ms | 999–1000 ms |
| Condition | 3,000 | 200.000 ms | 199–201 ms |
| Motion/power | 5,999 | 100.000 ms | 99–101 ms |

All 600 heartbeat records carried flags 3. All 3,000 condition records showed
`RUN`, warnings 0, and faults 0. The summary reported zero missing or stale
temperature records, timeout transitions, observed restarts, startup banners,
warning/error lines, malformed records, decoding errors, and late intervals.
Temperature values ranged from 23.0 to 24.9 C (mean approximately 24.107 C).
Condition percentage, motion, voltage, and current remain simulated; temperature
is the DHT11 channel, with no independent reference-accuracy claim.

Independent CSV grouping and interval recomputation confirmed these results.
The raw log contains no timeout, warning/error, startup, or reset messages. The
raw file ends with an incomplete next line (`m`), which the fixed-duration
capture deliberately excludes from CSV. The 5,999 complete motion/power records
have no interval above 101 ms; the count is consistent with a capture/serial
boundary and does not demonstrate an in-run dropped CAN frame. It also does not
prove zero CAN sequence gaps: sequence numbers are still absent from serial
telemetry. Reference temperature accuracy and DHT loss/recovery remain pending.

The capture did not include a startup version banner (`firmware_versions_observed`
is empty). Its `local_source_commit` is the unchanged Git HEAD ancestry, not a
unique identifier for the uncommitted 0.2.0 source changes. Do not represent it as
a committed 0.2.0 release or as a device-firmware readback verification.

Local artifacts under `data/can-soak-20261020T025800Z-e5aae7/`:

| File | SHA-256 |
|---|---|
| `raw.log` | `cad804ee49ed5060812df5ac5d400a7111e1e4a2bf08242342b34fadf0d49807` |
| `telemetry.csv` | `f326a6ba2708b8c97f0319be7c2ca6bd1ac358278cd9bbe2ebc3ae5b9dc51770` |
| `summary.json` | `e4cc909ce7008bb6c422ce16d60ba2d929e5e8915f5b5cba156a0e319770e370` |

### DHT11 OUT data-loss and recovery — PASS (observed missing/recovery states)

Procedure requested: leave both ESP32s, DHT power, and CAN wiring powered and
connected; open only the DHT module's middle OUT data connection, wait for
missing temperature, then reconnect the same data wire without pressing reset.
The operator supplied two terminal screenshots showing:

- **Missing data:** condition records at dashboard times 2914841–2916641 ms
  showed `DEGRADED`, warning 64, faults 0, and `nan` temperature. Heartbeat
  at dashboard time 2915713 ms carried flags 3 and sensor uptime 3042 seconds.
  Motion/power records continued alongside the missing-temperature records.
- **Recovered:** later condition records at dashboard times 2940241–2942041 ms
  showed `RUN`, warning 0, faults 0, and measured temperature 24.1–24.8 C.
  Heartbeats carried flags 3 and sensor uptime 3067–3068 seconds, with continued
  motion/power traffic and advancing dashboard timestamps.

The later recovery screenshot was attached first, but its timestamps establish
the chronology. Both dashboard time and sensor uptime continued increasing,
consistent with recovery without a board reset. Missing readings were explicitly
unavailable rather than replaced by a simulated temperature. No CAN outage is
shown in the visible record windows; this is not a timed full-run proof of zero
frame loss or zero resets between all observations.

**Missing-state signaling and return to valid hardware temperature: PASS.**
The intermediate warning-128 STALE stage, exact transition delays, and the
two-good-read recovery count are not directly captured in these screenshots;
do not claim those individual bench observations as verified. Temperature
reference accuracy and raw CAN sequence-gap verification remain pending.

## Firmware 0.3.0 — Deployment and sequence/session bench evidence

Operator screenshots confirmed successful builds of `esp32dev_sensor_sim`,
`esp32dev_sensor_dht11`, and `esp32dev_dashboard`, followed by successful,
hash-verified uploads of dashboard and DHT11 sensor. The dashboard startup
banner reported `firmware=0.3.0`, schema 0.3, and session freshness 250 ms.
It received session `1375652050`, mixed-source annotations, and `HW_VALID`
temperature at 24.6 C. No additional hardware channels were enabled.

Both captures below used dashboard COM12 at 115200 baud, with
`--require-hardware-temperature --require-sequences`. Their version attribution
comes from the preceding build/upload/startup observations: neither capture
contains a version banner. Their `local_source_commit` value
`04059025b9d96b98805ee44b7d3d29c7181dc079` is Git ancestry, not a unique identifier
for the uncommitted 0.3.0 source or a device firmware readback. No release commit
is claimed. Original capture artifacts are retained unchanged and ignored by Git.

### BT-02 / BT-03 / BT-04 — Healthy mixed-source soak — PASS

| Field | Result |
|---|---|
| Start UTC | 2026-10-20T04:05:11.945848+00:00 |
| Local start | 2026-10-19 23:05:11 America/Chicago |
| Capture | `can-soak-20261020T040511Z-8b0668` |
| Duration | 600.031 seconds, complete |
| Overall / sequence result | PASS / PASS |
| Boot session | `1375652050`, unchanged throughout |

| Stream | Complete records | Mean interval | Maximum interval | Normal sequence wraps |
|---|---:|---:|---:|---:|
| Heartbeat | 600 | 999.998 ms | 1000 ms | 2 |
| Condition | 2,999 | 200.000 ms | 201 ms | 12 |
| Motion/power | 5,999 | 100.000 ms | 101 ms | 24 |

All 9,598 parsed records have sequence/session metadata. Every stream reported
zero inferred missing publication sequences, duplicates, late/out-of-order
records, ambiguous rebases, unattributed records, missing metadata, and session
changes. Each established one initial baseline; normal counter wraps were not
treated as anomalies. No discontinuities were observed between retained
records; this does not infer traffic before/after capture or prove zero raw CAN
frame loss. Counts are consistent with finite serial-capture boundaries.

All 2,999 condition records were `RUN`, warnings 0, faults 0. DHT11 temperature
ranged 24.0–24.9 C (mean approximately 24.481 C); all heartbeat flags were 3.
There were zero stale/missing temperatures, timeout events, observed restarts,
startup banners, warning/error lines, malformed records, or decode errors.
Independent CSV grouping confirmed counts, condition states, temperature range,
and session coverage. These are decoded-serial bench screening results, not
electrical timing certification or an independent temperature calibration.

Local artifacts under `data/can-soak-20261020T040511Z-8b0668/`:

| File | SHA-256 |
|---|---|
| `raw.log` | `c08a677bd9347bcdae532b05378d03164668ba28e01d8cb9fd23e359904446b2` |
| `telemetry.csv` | `9e835f54a1eca5a71959d7ac415c2889a61777fca43e71cc06ae64dfedb9d8f6` |
| `summary.json` | `4ed1c86555379eb422599036281d2edba4ef2e0a32068cbc90cfe1917ce4975d` |

### BT-07 — Deliberate sensor EN reset — Recovery/session behavior verified

| Field | Result |
|---|---|
| Start UTC | 2026-10-20T04:23:03.177749+00:00 |
| Local start | 2026-10-19 23:23:03 America/Chicago |
| Capture | `can-soak-20261020T042303Z-f9d904` |
| Duration | 60.062 seconds, complete |
| Procedure | Operator briefly pressed sensor EN once; dashboard remained powered |
| Automated result | REVIEW, retained as expected for deliberate interruption |
| Records | Heartbeat 59; condition 296; motion/power 592 |

The log contains exactly one observed sensor restart and one session change per
stream, from `1375652050` to `1593007814`. Dashboard time continued increasing;
no dashboard restart or startup banner was captured. At dashboard time
1163617 ms, the session notice was `CHANGED`. New condition/motion sequences
started at 0 at 1163618 ms; heartbeat at 1164090 ms had sensor uptime 1 second.
Each stream rebaselined rather than treating reset counters as lost messages.
All inferred-missing, duplicate, late, ambiguous, unattributed, and
missing-metadata counters were zero. This does not mean there was no outage:
maximum inter-record gaps were 1518 ms heartbeat and 905 ms condition/motion.

Twenty-one condition records were `DEGRADED/64/0/nan` during qualification.
`HW_VALID` and `RUN/0/0/24.0` returned at dashboard time 1167818 ms, 4.200 seconds
after the first post-reset condition record (not a measured delay from the
physical button press). The other 275 condition records were RUN with no
warnings/faults. There were no STALE records, timeout transitions, warning/error
lines, malformed records, or decode errors. This short reset did not assert
availability timeouts; timeout assertion latency is not established by this run.

The intentional restart, changed sessions, temporary missing temperature, and
long intervals correctly caused REVIEW. The capture must not be relabeled a
healthy soak PASS. Its observed reset/session handling and temperature recovery
meet the recorded BT-07 variant, not an exhaustive reset/fault campaign.

Local artifacts under `data/can-soak-20261020T042303Z-f9d904/`:

| File | SHA-256 |
|---|---|
| `raw.log` | `1368cb95772c1707f31b8dbf077a04d156bc55443a1ef56e5f7ca820710e5fb9` |
| `telemetry.csv` | `4856a019d5bd3e94a39b290f7700f3853a0c8a1f6d21328e0c575a814c8a01a0` |
| `summary.json` | `9d30d113204ca3a6c902a67f283bbf11730081f9bfe0e6213c00b5a6c7565cc9` |

### BT-05 / BT-06 variant — CANH open at startup and power-off restoration

Evidence is operator chat photographs/screenshots, not a saved timed capture.
Wall-clock timestamps were not recorded for this manual sequence. Firmware
0.3.0 and dashboard role were visible in both relevant startup screenshots.

1. Both USB supplies were disconnected. The operator reported unplugging only
   dashboard CANH, leaving CANL, common ground, power, TX/RX, and DHT wiring
   untouched. Photo `IMG_7011.jpeg` shows the area, but the hand obscures the
   exact removed contact, so CANH identification relies on the operator report.
2. After power-up, COM12 was absent. With sensor USB removed, the available
   ports were COM1 and COM8. Monitoring COM8 confirmed role DASHBOARD;
   this supersedes the old port assumption for that connection only.
3. With the reported CANH opening and both boards powered, a dashboard reset
   produced `#event,data_timeout,1` and `#event,heartbeat_timeout,1` and no
   telemetry in the supplied screenshot (`codex-clipboard-37c46433-d56f-4b97-a06a-72fe1b4bde70.png`).
4. Both boards were powered off before the operator restored CANH. Dashboard
   was then powered/monitored first, reset, and showed both timeout assertions
   while sensor USB was still disconnected. Connecting sensor USB returned
   traffic without another requested dashboard reset.
5. The recovery screenshot (`codex-clipboard-0e23fbf0-b411-4d33-a056-74f6d94c17f1.png`)
   shows one startup followed by both assertions, initial session `709527784`,
   condition at 16516 ms, motion/power at 16527 ms, `data_timeout,0`, heartbeat
   at 16988 ms with sensor uptime 1, and `heartbeat_timeout,0`.
6. The later live screenshot (`codex-clipboard-34084788-464e-4fb3-8ec0-63df1e521973.png`)
   shows continuing traffic at dashboard times 40615–42315 ms, condition
   `RUN/0/0` at 24.7 C, and the same session. The visible motion sequence also
   wraps normally from 255 to 0; this screenshot is not a timed full-run audit.

**Observed variant verified:** missing-traffic alerts with CANH open at startup,
and traffic/alert recovery after power-off restoration and sensor startup.
BT-05/BT-06 as a continuous, live CAN-wire interruption/reconnection test remain
unverified. Exact timeout assertion/clearing latencies were not measured;
startup with sensor off is a different fault condition from a live wire break.
No reverse-polarity test was performed. Do not move bus/power wiring while
powered based on this record.

The dashboard again printed the core-dump checksum warning before continuing
normal application startup. Its cause remains unresolved; it was not erased,
repaired, or established as a new runtime crash by these observations.

## INA219 extension — communication and branch-conduction observations

This is a separate sensor-only diagnostic stage, not new CAN hardware-power
acceptance. Project 1 files and the tested 0.3.0 application targets are unchanged.

- Photos `IMG_7012.jpeg` / `IMG_7013.jpeg` show an INA219-labeled module, installed
  header, and R100 shunt. Operator-reported logic mapping is VCC to sensor3V3,
  GND to sensorGND, SCL22, SDA21. `IMG_7014.jpeg` does not independently expose
  every pin label or hidden breadboard contact.
- `esp32dev_ina219_check` build completed successfully (10.190 seconds) in the
  operator terminal. Its sensor-only upload completed successfully (19.101
  seconds) with written-data hash verification.
- Screenshot `codex-clipboard-1660bbb7-2cbe-4ac5-acdf-397a3d4c450b.png` shows
  repeated `REGISTER_READ` at address 0x40 with configuration 0x399F on COM8.
  **Basic I2C communication observed.** This is not a unique identity-register
  check or proof of voltage/current accuracy. CAN and DHT are disabled by this
  temporary firmware; the dashboard was kept unplugged.
- Operator measured a loose resistor as 0.331 kilohm (331 ohms). Subsequent photos
  `IMG_7015.jpeg` / `IMG_7016.jpeg` were supplemented by explicit wire tracing:
  white VIN+ to sensor3V3, another white VIN- to resistor, resistor to LED anode,
  and green LED cathode to sensorGND. Hidden contacts were not independently
  verified by photography. After sensor-only power-up, the operator reported
  that the red LED lights. **Branch conduction observed**, not shunt-routing
  correctness or a reference-current comparison.
- Measurement diagnostic (`esp32dev_ina219_measure`) results follow below.
  Hardware-power CAN integration and a fresh integrated soak are not yet performed.

See [INA219 stages](../docs/INA219.md) for wiring, scaling, explicit unavailable
states, and the distinction between a hardware sample and accuracy acceptance.

### INA219 standalone measurement, reference, and fault observations

The operator reported successful measurement-target build and upload. Unlike
the earlier communication-target upload, no terminal screenshot or exact
duration was supplied for these two success reports. Runtime screenshots
confirm execution of the measurement target on sensor COM8 at 115200 baud.
No release commit/device-image readback or new integrated CAN test is claimed.

| Stage | Result and evidence |
|---|---|
| Loaded | `codex-clipboard-e407f4b8-ead6-47fe-a4ad-207eb9c98da1.png` shows HW_SAMPLE, 3.328–3.332 V, 4.1–4.3 mA, and 0.410–0.430 mV shunt |
| Voltage reference | Operator reported 3.31 V from INA219 VIN- relative to sensor GND, using DC volts; difference from displayed hardware range approximately 0.02 V |
| Current reference | Operator reported 1.41 V across the separately measured 331-ohm resistor; `1000 * 1.41 / 331 = 4.2598 mA`, consistent with the displayed loaded range |
| No load | `codex-clipboard-5e92c1ec-d2e4-469c-ad22-43c84511ae25.png`: HW_SAMPLE, 3.332–3.336 V, -0.1 to 0.0 mA, -0.010 to 0.000 mV shunt |
| Load restored | `codex-clipboard-e538677c-b207-4ad4-9b6a-b091964d1e7d.png`: HW_SAMPLE, 3.328–3.332 V, 4.2 mA, 0.420 mV shunt |
| SDA open | `codex-clipboard-38a725e5-4fe3-44bf-867c-3d3890f65c76.png`: repeated WRITE_FAILED,nan,nan,nan after the new monitor header; the prior valid sample at the top predates this session |
| SDA restored | Operator reported "returned to expected" after power-off SDA restoration and sensor-only power-up; no additional recovery screenshot provided |

For no-load, only the VIN--to-resistor connection was opened; VIN+ remained
on sensor3V3 with the logic connections intact. For communication loss, only
INA219 SDA was disconnected, with power/SCL/LED branch unchanged. Each wire
change and restoration was requested with both USB supplies disconnected.
These demonstrate separate numeric no-load and explicit unavailable behaviors;
the recovery observations include power cycling, not continuous powered-bus
reconnection or measured retry latency. This is not a full missing-power-device
test: SDA was opened, not every INA219 connection removed.

Reference values are manually reported and not simultaneous time-stamped
captures; meter calibration/uncertainty and controlled conditions were not
established. The comparison supports approximate bench agreement, not a
traceable calibration, certified accuracy, or an exact percentage-error claim.
The lit LED plus loaded/no-load change support a measured branch response, but
hidden breadboard paths were not independently continuity-audited.

The first loaded screenshots also contain garbled/repeated text, particularly
`codex-clipboard-968f24da-96d3-4a6c-a77e-a38582b354f1.png`. Later supplied windows
are readable, but the underlying cause remains unresolved. No raw timed serial
capture was saved for these observations, so no clean long-run stability,
zero reset/corruption, or exact sample-period PASS is claimed for that stage.
At that point CAN/DHT were disabled on the sensor's diagnostic image. Subsequent
combined firmware preparation and its new healthy soak are recorded below;
they do not retroactively diagnose the earlier garbled-text cause.

## 0.4.0 preparation gate — software evidence before deployment

The operator authorized preparation of the updated sensor/dashboard following
the standalone results above. `esp32dev_sensor_dht11_ina219` now enables DHT11
temperature and checked INA219 LED-branch voltage/current, while condition
percentage and motion remain simulated. Main application version is 0.4.0;
wire protocol version 1, IDs/DLC/offsets, and sequence/session columns are retained.
New source flag 4, warning 32, paired unavailable-power sentinels, two-acquisition
qualification, and 300 ms snapshot expiry are documented in
[integration contract](../docs/INA219_CAN.md).

All 66 Python host tests pass, including the original 45 and 21 new power/source/
capture cases. Fake serial tests exercise the actual capture/parser/analysis code,
not MCU hardware. C++ state/codec assertions were prepared for application builds;
MCU compilation and bench execution were still pending at this preparation gate.
The assistant's compiler preflight again failed before compiling source with
Windows Illegal System DLL Relocation. No system/compiler repair, release
commit, or firmware flash was performed.

The actual PlatformIO config parser also verified the combined target's inherited
role/DHT/INA flags and pinned DHT libraries. Read-only replay of the four saved
normalized CSVs retained their original record counts (9,600, 9,599, 9,598, 947)
and three PASS / one intentional-reset REVIEW statuses. Replay excludes raw
comment/event lines and is a host-regression check, not repeated physical tests.

At this gate, operator builds/uploads, simultaneous hardware reception, a fresh
600-second combined capture, and integrated fault tests were still pending.
Subsequent deployment and the new healthy result follow below. Previous 0.3.0
and standalone observations are not relabeled as new integration tests.

## 0.4.0 deployment and combined hardware CAN soak

Screenshot `codex-clipboard-38553da8-89c2-4f0d-a38d-f547f9982c43.png` shows all
four application targets built successfully in the operator terminal:
sensor_sim 10.587 s, sensor_dht11 11.193 s, sensor_dht11_ina219 15.563 s, and
dashboard 10.709 s (overall reported 48.053 s). This evaluates the compiled C++
state/encoding assertions, including the INA219 conversion/profile assertions
included by the combined sensor. The assistant-side DLL launch failure did not
prevent these operator-side builds. The operator then reported successful
dashboard-first and combined-sensor uploads, one physical USB connection at a
time. No upload screenshot, duration, or independent device-image readback was
provided for those success reports.

Live screenshot `codex-clipboard-9a4fb783-3e71-46f8-858e-7c7440aebef2.png` shows
heartbeat flags 7, session 1054558302, numeric temperature 24.1–24.4 C,
approximately 3.328–3.332 V / 4.2 mA, RUN, and warnings/faults 0. The screenshot
does not contain a version banner or source-comment lines. Firmware attribution
is based on the preceding build/upload sequence and matching combined-mode
telemetry, not a binary/version attestation.

### BT-02 / BT-03 / BT-04 — DHT11 + INA219 healthy soak — PASS

| Field | Result |
|---|---|
| Start UTC | 2026-10-20T06:18:28.943602+00:00 |
| Local start | 2026-10-20 01:18:28 America/Chicago |
| Capture | `can-soak-20261020T061828Z-1803e0` |
| Duration | 600.000 seconds, complete |
| Dashboard interface | COM8, 115200 baud; connection-specific assignment |
| Requirements enabled | Hardware temperature, hardware power, and sequences |
| Overall / sequence result | PASS / PASS, no review reasons |
| Session | 1054558302, unchanged for all 9,598 retained records |

| Stream | Complete records | Mean interval | Min–max interval | Normal sequence wraps |
|---|---:|---:|---:|---:|
| Heartbeat | 600 | 999.998 ms | 999–1000 ms | 2 |
| Condition | 2,999 | 200.000 ms | 199–201 ms | 12 |
| Motion/power | 5,999 | 100.000 ms | 99–101 ms | 23 |

All 600 heartbeats had flags 7. All 2,999 condition records were RUN with
warnings/faults 0. Every retained record had sequence/session metadata; each
stream reported zero inferred missing sequences, duplicates, late/out-of-order
records, ambiguity, unattributed records, missing metadata, and session changes.
Initial baselines and normal wraps are not failures.

| Hardware channel | Captured range | Mean (approximate) |
|---|---:|---:|
| DHT11 temperature | 24.0–24.9 C | 24.481 C |
| INA219 bus voltage (VIN- to GND) | 3.324–3.332 V | 3.33136 V |
| INA219 LED-branch current | 4.1–4.3 mA | 4.20015 mA |

The summary reports zero missing/stale temperature, unavailable power samples,
power-missing condition warnings, timeout events, observed sensor/dashboard
restarts, startup banners, warning/error lines, malformed/unexpected lines, and
UTF-8 decode errors. No source-comment/version banner was received inside the
timed capture (`firmware_versions_observed` is empty).

Independent CSV grouping confirmed counts, state bits, ranges/means, source
flags, and session coverage. Independent raw-log parsing confirmed all 9,598
complete records are valid UTF-8 and recognized telemetry; stream intervals
match the ranges above and every retained adjacent sequence advances modulo
256 within the same session. The raw file ends with an incomplete `c`, excluded
at the fixed-duration boundary. Counts are consistent with that capture window,
not evidence of an in-window sequence gap. No garbled/repeated text was observed
in these complete records; the earlier standalone serial-text cause remains
unresolved, not proven repaired.

These are decoded-serial timing/continuity and simultaneous hardware-channel
availability results, not proof of zero raw CAN loss, exact ADC sampling cadence,
traceable sensor accuracy, process protection, or integrated fault handling.
Only temperature, bus voltage, and LED-branch current are hardware-based;
condition percentage and motion remain SIM. Current is not total ESP32/USB
current. Separate INA219 startup-loss/restoration and combined sensor-reset
evidence follow below. Startup checksum diagnosis and reverse-polarity/live
wire-break tests retain their earlier unverified boundaries.

`local_source_commit` remains ancestry `04059025b9d96b98805ee44b7d3d29c7181dc079`,
not a unique committed 0.4.0 release or a device readback. Capture artifacts were
not rewritten and remain ignored by Git under
`data/can-soak-20261020T061828Z-1803e0/`:

| File | SHA-256 |
|---|---|
| `raw.log` | `b4d9c9dea4ad5c3c9a75b7dfea87d352e28c4d8b0d5c95a6e5fdaeafe8d37d12` |
| `telemetry.csv` | `7e4e1eb50f002c49f8c355a0fc13c537432e2ead62a0230f793b25d5006b9f11` |
| `summary.json` | `590193402fa33051193e704799f494877b69e9ea6a49c6f804cea83eaf3037d0` |

### 0.4.0 integrated INA219 SDA-open startup — expected REVIEW

With both USB supplies off, the operator reported opening only SDA at the
INA219 end, leaving SCL, logic power/ground, LED branch, DHT11, and CAN unchanged.
Dashboard was powered/monitored first, then sensor. Live screenshot
`codex-clipboard-355052d7-cc06-4408-a8c4-1d6d04409bce.png` shows heartbeat flags
7, continuing traffic, DEGRADED/warning 32/faults 0, numeric DHT temperature,
and paired `nan,nan` power. A separate steady-fault capture followed; it does
not include the wire-opening instant or startup qualification transitions.

Capture `can-soak-20261020T063943Z-a24273` started at
2026-10-20T06:39:43.616549+00:00 (01:39:43 America/Chicago), using dashboard COM8
at 115200 baud, with all three hardware-temperature/power/sequence requirements.
The complete capture lasted 30.079 seconds and retained 480 records:

| Stream | Records | Mean / maximum interval |
|---|---:|---:|
| Heartbeat | 30 | 1000 / 1000 ms |
| Condition | 150 | 200 / 200 ms |
| Motion/power | 300 | 100 / 100 ms |

Overall REVIEW has exactly one reason: power readings unavailable. All 300
motion/power records have paired NaNs; all 150 conditions are DEGRADED with
warning 32 and faults 0. DHT temperature remains numeric, with no missing/stale
records. All heartbeats have flags 7. Session 1558336123 is unchanged throughout.
Sequence screening passes with zero inferred missing, duplicate, late,
ambiguous, unattributed, missing-metadata, or session-change records; motion
sequence wraps once normally. No timeouts, observed restarts, startup banners,
warning/error lines, malformed/unexpected lines, or decode errors were recorded.
CSV grouping independently confirmed counts, states, NaN pairs, flags, and
session coverage. Raw log contains 480 telemetry lines and a final incomplete
`c`, excluded at the capture boundary. Result screenshot:
`codex-clipboard-64664e86-1d3b-4c10-b89b-60b388c0341e.png`.

This supports continued decoded CAN/DHT operation with explicit unavailable
power when SDA is absent at sensor startup. It is not live communication-loss
detection/retry latency, complete module removal, raw CAN loss freedom, or
integrated restoration evidence. Restoration was pending at this stage and is
recorded next; combined reset is recorded afterward.
No version banner was captured; source attribution follows the prior deployment
workflow and matching telemetry. Capture artifacts remain unmodified/ignored:

| File | SHA-256 |
|---|---|
| `raw.log` | `5e78db1dc37a03b486d052a70e5e8508b61f77e75b364f394fd49f59669ec745` |
| `telemetry.csv` | `cd21ea64db8d1aa8779d19fd4c1c4018823d71e72fc18c7d91aaaf2ceaf37a19` |
| `summary.json` | `92a61d395ddf9b34bcef57b4dc2d03669a0978587fa75f327b3ffd70ff73f168` |

### 0.4.0 integrated SDA restoration after power-off — PASS

After the SDA-open capture, the operator unplugged both USB supplies and
reported restoring only INA219 SDA to its original sensor GPIO21 connection.
Dashboard USB/monitor were started first, followed by sensor USB. Screenshot
`codex-clipboard-89d6a626-01b1-4cf1-ab81-896df3bba06f.png` shows RUN, warning/fault
0, numeric temperature, and 3.328–3.332 V / 4.1–4.3 mA power readings. A separate
restored-state capture followed without further wiring changes.

| Field | Result |
|---|---|
| Capture | `can-soak-20261020T064525Z-2a7ab3` |
| Start UTC / local | 2026-10-20T06:45:25.737823+00:00 / 01:45:25 America/Chicago |
| Duration / interface | 30.094 seconds complete / dashboard COM8, 115200 baud |
| Requirements | Hardware temperature, hardware power, sequences |
| Overall / sequence | PASS / PASS; no review reasons |
| Counts | 30 heartbeat, 150 condition, 300 motion/power |
| Mean / maximum intervals | 1000/1000, 200/200, 100/100 ms respectively |
| Session / source flags | 3499313220 throughout all 480 records / all 30 heartbeats flags 7 |
| Hardware ranges | Temperature 24.0–24.9 C; voltage 3.328–3.332 V; LED-branch current 4.1–4.3 mA |

All 150 conditions are RUN with warning/fault 0. There are no missing/stale
temperatures, unavailable-power records/warnings, observed restarts/timeouts,
startup banners, malformed/unexpected lines, warning/error lines, or decode
errors. Every stream has zero inferred missing, duplicate, late, ambiguous,
unattributed, missing-metadata, or session-change records; motion wraps once
normally. CSV grouping independently confirmed counts, flags, session, states,
and hardware ranges. Independent raw grouping confirmed 480 telemetry records
and unit modulo-256 adjacent sequence steps; the final incomplete `c` is excluded
at the duration boundary. Result screenshot:
`codex-clipboard-2af532b2-4542-49e6-a4b2-0d941ed89b52.png`.

Together with the preceding SDA-open run, this records explicit unavailable
power with continued DHT/CAN traffic, followed by healthy operation after
power-off SDA restoration and reboot. The separate captures have different boot
sessions as expected; they do not measure the restoration instant, live I2C
recovery, acquisition qualification latency, or uninterrupted fault-transition
behavior. The fault capture remains REVIEW, not relabeled by later recovery.
Combined deliberate sensor reset is recorded next. No startup/version banner
was captured; deployment attribution and prior accuracy/raw-CAN limits remain.
Artifacts were not rewritten and remain ignored under the capture directory:

| File | SHA-256 |
|---|---|
| `raw.log` | `92c5ae969d2052a0b7e0bccde6d34f556b71aec2b700485fa2794ea81f6f5120` |
| `telemetry.csv` | `de75122c286dc45f35cc4f7cec5ee26ac11d6fe548230997efb4d3671ca854e5` |
| `summary.json` | `b1bc62a56ae86c3739bebc42be3494a118487f773a7ee703059816ca36721ab3` |

### BT-07 — 0.4.0 combined sensor EN reset — recovery observed, expected REVIEW

Both USB supplies and all wiring remained connected. The operator was asked
to press/release EN on only the sensor during a 60-second capture; the supplied
result screenshot is `codex-clipboard-35ac2715-e31f-4751-93ec-72e301a9ca91.png`.
The exact physical button time was not independently recorded. The session
transition occurs around 40 seconds into capture, not at an assumed 15-second
instruction offset. This test is a reset, not a powered wire-disconnection test.

| Field | Result |
|---|---|
| Capture | `can-soak-20261020T064845Z-f7f95e` |
| Start UTC / local | 2026-10-20T06:48:45.408761+00:00 / 01:48:45 America/Chicago |
| Duration / interface | 60.063 seconds complete / dashboard COM8, 115200 baud |
| Requirements | Hardware temperature, hardware power, sequences |
| Overall / sequence | REVIEW / REVIEW, retained intentionally |
| Counts | 59 heartbeat, 296 condition, 591 motion/power; 946 total |
| Mean intervals | 1015.931 / 203.132 / 101.566 ms respectively |
| Maximum gaps | 1924 / 1125 / 1025 ms respectively |
| Observed restarts | One sensor, zero dashboard |
| Sessions | 3499313220 (624 records) -> 1759568240 (322 records) |

Every stream records exactly one session change/rebaseline, with zero inferred
missing publications within sessions, duplicates, late/out-of-order records,
ambiguity, unattributed records, or missing metadata. All 59 heartbeats retain
flags 7. Dashboard timestamps continue across the change with no startup banner
or observed restart. REVIEW correctly preserves the reset/session transition,
communication timeout, startup-unavailable channels, and enlarged intervals;
this is not a healthy-soak PASS.

The data-timeout assertion at host elapsed 39.828 seconds clears at 39.969
seconds, a 141 ms interval between captured event timestamps. No heartbeat
timeout event is recorded. The first new-session condition and motion/power
arrive at dashboard time 335113 ms: condition is DEGRADED, warning 96 (both
channels missing), faults 0, temperature NaN; power is paired NaNs. The next
motion/power at 335212 ms is still missing. At 335312 ms power is numeric
3.332 V / 4.2 mA and `#power_quality,HW_VALID` is reported, while condition
warning is 64 for temperature alone. This is 199 ms after the first new-session
record, not a button-to-recovery or ADC qualification-latency measurement.

There are 21 missing-temperature conditions (one warning 96 and twenty warning
64), zero STALE conditions, two unavailable-power records, and one power-warning
condition. At 339312 ms temperature becomes HW_VALID, with RUN/warnings 0 and
24.1 C: 4199 ms after the first new-session condition, not timed from EN.
The remaining 80 new-session condition records are RUN/warnings/faults 0;
numeric power continues through the end. The final records show 24.2 C and
3.332 V / 4.2 mA. DHT qualification, INA qualification, session rebasing, and
dashboard timeout clearing are therefore observed in the combined image.

CSV grouping independently confirmed sessions/counts and condition states;
all adjacent within-session sequence steps advance modulo 256. Raw transitions
were inspected, including the HW_VALID comments and final recovery records.
Summary reports zero warning/error, malformed, unexpected, or decode-error
lines. Four other-lines are recognized quality comments, not corrupted data;
the raw log ends with an incomplete `m`, excluded at the capture boundary.
No firmware version banner was captured; prior deployment attribution applies.
These are decoded-serial observations from one deliberate reset, not exhaustive
reset coverage, exact physical CAN timing, calibrated accuracy, or live I2C
recovery proof. No firmware/code changes or capture rewrites were made.

Artifacts remain ignored under the capture directory:

| File | SHA-256 |
|---|---|
| `raw.log` | `e8cf4eba39d9ed5639de96f05601f86290f4178cc52b2a6ff149c073f11619c1` |
| `telemetry.csv` | `c008912458c95e576260a450a98dbccb3ca1d5990d15ed947eb38fe321039ed8` |
| `summary.json` | `b6c468c1ba2fdba5eebb9f597fbfc50fcaba8f1ac57daeb6636f7d9bee7fa455` |

### 0.4.0 combined DHT11 OUT-open startup — expected REVIEW

With both USB supplies off, the operator reported opening only the DHT11 OUT
connection at the module end (sensor GPIO4 path), retaining DHT power/ground,
INA219 logic/load, and CAN wiring. Dashboard USB/monitor were started first,
then sensor USB. Live screenshot
`codex-clipboard-f097daa1-63d0-47ce-ae58-0a6891a8a607.png` shows DEGRADED/warning
64/faults 0, temperature NaN, flags 7, and continuing numeric power and CAN
traffic. The timed capture follows this established steady fault, not the
physical opening instant or startup transition.

| Field | Result |
|---|---|
| Capture | `can-soak-20261020T223544Z-99fa1b` |
| Start UTC / local | 2026-10-20T22:35:44.071751+00:00 / 17:35:44 America/Chicago |
| Duration / interface | 30.078 seconds complete / dashboard COM8, 115200 baud |
| Requirements | Hardware temperature, hardware power, sequences |
| Overall / sequence | REVIEW solely for missing/stale temperature / PASS |
| Counts | 30 heartbeat, 150 condition, 300 motion/power; 480 total |
| Mean intervals | 999.966 / 199.993 / 99.997 ms respectively |
| Maximum intervals | 1000 / 201 / 101 ms respectively |
| Session / flags | 444307088 throughout / all 30 heartbeats flags 7 |
| Power ranges | 3.328–3.332 V; 4.1–4.3 mA in the resistor-limited LED branch |

All 150 conditions are DEGRADED with warning 64, faults 0, and NaN temperature;
the summary confirms 150 MISSING and zero STALE temperature records. All 300
power pairs are numeric, with zero missing-power records or power-warning
conditions. No timeout events, observed resets, startup banners, warning/error
lines, malformed/unexpected lines, or decode errors were captured. Every stream
has zero inferred missing publications, duplicates, late/out-of-order records,
ambiguity, unattributed records, missing metadata, and session changes.
Condition and motion/power sequences each wrap once normally.

Independent CSV grouping confirmed counts, states, NaNs, source flags, session,
power ranges, and unit modulo-256 within-session adjacent sequence steps.
Raw grouping confirmed 480 telemetry lines with only a trailing incomplete `m`
excluded at the capture boundary. Result screenshot:
`codex-clipboard-f115921c-cf1c-419a-aa2b-94a569180d28.png`.

This demonstrates continued INA219/DHT-independent CAN publication with
explicit missing temperature when DHT OUT is absent at startup. It does not
measure a live loss transition, intermediate STALE behavior, physical detection
latency, raw CAN error freedom, or calibrated power accuracy. Combined OUT
restoration is recorded next. No firmware/version banner was captured; prior
deployment attribution applies. No firmware or capture files were changed.
Artifacts remain ignored under the capture directory:

| File | SHA-256 |
|---|---|
| `raw.log` | `a6927149743b3a9725c5b5f2490b1910b06381d7611b520b2091ea1b0de1216c` |
| `telemetry.csv` | `a96df037c4baa662f487046a4c44ff285e81f88a921d3969122121ad09159a1c` |
| `summary.json` | `38e7423294664e382b834b1121ef2a4b58969b6219f5fa0b7f32d34356a0693e` |

### 0.4.0 combined DHT11 OUT restoration after power-off — PASS

Following the OUT-open capture, the operator unplugged both USB supplies and
reported reconnecting only DHT11 OUT to its original GPIO4 path. Dashboard
USB/monitor were started first, then sensor USB, with other wiring unchanged.
Live screenshot `codex-clipboard-5d257746-a9da-4e84-a88f-827bf7ecf122.png` shows
RUN/warnings/faults 0, temperature 24.6–24.7 C, numeric INA219 readings, and
heartbeat flags 7. A separate restored-state capture followed.

| Field | Result |
|---|---|
| Capture | `can-soak-20261020T224732Z-2168b2` |
| Start UTC / local | 2026-10-20T22:47:32.102193+00:00 / 17:47:32 America/Chicago |
| Duration / interface | 30.063 seconds complete / dashboard COM8, 115200 baud |
| Requirements | Hardware temperature, hardware power, sequences |
| Overall / sequence | PASS / PASS; no review reasons |
| Counts | 30 heartbeat, 150 condition, 300 motion/power; 480 total |
| Mean / maximum intervals | 1000/1000, 200/200, 100/100 ms respectively |
| Session / flags | 1563320682 throughout / all 30 heartbeats flags 7 |
| Hardware ranges | DHT11 24.0–24.7 C; INA219 3.328–3.332 V / 4.1–4.3 mA LED-branch current |

All 150 conditions are RUN with warning/fault 0. No missing/stale temperature,
unavailable-power samples/warnings, timeouts, observed restarts, startup
banners, warning/error lines, malformed/unexpected lines, or decode errors were
recorded. All streams have zero inferred missing publications, duplicates,
late/out-of-order records, ambiguity, unattributed records, missing metadata,
and session changes; condition and motion/power each wrap once normally.
Independent CSV grouping confirmed counts, states, source flags, session,
hardware ranges, and unit modulo-256 within-session adjacent sequence steps.
Raw grouping confirmed 480 telemetry lines and only a trailing incomplete `c`,
excluded at the fixed-duration boundary. Result screenshot:
`codex-clipboard-0f937e21-f8da-41aa-8d7e-8cfc60db85c7.png`.

Together with the preceding OUT-open capture, this records missing temperature
with continued INA219/CAN operation, followed by restored healthy operation
after power-off reconnection and reboot. The two separate boot sessions/windows
do not measure live reconnection, STALE transitions, or physical recovery latency.
The fault capture remains REVIEW. Accuracy, raw-CAN, startup-warning, and
earlier garbled-text limitations remain unchanged. No version banner was
captured; prior deployment attribution applies. No firmware/capture files were
changed; artifacts remain ignored under the capture directory:

| File | SHA-256 |
|---|---|
| `raw.log` | `0359f24fc8350892bcc357f96838a67cb3273fe46259de6d0b0da66ce9bb68ad` |
| `telemetry.csv` | `f1acd0541636a12f18a6d9a4569020b4aaa7da4782948ac74591d1fccf0a3339` |
| `summary.json` | `dfaf860d1193ce25d92431448a4489e68a0114905a639e557b41268e037907a7` |

## 2026-11-01 — MPU communication diagnostic preparation (not bench PASS)

The operator reported Project 1's hardware was already dismantled; its HW-123
module is being reused without changes to Project 1's saved sources. Photos
`20261101_130941549_iOS.heic` / `20261101_130947406_iOS.heic` show installed
headers and labeled VCC/GND/SCL/SDA/XDA/XCL/AD0/INT. `IMG_7150.jpeg` shows sensor
power distribution moved to separate breadboard rails. Overlapping wires hide
some contacts; this is not independent verification of every endpoint.
Operator reported VCC-to-sensor3V3 and GND-to-sensorGND continuity beeps, then
OL across rails on an unspecified resistance range, shared SDA21/SCL22 connected,
and AD0 grounded while both USB supplies were unplugged. OL is over-range,
not a calibrated insulation test. XDA/XCL/INT remain unused by instruction.

Prepared `esp32dev_mpu6050_check`, compiling only `src/Mpu6050Check.cpp`:
read WHO_AM_I register 0x75 at 0x68, and independently INA config 0x00 at 0x40,
with SDA21/SCL22, 100 kHz, 25 ms transaction timeout, two-second repeat delay.
It writes register pointers only, not device settings; CAN/DHT/motion sampling
are disabled. No existing diagnostic or application source was changed.
Normal application motion remains simulated. A matching identity byte would
support register compatibility, not genuine-chip proof or motion accuracy.

All existing 66 host tests passed again; actual PlatformIO config parsing
confirmed the single-source diagnostic filter/flag and unchanged application
defaults/combined flags. These tests do not execute the new MCU Wire code.
MCU build, upload, rail-voltage check, and repeated physical reads are pending.
The assistant did not retry the known Windows compiler DLL failure or flash
either board. The next requested operator step is build only with USB unplugged.
See [MPU stage](../docs/MPU6050.md) for wiring and temporary-image boundaries.

### MPU/INA219 shared-bus communication observed (sensor diagnostic only)

Operator build screenshot
`codex-clipboard-35629bf2-0e47-4171-90f6-323ec12e8625.png` shows
`esp32dev_mpu6050_check` SUCCESS in 14.724 s, RAM 21,800 bytes and flash
287,249 bytes. Upload screenshot
`codex-clipboard-209cb08f-4d27-454d-9991-498de244110b.png` shows SUCCESS in
19.331 s, 287,616 firmware-image bytes written, data hash verified, and RTS
reset. Physical sensor alone was connected by instruction; no port identity
is visible in those supplied windows.

Monitor screenshot `codex-clipboard-9d124993-b653-44ef-9288-67221cd14715.png`
shows repeated alternating responses:

```text
#mpu6050_check,address=0x68,status=ID_MATCH,who_am_i=0x68
#ina219_check,address=0x40,status=REGISTER_READ,config=0x019B
```

No failed-read line is visible in this window. The MPU identity matches the
expected register byte and INA configuration reads succeed on the same bus.
The observed INA value is not an identity or measurement/calibration proof;
this program does not rewrite configuration. No startup banner, serial port,
time-stamped raw capture, motion sample, or explicit measured rail voltage was
provided. The repeated screenshot output supports this limited communication
gate, not long-run error freedom, silicon authenticity, acceleration response,
vibration RMS, or CAN task coexistence. The sensor currently runs the temporary
diagnostic with CAN/DHT disabled; dashboard was left unplugged by instruction.
Project 1 sources and normal 0.4.0 application sources are unchanged.

### MPU acceleration diagnostic preparation — no measurement result yet

After the communication check, operator reported sensor USB unplugged, with
dashboard remaining unplugged and wiring unchanged. Prepared separate target
`esp32dev_mpu6050_measure`, source `src/Mpu6050Measure.cpp`, and actual conversion/
profile/timeout helper assertions in `include/Mpu6050Diagnostic.hpp`. This
resets/configures only the MPU at 0x68 for +/-4 g (8192 counts/g), 44 Hz accel
filter, nominal 100 Hz internal data, checked DATA_RDY and coherent six-byte
burst, then prints gravity-including XYZ/norm in g with at least 200 ms pause.
It does not acquire all internal samples or compute vibration RMS; INA219
settings, existing diagnostics, normal CAN firmware, and Project 1 are untouched.
Read/settings/ready failures emit NaNs and trigger paced reinitialization;
near-full-scale samples emit NaNs without substituting low/previous values.

All existing 66 host tests pass. Actual PlatformIO parsing verifies distinct
single-source filters and flags for both motion diagnostics, unchanged default
environments, and no motion-diagnostic flag in the combined application.
These checks do not execute the new driver or C++ assertions. MCU build/upload,
stationary gravity readings, gentle tilt response, and diagnostic failure/
recovery are pending. No assistant flash/compiler retry was performed.

### MPU acceleration build/upload and orientation response — accuracy unresolved

Operator screenshots show `esp32dev_mpu6050_measure` build SUCCESS in 11.025 s
(RAM 21,804 bytes, flash 288,169 bytes;
`codex-clipboard-7943844a-ff1a-4cd7-a0df-7cc62b30385b.png`) and upload SUCCESS
in 19.593 s (288,528 firmware-image bytes, data hash verified;
`codex-clipboard-cac7daa3-d9c0-4b5b-a6d8-f120d84b5ebd.png`). The compiled
diagnostic includes the actual helper/static assertions. The physical sensor
alone was powered by instruction; supplied windows do not show its port or
startup banner. CAN/DHT remain disabled in this temporary image.

Initial stationary screenshot `codex-clipboard-3fff1031-ae27-42b4-ad2b-9c36dc24112f.png`
shows HW_SAMPLE around X +1.025 g, Y -0.023 g, Z -0.098 g and norm 1.03 g.
Subsequent requested tilt windows remained X-dominant and were not treated as
axis-response PASS. `IMG_7151.jpeg` shows the module still upright/seated in
the breadboard; no physical 90-degree pose was independently established from
the prior screenshots. With both USB supplies unplugged, the operator removed
the MPU and reported wiring five female-to-male extension leads: VCC sensor3V3,
GND sensorGND, SCL22, SDA21, AD0 sensorGND, with XDA/XCL/INT left unconnected.
No new physical photo independently confirms those final lead endpoints.

After sensor-only power-up, these supplied HW_SAMPLE windows show:

| Requested position | Representative axes X/Y/Z (g) | Norm observation | Screenshot |
|---|---|---|---|
| Chip side up, stationary | -0.34 / -0.31 / +0.74 | About 0.86–0.87 g | `codex-clipboard-dbd577ce-cbed-49ad-b40d-5737e4462b08.png` |
| Chip side down, stationary | -0.07 / -0.13 / -1.18 | About 1.19 g | `codex-clipboard-6de9bfc2-1fda-4b94-898b-7c300fb274ec.png` |

Z polarity reverses, supporting physical orientation response. The differing
stationary magnitudes do not support an accuracy PASS. Poses are manual and
not independently level-verified or simultaneous reference measurements; there
is no timed raw capture, six-face averaging, or metrological calibration.
No failure statuses are visible in these windows, not proof of zero failures
throughout the session. A previous 1.03 g norm cannot validate all orientations.

Read-only source review confirms 0x08 +/-4 g and signed 8192-counts/g scaling
consistent with the primary driver reference; no changes or per-vector
normalization were applied. Cause remains unresolved, without declaring bad
silicon or deriving correction constants from these windows. Actual module
VCC/GND DC voltage at the new leads is the next check. Normal 0.4.0 motion stays
SIM; this is not vibration RMS, CAN acquisition scheduling, calibration,
missing-motion handling, or diagnostic recovery acceptance.

### MPU module supply and pose-capture preparation

Operator subsequently reported 3.31 V from MPU VCC to GND on the new jumper
connections, with only sensor USB connected. No meter screenshot, calibrated
reference, internal-regulator measurement, or ripple measurement was supplied.
This agrees with the intended 3.3 V input rail but does not identify the cause
of the orientation-dependent gravity-magnitude discrepancy.

Prepared `tools/capture_mpu_pose.py` to collect one operator-labeled stationary
pose at a time, without flashing/reset commands, device configuration writes,
or calibration/normalization. It saves exact raw bytes, parsed records (including
explicit unavailable records), and JSON mean/stddev/range summaries to unique
ignored `data/mpu-pose-*/` directories. RECORDED is a recording-quality result,
not accuracy PASS; accuracy is always NOT_VALIDATED. Fault evidence, malformed
data, timestamp anomalies, sparse coverage, interruptions, and serial errors
remain REVIEW. No actual timed pose capture has yet been made with this tool.

77 host tests passed (prior 66 plus 11 new parser/averaging/fake-serial cases).
Tests include signed/zero values, failure retention after recovery, malformed/
nonfinite/inconsistent norm rejection, fragments, timestamp rollover/reset,
serial-open errors, unique outputs, inactive DTR/RTS before opening, and keeping
a 0.87 g mean unchanged. These are host software checks, not new MCU execution,
calibration, verified physical poses, or additional board flashes.

### First timed MPU z_up capture — REVIEW (text corruption, not calibration PASS)

Screenshot `codex-clipboard-cc97b5d7-a321-46b4-8402-b78f5fff345c.png` reports
`mpu-pose-20261101T143008Z-z_up-104325`, independently inspected locally:

| Field | Observed result |
|---|---|
| Start UTC | 2026-11-01T14:30:08.039588+00:00 |
| Interface / duration | Sensor COM8, 115200 baud / 15.031 s complete |
| Result / accuracy | REVIEW / NOT_VALIDATED |
| Parsed acquisitions | 68 HW_SAMPLE, 0 explicit unavailable |
| Malformed complete lines | 1,013; sole automatic review reason |
| Device times | 901290–916004 ms, no duplicate/backward steps, max interval 221 ms |
| X mean / stddev | -0.201332 / 0.001846 g |
| Y mean / stddev | -0.189300 / 0.001684 g |
| Z mean / stddev | +0.813376 / 0.002317 g |
| Norm mean / stddev / range | 0.859037 / 0.002079 / 0.8542–0.8640 g |

Raw file is 36,871 bytes / 1,081 complete lines. Lines 1–1012 are identical
truncated numeric fragments. Line 1013 is a fused/repeated timestamp-901069
prefix line. All subsequent 68 lines are normally formatted samples, first at
host elapsed 0.406 s, last at 15.031 s; no final partial bytes. Counts/stats
were verified against raw/parsed artifacts. The corruption is present in saved
raw bytes, not merely a screenshot rendering effect or ordinary single-line
capture boundary. This pattern's concentration at opening does not identify
its root cause; device output, USB/serial, or host handoff behavior remain to
be distinguished. Similarity to prior standalone garbling is not proof of a
shared cause, repair, or a CAN timing defect.

The valid-subset statistics remain descriptive only; no correction constants
or vector normalization were applied. This is not calibrated pose/accuracy
acceptance or a clean serial run. The pose was operator-reported, not jig- or
level-verified. Planned next diagnostic is a fresh sensor power cycle/direct
capture without opening the monitor first, with wiring/firmware unchanged.
78 host tests pass after adding a synthetic regression for this opening-burst
pattern: 1,013 malformed lines remain REVIEW even followed by 68 valid records.
No real capture was rewritten or deleted. Preserved ignored artifact hashes:

| File | SHA-256 |
|---|---|
| `raw.log` | `47004e2bf8b6d6e5c0f5f17f5d31af4456df963b676a2172a0442b6097f37439` |
| `samples.json` | `aff1e85af345422c911d5702f3e9c39182ce3210c1b95468506a89806299bbb8` |
| `summary.json` | `56b3ae79d746225c9efd5bbcf25d03090a7512e4a8ea848fc800d5965788a9c4` |

### Direct-start MPU z_up retry — RECORDED, not accuracy PASS

With wiring/measurement firmware unchanged, the operator reported sensor USB
off, then was instructed to power only sensor, leave the MPU chip side up and
still, and run the capture directly after five seconds without a serial monitor
handoff. Screenshot `codex-clipboard-e3d0f68e-e3cc-4762-932e-6c0cd48c38bc.png`
shows the old REVIEW and new result separately; the old evidence is not replaced.

`mpu-pose-20261101T144727Z-z_up-187421` started
2026-11-01T14:47:27.950359+00:00 on COM8/115200 and completed 15.062 s.
Result RECORDED, accuracy NOT_VALIDATED, no recording review reasons:
69 HW_SAMPLE records, zero unavailable/malformed records, no observed device
timestamp resets/duplicates, max interval 221 ms. Raw has exactly 69 complete
telemetry lines and no final partial fragment or startup banner. Device times
13179–28111 ms are consistent with the requested fresh sensor boot, not exact
USB/button timing evidence. Raw grouping and parsed statistics were independently
checked against the summary.

| Channel | Mean g | Population stddev g | Range g |
|---|---:|---:|---:|
| X | -0.226823 | 0.001614 | -0.2301 to -0.2229 |
| Y | -0.045000 | 0.001510 | -0.0486 to -0.0409 |
| Z | +0.819335 | 0.002340 | +0.8143 to +0.8256 |
| Norm | 0.851348 | 0.002316 | 0.8458 to 0.8574 |

This is a clean recording window, not stationary-gravity accuracy acceptance.
The low norm persists without observed text corruption, so parsing corruption
does not alone explain it. Pose geometry is still operator-reported; no jig,
level reference, calibration fit, normalization, or firmware modification was
applied. A single clean direct-start retry does not establish the earlier
corruption's cause or prove it fixed. Next compare a separately captured z_down
pose without opening the monitor or changing wiring/settings. Artifacts remain
unchanged/ignored:

| File | SHA-256 |
|---|---|
| `raw.log` | `985482d0855b225a0f8b5d4dab261c9fa6b3589c087fe21a2b36a3730f19370e` |
| `samples.json` | `2faa19983e49f915360160b49d99c82ce09353439015ff378c0378d0e5e8e8c5` |
| `summary.json` | `a133800e81f0bcb93ea53bb551c6201809dee46f2ba72a4aad551d8135b9a8d6` |

### First timed MPU z_down — RECORDED; stationarity/accuracy not accepted

Screenshot `codex-clipboard-0358824e-c661-4970-bad8-f7b5661de3ef.png` shows
`mpu-pose-20261101T145024Z-z_down-0e8dcd`, started
2026-11-01T14:50:24.758861+00:00 on sensor COM8/115200, complete 15.016 s.
The sensor stayed powered, wiring unchanged, monitor closed; a chip-side-down
pose was requested. Actual pose geometry remains operator-reported, not
independently verified from a physical photo or level fixture.

It is RECORDED/accuracy NOT_VALIDATED: 69 HW_SAMPLE records, zero unavailable/
malformed lines, no observed timestamp resets/duplicates, max interval 221 ms.
Raw has 69 complete normally formatted records, device times 189962–204897 ms,
no final partial data, and no startup banner. No opening corruption is present
in this window; the earlier corruption's cause is still unresolved.

| Channel | Mean g | Population stddev g | Range g |
|---|---:|---:|---:|
| X | -0.051088 | 0.021350 | -0.1357 to -0.0282 |
| Y | -0.075588 | 0.030033 | -0.1134 to +0.0574 |
| Z | -1.185154 | 0.013469 | -1.2064 to -1.0973 |
| Norm | 1.189233 | 0.013008 | 1.1071 to 1.2122 |

Compared with clean z_up (+0.819335 g Z, norm 0.851348 g), Z reverses and the
orientation-dependent magnitude discrepancy persists. Offset is a hypothesis,
not an accepted correction estimate; level/alignment and stationarity have
not been established. X/Y standard deviations exceed the previous pose's
values substantially, and recording RECORDED does not screen physical motion.
A supported, hands-off z_down repeat is requested to distinguish positioning/
movement effects from unstable measurements before calibration data acceptance.
No constants were fitted/applied, firmware changed, or capture artifacts
rewritten. No favorable sample subset was selected. Preserved ignored hashes:

| File | SHA-256 |
|---|---|
| `raw.log` | `14dae8cfee6eb072b9cebd61bdf8161be7b9fae7b64dc6a12e92f612292c7bfb` |
| `samples.json` | `de0747a4ccbc1facdad96d526708bc9a51448d45ee1a3950214f0a62f93e74bc` |
| `summary.json` | `02aca402907726e21d15e1f79bdd86a1884715806f997401d0df1e837e85da99` |

### Supported z_down repeat — RECORDED, not precision calibration

Operator reported lead tension prevented hands-off face-down positioning and
that cardboard/tape fixture materials were unavailable. The next attempt was
explicitly hand-supported by an edge, wrist/forearm on the table and leads
relaxed, after the requested power-off/setup. This is an allowed basic diagnostic
position, not independently level-verified or rigid-jig calibration geometry.

Screenshot `codex-clipboard-02e182fb-eddd-4e37-b2a1-f48f424d4665.png` shows
`mpu-pose-20261101T150241Z-z_down-1fdaea`, started
2026-11-01T15:02:41.715911+00:00 on COM8/115200, complete 15.016 s.
RECORDED/accuracy NOT_VALIDATED, no recording review reasons: 68 HW_SAMPLE,
zero unavailable/malformed records, no observed timestamp reset/duplicate,
maximum interval 221 ms, no final fragment. Raw inspection confirms exactly
68 normally formatted complete telemetry lines (device 926982–941696 ms),
with no startup banner. No new text corruption is observed in this window.

| Channel | Mean g | Population stddev g | Range g |
|---|---:|---:|---:|
| X | -0.110059 | 0.003387 | -0.1177 to -0.1042 |
| Y | -0.170072 | 0.012570 | -0.1812 to -0.0771 |
| Z | -1.171813 | 0.002982 | -1.1821 to -1.1638 |
| Norm | 1.189262 | 0.002420 | 1.1821 to 1.1966 |

Z/norm are steadier than in the prior run, while mean norm is still about
1.189 g. Y has an initial transient; the entire capture is retained and no
quiet subset selected. This supports continued orientation-dependent magnitude
concern, not accuracy acceptance or exact sensor-bias estimation. No firmware,
calibration constants, normalization, or saved-capture data were changed.
Next check other axis responses with the same lightly supported method,
preserving the manual-pose limitation rather than requiring unavailable
mounting supplies. All three ignored artifacts remain unchanged:

| File | SHA-256 |
|---|---|
| `raw.log` | `fd379612f46ae32a8362e02131a31e9b9330bc61fb52dead469a2d6edd7c0ed8` |
| `samples.json` | `c57c8ac132bcfb93ef53be3dbcc1cc7c58a1523a517c56b699ddd612f03c37f4` |
| `summary.json` | `2fb814ebe78a28a4714fe82c24dbee2d4edd16166a42512869bbe783e8385480` |

### Two upright attempts and untouched control — calibration remains on hold

Screenshots `codex-clipboard-c53b0f4f-5869-466e-946b-355c29005823.png` and
`codex-clipboard-6e5ec5a6-a4b3-4ddd-b021-99087abdc8c2.png` show two attempts
the operator called upright, then a further run explicitly without touching
the MPU. All three retain the requested x_up label and use COM8/115200 with
the same sensor measurement firmware. Their geometry is not independently
verified; all numerical vectors remain Z-dominant, not validated X-up endpoints.

| Capture | UTC start | Elapsed s | Result | HW samples / malformed lines |
|---|---|---:|---|---:|
| `mpu-pose-20261101T150559Z-x_up-d7feb3` | 2026-11-01T15:05:59.407522+00:00 | 15.078 | REVIEW | 68 / 4327 |
| `mpu-pose-20261101T150653Z-x_up-1e062f` | 2026-11-01T15:06:53.654513+00:00 | 15.078 | REVIEW | 68 / 1461 |
| `mpu-pose-20261101T151107Z-x_up-1dd191` | 2026-11-01T15:11:07.974186+00:00 | 15.063 | RECORDED | 68 / 0 |

All complete, accuracy NOT_VALIDATED, no explicit unavailable records, observed
timestamp reset/duplicate, or final fragment; maximum device interval 221 ms.
Both REVIEW reasons are solely malformed complete lines. Raw inspection finds
the first REVIEW begins with 1455 repeats of one numeric tail, 2871 of another,
and a fused prefix line, followed by 68 normally formatted records. The second
starts with 1460 repeated tails and one fused line, then 68 normal records.
The first valid sample arrives at host elapsed 0.437 s in each. The untouched
control has exactly 68 normally formatted raw lines, device 1433186–1447900 ms.
This is actual raw-byte corruption at opening, not normal capture-boundary
variation or only terminal rendering. It has recurred without a monitor handoff.

| Run | Mean X / Y / Z g | Mean norm / norm stddev g |
|---|---|---|
| First upright attempt (valid subset only) | -0.384646 / -0.006444 / +0.761412 | 0.853215 / 0.002098 |
| Second upright attempt (valid subset only) | -0.200947 / -0.123304 / +0.820101 | 0.854485 / 0.004782 |
| Untouched control | -0.204162 / -0.108250 / +0.822984 | 0.854813 / 0.001914 |

The untouched control's norm range is 0.8499–0.8609 g. It supports a steady
low-norm observation, not accuracy PASS or proof that touching caused the prior
garbling; supported runs were also clean. Neither source nor pose labels were
modified to force a result, and no calibration was fitted. Pause additional
pose collection and request a physical photo of the unchanged current position.

Read-only review of installed pyserial 3.5 Windows read/open code, local Arduino
printf/UART wrappers, and capture raw-writing loop found no confirmed cause.
Port enumeration identifies CP210x (10C4:EA60, SER 0001) on COM8, not a unique
board-role identity. CIM queries for driver details were unavailable; no driver
installation/change was attempted. Installed Windows pyserial open() purges
driver buffers internally; raw captures cover bytes returned after opening,
not all pre-open electrical UART traffic. No extra input-buffer discard, MCU
configuration change, firmware reflash, or calibration correction was applied.
All original ignored artifacts remain unchanged:

| Run / file | SHA-256 |
|---|---|
| d7feb3 `raw.log` | `da40986e0d563a088c8fe0e4d7b1df88624e4d4909569330fdf09553f8fbcfc2` |
| d7feb3 `samples.json` | `970cde59912373f17c5fd2202bdd6671ecd9e1579c45a0eecc88792aeac0d706` |
| d7feb3 `summary.json` | `c6b81158fb2f612977e53a1de7940f0618b8fbb4fe83092ac69096a4fa3e9ad9` |
| 1e062f `raw.log` | `b1dcec49e0fc00dbc68a92d7e77568bd7cf8d97fc66e173f8256cc43f747a2d0` |
| 1e062f `samples.json` | `fae3c0eb0fec966b1ae8ff718210ae8b2a7bf9db294534a9e28c433c11ff770f` |
| 1e062f `summary.json` | `d3f1d6c8657cb879054504c805fbb2097a991d32ed16659c1551b2bd1049b074` |
| 1dd191 `raw.log` | `f4d8951cf9c2c287a0777f8ed30dd76ae6186b6096ee4dd8bab8783bf112196c` |
| 1dd191 `samples.json` | `189d31c82cecf46a0a9953c4ecdeacc904b1584d586ad043a63e31042e45978a` |
| 1dd191 `summary.json` | `c5740e1ebad3eecec02a72e31568935525cfe4c815f2d7ef38ee0374f52e6cd9` |

### Physical flat/upright clarification and X-dominant response — still REVIEW

Requested photo `IMG_7160.jpeg` shows the untouched module lying approximately
chip-side-up on the mat. That is consistent with the prior Z-dominant vectors;
the x_up command label was not physical X-axis alignment evidence. Earlier
data labels remain unchanged. Instructions were clarified because pointing a
printed arrow toward the top of a photo is not the same as making it vertical.
The operator then reported holding the module vertical and supplied
`IMG_7161.jpeg`, showing a vertically held chip face, connector row at bottom.
This supports approximate upright orientation at photo time, not a leveled
pose throughout the capture. Actual observed axis response takes precedence
over the assistant's earlier top/bottom connector-position assumption.

Result screenshot `codex-clipboard-990ccca2-2b82-4cde-b741-b47c4a476ce8.png`
matches `mpu-pose-20261101T152153Z-x_up-b71e14`, independently read locally:
started 2026-11-01T15:21:53.996929+00:00 on COM8/115200, 15.000 s complete,
68 HW_SAMPLE, zero unavailable records, 499 malformed lines, no observed
timestamp resets/duplicates, max interval 221 ms, no final fragment. Raw has
567 complete lines, including 498 repeated numeric-tail fragments and one
additional malformed record. Recording stays REVIEW, accuracy NOT_VALIDATED.

| Channel | Valid-subset mean g | Population stddev g |
|---|---:|---:|
| X | +0.999716 | 0.013030 |
| Y | -0.014771 | 0.028673 |
| Z | -0.393659 | 0.026662 |
| Norm | 1.075241 | 0.013534 |

Norm range is 1.0337–1.1047 g. X dominance now supports a genuine orientation
response; it is not calibrated accuracy, perfect 90-degree geometry, or clean
serial continuity. No correction was fitted or applied. Pause repeated pose
collection and investigate serial transport; proposed next physical diagnostic
is sensor power-off followed by a known-working, connector-compatible USB cable
substitution. No cable/driver/firmware change has yet been performed. Original
ignored artifacts remain unchanged:

| File | SHA-256 |
|---|---|
| `raw.log` | `baa727276664fad5dc7a2816a8b14feec0181a3a0a1eb5a320e6446cc259d051` |
| `samples.json` | `6c211e18d1781bc6fc89934d27beef51b55266b042b0f300d5ba28c59aa9190a` |
| `summary.json` | `f025a904ca29ecbbc178f06726a5bc73088c13370bdd89daa1d244b8962aa3d2` |

### USB cable substitution — first clean window, not a confirmed fix

After sensor-off confirmation, the operator replied ready to instructions to
substitute the dashboard's compatible working USB data cable on the same host
USB port, leave dashboard unplugged, and rest the MPU chip-side-up. These setup
details are operator-reported, not independently photographed; power cycling
also occurred, so this is not an isolated cable-only causal proof. Firmware,
drivers, calibration, and breadboard wiring were not changed by this workflow.

Screenshot `codex-clipboard-ad96c487-de78-4a82-b7bb-18e27c25f188.png` matches
`mpu-pose-20261101T153012Z-z_up-7b6aac`: start
2026-11-01T15:30:12.189535+00:00, COM8/115200, complete 15.062 s,
RECORDED/accuracy NOT_VALIDATED. There are 69 HW_SAMPLE, zero unavailable/
malformed records, no observed timestamp restart/duplicate, max interval 221 ms,
no final fragment. Raw inspection confirms 69 complete normally formatted
lines, device times 177007–191939 ms, no startup banner in the window.
Means XYZ are -0.200878/-0.181733/+0.815013 g; mean norm 0.858852 g,
population stddev 0.002296 g, range 0.8526–0.8645 g.

This window is clean, but a previous power-cycle/direct-start run with the old
setup was also clean. Do not conclude the original cable failed or corruption
fixed. The low-norm accuracy concern remains separate. Next request two more
captures with the same cable/host port and no power cycle, movement, or monitor
handoff, to check repeated port reopening. Original capture hashes:

| File | SHA-256 |
|---|---|
| `raw.log` | `b7cfee7686989106dad384d913d16fb0dffed5eabc018bd480657b19d476ca1f` |
| `samples.json` | `e13640d159a04c24e21b450f7644d49346abc01178ee70aa1dd2dd81ede7c2cb` |
| `summary.json` | `731135fc22449a4bc4a47a4d10af854e93780f77a1b7558e4d0f44572de5686f` |

### Replacement-cable port reopen checks — two additional clean recordings

Screenshot `codex-clipboard-30b33e35-964f-490c-a704-a28bcc1b76e2.png` shows
two capture invocations in one loop, without requested movement, power cycle,
or monitor handoff. The same substituted cable/host port remained by instruction;
physical setup is operator-reported. Both files were independently inspected:

| Capture | Start UTC | Elapsed s | HW samples | Mean norm / stddev g |
|---|---|---:|---:|---|
| `mpu-pose-20261101T153355Z-z_up-7d5d5f` | 2026-11-01T15:33:55.516200+00:00 | 15.047 | 69 | 0.858007 / 0.002476 |
| `mpu-pose-20261101T153410Z-z_up-3445a6` | 2026-11-01T15:34:10.753143+00:00 | 15.031 | 69 | 0.858512 / 0.002395 |

Both COM8/115200 captures are complete, RECORDED/accuracy NOT_VALIDATED,
with no review reasons, unavailable/malformed records, observed timestamp
reset/duplicate, startup banner, or final partial data; max interval 221 ms.
Each raw file contains exactly 69 normally formatted telemetry lines. Device
times are 400351–415283 and 415504–430436 ms respectively, consistent with
no intervening reset in these windows. Means XYZ are respectively
-0.203800/-0.181171/+0.813517 and -0.203791/-0.181307/+0.814030 g.

The three replacement-connection windows total 207 cleanly formatted HW_SAMPLE
records. This is useful bounded working-connection evidence, not proof of
old-cable failure or resolution of all earlier opening corruption. The older
setup had also produced clean windows; no long-run or reverse-substitution
comparison has been performed. Keep the replacement connection for subsequent
tests without changing drivers/firmware. Low norm remains, no calibration is
accepted, and manual pose geometry is unverified. Next check remaining Y-axis
response; all prior failed runs remain preserved. Original ignored hashes:

| Run / file | SHA-256 |
|---|---|
| 7d5d5f `raw.log` | `46d140f1035e1a4a22423bd35a6571aae1c5a8ce1dce692fe7a24e78c75499bc` |
| 7d5d5f `samples.json` | `58cc3c2ca050db1271130094d95f8cd365a70b2027ab0f000da627a5551adc04` |
| 7d5d5f `summary.json` | `3e705cfe63ee040b153445a97adcce99076b7210bcd7437818177e828c796c0e` |
| 3445a6 `raw.log` | `c127c4e2697893430add37b1f61f691fba0e2bba4fb443d129d76be68a8406e0` |
| 3445a6 `samples.json` | `2af7f77e08572d22633b1a4162458e487f5ff53b4d6b81169f0872089a73492a` |
| 3445a6 `summary.json` | `8ac21628d8a8aba1f720678d072002fd81d9699f43ac75b3f5c06aa81e77b834` |

### Intended Y-up attempt — clean recording, physical axis alignment unresolved

Screenshot `codex-clipboard-21f61f95-6bf3-4ae6-a7f1-bb222724fd85.png` matches
`mpu-pose-20261101T154522Z-y_up-151918`: start
2026-11-01T15:45:22.579305+00:00, sensor COM8/115200, complete 15.047 s.
RECORDED/accuracy NOT_VALIDATED: 69 HW_SAMPLE, zero malformed/unavailable
records, no observed timestamp restart/duplicate or final partial data,
max interval 221 ms. Raw grouping confirms 69 normally formatted complete
records (device times 1087292–1102224 ms); no startup banner in the window.

Mean XYZ/norm is +1.018591/-0.006212/-0.044984/1.020125 g; population stddev
is 0.014190/0.020812/0.025389/0.014276 g respectively. Despite the y_up label,
the numeric vector is X-dominant. No physical photo of this exact position
was supplied yet, so neither user positioning nor sensor/code-axis mapping is
assumed at fault. The pose argument is only a saved label, not a device command
or coordinate transformation. Request the unchanged physical position photo
before another run. No Y-up endpoint, calibration acceptance, axis remapping,
correction fit, or wiring change follows from this attempt. Original artifacts:

| File | SHA-256 |
|---|---|
| `raw.log` | `031dd8eaebfebf843a1a1c43a5217e437a0c859122321155dca87759ed68c9f5` |
| `samples.json` | `7319281b9806a413cb6288dad90b03ae05786dd638b2ff3cae3f7148a3661891` |
| `summary.json` | `689881fb42e80c1e975a6754256c98f1601f104374d2bae435a90f30059a5217` |

### Physical orientation clarification followed by Y-dominant recording

Operator supplied `IMG_7162.jpeg`, showing chip face vertical and connector row
across the bottom, the previous X-dominant pose. Additional capture
`mpu-pose-20261101T155007Z-y_up-dcade3` (screenshot
`codex-clipboard-79de276e-1fe2-4a35-a04f-9c643acc3f86.png`) remains labeled
y_up as requested but does not establish that endpoint: mean XYZ is
+1.024655/-0.030733/-0.106909 g, norm 1.031626 g. It started
2026-11-01T15:50:07.577172+00:00, complete 15.047 s, RECORDED/accuracy
NOT_VALIDATED, 69 HW_SAMPLE. Do not retrospectively relabel its raw artifacts.

After the clarified quarter-turn instruction, operator explicitly reported
having interpreted the board orientation incorrectly. Screenshot
`codex-clipboard-32b2e965-63a9-4198-8a5e-9d7e4aa54a6c.png` matches
`mpu-pose-20261101T155548Z-y_up-624357`: start
2026-11-01T15:55:48.071231+00:00, complete 15.047 s, RECORDED/accuracy
NOT_VALIDATED, 69 HW_SAMPLE. Both runs use COM8/115200; both have zero
unavailable/malformed records, observed timestamp reset/duplicate, or final
fragment, max device interval 221 ms. Each raw file independently has exactly
69 normally formatted complete records; no startup banner in either window.

Corrected-pose mean XYZ/norm is -0.023984/+0.972925/-0.333316/1.029046 g;
population stddev respectively 0.018953/0.013014/0.016893/0.011677 g.
Norm range is 0.9947–1.0586 g. Y dominance with X near zero supports the
intended orientation response. A corrected-position physical photo was not
provided, and hand-supported level/alignment remains unverified. All three
axes have now shown gravity/orientation response across the prior windows,
not calibrated accuracy, a complete six-face procedure, or CAN motion behavior.
No device constants, firmware, wiring, or axis mapping were changed; the
earlier magnitude discrepancy remains. Next compare opposite Y with chip face
still toward the operator and connector row vertical at the right edge.

Original artifacts remain unchanged and ignored:

| Run / file | SHA-256 |
|---|---|
| dcade3 `raw.log` | `ba853477cf8a3121ce31555d3d641464632942544510db53f847f2fd8a9803f9` |
| dcade3 `samples.json` | `2e15eb479a1d20a4f1641efcafa8df6b19115401441ae423b3188835dbd89223` |
| dcade3 `summary.json` | `f88be1ebbff3957b1d3a369e3fecee653c183f9423999bc6321a16468bf49ade` |
| 624357 `raw.log` | `f15b5813cb17e1b0aa1190dce41eb27b6d19ec9245d7594a71952e35ae3914bb` |
| 624357 `samples.json` | `2438f819b319f53c2399cd6e48751e0b18451708a76386d620754b002b8a9566` |
| 624357 `summary.json` | `143341dded05bbd770794c11f7444e6c9d3515f019b8cf4699c027d301fac84f` |

### Opposite-Y recording — response observed, 6,615 malformed lines recur

Screenshot `codex-clipboard-884577a8-50e7-4cdd-9ae2-f669cd527084.png`
matches `mpu-pose-20261101T160030Z-y_down-62e8f5`: start
2026-11-01T16:00:30.372194+00:00, sensor COM8/115200, complete 15.078 s.
REVIEW/accuracy NOT_VALIDATED: 67 HW_SAMPLE, zero explicit unavailable records,
6,615 malformed complete lines (sole review reason), no observed timestamp
reset/duplicate, max interval 221 ms, no final fragment. Raw inspection finds
3048 identical numeric-tail lines, 3566 of another tail, one fused-prefix line,
and 67 normally formatted records: 6,682 complete raw lines total.

Valid-subset mean XYZ/norm is +0.072940/-1.027154/-0.184178/1.046381 g;
population stddev respectively 0.019290/0.013760/0.014868/0.012757 g. Norm
range is 0.9872–1.0787 g. Y reverses relative to the corrected +Y attempt,
supporting opposite response. Pose is still manually reported, not jig/level
verified; this is not calibration or serial acceptance.

The malformed burst recurs after the replacement-cable procedure and three
clean sampled windows. No cause/fix is established; the prior clean windows
do not prove a defective original cable, and hand movement is not established
as causing serial corruption. Pause further pose captures rather than continue
collecting correction endpoints through an unresolved serial problem.

Read-only registry lookup for the enumerated CP210x COM8 instance returns
driver version 11.6.0.420, provider Silicon Laboratories Inc., DriverDate
8-14-2026, InfPath oem26.inf, InfSection SiLabsDDInstallSection. This obtains
installed metadata despite the earlier unavailable CIM interface; no driver
write/install/downgrade was performed. Manufacturer download listing identifies
the same 11.6.0 package family, not proof that the driver is fault-free or the
loaded binary independently verified. Next request sensor power-off and a
different direct computer USB port using the same replacement cable. Current
hub/topology is unverified; this is an isolation step, not an asserted fix.
No firmware, calibration, sensor wiring, or original artifacts were changed.

| File | SHA-256 |
|---|---|
| `raw.log` | `0ea900cef745349d9504b98b2ce2d4b23bf37ba7767b5381159a8d25baba7279` |
| `samples.json` | `eb76eef544c06d42bae9db31db8cc71ca90aaa3db9bfa8d206080869d24f90a2` |
| `summary.json` | `fde79a8b716c19badafc01b7a809e6f6cb8a44b8feac2087dccb7bed7ef4aa1d` |

### 2026-11-09 — MPU6050 six-position fit and separate tilt checks

The alternate direct USB path (CP210x COM8, location 1-1) still produced
1,219 malformed opening lines in the Python capture. A separate .NET
SerialPort capture also received 2,629 malformed opening lines followed by
67 valid samples. The source of the repeated fragments remains unresolved.

For the pose series, the operator opened COM8 at 115200, kept the port open,
and tapped the sensor ESP32's EN button at the prompt. The 25-second .NET
capture saved all bytes. The boot banner and MPU diagnostic header separate
the pre-reset stream from the new measurement session. The table uses all
hardware samples after that header. Raw copies, including opening fragments
and boot output, are in `data/mpu-pose-20261109-reset-series/`. Their SHA-256
hashes matched the originals in the Windows temporary folder after copying.

| Intended pose | Raw log UUID prefix | Post-reset samples | Mean X / Y / Z (g) |
| --- | --- | ---: | --- |
| X-up | `dd50b939` | 81 | +1.02561 / +0.03044 / -0.15641 |
| X-down | `ddc0e7b2` | 73 | -0.96604 / -0.00139 / -0.12872 |
| Y-up | `f2e7e157` | 75 | -0.02474 / +0.97649 / -0.03781 |
| Y-down | `4b769b36` | 72 | +0.08813 / -1.02764 / -0.08722 |
| Z-up | `32b3edb1` | 80 | -0.20595 / -0.01291 / +0.82400 |
| Z-down | `513b4a71` | 71 | -0.09761 / +0.01690 / -1.18856 |

The per-axis correction subtracts the midpoint of the opposing means, then
divides by half their difference. X/Y/Z midpoints are +0.029786, -0.025571,
and -0.182281 g. The corresponding half-spans are 0.995823, 1.002065, and
1.006280 g. This was evaluated offline with manually held poses. Exact
alignment and traceable per-axis accuracy remain unverified.

| Separate tilt capture | Raw log UUID prefix | Post-reset samples | Mean raw norm (g) | Mean corrected norm (g) |
| --- | --- | ---: | ---: | ---: |
| Positive Z, earlier capture | `12585af5` | 47 | 0.93700 | 1.01994 |
| Positive Z | `71a17537` | 72 | 0.96575 | 1.01380 |
| Negative Z | `5cf96ba0` | 81 | 1.16011 | 0.99895 |

The six reference and three tilt captures had zero malformed complete
records and zero unavailable readings after their diagnostic headers. The
negative-Z tilt had 1,848 malformed lines before the reset and 27 valid
samples in that same pre-reset segment. The capture procedure gives a clean
post-reset window in these runs. The opening issue still occurs, and valid
samples sometimes resume before EN. The result supports static gravity
response and improvement at the sampled tilts. Motion RMS, dynamic response,
and MPU telemetry over CAN remain separate validation steps.

### 2026-11-09 — Standalone still, rocking, and still motion check

The operator built both MPU diagnostic environments and uploaded
`esp32dev_mpu6050_motion_check` to the sensor ESP32 on COM8. The 30-second
.NET serial capture at 115200 recorded EN, start-rocking, and set-down
prompts at byte offsets 232973, 233922, and 234274, respectively. The boot
banner began at byte 233153 and the motion diagnostic header at byte 233457.
The opening burst included 17,853 copies of one partial record.

All 20 post-reset records were valid `HW_WINDOW` records, each with 100
samples and a first-to-last span of 987–990 ms. The three initial still
windows read 3.01–3.15 mg RMS. Two windows after the rocking prompt remained
at baseline during the transition, then six read 23.52–65.57 mg RMS. Three
windows after the set-down prompt remained elevated, then the final six
settled at 2.95–3.19 mg RMS. Prompt positions are host markers and do not
establish the exact time of physical movement.

The metric is the RMS of the acceleration vector about its window mean.
The observed increase includes changing tilt. This check establishes
response to gentle bench motion and return to the still baseline.
Reference vibration accuracy and CAN delivery remain unverified.

Raw copy: `data/mpu-motion-a19da32c-96f5-419e-8a42-76de651bfb6e.log`.
SHA-256: `3471775e68fb5c1d3c10ba79bc537d2024ed5c2dde25a0e2021412d59cf1dab2`.

### 2026-11-09 — CAN transmission delay comparison

The bench-only `esp32dev_mpu6050_tx_isolation` image ran all three
sensor acquisitions with the CAN driver ready and delayed publication by
90 seconds. The sensor monitor on COM12 recorded `HW_WINDOW` at 1860 ms
and `#bench,can_tx=starting,t_ms=90540`. There were zero logged MPU I²C
errors before the marker and six after it through the last status at
191053 ms. All six error timestamps were on the same 100 ms phase as
the marker. Three timeouts invalidated windows on sample timing. The
other three read errors were followed by valid windows marked as retried.
INA219 reported `SAMPLE` with no logged result change in this monitor run.
This establishes a timing association in the observed run. The electrical
bus waveform and precise cause of the association remain unmeasured.

Monitor copy: `data/device-monitor-261109-083852.log`.
SHA-256: `0d104c4cdd4cd33fcb56b20000ef37bc68d34e371a2c3ec3d09f0c719d64f65d`.

### 2026-11-09 — CAN transmission delay with replacement MPU6050

The operator installed a newly soldered MPU6050 and repeated the CAN
transmission delay test on COM12. The monitor recorded `HW_WINDOW` at
1864 ms and CAN publication starting at 90540 ms. It recorded zero MPU
I²C errors before that marker and ten afterward through the last status
at 167631 ms. The first error was 7.299 seconds after the marker. All ten
error timestamps were within 1 ms of the marker's 100 ms phase. Three
timeouts produced `SAMPLE_TIMING` and seven other errors recovered as
`HW_WINDOW_RETRY`. The symptom reproduced with a second module. This
comparison does not distinguish electrical coupling from firmware timing.

Monitor copy: `data/device-monitor-261109-100028.log`.
SHA-256: `0b77233854060d09733474cc7610d5e1346547a5086df77dec29cece7011a897`.

### 2026-11-09 — 600-second CAN soak with serialized CAN and I²C

With the replacement MPU6050 installed, the sensor ran the bench-only
`esp32dev_mpu6050_tx_serialized` image. It reported a valid motion window
at 1863 ms and began CAN publication at 90540 ms. The sensor monitor log
had no further status changes, read errors, or drain failures when checked
after the dashboard capture.

The dashboard on COM8 captured 600.094 seconds with hardware temperature,
power, motion, and sequence metadata required. The overall and sequence
checks passed. The capture contained 599 heartbeat, 2965 condition, and
5930 motion/power records with no inferred sequence gaps, session changes,
timeouts, or unavailable hardware readings. Mean periods were 1001.042,
202.320, and 101.177 ms. Maximum intervals were 1017, 218, and 117 ms.
Still motion ranged from 2 to 5 mg. This confirms steady operation under
this bench configuration and its timing limits. End-to-end motion response
and intentional MPU fault and recovery are separate acceptance checks.

Dashboard evidence: `data/can-soak-20261109T162248Z-ca6947/summary.json`,
`raw.log`, and `telemetry.csv` in the same directory.

### 2026-11-09 — Still, rocking, and still at the CAN dashboard

The same bench image and replacement MPU6050 produced a dashboard baseline
of 3 mg for the first 30 seconds, a peak of 377 mg at 33.891 seconds, and a
return to 3 mg at 49.828 seconds. Temperature and power stayed available,
and all stream sequence checks passed.

The overall result was REVIEW. The raw dashboard log started with 1025
copies of the same partial line. Separately, one sensor window reported
`NEAR_FULL_SCALE` at sensor uptime 1608441 ms and recovered to `HW_WINDOW`
at 1609550 ms. The dashboard marked motion unavailable for 11 records
from 46.719 to 47.735 host seconds, then reported `HW_VALID`. The sensor
log showed no I²C error or CAN drain failure for this event. The near-rail
raw sample's physical cause was not measured.

Dashboard evidence: `data/can-soak-20261109T163732Z-b88414/summary.json`,
`raw.log`, and `telemetry.csv` in the same directory. Sensor monitor:
`device-monitor-261109-101127.log`.

### 2026-11-09 — MPU6050 SDA open with serialized CAN and I²C

With both boards off, the operator opened SDA at the MPU6050, leaving
the INA219 on the shared bus. The first dashboard capture was empty
(`data/can-soak-20261109T164303Z-517952/summary.json`). A sensor monitor
run with SDA open logged `READ_FAILED` and `invalid_tx_drain`. The
dashboard boot was then verified, and the operator reset the sensor
with the dashboard already running before the successful fault capture.

The 30-second COM8 capture reported REVIEW solely for unavailable motion.
All 280 motion/power records flagged unavailable motion. All 150 condition
records were DEGRADED with warning mask 16 (`0x10`). Temperature and power
stayed available, CAN sequences passed, and no timeouts were recorded.
Motion/power frames averaged 107 ms (maximum 108 ms) with a 100 ms target.
This establishes fault isolation for startup with the MPU branch open on
the bench firmware. Recovery after restoring SDA requires a separate run.

Dashboard evidence: `data/can-soak-20261109T170616Z-3ac914/summary.json`,
`raw.log`, and `telemetry.csv` in the same directory.

### 2026-11-09 — MPU6050 recovery after restoring SDA

After restoring the MPU6050 SDA branch and rebooting, the operator captured
30.016 seconds on COM8 with the same bench firmware. Motion returned to
3–5 mg with zero unavailable records. All 149 condition records reported
RUN with warning mask 0. Temperature and power stayed available, all
sequence checks passed, and no timeouts occurred.

The capture tool reported REVIEW solely because the raw log began with
849 copies of one partial serial line. All 849 preceded the first
decoded telemetry record. The hardware recovery was observed despite the
serial startup artifact.

Dashboard evidence: `data/can-soak-20261109T171617Z-5f2f88/summary.json`,
`raw.log`, and `telemetry.csv` in the same directory.

### 2026-11-09 — 600-second soak on regular MPU6050 sensor firmware

After confirming the roles and restoring the dashboard firmware on COM3,
the operator uploaded `esp32dev_sensor_dht11_ina219_mpu6050` to the sensor
on COM8. The regular build uses the serialized CAN/I²C schedule without
the 90-second bench delay.

The dashboard on COM3 captured 600.031 seconds. Overall and sequence
checks passed with hardware temperature, power, and motion required.
The capture contained 600 heartbeat, 2968 condition, and 5932 motion/power
records. Mean periods were 1000.377, 202.171, and 101.140 ms. Maximum
intervals were 1013, 217, and 116 ms. Motion stayed at 2–4 mg with the
module still. There were zero unavailable readings, inferred sequence
gaps, duplicates, late records, session changes, communication timeouts,
or unexpected serial lines. The rocking response and deliberate MPU loss
and restoration above were performed with the bench variant.

Dashboard evidence: `data/can-soak-20261109T174446Z-af3f67/summary.json`,
`raw.log`, and `telemetry.csv` in the same directory.
