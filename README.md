# ESP32 CAN Condition-Monitor Network

A two-node embedded network that moves machine-condition data between ESP32
controllers over a physical CAN bus. The first milestone uses deterministic
simulated sensor data so the communication, fault detection, and test process
can be proven before Project 1 hardware is integrated.

## Why this project matters

This project adds vehicle-networking experience to the sensing, telemetry, and
fault-management work demonstrated in the ESP32 Machine Condition-Monitoring
ECU. It is designed to show:

- ESP32 TWAI/CAN controller configuration at 500 kbit/s
- 29-bit CAN identifiers and compact, versioned binary messages
- Two independently flashed ECU roles
- Heartbeat supervision, receive timeouts, and sequence-gap detection
- Host-side golden-vector tests for the on-wire protocol
- A documented bench-validation process

The identifier layout is J1939-inspired, but this milestone does **not** claim
full SAE J1939 compliance. J1939 PGNs and source addressing are a later phase.

## Architecture

```text
ESP32 Sensor ECU                 ESP32 Dashboard ECU
  deterministic data              decode + CSV telemetry
  heartbeat/condition   CAN        sequence-gap checks
  motion/power frames  =====>      heartbeat/data timeouts
        |                                  |
   SN65HVD230                         SN65HVD230
        +----------- twisted pair --------+
```

## Current milestone

- `esp32dev_sensor_sim` publishes three extended CAN frames.
- `esp32dev_dashboard` receives and validates them, then prints decoded CSV.
- Python tests independently verify the exact eight bytes used by every frame.
- Physical wiring and bench tests are documented but should start only after
  the termination resistance is confirmed with power disconnected.

## Quick start

1. Install Visual Studio Code, PlatformIO, and Python 3.
2. Run the host protocol tests:

   ```powershell
   py -m unittest discover -s tools -p "test_*.py" -v
   ```

3. Build each ECU role:

   ```powershell
   platformio run -e esp32dev_sensor_sim
   platformio run -e esp32dev_dashboard
   ```

4. Read [the wiring guide](docs/WIRING.md) before connecting either CAN module.
5. Flash one environment to each ESP32, connect the terminated CAN bus, and
   monitor the dashboard ESP32 at 115200 baud.

## CAN interface

| Setting | Value |
|---|---:|
| Bit rate | 500 kbit/s |
| ESP32 TX | GPIO17 |
| ESP32 RX | GPIO16 |
| Frame format | Extended, 29-bit |
| Payload length | 8 bytes |

| Identifier | Name | Period |
|---:|---|---:|
| `0x18FF0001` | Heartbeat | 1000 ms |
| `0x18FF1001` | Condition | 200 ms |
| `0x18FF1101` | Motion/power | 100 ms |

See [requirements](docs/REQUIREMENTS.md), [project plan](docs/PROJECT_PLAN.md),
and [test plan](docs/TEST_PLAN.md) for the acceptance criteria and roadmap.

## Safety note

The SN65HVD230 is a 3.3 V transceiver. Do not connect its VCC to 5 V. Power off
both boards before changing wiring or measuring bus resistance. This is a bench
learning project, not safety-rated automotive hardware.

## License

No license has been selected yet. All rights are reserved by the author.

