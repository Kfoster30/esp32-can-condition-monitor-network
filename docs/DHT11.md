# First hardware channel: DHT11 temperature

## Scope and validation status

Firmware 0.2.0 adds `esp32dev_sensor_dht11`. Only temperature comes from hardware;
condition percentage, vibration, bus voltage, and current remain simulated.
The all-simulated build remains available. Project 1 files are not modified.

Host codec/capture tests pass, and operator screenshots confirm successful
builds/uploads and first qualified DHT11 temperature reception over CAN. See
[bench results](../evidence/bench_results.md). A new 600-second mixed-source
capture passed with source flags 3, zero stale/missing temperatures, and the
expected 1/5/10 Hz telemetry rates. Manual screenshots also confirm OUT data
loss produces `DEGRADED/64/nan` and reconnection restores `RUN/0` with numeric
hardware temperature while other CAN telemetry continues. The intermediate
STALE stage, exact transition timing, reference comparison, and controlled
thermal-response validation remain pending. The earlier 0.1.0
all-simulated PASS is retained separately from this new result.

Firmware 0.3.0 additionally passed a fresh 600-second mixed-source capture with
sequence/session metadata and no stale/missing temperatures. A separate sensor
reset produced explicit missing temperature during startup, then returned to
`HW_VALID` / `RUN` at 24.0 C, 4.200 seconds after the first post-reset condition
record. That deliberate-interruption capture correctly remains REVIEW, not a
healthy soak PASS. See the 0.3.0 sections in the bench results for evidence and
limits; reference accuracy and intermediate STALE-stage timing remain unverified.

## Power-off wiring

The operator's photographed module is a three-pin DHT11 breakout with an installed
header. Facing the blue vented sensor with the pins pointing down, its labels are:

| DHT11 label | Sensor ESP32 only | Current wire color |
|---|---|---|
| Left `+` | 3V3 | Green |
| Center `out` | D4 / GPIO4 | Red |
| Right `-` | GND | Orange |

Keep both USB supplies disconnected while changing wiring. Share the sensor
ESP32's 3V3/GND with its Waveshare module using correctly connected breadboard
holes; never connect the two ESP32 3V3 outputs together. Leave CAN TX/RX on
GPIO17/GPIO16 and keep the separate common-ground link. Do not supply this module
from 5 V for this direct-ESP32 wiring.

The photograph/description does not verify hidden breadboard contacts. Before
powering, check the DHT `+` to sensor 3V3, `-` to sensor GND, and `out` to GPIO4
paths with both boards unpowered. Do not rely on wire color alone.

## Build and deployment sequence

First build without touching either board:

```powershell
platformio run -e esp32dev_sensor_sim -e esp32dev_dashboard -e esp32dev_sensor_dht11
```

Only after successful builds and a wiring check:

1. Upload `esp32dev_dashboard` 0.2.0 to the physical dashboard.
2. Upload `esp32dev_sensor_dht11` 0.2.0 to the physical sensor.
3. Re-identify ports. Previously COM8 was sensor and COM12 dashboard, but that
   assignment may change. Never assume a port identifies a role permanently.
4. Start both boards and monitor the dashboard at 115200 baud.
5. Allow at least 5–6 seconds for two valid reads, sampled every two seconds.

Update the dashboard first. An old 0.1.0 dashboard does not translate the missing
temperature sentinel to `nan` and may print a misleading -3276.8 C value. No
upload command is included here until physical board identity is confirmed.

## Quality and source contract

CAN IDs, DLC, field offsets, and protocol version 1 stay unchanged. Firmware
version advances to 0.2.0. Newly assigned bits and the reserved int16 sentinel are:

| Location | Value | Meaning |
|---|---:|---|
| Heartbeat flags bit 0 | 1 | At least one channel is simulated |
| Heartbeat flags bit 1 | 2 | DHT11 temperature hardware mode enabled |
| Condition warnings bit 6 | 64 | Temperature unavailable |
| Condition warnings bit 7 | 128 | Retained temperature is stale |
| Condition temperature int16 | -32768 (`00 80` little-endian) | Unavailable; dashboard prints `nan` |

Heartbeat flags are 1 in all-simulated mode and 3 in this mixed mode. Mode flag 3
alone does not prove sensor health: inspect condition warnings and temperature.
The two temperature-quality warning bits must not both be set. A missing warning
requires the sentinel; the sentinel without the missing warning is invalid.

- Startup/qualification: temperature `nan`, warning 64, state `DEGRADED`.
- Two consecutive finite readings within the 0–50 C DHT11 screening envelope:
  measured temperature, warning 0, state `RUN`.
- First failed read after qualification: preserve the last qualified value,
  warning 128, state `DEGRADED`.
- Last qualified sample older than five seconds: `nan`, warning 64.
- Recovery requires two consecutive valid reads again. No synthetic temperature
  fallback is allowed in the hardware build.

These bits describe data quality, not an over-temperature protection function.
The slow/coarse DHT11 is for an ambient-temperature demonstration, not machinery
thermal protection or a calibrated accuracy claim. The other fields remain SIM.

Source annotations arrive in comment lines, for example:

```text
#sources,condition=SIM,temperature=DHT11,motion=SIM,power=SIM
#temperature_quality,HW_VALID
condition,12345,RUN,0,0,90.00,24.0
```

The final condition column is temperature in C. `#temperature_quality` prints
on changes, not for every frame; condition warning bits accompany every sample.
Before the first heartbeat, the dashboard has not yet identified source mode.
The unchanged `simulation=1` sensor banner means simulated channels remain, not
that the DHT temperature is synthetic.

Format note: the examples above describe 0.2.0. Firmware 0.3.0 appends sequence
and session columns, so condition temperature is then **third from last** in
live serial output. Normalized capture CSV keeps temperature in `field_5`.
See [sequence observability](SEQUENCES.md); old evidence is retained unchanged.

## Scheduling

The pinned Adafruit DHT 1.4.7 read includes a 20 ms start pulse plus a timing-
sensitive transaction. It runs in a FreeRTOS task on classic ESP32 core 0, while
the Arduino/CAN loop runs on core 1. A one-item overwrite queue transfers the
latest qualified state without blocking the publisher. The first read follows a
two-second warm-up. If queue/task creation fails, the firmware emits an error
and continues to transmit explicitly missing temperature rather than fake data.

This isolates the blocking read; it does not substitute for a physical timing
test. Compile-time assertions exercise the actual C++ qualification, expiry,
recovery, sentinel, and millis-rollover functions in every firmware build.

## Bench acceptance and remaining checks

1. Confirm the measured room temperature becomes finite with warning 0 after
   qualification; compare with a separate thermometer if one is available.
2. Gently warm the sensor with a hand held near it; do not use a flame, heat gun,
   or liquid. Observe the remote dashboard response at the sensor's slow rate.
3. Verify DHT data-loss handling with a prepared safe data-line break. Keep its
   power and the CAN bus intact. Observe stale then missing, followed by two-read
   recovery. Never move power wires while either board is powered.
4. After restoring valid readings, close serial monitors and capture a new run:

   ```powershell
   py tools/capture_can.py --port COM12 --seconds 600 --require-hardware-temperature
   ```

   Run from the project directory and replace COM12 if mapping has changed.
   Start only after qualification. The report must observe heartbeat flags 3
   throughout and zero stale/missing records, in addition to existing timing
   and communication checks. For current 0.3.0 deployments, also add
   `--require-sequences` to screen decoded telemetry continuity. This does not
   prove zero raw CAN frame loss; older 0.2.0 captures remain sequence-unverified.

References: [Adafruit DHT library 1.4.7](https://github.com/adafruit/DHT-sensor-library/blob/1.4.7/DHT.cpp),
[ESP-IDF FreeRTOS](https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32/api-reference/system/freertos.html).
