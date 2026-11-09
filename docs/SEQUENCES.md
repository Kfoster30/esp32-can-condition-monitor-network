# Sequence observability (firmware 0.3.0)

## Status

All three environments built successfully in the operator's terminal; dashboard
and DHT11 sensor uploads were hash-verified. The 600.031-second capture
`can-soak-20261020T040511Z-8b0668` passed overall and sequence checks (BT-04),
with metadata on all 9,598 records and zero observed sequence anomalies.
A separate intentional sensor-reset capture correctly reported REVIEW and one
session change per stream, without fabricated reset-related sequence gaps.
See [bench results](../evidence/bench_results.md) for timing, artifacts, hashes,
source attribution, and limitations. Earlier 0.1/0.2 captures cannot establish
sequence continuity retroactively. Continuous live CAN-wire fault injection
remains unverified; the manual startup-open/restoration variant is separate.

No wiring or DHT11 pin changes are required. Both updated roles must compile
and flash. Update the dashboard first, then the DHT11 sensor, identifying the
physical board rather than assuming COM numbers are permanent.

## Why a session marker is added

Compatibility note: prepared 0.4.0 preserves this frame and the sequence/session
columns, adding source flag bit 2 for INA219 mode (flags 5/7). The updated
dashboard/host accept defined combinations 1/3/5/7. The old 0.3 dashboard must
be updated before pairing with an INA-enabled sensor; see [0.4.0 integration](INA219_CAN.md).

The original dashboard treated every unexpected byte as a modulo-256 gap.
That incorrectly inflated counts for duplicates, late records, and sensor resets.
The new dashboard exports the actual sequence byte and its attributed boot
session, and the host computes separate counters. Availability timeouts are still
maintained on the dashboard independently.

Sensor firmware generates one diagnostic 32-bit `esp_random()` boot ID during
setup and retains it until reset. A new control frame has extended CAN ID
`0x18FEFF01`, DLC 8, and this layout:

| Byte(s) | Field |
|---|---|
| 0 | Protocol version, 1 |
| 1 | Sensor role, 1 |
| 2–5 | Boot ID, little-endian uint32 |
| 6 | Source flags: 1 (SIM), 3 (DHT11); 0.4 adds 5 (INA219) / 7 (DHT11+INA219) |
| 7 | Reserved, must be zero |

The sensor queues a session marker before **every due publication batch**,
normally about every 100 ms, on the same transmit FIFO. If that marker cannot
be queued, it does not queue the batch's data. Queue success is not proof of
on-wire delivery or acknowledgement; the existing normal-mode CAN link still
requires its receiver. Existing three data-frame IDs/layouts remain unchanged.

The dashboard attributes data only while its last received marker is at most
250 ms old. Otherwise it prints `UNKNOWN` rather than guessing an epoch. This
age is below the sensor firmware's fixed 500 ms setup delay, so an old session
normally expires across a reset. New IDs cause a `#session,...,CHANGED` notice;
the first observed ID is `INITIAL` and is not counted as a restart.

Boot IDs are diagnostic, not authenticated, guaranteed unique, or suitable for
security. Without RF enabled, `esp_random()` must not be assumed cryptographically
random. Collisions and lost/reordered control frames remain limitations. This
design assumes one ordered publisher on the known sensor IDs, not a hostile or
multi-sensor bus. Metadata is a bounded observation, not proof of all CAN traffic.

## Serial and CSV formats

0.3.0 appends **sequence,session** to each existing telemetry line:

```text
heartbeat,12345,1,1,3,12,11,305419896
condition,12345,RUN,0,0,90.00,24.0,60,305419896
motion_power,12345,100,125,3.300,4.2,122,305419896
```

Before attribution is available the final field is literally `UNKNOWN`.
Session/source notices are comment lines and are retained in `raw.log`.

**Temperature is now the third-from-last value on a condition line**, not the
last value as in 0.1/0.2. The normalized capture CSV has ten columns:

```text
host_elapsed_s,type,device_t_ms,field_1,field_2,field_3,field_4,field_5,sequence,session
```

`field_5` is condition temperature; shorter heartbeat/motion records have that
slot padded blank. Legacy serial formats still parse, with blank sequence/session
CSV columns. No old capture file is rewritten.

## Host interpretation

Analysis is per stream and per observed session. The first record establishes
an arbitrary baseline; no loss before capture start is inferred.

| Observation | Classification and action |
|---|---|
| Next byte modulo 256 | In order, advance anchor; 255 to 0 is normal |
| Forward byte distance 2–127 | Infer distance minus one missed publication sequences; advance |
| Same byte | Duplicate; do not move anchor |
| Backward half-range distance (129–255 modulo) | Late/out of order; do not move anchor |
| Exactly half-range jump (128) | Ambiguous, rebaseline without inventing a loss count |
| Silence of at least 128 nominal periods | Ambiguous, rebaseline even if byte looks sequential |
| New boot-session ID | Count session change separately; establish new baseline, no gap |
| Unknown boot-session attribution | Count unattributed record; invalidate anchor |
| Dashboard timestamp restart | Invalidate anchors; retain the separate restart indicator |

Sequence counters increment when the sensor attempts to publish, including
failed enqueue attempts. Therefore a reported skip is an inferred **publication
sequence discontinuity**, not proof of a particular number of physical bus-error
events or a way to distinguish transmit rejection, receive loss, and host serial
loss. An 8-bit counter cannot resolve arbitrary delays/reordering, or exact loss
across multiple wraps. The analyzer explicitly reports some ambiguities but
does not claim to resolve all possible aliasing or boot-ID collisions.

For a stability run, any inferred skips, duplicates, late records, ambiguity,
unknown attribution, or session changes produce REVIEW, each reported separately.
The sequence subsection reports PASS only if all three streams have metadata
coverage and no observed anomalies. Legacy-only data remain UNVERIFIED; they
may still pass the older timing checks unless `--require-sequences` is requested.

## Repeat the bench check

After successful builds/uploads, both nodes operating, DHT11 qualified, and serial
monitors closed, run from the project directory (adjust dashboard port if needed):

```powershell
py tools/capture_can.py --port COM12 --seconds 600 --require-hardware-temperature --require-sequences
```

Expect both overall PASS and sequence PASS, with zero missing/duplicate/late/
ambiguous/unattributed/session-change counters. Normal wraps are expected and
must not cause a failure. A separate deliberate sensor reset test should show
a session change/rebaseline rather than hundreds of fabricated gaps; that run
is intentionally not a healthy stability PASS. Source and temperature validity,
time continuity, and sequence coverage are independent evidence.

References: [Espressif TWAI](https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32/api-reference/peripherals/twai.html),
[Espressif random-number limitations](https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32/api-reference/system/random.html).
