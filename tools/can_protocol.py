"""Host-side reference codec for the ESP32 CAN protocol."""

from dataclasses import dataclass
import struct

PAYLOAD_LENGTH = 8
PROTOCOL_VERSION = 1

ID_HEARTBEAT = 0x18FF0001
ID_CONDITION = 0x18FF1001
ID_MOTION_POWER = 0x18FF1101
ID_SESSION = 0x18FEFF01

FLAG_ANY_SIMULATED = 0x01
FLAG_DHT_TEMPERATURE = 0x02
FLAG_INA_POWER = 0x04
FLAG_MPU_MOTION = 0x08
WARN_MOTION_MISSING = 0x10
WARN_POWER_MISSING = 0x20
MOTION_UNAVAILABLE = 65535
MOTION_QUALITY_MISSING = 0
MOTION_QUALITY_VALID = 100
BUS_UNAVAILABLE = 65535
CURRENT_UNAVAILABLE = -32768
WARN_TEMPERATURE_MISSING = 0x40
WARN_TEMPERATURE_STALE = 0x80
TEMPERATURE_UNAVAILABLE = -32768


def valid_source_flags(flags: int) -> bool:
    return flags in (1, 3, 5, 7, 15)


def _check_power(bus: int, current: int) -> None:
    if (bus == BUS_UNAVAILABLE) != (current == CURRENT_UNAVAILABLE):
        raise ValueError("Unavailable voltage/current must use paired sentinels")


def _check_channel_warnings(state: int, warnings: int) -> None:
    if warnings & (WARN_POWER_MISSING | WARN_MOTION_MISSING) and state not in (2, 3):
        raise ValueError("Missing power or motion must degrade condition state")


def _check_temperature(warnings: int, value: int) -> None:
    missing = bool(warnings & WARN_TEMPERATURE_MISSING)
    stale = bool(warnings & WARN_TEMPERATURE_STALE)
    if missing and (stale or value != TEMPERATURE_UNAVAILABLE):
        raise ValueError("missing temperature requires sentinel and must not be stale")
    if not missing and value == TEMPERATURE_UNAVAILABLE:
        raise ValueError("temperature sentinel requires missing warning")


@dataclass(frozen=True)
class Session:
    protocol_version: int
    role: int
    boot_id: int
    flags: int

    def encode(self) -> bytes:
        self._validate()
        return struct.pack("<BBIBB", self.protocol_version, self.role, self.boot_id, self.flags, 0)

    def _validate(self):
        if self.protocol_version != 1 or self.role != 1 or not valid_source_flags(self.flags):
            raise ValueError("Unsupported session metadata")
        if not 0 <= self.boot_id < 2**32:
            raise ValueError("Invalid boot identifier")

    @classmethod
    def decode(cls, payload: bytes) -> "Session":
        _check_length(payload)
        version, role, boot_id, flags, reserved = struct.unpack("<BBIBB", payload)
        if reserved != 0:
            raise ValueError("Session reserved byte must be zero")
        result = cls(version, role, boot_id, flags)
        result._validate()
        return result


@dataclass(frozen=True)
class Heartbeat:
    protocol_version: int
    role: int
    sequence: int
    flags: int
    uptime_seconds: int

    def encode(self) -> bytes:
        self._validate()
        return struct.pack("<BBBBI", self.protocol_version, self.role,
                           self.sequence, self.flags, self.uptime_seconds)

    @classmethod
    def decode(cls, payload: bytes) -> "Heartbeat":
        _check_length(payload)
        result = cls(*struct.unpack("<BBBBI", payload))
        result._validate()
        return result

    def _validate(self):
        if self.protocol_version != 1 or self.role != 1 or not valid_source_flags(self.flags):
            raise ValueError("Unsupported heartbeat metadata")


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
        _check_temperature(self.warnings, self.temperature_deci_c)
        _check_channel_warnings(self.state, self.warnings)
        return struct.pack("<BBBBHh", self.sequence, self.state, self.warnings,
                           self.faults, self.condition_centi_percent,
                           self.temperature_deci_c)

    @classmethod
    def decode(cls, payload: bytes) -> "Condition":
        _check_length(payload)
        result = cls(*struct.unpack("<BBBBHh", payload))
        if not 0 <= result.state <= 3:
            raise ValueError("invalid system state")
        _check_temperature(result.warnings, result.temperature_deci_c)
        _check_channel_warnings(result.state, result.warnings)
        return result


@dataclass(frozen=True)
class MotionPower:
    sequence: int
    quality: int
    vibration_mg: int
    bus_millivolts: int
    current_deci_ma: int

    def encode(self) -> bytes:
        _check_power(self.bus_millivolts, self.current_deci_ma)
        return struct.pack("<BBHHh", self.sequence, self.quality,
                           self.vibration_mg, self.bus_millivolts,
                           self.current_deci_ma)

    @classmethod
    def decode(cls, payload: bytes) -> "MotionPower":
        _check_length(payload)
        result = cls(*struct.unpack("<BBHHh", payload))
        _check_power(result.bus_millivolts, result.current_deci_ma)
        return result


def _check_length(payload: bytes) -> None:
    if len(payload) != PAYLOAD_LENGTH:
        raise ValueError(f"CAN payload must contain {PAYLOAD_LENGTH} bytes")

