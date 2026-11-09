#pragma once

#include <stdint.h>

namespace ina219diag {
// R100 (nominal 100 milliohm), 16 V bus, +/-40 mV shunt, 12-bit ADCs.
// Triggered shunt+bus acquisition stops after each conversion pair.
constexpr uint16_t kConfiguration = 0x019B;
constexpr uint16_t kCalibration = 8192;
constexpr uint32_t kConversionTimeoutMs = 50;

constexpr int32_t signedWord(uint16_t raw) {
  return raw < 0x8000u ? static_cast<int32_t>(raw)
                       : static_cast<int32_t>(raw) - 65536;
}

constexpr uint16_t busMillivolts(uint16_t raw) {
  return static_cast<uint16_t>((raw >> 3) * 4u);
}

constexpr int32_t currentMicroamps(uint16_t raw) {
  return signedWord(raw) * 50;  // 50 uA/LSB with calibration 8192 and R100.
}

constexpr int32_t shuntMicrovolts(uint16_t raw) {
  return signedWord(raw) * 10;
}

constexpr bool conversionReady(uint16_t bus) { return (bus & 2u) != 0; }
constexpr bool overflow(uint16_t bus) { return (bus & 1u) != 0; }

enum class Status { kSample, kNotReady, kOverflow, kSettingsMismatch, kRange };

constexpr Status classify(uint16_t bus, uint16_t current, uint16_t shunt,
                          uint16_t configuration, uint16_t calibration) {
  return configuration != kConfiguration || calibration != kCalibration
      ? Status::kSettingsMismatch
      : overflow(bus) ? Status::kOverflow
      : !conversionReady(bus) ? Status::kNotReady
      // Screening envelope for THIS 3.3 V / 331-ohm LED branch, not device ratings
      // or a hardware current limiter. Reject shunt rail values as saturation.
      : busMillivolts(bus) > 5000u || currentMicroamps(current) < -12000 ||
        currentMicroamps(current) > 12000 || shuntMicrovolts(shunt) <= -40000 ||
        shuntMicrovolts(shunt) >= 40000 ? Status::kRange
      : Status::kSample;
}

// Compile-time checks use the same helpers that the actual diagnostic executes.
static_assert((kConfiguration & 0x2000u) == 0, "16 V bus range");
static_assert((kConfiguration & 0x1800u) == 0, "40 mV shunt gain");
static_assert((kConfiguration & 0x0780u) == 0x0180u, "12-bit bus ADC");
static_assert((kConfiguration & 0x0078u) == 0x0018u, "12-bit shunt ADC");
static_assert((kConfiguration & 7u) == 3u, "Triggered bus plus shunt");
static_assert(kCalibration == 40960000u / (50u * 100u), "R100 calibration equation");
static_assert(signedWord(0) == 0, "Signed zero");
static_assert(signedWord(0x7FFF) == 32767, "Positive boundary");
static_assert(signedWord(0x8000) == -32768, "Negative boundary");
static_assert(signedWord(0xFFFF) == -1, "Two's complement");
static_assert(busMillivolts(0x19CA) == 3300, "Bus voltage and status-bit mask");
static_assert(currentMicroamps(84) == 4200, "4.2 mA current vector");
static_assert(currentMicroamps(0xFFAC) == -4200, "Negative current vector");
static_assert(shuntMicrovolts(42) == 420, "0.42 mV shunt vector");
static_assert(classify(0x19CA, 84, 42, kConfiguration, kCalibration) == Status::kSample,
              "Complete in-envelope sample");
static_assert(classify(2, 0, 0, kConfiguration, kCalibration) == Status::kSample,
              "Measured zero is not fabricated missing data");
static_assert(classify(0x19C8, 84, 42, kConfiguration, kCalibration) == Status::kNotReady,
              "No completed conversion");
static_assert(classify(0x19CB, 84, 42, kConfiguration, kCalibration) == Status::kOverflow,
              "Overflow rejected");
static_assert(classify(0x19CA, 84, 42, 0x399F, kCalibration) == Status::kSettingsMismatch,
              "Wrong configuration rejected");
static_assert(classify(0x19CA, 84, 42, kConfiguration, 0) == Status::kSettingsMismatch,
              "Uncalibrated current rejected");
static_assert(classify(0x271A, 84, 42, kConfiguration, kCalibration) == Status::kRange,
              "Above 5 V bench envelope rejected");
static_assert(classify(0x19CA, 241, 42, kConfiguration, kCalibration) == Status::kRange,
              "Above 12 mA bench envelope rejected");
static_assert(classify(0x19CA, 0xFF0F, 42, kConfiguration, kCalibration) == Status::kRange,
              "Below -12 mA bench envelope rejected");
static_assert(classify(0x19CA, 84, 4000, kConfiguration, kCalibration) == Status::kRange,
              "Positive shunt rail rejected");
static_assert(classify(0x19CA, 84, 0xF060, kConfiguration, kCalibration) == Status::kRange,
              "Negative shunt rail rejected");
}  // namespace ina219diag
