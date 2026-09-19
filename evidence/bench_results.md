# Bench Results

## BT-01 — CAN termination inspection

| Field | Result |
|---|---|
| Date | 2026-09-19 |
| Equipment | Digital multimeter |
| Module 1, CANH to CANL | 123 ohms |
| Module 2, CANH to CANL | 123 ohms |
| Interpretation | Each module has onboard termination |
| Predicted assembled-bus resistance | 61.5 ohms |
| External termination required | No |
| Status | Partial pass; verify assembled bus before power-up |

Final BT-01 acceptance requires measuring approximately 60–62 ohms between
CANH and CANL after the complete two-node bus is wired and while both ESP32s
remain unpowered.
