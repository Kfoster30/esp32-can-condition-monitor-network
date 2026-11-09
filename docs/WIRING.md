# Bench Wiring

## Stop before wiring

Do not power either ESP32 yet. First determine whether each SN65HVD230 module
already has a 120-ohm resistor connected across CANH and CANL. Some versions do
and some do not.

With a module completely disconnected, measure resistance directly between its
CANH and CANL terminals:

- About 120 ohms: that module includes termination.
- Open circuit or a much higher value: it does not include termination.

The finished two-node bus needs exactly two 120-ohm terminators, one at each
physical end. With both boards unpowered and the complete bus connected, the
resistance measured between CANH and CANL should be approximately 60 ohms.

## Verified modules

On 2026-09-19, both selected SN65HVD230 modules measured approximately 123 ohms
between CANH and CANL while disconnected. Both modules therefore contain
onboard termination. No external 120-ohm resistors should be added to this
two-node bench bus. The predicted complete-bus resistance is approximately
61.5 ohms.

## Connections for each ECU

| SN65HVD230 module | ESP32 |
|---|---|
| VCC / 3V3 | 3V3 |
| GND | GND |
| CTX / TXD | GPIO17, commonly labeled `TX2` |
| CRX / RXD | GPIO16, commonly labeled `RX2` |

Many ESP32 DevKit boards do not use Arduino-style `D16` and `D17` labels.
`RX2` is GPIO16 and `TX2` is GPIO17; these labels identify the same physical
pins. The firmware uses them as CAN controller signals rather than as a UART.

Between modules:

| Module 1 | Module 2 |
|---|---|
| CANH | CANH |
| CANL | CANL |
| GND | GND |

Use the black/white twisted pair only for CANH and CANL. Choose one color for
CANH and keep that choice at both ends. A separate conductor provides the
common ground between boards.

## First power-up checklist

1. Both USB cables disconnected.
2. No continuity between 3.3 V and GND.
3. About 60 ohms between CANH and CANL on the completed bus.
4. VCC is connected to 3.3 V, never 5 V.
5. CANH and CANL are not reversed.
6. Both boards share ground.
7. Cable is short for the initial bench test.

After these checks, power the dashboard ECU first and the sensor ECU second.

## Optional first hardware channel

The 0.2.0 DHT11 build adds temperature on the **sensor** ESP32's GPIO4 without
changing CAN wiring. The photographed three-pin module is labeled `+`, `out`,
`-` and uses sensor 3V3, GPIO4, GND respectively. See [DHT11 integration](DHT11.md)
for power-off wiring, source/quality labeling, firmware deployment order, and
the still-pending hardware acceptance tests.

## Combined DHT11/INA219 application

Firmware 0.4.0's `esp32dev_sensor_dht11_ina219` uses the same tested logic/load
wiring from the [INA219 diagnostic stages](INA219.md). No new connections are
required: INA VCC/GND to sensor3V3/GND, SDA21, SCL22, DHT data4, CAN TX17/RX16.
VIN+ connects to sensor3V3; VIN- feeds only the measured 331-ohm resistor/red-LED
branch to sensorGND. Do not bypass its resistor or use an external supply. Only
ground is shared with dashboard; do not tie the two regulated 3V3 outputs.
See [combined integration](INA219_CAN.md) for deployment gates, the completed
healthy 600-second CAN soak, and remaining integrated fault checks. Keep both
boards unplugged before changing any wiring.

## MPU6050-compatible communication stage

The operator has reused the dismantled Project 1 HW-123 module and moved sensor
3V3/GND distribution onto separate breadboard rails. MPU VCC/GND use those
sensor-fed rails, SDA21/SCL22 share the INA219 signal bus, AD0 is grounded, and
XDA/XCL/INT remain unused. These connections are operator-reported; photo
overlap and an OL resistance reading are not a complete wiring/voltage audit.
See [MPU6050 stage](MPU6050.md) for pin mapping, evidence boundaries, and the
temporary read-only-register diagnostic. Do not change dashboard3V3 wiring or
power either board during physical changes. Normal firmware motion stays SIM.
