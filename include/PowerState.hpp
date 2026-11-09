#pragma once

#include "CanProtocol.hpp"

namespace power {
constexpr uint32_t kFreshnessMs = 300;

struct State {
  uint8_t goodStreak;
  uint16_t millivolts;
  int16_t deciMa;
  uint32_t sampledAtMs;
};

constexpr State initial() {
  return State{0, protocol::kBusUnavailable, protocol::kCurrentUnavailable, 0};
}

constexpr int16_t roundDeciMa(int32_t microamps) {
  // Round symmetric ties away from zero. Input is limited by INA219 acquisition.
  return static_cast<int16_t>((microamps >= 0 ? microamps + 50 : microamps - 50) / 100);
}

constexpr State observe(State old, bool valid, uint16_t bus, int16_t current, uint32_t now) {
  return valid && !protocol::powerMissing(bus, current) && protocol::validPowerEncoding(bus, current)
      ? State{static_cast<uint8_t>(old.goodStreak > 0 &&
                  static_cast<uint32_t>(now - old.sampledAtMs) <= kFreshnessMs ? 2 : 1),
              bus, current, now}
      : State{0, protocol::kBusUnavailable, protocol::kCurrentUnavailable, now};
}

constexpr bool available(State state, uint32_t now) {
  return state.goodStreak >= 2 && static_cast<uint32_t>(now - state.sampledAtMs) <= kFreshnessMs;
}

constexpr uint16_t busValue(State state, uint32_t now) {
  return available(state, now) ? state.millivolts : protocol::kBusUnavailable;
}

constexpr int16_t currentValue(State state, uint32_t now) {
  return available(state, now) ? state.deciMa : protocol::kCurrentUnavailable;
}

constexpr uint8_t warnings(State state, uint32_t now) {
  return available(state, now) ? 0 : protocol::kWarnPowerMissing;
}
}  // namespace power
