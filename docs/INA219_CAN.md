# Firmware 0.4.0 — DHT11 and INA219 over CAN

## Status and validation boundaries

The operator authorized CAN integration after the standalone INA219 checks in
[bench results](../evidence/bench_results.md). The new application target is
`esp32dev_sensor_dht11_ina219`; it replaces only temperature, voltage, and current
with hardware channels. Condition percentage and vibration remain simulated.
The original simulation and DHT-only application targets remain selectable,
and both standalone diagnostics are unchanged. Project 1 is untouched.

66 Python codec/capture/sequence tests pass, including combined source flags,
missing-power vectors, zero/signed current, capture failure handling, and fake
serial integration. C++ compile-time assertions cover power qualification,
expiry, rounding, sentinel pairs, and millis rollover, in addition to the
existing temperature/INA219 conversion checks. These Python tests do not run
the MCU I2C driver or FreeRTOS tasks. All four application targets have now built
successfully in the operator terminal, evaluating the included C++ assertions;
dashboard-first and combined-sensor uploads were reported successful. The
assistant compiler executable's Windows DLL launch error is separate from these
operator-side build results. Repeat the build gate before future deployment.

The actual PlatformIO configuration parser verified the combined target's role,
DHT/INA flags, and inherited pinned DHT libraries. Read-only replay of the four
historical normalized CSVs through the new analyzer retained their three PASS
and one intentional-reset REVIEW statuses and all record counts (9,600 / 9,599 /
9,598 / 947). This replay excludes raw comment/event lines and is regression
evidence only; it neither rewrites those artifacts nor constitutes a new capture.

No firmware was flashed by the assistant. The new combined 600-second capture
`can-soak-20261020T061828Z-1803e0` passed overall and sequence screening, with
flags 7 throughout, no unavailable temperature/power, and zero observed resets,
timeouts, malformed/unexpected/undecodable complete records or sequence anomalies.
See [bench results](../evidence/bench_results.md) for counts, ranges, artifact
hashes, and source-attribution limits. A separate 30-second integrated SDA-open
startup capture (`can-soak-20261020T063943Z-a24273`) is expected REVIEW solely
for unavailable power: 30/150/300 records, warning 32, numeric temperature,
sequence PASS, no observed timeout/reset. After power-off SDA restoration and
reboot, `can-soak-20261020T064525Z-2a7ab3` passed overall and sequences over
30 seconds (30/150/300 records, RUN/warnings 0, numeric temperature/power).
This demonstrates restored healthy operation, not live loss/retry latency.
The subsequent combined deliberate sensor reset capture
`can-soak-20261020T064845Z-f7f95e` is expected REVIEW: one sensor restart/session
change per stream, data timeout assertion/clearing, brief missing power and
temperature, then numeric hardware readings and RUN/warnings 0. Dashboard
continued without an observed restart. See bench results for exact boundaries;
the physical EN time and live I2C retry latency were not measured.

The combined startup DHT OUT-open capture `can-soak-20261020T223544Z-99fa1b`
is expected REVIEW solely for missing temperature, with sequence PASS:
30/150/300 records, all conditions warning 64/NaN temperature, numeric power
3.328–3.332 V / 4.1–4.3 mA, no observed reset/timeout. This checks independence
in a steady startup fault, not a live loss/STALE transition. After power-off DHT
OUT restoration and reboot, `can-soak-20261020T224732Z-2168b2` passed overall
and sequences over 30 seconds: 30/150/300 records, RUN/warnings 0, numeric
temperature/power, no observed resets/timeouts or unavailable readings. These
separate windows do not establish live reconnection or recovery latency.
Earlier standalone/0.3.0 results are not retroactively relabeled 0.4.0 tests.

## Wiring and measured quantity

No new connections are required. Retain sensor DHT11 on GPIO4, INA219 logic at
3V3/GND/SDA21/SCL22, and CAN TX17/RX16 on the two standard ESP32s. The only INA219
measurement branch is sensor3V3 -> VIN+ -> internal R100 shunt -> VIN- -> measured
331-ohm resistor -> red LED -> sensorGND. Do not bypass the shunt or resistor.
Dashboard and sensor share ground, not their regulated 3V3 outputs.

Bus voltage is VIN- relative to sensorGND. Current is only the resistor-limited
LED branch, NOT whole-board/USB supply current or machine current. No battery,
external supply, motor, or high-energy measurement is within this build's scope.
The checked acquisition retains the standalone diagnostic's 16 V / 40 mV
profile, calibration 8192 for nominal R100, and software screening bounds of
0–5 V, +/-12 mA, and shunt strictly inside +/-40 mV. These limits are not a fuse,
current limiter, or permission to energize a different load. Power off both
boards before any wiring changes.

## Acquisition and validity

- A dedicated INA219 worker runs on classic ESP32 core 0, separately from the
  DHT task on core 0. The Arduino/CAN loop stays on core 1 and never calls Wire.
- Each acquisition writes known calibration and triggers a new paired 12-bit
  shunt/bus conversion, polls conversion-ready, checks all I2C operations and
  setting readbacks, and rejects overflow or out-of-bench-range values.
- Wire timeout is 25 ms per transaction; conversion-ready polling has a 50 ms
  budget. These are not a strict whole-acquisition deadline. There is a minimum
  100 ms pause AFTER acquisition, with no busy catch-up loop or exact 10 Hz
  sampling claim. CAN motion/power publication remains every 100 ms.
- A one-item overwrite queue transfers the latest checked result to the main
  loop. Only the main task prints serial diagnostics, avoiding worker/main
  output interleaving. This does not diagnose or prove correction of the earlier
  standalone garbled-text observation.
- Startup and recovery require two consecutive successful acquisitions, no
  more than 300 ms apart. The first successful sample remains unavailable.
- Any failed acquisition invalidates the worker state. A main-loop snapshot
  older than 300 ms is also unavailable, including when the worker stalls.
  There is NO retained numeric STALE power mode and NO simulated fallback.
  The old snapshot can remain available until the failure is observed or the
  age expires; this is not instantaneous physical fault detection.
- The main loop copies both worker states before reading millis, preventing a
  later worker timestamp from appearing to be in the future. Expiry uses
  unsigned millis subtraction. Current is rounded to the unchanged CAN 0.1 mA
  resolution (symmetric nearest, ties away from zero); signed near-zero current
  is retained to that resolution, not arbitrarily clamped positive.
- Missing INA219, DHT failure, or worker/queue creation failure does not stop
  the CAN publication loop. Task/queue creation errors are logged; the affected
  data stay unavailable. A failed Wire initialization remains unavailable until
  reboot; ordinary failed device reads are attempted again on following cycles.

## Wire and serial contract

Protocol version remains 1; extended CAN IDs, DLC 8, and existing byte offsets
are unchanged. Session/heartbeat source flags gain bit 2 (`4`) for INA219 mode:

| Flags | Channel modes |
|---:|---|
| 1 | Existing all-SIM target |
| 3 | Existing DHT11-only target; power remains SIM |
| 5 | Supported protocol combination: INA219 mode with SIM temperature |
| 7 | New combined DHT11/INA219 target |

Flags state enabled source mode, NOT sensor health or measurement accuracy.
Bit 0 remains set because condition percentage and motion are simulated.
Unknown bits and unsupported role/version metadata are rejected by the updated
host and dashboard decoders.

The motion/power bus uint16 reserves 65535 (`ff ff`), and current int16 reserves
-32768 (`00 80`), as a paired unavailable value. A half-missing pair is invalid.
Both real zeros remain valid numeric measurements. `quality=100` is unchanged
for the simulated motion channel; it is NOT proof of valid hardware power.
Missing power is self-labeled by the pair, independently of condition timing.

Condition warning bit 5 (`32`) means unavailable power and forces DEGRADED
(or FAULT when independently applicable). Temperature's warning bits 64/128
and its sentinel remain unchanged. Warning 96 at startup can mean both power
and temperature missing. Different frame periods mean warning changes and
power pairs need not arrive atomically; no cross-frame simultaneity is assumed.

Dashboard source comments identify `power=INA219`, and `#power_quality` reports
SIM, MISSING, or HW_VALID on transitions. HW_VALID means qualified/fresh checked
acquisition, not reference-certified accuracy. Missing bus/current print as
`nan,nan`, while simulated motion continues. DHT11 quality remains independent.
The schema banner is 0.4; the same final sequence/session columns and ten-column
normalized CSV layout are retained. Condition temperature stays third from
last in serial, and voltage/current precede the final sequence/session pair in
motion/power serial. No old capture file is rewritten.

Golden vectors (little-endian, spaced here for readability):

| Frame | Vector |
|---|---|
| Heartbeat, flags 7, seq 42, uptime 0x12345678 | `01 01 2a 07 78 56 34 12` |
| Session, boot 0x12345678, flags 7 | `01 01 78 56 34 12 07 00` |
| Motion/power, seq 9, quality 100, vibration 125, 3332 mV, 4.2 mA | `09 64 7d 00 04 0d 2a 00` |
| Same motion values, unavailable power | `09 64 7d 00 ff ff 00 80` |
| Condition, seq 0, DEGRADED, power warning 32, 90.00%, 24.0 C | `00 02 20 00 28 23 f0 00` |

## Build and deployment gates

Close the serial monitor; keep both USB supplies disconnected for the build.
Build the combined target AND the retained applications to screen regressions:

```powershell
platformio run -e esp32dev_sensor_sim -e esp32dev_sensor_dht11 -e esp32dev_sensor_dht11_ina219 -e esp32dev_dashboard
```

This command does not upload. After all builds succeed, separately identify
and upload the physical dashboard FIRST, with the sensor USB disconnected.
Then unplug dashboard USB and upload the combined image to the physical sensor
(the board with DHT/INA). Never infer a board role from a remembered port number;
the most recent sensor diagnostic was COM8, and dashboard has previously used
COM8 or COM12 depending on connection order.

An old 0.3 dashboard does not support INA mode flags or missing-power rendering
and can mislabel/print sentinel numbers. Do not pair it with the new sensor for
measurement or acceptance. The new dashboard supports prior defined SIM/DHT
formats; older captures remain sequence-unverified when metadata are absent.
The standalone measurement image does not publish CAN: it must be replaced by
the new combined application before any integrated test.

## Repeat healthy acceptance and finish remaining fault checks

The healthy 600-second gate below passed once on dashboard COM8 with both
hardware requirements and sequences enabled (600 / 2,999 / 5,999 records).
Its raw complete records contained no garbled text; the previous standalone
garbling cause remains unresolved. Physical source comments and a startup
version banner were not included in the supplied live window or timed capture;
source flags, values, and the preceding build/upload sequence provide the
recorded attribution. Do not represent this as a device-image attestation.
Step 3's startup SDA-open and power-off restoration cases are now recorded;
one deliberate combined sensor reset from step 4 is also recorded. Other
fault variants retain their limits; this is not an exhaustive fault campaign.

1. Confirm 0.4.0 DASHBOARD startup, flags 7/source labels, fresh hardware power,
   valid DHT after qualification, unchanged simulated channels, and plausible
   readings consistent with the earlier limited LED reference. Do not infer
   traceable calibration from approximate comparison.
2. After both channels qualify, close all monitors and make a healthy capture
   on the freshly identified dashboard port with no EN/BOOT/wiring changes:

   ```powershell
   py tools/capture_can.py --port COM12 --seconds 600 --require-hardware-temperature --require-hardware-power --require-sequences
   ```

   COM12 is only an example; run from the project directory. Require overall
   PASS and sequence PASS, flags 7, no unavailable power/temperature, resets,
   timeouts, malformed/unexpected/undecodable text, or observed sequence anomalies.
   Capture saves exact raw bytes, so terminal rendering and actual serial data
   can be investigated separately if the earlier garbled text recurs. Screening
   is not raw CAN error-free certification or an exact acquisition-period test.
3. Separately test INA219 SDA loss/restoration using power-off wire changes,
   recording warnings/NaNs and subsequent qualified recovery. Do NOT call this
   a live hot-plug latency test. Verify DHT and CAN availability independently;
   intentional missing data require REVIEW, never healthy-soak PASS.
4. Retain the existing DHT fault/reset/sequence tests and repeat relevant
   integration cases. Do not automatically carry forward standalone results
   or previous 0.3.0 timing evidence to the new combined image.

The capture parser accepts only paired power NaNs, records missing-power
samples and warning-32 condition records separately, and retains all fault
evidence after numeric recovery. `--require-hardware-temperature` accepts flags
3/7, `--require-hardware-power` accepts flags 5/7; both requirements together
force flags 7 throughout. Unexpected non-comment lines now require REVIEW.
Unrecognized comment lines remain retained but are not a comprehensive
integrity/authentication check. Every healthy pass remains bounded by the
sequence/boot-ID limitations in [sequence observability](SEQUENCES.md).

References: [TI INA219](https://www.ti.com/lit/ds/symlink/ina219.pdf),
[Espressif I2C](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/i2c.html),
[ESP-IDF FreeRTOS](https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32/api-reference/system/freertos.html).
