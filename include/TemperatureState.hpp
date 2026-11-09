#pragma once

#include "CanProtocol.hpp"

// Pure, compile-time-testable qualification/freshness logic. Hardware reads
// happen elsewhere; no simulated fallback is allowed in this state machine.
namespace temperature {

constexpr uint32_t kStaleAfterMs = 5000;
enum class Quality : uint8_t { kMissing, kStale, kValid };

struct State {
  bool hasQualified;
  uint8_t goodStreak;
  int16_t deciC;
  uint32_t qualifiedAtMs;
};

constexpr State initial() {
  return State{false, 0, protocol::kTemperatureUnavailable, 0};
}

constexpr State observe(State old, bool valid, int16_t deciC, uint32_t now) {
  return valid
      ? State{old.hasQualified || old.goodStreak >= 1,
              static_cast<uint8_t>(old.goodStreak < 2 ? old.goodStreak + 1 : 2),
              old.goodStreak >= 1 ? deciC : old.deciC,
              old.goodStreak >= 1 ? now : old.qualifiedAtMs}
      : State{old.hasQualified, 0, old.deciC, old.qualifiedAtMs};
}

constexpr Quality quality(State state, uint32_t now) {
  return !state.hasQualified || static_cast<uint32_t>(now - state.qualifiedAtMs) > kStaleAfterMs
      ? Quality::kMissing
      : (state.goodStreak >= 2 ? Quality::kValid : Quality::kStale);
}

constexpr uint8_t warnings(State state, uint32_t now) {
  return quality(state, now) == Quality::kMissing ? protocol::kWarnTemperatureMissing
      : (quality(state, now) == Quality::kStale ? protocol::kWarnTemperatureStale : 0);
}

constexpr int16_t wireValue(State state, uint32_t now) {
  return quality(state, now) == Quality::kMissing
      ? protocol::kTemperatureUnavailable : state.deciC;
}

}  // namespace temperature
