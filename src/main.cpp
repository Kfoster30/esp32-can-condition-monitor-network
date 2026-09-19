#include <Arduino.h>
#include <driver/twai.h>

#include "AppConfig.hpp"
#include "CanProtocol.hpp"

namespace {

bool canReady = false;

bool startCan() {
  const twai_general_config_t general = TWAI_GENERAL_CONFIG_DEFAULT(
      config::kCanTxPin, config::kCanRxPin, TWAI_MODE_NORMAL);
  const twai_timing_config_t timing = TWAI_TIMING_CONFIG_500KBITS();
  const twai_filter_config_t filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&general, &timing, &filter) != ESP_OK) {
    Serial.println("#error,can_driver_install_failed");
    return false;
  }
  if (twai_start() != ESP_OK) {
    Serial.println("#error,can_start_failed");
    twai_driver_uninstall();
    return false;
  }
  Serial.println("#can,ready,bitrate=500000,tx=17,rx=16");
  return true;
}

bool sendFrame(uint32_t identifier, const uint8_t* payload) {
  twai_message_t message = {};
  message.identifier = identifier;
  message.extd = 1;
  message.data_length_code = protocol::kPayloadLength;
  memcpy(message.data, payload, protocol::kPayloadLength);
  return twai_transmit(&message, pdMS_TO_TICKS(20)) == ESP_OK;
}

const char* stateName(protocol::SystemState state) {
  switch (state) {
    case protocol::SystemState::kSelfTest: return "SELF_TEST";
    case protocol::SystemState::kRun: return "RUN";
    case protocol::SystemState::kDegraded: return "DEGRADED";
    case protocol::SystemState::kFault: return "FAULT";
  }
  return "UNKNOWN";
}

#if ECU_ROLE_SENSOR

uint8_t heartbeatSequence = 0;
uint8_t conditionSequence = 0;
uint8_t motionPowerSequence = 0;
uint32_t lastHeartbeatMs = 0;
uint32_t lastConditionMs = 0;
uint32_t lastMotionPowerMs = 0;

void publishHeartbeat(uint32_t now) {
  protocol::Heartbeat heartbeat = {
      config::kProtocolVersion,
      config::kRoleSensor,
      heartbeatSequence++,
      static_cast<uint8_t>(SENSOR_SIMULATION ? 0x01 : 0x00),
      now / 1000u,
  };
  uint8_t payload[protocol::kPayloadLength];
  protocol::encode(heartbeat, payload);
  if (!sendFrame(config::kIdHeartbeat, payload)) {
    Serial.println("#warning,heartbeat_tx_failed");
  }
}

void publishCondition(uint32_t now) {
  const uint16_t phase = static_cast<uint16_t>((now / 200u) % 200u);
  const uint16_t triangle = phase <= 100u ? phase : 200u - phase;
  protocol::Condition condition = {
      conditionSequence++,
      protocol::SystemState::kRun,
      0,
      0,
      static_cast<uint16_t>(8500u + triangle * 10u),
      static_cast<int16_t>(240 + triangle / 5u),
  };
  uint8_t payload[protocol::kPayloadLength];
  protocol::encode(condition, payload);
  if (!sendFrame(config::kIdCondition, payload)) {
    Serial.println("#warning,condition_tx_failed");
  }
}

void publishMotionPower(uint32_t now) {
  const uint16_t phase = static_cast<uint16_t>((now / 100u) % 100u);
  protocol::MotionPower value = {
      motionPowerSequence++,
      100,
      static_cast<uint16_t>(80u + phase),
      static_cast<uint16_t>(3300u + phase / 5u),
      static_cast<int16_t>(35 + phase / 10u),
  };
  uint8_t payload[protocol::kPayloadLength];
  protocol::encode(value, payload);
  if (!sendFrame(config::kIdMotionPower, payload)) {
    Serial.println("#warning,motion_power_tx_failed");
  }
}

void runRole() {
  const uint32_t now = millis();
  if (now - lastHeartbeatMs >= config::kHeartbeatPeriodMs) {
    lastHeartbeatMs = now;
    publishHeartbeat(now);
  }
  if (now - lastConditionMs >= config::kConditionPeriodMs) {
    lastConditionMs = now;
    publishCondition(now);
  }
  if (now - lastMotionPowerMs >= config::kMotionPowerPeriodMs) {
    lastMotionPowerMs = now;
    publishMotionPower(now);
  }
  delay(1);
}

#else

struct StreamStatus {
  bool seen = false;
  uint8_t lastSequence = 0;
  uint32_t lastRxMs = 0;
  uint32_t sequenceGaps = 0;
};

StreamStatus heartbeatStatus;
StreamStatus conditionStatus;
StreamStatus motionPowerStatus;
bool heartbeatTimedOut = false;
bool dataTimedOut = false;
uint32_t dashboardStartMs = 0;

void updateSequence(StreamStatus& status, uint8_t sequence, uint32_t now) {
  if (status.seen) {
    const uint8_t expected = static_cast<uint8_t>(status.lastSequence + 1u);
    if (sequence != expected) {
      status.sequenceGaps += static_cast<uint8_t>(sequence - expected);
    }
  }
  status.seen = true;
  status.lastSequence = sequence;
  status.lastRxMs = now;
}

void receiveFrames() {
  twai_message_t message = {};
  while (twai_receive(&message, 0) == ESP_OK) {
    if (!message.extd || message.rtr) continue;
    const uint32_t now = millis();

    if (message.identifier == config::kIdHeartbeat) {
      protocol::Heartbeat value;
      if (!protocol::decode(message.data, message.data_length_code, value)) continue;
      updateSequence(heartbeatStatus, value.sequence, now);
      Serial.printf("heartbeat,%lu,%u,%u,%u,%lu\n", now, value.protocolVersion,
                    value.role, value.flags, value.uptimeSeconds);
    } else if (message.identifier == config::kIdCondition) {
      protocol::Condition value;
      if (!protocol::decode(message.data, message.data_length_code, value)) continue;
      updateSequence(conditionStatus, value.sequence, now);
      Serial.printf("condition,%lu,%s,%u,%u,%.2f,%.1f\n", now,
                    stateName(value.state), value.warnings, value.faults,
                    value.conditionCentiPercent / 100.0f,
                    value.temperatureDeciC / 10.0f);
    } else if (message.identifier == config::kIdMotionPower) {
      protocol::MotionPower value;
      if (!protocol::decode(message.data, message.data_length_code, value)) continue;
      updateSequence(motionPowerStatus, value.sequence, now);
      Serial.printf("motion_power,%lu,%u,%u,%.3f,%.1f\n", now, value.quality,
                    value.vibrationMg, value.busMillivolts / 1000.0f,
                    value.currentDeciMa / 10.0f);
    }
  }
}

void checkTimeouts() {
  const uint32_t now = millis();
  const uint32_t heartbeatAge = heartbeatStatus.seen
      ? now - heartbeatStatus.lastRxMs
      : now - dashboardStartMs;
  const uint32_t conditionAge = conditionStatus.seen
      ? now - conditionStatus.lastRxMs
      : now - dashboardStartMs;
  const uint32_t motionPowerAge = motionPowerStatus.seen
      ? now - motionPowerStatus.lastRxMs
      : now - dashboardStartMs;
  const bool heartbeatNow = heartbeatAge > config::kHeartbeatTimeoutMs;
  const bool dataNow = conditionAge > config::kDataTimeoutMs ||
      motionPowerAge > config::kDataTimeoutMs;

  if (heartbeatNow != heartbeatTimedOut) {
    heartbeatTimedOut = heartbeatNow;
    Serial.printf("#event,heartbeat_timeout,%u\n", heartbeatTimedOut);
  }
  if (dataNow != dataTimedOut) {
    dataTimedOut = dataNow;
    Serial.printf("#event,data_timeout,%u\n", dataTimedOut);
  }
}

void runRole() {
  receiveFrames();
  checkTimeouts();
  delay(1);
}

#endif

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("#project=%s,firmware=%s\n", config::kProjectName,
                config::kFirmwareVersion);
#if ECU_ROLE_SENSOR
  Serial.printf("#role=SENSOR,simulation=%u\n", SENSOR_SIMULATION);
#else
  Serial.println("#role=DASHBOARD");
  Serial.println("type,t_ms,field_1,field_2,field_3,field_4,field_5");
  dashboardStartMs = millis();
#endif
  canReady = startCan();
}

void loop() {
  if (!canReady) {
    delay(1000);
    return;
  }
  runRole();
}
