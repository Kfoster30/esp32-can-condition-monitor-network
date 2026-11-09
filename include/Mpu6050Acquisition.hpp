#pragma once

#include <math.h>
#include <string.h>

#include "Mpu6050Diagnostic.hpp"
#include "SensorI2c.hpp"

namespace mpu6050 {
struct Window {
  uint16_t sampleCount;
  uint32_t spanMs;
  float rmsMg;
  bool recoveredRead;
};

class Reader {
 public:
  explicit Reader(sensor_i2c::Bus& bus) : bus_(bus) {}

  bool configured() const { return configured_; }

  void handleFailure(const char* reason) {
    if (strcmp(reason, "NEAR_FULL_SCALE") != 0 &&
        strcmp(reason, "SAMPLE_TIMING") != 0) {
      configured_ = false;
    }
  }

  const char* sample(float (&axes)[3]) {
    const char* reason = beginMeasurement();
    if (reason != nullptr) return reason;
    int32_t counts[3] = {};
    reason = readFreshSample(counts);
    if (reason != nullptr) return reason;
    // Check scale/awake state again before interpreting the coherent burst.
    reason = verifySettings();
    if (reason != nullptr) return reason;
    if (nearRail(counts)) return "NEAR_FULL_SCALE";
    for (size_t axis = 0; axis < 3; ++axis) {
      axes[axis] = mpu6050diag::accelerationG(counts[axis]);
    }
    return nullptr;
  }

  const char* window(Window& out, bool retryReadFailure = false) {
    constexpr uint16_t kSamplesPerWindow = 100;
    // Screen obvious gaps without claiming every sensor update was captured.
    constexpr uint32_t kMaxSampleGapMs = 18;
    // A single 25 ms I2C timeout can consume one or more sensor updates.
    // Accept a wider gap only for the fresh sample immediately after a retry.
    constexpr uint32_t kMaxRetriedSampleGapMs = 45;
    constexpr uint32_t kMinWindowMs = 900;
    constexpr uint32_t kMaxWindowMs = 1100;
    const char* reason = beginMeasurement();
    if (reason != nullptr) return reason;

    float mean[3] = {};
    float sumSquaredDeviation[3] = {};
    uint32_t firstSampleMs = 0;
    uint32_t lastSampleMs = 0;
    bool recoveredRead = false;
    for (uint16_t count = 1; count <= kSamplesPerWindow; ++count) {
      int32_t sample[3] = {};
      reason = readFreshSample(sample);
      const bool retry = reason != nullptr && retryReadFailure && !recoveredRead &&
                         strcmp(reason, "READ_FAILED") == 0;
      if (retry) {
        reason = readFreshSample(sample);
        recoveredRead = reason == nullptr;
      }
      if (reason != nullptr) return reason;
      if (nearRail(sample)) return "NEAR_FULL_SCALE";
      const uint32_t now = millis();
      if (count == 1) {
        firstSampleMs = now;
      } else if (now - lastSampleMs > (retry ? kMaxRetriedSampleGapMs : kMaxSampleGapMs)) {
        return "SAMPLE_TIMING";
      }
      lastSampleMs = now;
      for (size_t axis = 0; axis < 3; ++axis) {
        // Running mean and squared deviation avoid subtracting two large sums.
        const float value = mpu6050diag::accelerationG(sample[axis]);
        const float delta = value - mean[axis];
        mean[axis] += delta / count;
        sumSquaredDeviation[axis] += delta * (value - mean[axis]);
      }
    }
    reason = verifySettings();
    if (reason != nullptr) return reason;
    const uint32_t spanMs = lastSampleMs - firstSampleMs;
    if (spanMs < kMinWindowMs || spanMs > kMaxWindowMs) return "SAMPLE_TIMING";
    // The window mean removes fixed offsets and steady gravity. Changing tilt
    // still contributes to this bench motion metric.
    const float variance = (sumSquaredDeviation[0] + sumSquaredDeviation[1] +
                            sumSquaredDeviation[2]) / kSamplesPerWindow;
    const float rmsMg = 1000.0f * sqrtf(fmaxf(0.0f, variance));
    out = {kSamplesPerWindow, spanMs, rmsMg, recoveredRead};
    return nullptr;
  }

 private:
  struct Setting { uint8_t reg; uint8_t value; };
  static const Setting (&settings())[10] {
    static constexpr Setting values[] = {
        {0x6B, 0x01},  // Awake, PLL X-gyro clock, temperature enabled.
        {0x6C, 0x00},  // All axes enabled; gyro is not measured/published here.
        {0x6A, 0x00},  // No DMP, FIFO, or auxiliary I2C master.
        {0x23, 0x00},  // No sensor FIFO writes.
        {0x19, mpu6050diag::kSampleDivider},
        {0x1A, mpu6050diag::kFilterConfiguration},
        {0x1B, 0x00},  // Gyro self-test disabled, default gyro scale.
        {0x1C, mpu6050diag::kAccelConfiguration},
        {0x37, 0x00},  // Clear status only on INT_STATUS reads; no auxiliary bypass.
        {0x38, 0x01},  // Data-ready enabled; INT physical pin remains unconnected.
    };
    return values;
  }

  sensor_i2c::Bus& bus_;
  bool configured_ = false;

  bool writeRegister(uint8_t reg, uint8_t value) {
    const sensor_i2c::Lock lock(bus_);
    Wire.beginTransmission(mpu6050diag::kAddress);
    const size_t pointerCount = Wire.write(reg);
    const size_t dataCount = Wire.write(value);
    const uint8_t error = Wire.endTransmission(true);
    return pointerCount == 1 && dataCount == 1 && error == 0;
  }

  bool readRegisters(uint8_t reg, uint8_t* bytes, size_t length) {
    const sensor_i2c::Lock lock(bus_);
    Wire.beginTransmission(mpu6050diag::kAddress);
    if (Wire.write(reg) != 1) {
      Wire.endTransmission(true);
      return false;
    }
    if (Wire.endTransmission(false) != 0) return false;
    const size_t received = Wire.requestFrom(mpu6050diag::kAddress, length, true);
    if (received != length || Wire.available() != static_cast<int>(length)) {
      while (Wire.available() > 0) Wire.read();
      return false;
    }
    for (size_t index = 0; index < length; ++index) {
      const int value = Wire.read();
      if (value < 0) return false;
      bytes[index] = static_cast<uint8_t>(value);
    }
    return true;
  }

  const char* verifySettings() {
    uint8_t value = 0;
    if (!readRegisters(0x75, &value, 1)) return "READ_FAILED";
    if (value != mpu6050diag::kIdentity) return "ID_MISMATCH";
    for (const Setting& setting : settings()) {
      if (!readRegisters(setting.reg, &value, 1)) return "READ_FAILED";
      if (value != setting.value) return "SETTINGS_MISMATCH";
    }
    return nullptr;
  }

  const char* configure() {
    uint8_t value = 0;
    if (!readRegisters(0x75, &value, 1)) return "READ_FAILED";
    if (value != mpu6050diag::kIdentity) return "ID_MISMATCH";
    if (!writeRegister(0x6B, 0x80)) return "WRITE_FAILED";
    delay(100);
    if (!readRegisters(0x6B, &value, 1)) return "READ_FAILED";
    if ((value & 0x80u) != 0) return "RESET_NOT_COMPLETE";
    if (!writeRegister(0x6B, 0x01)) return "WRITE_FAILED";
    delay(100);  // Clock/gyro startup, not a measured timing guarantee.
    for (const Setting& setting : settings()) {
      if (!writeRegister(setting.reg, setting.value)) return "WRITE_FAILED";
    }
    delay(100);  // Filter settling before the first displayed acquisition.
    return verifySettings();
  }

  const char* beginMeasurement() {
    if (!bus_.ready()) return "I2C_INIT_FAILED";
    if (!configured_) {
      const char* reason = configure();
      if (reason != nullptr) return reason;
      configured_ = true;
    }
    const char* reason = verifySettings();
    if (reason != nullptr) return reason;
    // Clear any pending DATA_RDY before the first sample in this acquisition.
    uint8_t status = 0;
    return readRegisters(0x3A, &status, 1) ? nullptr : "READ_FAILED";
  }

  const char* readFreshSample(int32_t (&sample)[3]) {
    uint8_t status = 0;
    const uint32_t started = millis();
    while (true) {
      delay(2);
      if (!readRegisters(0x3A, &status, 1)) return "READ_FAILED";
      if (mpu6050diag::expired(millis(), started)) return "DATA_READY_TIMEOUT";
      if ((status & 1u) != 0) break;
    }
    uint8_t bytes[6] = {};
    if (!readRegisters(0x3B, bytes, sizeof(bytes))) return "READ_FAILED";
    for (size_t axis = 0; axis < 3; ++axis) {
      sample[axis] = mpu6050diag::signedWord(bytes[2 * axis], bytes[2 * axis + 1]);
    }
    return nullptr;
  }

  bool nearRail(const int32_t (&sample)[3]) {
    return mpu6050diag::nearRail(sample[0]) || mpu6050diag::nearRail(sample[1]) ||
           mpu6050diag::nearRail(sample[2]);
  }
};

}  // namespace mpu6050
