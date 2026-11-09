#pragma once

#include <stddef.h>
#include <stdint.h>

namespace protocol {

constexpr size_t kPayloadLength = 8;

// Heartbeat bit 0 means at least one channel is simulated, not necessarily all.
constexpr uint8_t kFlagAnySimulated = 0x01;
constexpr uint8_t kFlagDhtTemperature = 0x02;
constexpr uint8_t kFlagInaPower = 0x04;
constexpr uint8_t kFlagMpuMotion = 0x08;
constexpr uint8_t kWarnMotionMissing = 0x10;
constexpr uint16_t kMotionUnavailable = 0xFFFF;
constexpr uint8_t kMotionQualityMissing = 0;
constexpr uint8_t kMotionQualityValid = 100;
constexpr uint8_t kWarnPowerMissing = 0x20;
constexpr uint16_t kBusUnavailable = 0xFFFF;
constexpr int16_t kCurrentUnavailable = -32768;

constexpr bool validSourceFlags(uint8_t flags) {
  // This revision enables hardware motion only in the combined sensor build.
  return flags == (kFlagAnySimulated | kFlagDhtTemperature | kFlagInaPower | kFlagMpuMotion) ||
         ((flags & kFlagAnySimulated) != 0 && (flags & 0xF8u) == 0);
}

constexpr bool validHardwareMotionEncoding(uint8_t quality, uint16_t motionMg) {
  return motionMg == kMotionUnavailable ? quality == kMotionQualityMissing
                                       : quality == kMotionQualityValid;
}

constexpr bool validPowerEncoding(uint16_t bus, int16_t current) {
  return (bus == kBusUnavailable) == (current == kCurrentUnavailable);
}

constexpr bool powerMissing(uint16_t bus, int16_t current) {
  return bus == kBusUnavailable && current == kCurrentUnavailable;
}
// Reserved condition-warning bits describe temperature data quality, not a
// temperature-limit alarm. Missing values must carry the int16 sentinel.
constexpr uint8_t kWarnTemperatureMissing = 0x40;
constexpr uint8_t kWarnTemperatureStale = 0x80;
constexpr int16_t kTemperatureUnavailable = -32768;

constexpr bool validTemperatureEncoding(uint8_t warnings, int16_t value) {
  return (warnings & kWarnTemperatureMissing)
      ? (!(warnings & kWarnTemperatureStale) && value == kTemperatureUnavailable)
      : value != kTemperatureUnavailable;
}

constexpr bool sessionFresh(bool known, uint32_t lastMs, uint32_t now, uint32_t limitMs) {
  return known && static_cast<uint32_t>(now - lastMs) <= limitMs;
}

enum class SystemState : uint8_t {
  kSelfTest = 0,
  kRun = 1,
  kDegraded = 2,
  kFault = 3,
};

struct Heartbeat {
  uint8_t protocolVersion;
  uint8_t role;
  uint8_t sequence;
  uint8_t flags;
  uint32_t uptimeSeconds;
};

// Repeated before each due publication batch. No sequence counter is needed on
// this control frame: bootId identifies the epoch of the following data frames.
struct Session {
  uint8_t protocolVersion;
  uint8_t role;
  uint32_t bootId;
  uint8_t flags;
};

struct Condition {
  uint8_t sequence;
  SystemState state;
  uint8_t warnings;
  uint8_t faults;
  uint16_t conditionCentiPercent;
  int16_t temperatureDeciC;
};

struct MotionPower {
  uint8_t sequence;
  uint8_t quality;
  uint16_t vibrationMg;
  uint16_t busMillivolts;
  int16_t currentDeciMa;
};

inline void putU16(uint8_t* dst, uint16_t value) {
  dst[0] = static_cast<uint8_t>(value & 0xFFu);
  dst[1] = static_cast<uint8_t>((value >> 8u) & 0xFFu);
}

inline void putI16(uint8_t* dst, int16_t value) {
  putU16(dst, static_cast<uint16_t>(value));
}

inline void putU32(uint8_t* dst, uint32_t value) {
  dst[0] = static_cast<uint8_t>(value & 0xFFu);
  dst[1] = static_cast<uint8_t>((value >> 8u) & 0xFFu);
  dst[2] = static_cast<uint8_t>((value >> 16u) & 0xFFu);
  dst[3] = static_cast<uint8_t>((value >> 24u) & 0xFFu);
}

inline uint16_t getU16(const uint8_t* src) {
  return static_cast<uint16_t>(src[0]) |
         (static_cast<uint16_t>(src[1]) << 8u);
}

inline int16_t getI16(const uint8_t* src) {
  return static_cast<int16_t>(getU16(src));
}

inline uint32_t getU32(const uint8_t* src) {
  return static_cast<uint32_t>(src[0]) |
         (static_cast<uint32_t>(src[1]) << 8u) |
         (static_cast<uint32_t>(src[2]) << 16u) |
         (static_cast<uint32_t>(src[3]) << 24u);
}

inline void encode(const Heartbeat& value, uint8_t* out) {
  out[0] = value.protocolVersion;
  out[1] = value.role;
  out[2] = value.sequence;
  out[3] = value.flags;
  putU32(&out[4], value.uptimeSeconds);
}

inline void encode(const Session& value, uint8_t* out) {
  out[0] = value.protocolVersion;
  out[1] = value.role;
  putU32(&out[2], value.bootId);
  out[6] = value.flags;
  out[7] = 0;
}

inline void encode(const Condition& value, uint8_t* out) {
  out[0] = value.sequence;
  out[1] = static_cast<uint8_t>(value.state);
  out[2] = value.warnings;
  out[3] = value.faults;
  putU16(&out[4], value.conditionCentiPercent);
  putI16(&out[6], value.temperatureDeciC);
}

inline void encode(const MotionPower& value, uint8_t* out) {
  out[0] = value.sequence;
  out[1] = value.quality;
  putU16(&out[2], value.vibrationMg);
  putU16(&out[4], value.busMillivolts);
  putI16(&out[6], value.currentDeciMa);
}

inline bool decode(const uint8_t* data, size_t length, Heartbeat& out) {
  if (length != kPayloadLength || data[0] != 1 || data[1] != 1 ||
      !validSourceFlags(data[3])) return false;
  out = {data[0], data[1], data[2], data[3], getU32(&data[4])};
  return true;
}

inline bool decode(const uint8_t* data, size_t length, Session& out) {
  if (length != kPayloadLength || data[0] != 1 || data[1] != 1 ||
      !validSourceFlags(data[6]) || data[7] != 0) return false;
  out = {data[0], data[1], getU32(&data[2]), data[6]};
  return true;
}

inline bool decode(const uint8_t* data, size_t length, Condition& out) {
  if (length != kPayloadLength || data[1] > 3 ||
      ((data[2] & (kWarnPowerMissing | kWarnMotionMissing)) && data[1] != 2 && data[1] != 3) ||
      !validTemperatureEncoding(data[2], getI16(&data[6]))) return false;
  out.sequence = data[0];
  out.state = static_cast<SystemState>(data[1]);
  out.warnings = data[2];
  out.faults = data[3];
  out.conditionCentiPercent = getU16(&data[4]);
  out.temperatureDeciC = getI16(&data[6]);
  return true;
}

inline bool decode(const uint8_t* data, size_t length, MotionPower& out) {
  if (length != kPayloadLength ||
      !validPowerEncoding(getU16(&data[4]), getI16(&data[6]))) return false;
  out.sequence = data[0];
  out.quality = data[1];
  out.vibrationMg = getU16(&data[2]);
  out.busMillivolts = getU16(&data[4]);
  out.currentDeciMa = getI16(&data[6]);
  return true;
}

}  // namespace protocol
