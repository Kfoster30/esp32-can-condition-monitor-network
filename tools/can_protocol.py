"""Host-side reference codec for the ESP32 CAN protocol."""

from dataclasses import dataclass
import struct

PAYLOAD_LENGTH = 8
PROTOCOL_VERSION = 1

ID_HEARTBEAT = 0x18FF0001
ID_CONDITION = 0x18FF1001
ID_MOTION_POWER = 0x18FF1101


@dataclass(frozen=True)
class Heartbeat:
    protocol_version: int
    role: int
    sequence: int
    flags: int
    uptime_seconds: int

    def encode(self) -> bytes:
        return struct.pack("<BBBBI", self.protocol_version, self.role,
                           self.sequence, self.flags, self.uptime_seconds)

    @classmethod
    def decode(cls, payload: bytes) -> "Heartbeat":
        _check_length(payload)
        return cls(*struct.unpack("<BBBBI", payload))


@dataclass(frozen=True)
class Condition:
    sequence: int
    state: int
    warnings: int
    faults: int
    condition_centi_percent: int
    temperature_deci_c: int

    def encode(self) -> bytes:
        if not 0 <= self.state <= 3:
            raise ValueError("state must be in the range 0..3")
        return struct.pack("<BBBBHh", self.sequence, self.state, self.warnings,
                           self.faults, self.condition_centi_percent,
                           self.temperature_deci_c)

    @classmethod
    def decode(cls, payload: bytes) -> "Condition":
        _check_length(payload)
        result = cls(*struct.unpack("<BBBBHh", payload))
        if not 0 <= result.state <= 3:
            raise ValueError("invalid system state")
        return result


@dataclass(frozen=True)
class MotionPower:
    sequence: int
    quality: int
    vibration_mg: int
    bus_millivolts: int
    current_deci_ma: int

    def encode(self) -> bytes:
        return struct.pack("<BBHHh", self.sequence, self.quality,
                           self.vibration_mg, self.bus_millivolts,
                           self.current_deci_ma)

    @classmethod
    def decode(cls, payload: bytes) -> "MotionPower":
        _check_length(payload)
        return cls(*struct.unpack("<BBHHh", payload))


def _check_length(payload: bytes) -> None:
    if len(payload) != PAYLOAD_LENGTH:
        raise ValueError(f"CAN payload must contain {PAYLOAD_LENGTH} bytes")

