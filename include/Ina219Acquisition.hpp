#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "Ina219Diagnostic.hpp"
#include "SensorI2c.hpp"

// Checked, blocking acquisition for the worker task only. The original
// standalone diagnostics remain unchanged as independently tested references.
namespace ina219 {
enum class Result : uint8_t { kSample, kInitFailed, kWriteFailed, kReadFailed,
                             kTimeout, kSettings, kOverflow, kRange };
struct Sample {
  Result result;
  uint16_t millivolts;
  int32_t microamps;
};

inline const char* name(Result value) {
  switch (value) {
    case Result::kSample: return "SAMPLE";
    case Result::kInitFailed: return "I2C_INIT_FAILED";
    case Result::kWriteFailed: return "WRITE_FAILED";
    case Result::kReadFailed: return "READ_FAILED";
    case Result::kTimeout: return "CONVERSION_TIMEOUT";
    case Result::kSettings: return "SETTINGS_MISMATCH";
    case Result::kOverflow: return "OVERFLOW";
    case Result::kRange: return "OUT_OF_BENCH_RANGE";
  }
  return "UNKNOWN";
}

class Reader {
 public:
  explicit Reader(sensor_i2c::Bus& bus) : bus_(bus) {}

  Sample sample() {
    if (!bus_.ready()) return failure(Result::kInitFailed);
    if (!write(5, ina219diag::kCalibration) || !write(0, ina219diag::kConfiguration))
      return failure(Result::kWriteFailed);
    uint16_t bus = 0;
    const uint32_t started = millis();
    do {
      delay(2);
      if (!read(2, bus)) return failure(Result::kReadFailed);
      if (ina219diag::conversionReady(bus)) break;
    } while (millis() - started < ina219diag::kConversionTimeoutMs);
    if (!ina219diag::conversionReady(bus)) return failure(Result::kTimeout);
    uint16_t shunt = 0, current = 0, configuration = 0, calibration = 0;
    if (!read(1, shunt) || !read(4, current) || !read(0, configuration) || !read(5, calibration))
      return failure(Result::kReadFailed);
    switch (ina219diag::classify(bus, current, shunt, configuration, calibration)) {
      case ina219diag::Status::kNotReady: return failure(Result::kTimeout);
      case ina219diag::Status::kOverflow: return failure(Result::kOverflow);
      case ina219diag::Status::kSettingsMismatch: return failure(Result::kSettings);
      case ina219diag::Status::kRange: return failure(Result::kRange);
      case ina219diag::Status::kSample: break;
    }
    return {Result::kSample, ina219diag::busMillivolts(bus), ina219diag::currentMicroamps(current)};
  }

 private:
  sensor_i2c::Bus& bus_;
  static constexpr uint8_t kAddress = 0x40;

  static Sample failure(Result reason) { return {reason, 0, 0}; }

  bool write(uint8_t reg, uint16_t value) {
    const sensor_i2c::Lock lock(bus_);
    Wire.beginTransmission(kAddress);
    const size_t pointer = Wire.write(reg);
    const size_t high = Wire.write(static_cast<uint8_t>(value >> 8));
    const size_t low = Wire.write(static_cast<uint8_t>(value & 0xFFu));
    const uint8_t error = Wire.endTransmission(true);
    return pointer == 1 && high == 1 && low == 1 && error == 0;
  }

  bool read(uint8_t reg, uint16_t& value) {
    const sensor_i2c::Lock lock(bus_);
    Wire.beginTransmission(kAddress);
    const size_t written = Wire.write(reg);
    const uint8_t error = Wire.endTransmission(true);
    if (written != 1 || error != 0) return false;
    const size_t received = Wire.requestFrom(kAddress, static_cast<size_t>(2), true);
    if (received != 2 || Wire.available() < 2) {
      while (Wire.available() > 0) Wire.read();
      return false;
    }
    const int high = Wire.read(), low = Wire.read();
    if (high < 0 || low < 0) return false;
    value = static_cast<uint16_t>((static_cast<uint16_t>(high) << 8) | low);
    return true;
  }
};
}  // namespace ina219
