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

Store captured logs and photographs in a future `evidence/` directory. Record
the firmware commit, board role, date, test ID, and result for every run.

