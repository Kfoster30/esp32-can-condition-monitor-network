# Requirements

## Functional requirements

| ID | Requirement | Verification |
|---|---|---|
| FR-01 | The sensor ECU shall transmit extended 29-bit CAN frames at 500 kbit/s. | Bench test |
| FR-02 | The sensor ECU shall publish a heartbeat at least once per second. | Log analysis |
| FR-03 | The sensor ECU shall publish condition data every 200 ms. | Log analysis |
| FR-04 | The sensor ECU shall publish motion/power data every 100 ms. | Log analysis |
| FR-05 | Every payload shall be exactly eight bytes and little-endian. | Host tests |
| FR-06 | The dashboard ECU shall decode all three defined message types. | Bench test |
| FR-07 | The dashboard ECU shall count unexpected sequence changes. | Fault injection |
| FR-08 | The dashboard ECU shall report a heartbeat timeout after 2.5 seconds. | Disconnect test |
| FR-09 | The dashboard ECU shall report stale data after 1.0 second. | Disconnect test |
| FR-10 | Dashboard output shall be machine-readable CSV-style telemetry. | Inspection |

## Quality and safety requirements

| ID | Requirement | Verification |
|---|---|---|
| QR-01 | Golden-vector tests shall define the byte-level wire format. | Automated test |
| QR-02 | Sensor and dashboard firmware shall compile independently. | Build |
| QR-03 | The bus shall have two 120-ohm end terminators. | Resistance measurement |
| QR-04 | SN65HVD230 modules shall be powered from 3.3 V. | Wiring inspection |
| QR-05 | Project 1 shall not be modified during this milestone. | Repository review |

