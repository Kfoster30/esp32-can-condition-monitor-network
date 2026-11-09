#pragma once

#include <stdint.h>

namespace mpu6050diag {
constexpr uint8_t kAddress = 0x68;
constexpr uint8_t kIdentity = 0x68;
constexpr uint8_t kAccelConfiguration = 0x08;  // +/-4 g, no self-test/high-pass.
constexpr uint8_t kFilterConfiguration = 0x03;  // 44 Hz accel bandwidth.
constexpr uint8_t kSampleDivider = 9;  // 1 kHz / (1 + 9) = nominal 100 Hz.
constexpr uint32_t kReadyTimeoutMs = 50;
constexpr int32_t kCountsPerG = 8192;

constexpr int32_t signedWord(uint8_t high, uint8_t low) {
  return ((static_cast<uint16_t>(high) << 8) | low) < 0x8000u
      ? static_cast<int32_t>((static_cast<uint16_t>(high) << 8) | low)
      : static_cast<int32_t>((static_cast<uint16_t>(high) << 8) | low) - 65536;
}

constexpr float accelerationG(int32_t raw) {
  return static_cast<float>(raw) / kCountsPerG;
}

// Conservative rail screening for this gentle hand-tilt diagnostic.
// A clipped numeric result is unavailable, not a fabricated low measurement.
constexpr bool nearRail(int32_t raw) { return raw <= -32760 || raw >= 32760; }

constexpr bool expired(uint32_t now, uint32_t started) {
  return static_cast<uint32_t>(now - started) >= kReadyTimeoutMs;
}

static_assert((kAccelConfiguration & 0x18u) == 0x08u, "4 g range");
static_assert((kAccelConfiguration & 0xE7u) == 0, "Self-test/high-pass disabled");
static_assert((kFilterConfiguration & 0x07u) == 3, "44 Hz accel filter");
static_assert(1000 / (1 + kSampleDivider) == 100, "Nominal internal data rate");
static_assert(signedWord(0x00, 0x00) == 0, "Numeric zero");
static_assert(signedWord(0x20, 0x00) == 8192, "Positive 1 g vector");
static_assert(signedWord(0xE0, 0x00) == -8192, "Negative 1 g vector");
static_assert(signedWord(0x7F, 0xFF) == 32767, "Positive boundary");
static_assert(signedWord(0x80, 0x00) == -32768, "Negative boundary");
static_assert(signedWord(0xFF, 0xFF) == -1, "Signed least-negative unit");
static_assert(accelerationG(8192) == 1.0f, "4 g scale positive");
static_assert(accelerationG(-8192) == -1.0f, "4 g scale negative");
static_assert(accelerationG(0) == 0.0f, "Real zero is not unavailable");
static_assert(!nearRail(32759) && !nearRail(-32759), "Inside rail screen");
static_assert(nearRail(32760) && nearRail(-32760), "Rail screen boundaries");
static_assert(!expired(49, 0) && expired(50, 0), "Ready wait boundary");
static_assert(!expired(33, 0xFFFFFFF0u) && expired(34, 0xFFFFFFF0u),
              "Ready wait across millis rollover");
}  // namespace mpu6050diag
