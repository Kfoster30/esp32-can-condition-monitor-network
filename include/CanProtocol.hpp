#pragma once

#include <Arduino.h>

namespace protocol {

constexpr size_t kPayloadLength = 8;

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
  if (length != kPayloadLength) return false;
  out = {data[0], data[1], data[2], data[3], getU32(&data[4])};
  return true;
}

inline bool decode(const uint8_t* data, size_t length, Condition& out) {
  if (length != kPayloadLength || data[1] > 3) return false;
  out.sequence = data[0];
  out.state = static_cast<SystemState>(data[1]);
  out.warnings = data[2];
  out.faults = data[3];
  out.conditionCentiPercent = getU16(&data[4]);
  out.temperatureDeciC = getI16(&data[6]);
  return true;
}

inline bool decode(const uint8_t* data, size_t length, MotionPower& out) {
  if (length != kPayloadLength) return false;
  out.sequence = data[0];
  out.quality = data[1];
  out.vibrationMg = getU16(&data[2]);
  out.busMillivolts = getU16(&data[4]);
  out.currentDeciMa = getI16(&data[6]);
  return true;
}

}  // namespace protocol
