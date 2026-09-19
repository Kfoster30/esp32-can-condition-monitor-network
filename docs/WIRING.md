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

## Connections for each ECU

| SN65HVD230 module | ESP32 |
|---|---|
| VCC / 3V3 | 3V3 |
| GND | GND |
| CTX / TXD | GPIO17 |
| CRX / RXD | GPIO16 |

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

