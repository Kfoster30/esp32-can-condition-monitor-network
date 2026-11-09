#include <Arduino.h>
#include <driver/twai.h>
#include <esp_system.h>

#include "AppConfig.hpp"
#include "CanProtocol.hpp"
#include "TemperatureState.hpp"
#include "PowerState.hpp"

#if ECU_ROLE_SENSOR && (SENSOR_DHT11 || SENSOR_INA219)
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#endif
#if ECU_ROLE_SENSOR && SENSOR_DHT11
#include <DHT.h>
#include <math.h>
#endif
#if ECU_ROLE_SENSOR && SENSOR_INA219
#include "Ina219Acquisition.hpp"
#endif
#if ECU_ROLE_SENSOR && SENSOR_MPU6050
#include "Mpu6050Acquisition.hpp"
#endif

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
#if SERIALIZE_CAN_I2C
  return twai_transmit(&message, 0) == ESP_OK;
#else
  return twai_transmit(&message, pdMS_TO_TICKS(20)) == ESP_OK;
#endif
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
uint32_t sensorBootId = 0;
#if BENCH_CAN_DELAY_MS > 0
uint32_t txDelayStartedMs = 0;
bool txStartPending = true;
#endif

uint8_t sourceFlags() {
  return static_cast<uint8_t>(protocol::kFlagAnySimulated |
      (SENSOR_DHT11 ? protocol::kFlagDhtTemperature : 0) |
      (SENSOR_INA219 ? protocol::kFlagInaPower : 0) |
      (SENSOR_MPU6050 ? protocol::kFlagMpuMotion : 0));
}

bool publishSession() {
  const protocol::Session value = {config::kProtocolVersion, config::kRoleSensor,
                                   sensorBootId, sourceFlags()};
  uint8_t payload[protocol::kPayloadLength];
  protocol::encode(value, payload);
  if (sendFrame(config::kIdSession, payload)) return true;
  Serial.println("#warning,session_tx_failed");
  return false;
}

#if SENSOR_DHT11
DHT dht(config::kDhtDataPin, DHT11);
QueueHandle_t temperatureQueue = nullptr;
temperature::State temperatureState = temperature::initial();
#if BENCH_DHT11_DELAY_MS > 0
uint32_t temperatureDelayStartedMs = 0;
bool temperatureStartPending = true;
#endif

void readTemperatureTask(void*) {
  dht.begin();
  auto state = temperature::initial();
  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    // Warm up and respect the library's two-second minimum sampling interval.
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(config::kDhtReadPeriodMs));
    const float reading = dht.readTemperature();
    const bool valid = isfinite(reading) && reading >= 0.0f && reading <= 50.0f;
    state = temperature::observe(state, valid,
        valid ? static_cast<int16_t>(lroundf(reading * 10.0f)) : 0, millis());
    xQueueOverwrite(temperatureQueue, &state);
  }
}

void startTemperature() {
  temperatureQueue = xQueueCreate(1, sizeof(temperature::State));
  if (temperatureQueue == nullptr) {
    Serial.println("#error,dht_queue_create_failed");
    return;
  }
  // Classic dual-core ESP32 only. Keep blocking DHT sampling off Arduino/CAN's
  // core 1; this still requires a new physical timing-validation run.
  if (xTaskCreatePinnedToCore(readTemperatureTask, "dht11", 4096, nullptr, 1,
                              nullptr, 0) != pdPASS) {
    vQueueDelete(temperatureQueue);
    temperatureQueue = nullptr;
    Serial.println("#error,dht_task_create_failed");
  }
}
#endif

#if SENSOR_INA219
sensor_i2c::Bus sensorBus;
constexpr BaseType_t kI2cCore = 0;

void initializeI2cTask(void* caller) {
  // The ESP32 assigns the peripheral interrupt to the initializing core.
  // Initialize Wire on the sensor core while TWAI stays on Arduino's core.
  sensorBus.begin();
  xTaskNotifyGive(static_cast<TaskHandle_t>(caller));
  vTaskDelete(nullptr);
}

bool startSensorBus() {
  if (xTaskCreatePinnedToCore(initializeI2cTask, "i2c_init", 4096,
                              xTaskGetCurrentTaskHandle(), 1, nullptr, kI2cCore) != pdPASS) {
    Serial.println("#error,i2c_init_task_create_failed");
    return false;
  }
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  if (!sensorBus.ready()) {
    Serial.println("#error,sensor_i2c_init_failed");
    return false;
  }
  Serial.printf("#i2c,ready,core=%d\n", static_cast<int>(kI2cCore));
  return true;
}

struct PowerSnapshot {
  power::State state;
  ina219::Result result;
};
QueueHandle_t powerQueue = nullptr;
power::State powerState = power::initial();
int lastPowerResult = -1;

void readPowerTask(void*) {
  ina219::Reader reader(sensorBus);
  auto state = power::initial();
  for (;;) {
    const auto sample = reader.sample();
    state = power::observe(state, sample.result == ina219::Result::kSample,
        sample.millivolts, power::roundDeciMa(sample.microamps), millis());
    const PowerSnapshot snapshot = {state, sample.result};
    xQueueOverwrite(powerQueue, &snapshot);
    // Never catch up by spinning after a timeout. Keep I2C off CAN's core 1.
    vTaskDelay(pdMS_TO_TICKS(config::kInaReadPauseMs));
  }
}

void startPower() {
  powerQueue = xQueueCreate(1, sizeof(PowerSnapshot));
  if (powerQueue == nullptr) {
    Serial.println("#error,ina219_queue_create_failed");
    return;
  }
  if (xTaskCreatePinnedToCore(readPowerTask, "ina219", 4096, nullptr, 1, nullptr, kI2cCore) != pdPASS) {
    vQueueDelete(powerQueue);
    powerQueue = nullptr;
    Serial.println("#error,ina219_task_create_failed");
  }
}
#endif

#if SENSOR_MPU6050
constexpr uint32_t kMotionFreshnessMs = 1500;
struct MotionSnapshot {
  uint16_t rmsMg;
  uint32_t completedMs;
  const char* result;
};
QueueHandle_t motionQueue = nullptr;
MotionSnapshot motionState = {protocol::kMotionUnavailable, 0, nullptr};
const char* lastMotionResult = nullptr;

uint16_t motionValue(uint32_t now) {
  return now - motionState.completedMs <= kMotionFreshnessMs
      ? motionState.rmsMg : protocol::kMotionUnavailable;
}

void readMotionTask(void*) {
  mpu6050::Reader reader(sensorBus);
  for (;;) {
    mpu6050::Window window = {};
    const char* reason = reader.window(window, true);
    const uint16_t value = reason == nullptr
        ? static_cast<uint16_t>(lroundf(window.rmsMg))
        : protocol::kMotionUnavailable;
    const char* result = reason != nullptr ? reason
        : window.recoveredRead ? "HW_WINDOW_RETRY" : "HW_WINDOW";
    const MotionSnapshot snapshot = {value, millis(), result};
    xQueueOverwrite(motionQueue, &snapshot);
    if (reason != nullptr) {
      reader.handleFailure(reason);
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}

void startMotion() {
  motionQueue = xQueueCreate(1, sizeof(MotionSnapshot));
  if (motionQueue == nullptr) {
    Serial.println("#error,mpu_queue_create_failed");
    return;
  }
  if (xTaskCreatePinnedToCore(readMotionTask, "mpu6050", 4096, nullptr, 1, nullptr, kI2cCore) != pdPASS) {
    vQueueDelete(motionQueue);
    motionQueue = nullptr;
    Serial.println("#error,mpu_task_create_failed");
  }
}
#endif

void publishHeartbeat(uint32_t now) {
  protocol::Heartbeat heartbeat = {
      config::kProtocolVersion,
      config::kRoleSensor,
      heartbeatSequence++,
      sourceFlags(),
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
#if SENSOR_DHT11
  condition.warnings = temperature::warnings(temperatureState, now);
  condition.temperatureDeciC = temperature::wireValue(temperatureState, now);
#endif
#if SENSOR_INA219
  condition.warnings |= power::warnings(powerState, now);
#endif
#if SENSOR_MPU6050
  if (motionValue(now) == protocol::kMotionUnavailable) {
    condition.warnings |= protocol::kWarnMotionMissing;
  }
#endif
  condition.state = condition.warnings == 0 ? protocol::SystemState::kRun
                                           : protocol::SystemState::kDegraded;
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
#if SENSOR_INA219
  value.busMillivolts = power::busValue(powerState, now);
  value.currentDeciMa = power::currentValue(powerState, now);
#endif
#if SENSOR_MPU6050
  value.vibrationMg = motionValue(now);
  value.quality = value.vibrationMg == protocol::kMotionUnavailable
      ? protocol::kMotionQualityMissing : protocol::kMotionQualityValid;
#endif
  uint8_t payload[protocol::kPayloadLength];
  protocol::encode(value, payload);
  if (!sendFrame(config::kIdMotionPower, payload)) {
    Serial.println("#warning,motion_power_tx_failed");
  }
}

void publishTelemetryBatch(uint32_t now) {
  // Queue the session marker first on the same FIFO.
  if (!publishSession()) return;
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
}

#if SERIALIZE_CAN_I2C
bool serializedTxStalled = false;

bool canTransmitIdle() {
  twai_status_info_t status = {};
  return twai_get_status_info(&status) == ESP_OK && status.state == TWAI_STATE_RUNNING &&
         status.msgs_to_tx == 0;
}

bool waitForCanTransmitIdle() {
  // Pending includes the frame in flight. Bound the pause if CAN stalls.
  constexpr uint32_t kDrainBudgetMs = 5;
  const uint32_t drainStartMs = millis();
  while (!canTransmitIdle()) {
    if (millis() - drainStartMs >= kDrainBudgetMs) return false;
    delay(1);
  }
  return true;
}

void publishSerializedTelemetry(uint32_t now) {
#if BENCH_SERIALIZE_CAN_I2C
  if (serializedTxStalled || !sensorBus.ready()) return;
#else
  // A previous batch may still be draining after the I2C pause expired.
  if (!canTransmitIdle()) return;
  if (!sensorBus.ready()) {
    publishTelemetryBatch(now);
    return;
  }
#endif

  bool drained;
  {
    // Try again on the next loop if an I2C transaction is already in progress.
    const sensor_i2c::Lock lock(sensorBus, 0);
    if (!lock.acquired()) return;
    publishTelemetryBatch(now);
    drained = waitForCanTransmitIdle();
  }

  const bool stalled = !drained;
  if (stalled == serializedTxStalled) return;
  serializedTxStalled = stalled;
#if BENCH_SERIALIZE_CAN_I2C
  Serial.println("#bench,can_i2c_serialization=invalid_tx_drain");
#else
  Serial.println(stalled ? "#warning,can_tx_drain_delayed" : "#can,tx_drain_recovered");
#endif
}
#endif

void runRole() {
#if SENSOR_DHT11
#if BENCH_DHT11_DELAY_MS > 0
  if (temperatureStartPending && millis() - temperatureDelayStartedMs >= BENCH_DHT11_DELAY_MS) {
    temperatureStartPending = false;
    Serial.printf("#bench,dht11_acquisition=starting,t_ms=%lu\n", millis());
    startTemperature();
  }
#endif
  if (temperatureQueue != nullptr) xQueuePeek(temperatureQueue, &temperatureState, 0);
#endif
#if SENSOR_INA219
  PowerSnapshot snapshot;
  if (powerQueue != nullptr && xQueuePeek(powerQueue, &snapshot, 0) == pdTRUE) {
    powerState = snapshot.state;
    if (lastPowerResult != static_cast<int>(snapshot.result)) {
      // Only the main task writes serial; worker messages cannot interleave it.
      Serial.printf("#ina219_read,%s\n", ina219::name(snapshot.result));
      lastPowerResult = static_cast<int>(snapshot.result);
    }
  }
#endif
#if SENSOR_MPU6050
  if (motionQueue != nullptr && xQueuePeek(motionQueue, &motionState, 0) == pdTRUE &&
      (lastMotionResult == nullptr || strcmp(lastMotionResult, motionState.result) != 0)) {
    Serial.printf("#mpu6050_read,%s,t_ms=%lu\n", motionState.result,
                  static_cast<unsigned long>(motionState.completedMs));
    lastMotionResult = motionState.result;
  }
#endif
  // Read time after copying the worker snapshot to avoid negative sample ages.
  const uint32_t now = millis();
#if BENCH_CAN_DELAY_MS > 0
  if (txStartPending) {
    if (now - txDelayStartedMs < BENCH_CAN_DELAY_MS) {
      delay(1);
      return;
    }
    txStartPending = false;
    Serial.printf("#bench,can_tx=starting,t_ms=%lu\n", now);
  }
#endif
  const bool due = now - lastHeartbeatMs >= config::kHeartbeatPeriodMs ||
                   now - lastConditionMs >= config::kConditionPeriodMs ||
                   now - lastMotionPowerMs >= config::kMotionPowerPeriodMs;
  if (due) {
#if SERIALIZE_CAN_I2C
    publishSerializedTelemetry(now);
#else
    publishTelemetryBatch(now);
#endif
  }
  delay(1);
}

#else

struct StreamStatus {
  bool seen = false;
  uint32_t lastRxMs = 0;
};

StreamStatus heartbeatStatus;
StreamStatus conditionStatus;
StreamStatus motionPowerStatus;
bool heartbeatTimedOut = false;
bool dataTimedOut = false;
uint32_t dashboardStartMs = 0;
bool sensorFlagsKnown = false;
uint8_t sensorFlags = 0;
int lastTemperatureQuality = -1;
int lastPowerQuality = -1;
int lastMotionQuality = -1;
bool sessionKnown = false;
uint32_t currentSession = 0;
uint32_t lastSessionMs = 0;

void printSourceLabels(uint8_t flags) {
  if (!sensorFlagsKnown || sensorFlags != flags) {
    Serial.printf("#sources,condition=SIM,temperature=%s,motion=%s,power=%s\n",
                  (flags & protocol::kFlagDhtTemperature) ? "DHT11" : "SIM",
                  (flags & protocol::kFlagMpuMotion) ? "MPU6050" : "SIM",
                  (flags & protocol::kFlagInaPower) ? "INA219" : "SIM");
    lastTemperatureQuality = -1;
    lastPowerQuality = -1;
    lastMotionQuality = -1;
  }
  sensorFlagsKnown = true;
  sensorFlags = flags;
}

void printTemperatureQuality(uint8_t warnings) {
  if (!sensorFlagsKnown) return;
  const int quality = !(sensorFlags & protocol::kFlagDhtTemperature) ? 0
      : (warnings & protocol::kWarnTemperatureMissing) ? 1
      : (warnings & protocol::kWarnTemperatureStale) ? 2 : 3;
  if (quality != lastTemperatureQuality) {
    const char* labels[] = {"SIM", "MISSING", "STALE", "HW_VALID"};
    Serial.printf("#temperature_quality,%s\n", labels[quality]);
    lastTemperatureQuality = quality;
  }
}

void printPowerQuality(bool missing) {
  if (!sensorFlagsKnown) return;
  const int quality = !(sensorFlags & protocol::kFlagInaPower) ? 0 : missing ? 1 : 2;
  if (lastPowerQuality != quality) {
    const char* names[] = {"SIM", "MISSING", "HW_VALID"};
    Serial.printf("#power_quality,%s\n", names[quality]);
    lastPowerQuality = quality;
  }
}

void printMotionQuality(bool missing) {
  if (!sensorFlagsKnown) return;
  const int quality = !(sensorFlags & protocol::kFlagMpuMotion) ? 0 : missing ? 1 : 2;
  if (lastMotionQuality != quality) {
    const char* names[] = {"SIM", "MISSING", "HW_VALID"};
    Serial.printf("#motion_quality,%s\n", names[quality]);
    lastMotionQuality = quality;
  }
}

void markReceived(StreamStatus& status, uint32_t now) {
  // Availability and sequence analysis are separate: the host analyzes the
  // actual sequence bytes below, rather than hidden/ambiguous modulo counters.
  status.seen = true;
  status.lastRxMs = now;
}

void printSequenceSuffix(uint8_t sequence, uint32_t now) {
  if (protocol::sessionFresh(sessionKnown, lastSessionMs, now, config::kSessionFreshnessMs)) {
    Serial.printf(",%u,%lu\n", sequence, currentSession);
  } else {
    Serial.printf(",%u,UNKNOWN\n", sequence);
  }
}

void receiveFrames() {
  twai_message_t message = {};
  while (twai_receive(&message, 0) == ESP_OK) {
    if (!message.extd || message.rtr) continue;
    const uint32_t now = millis();

    if (message.identifier == config::kIdSession) {
      protocol::Session value;
      if (!protocol::decode(message.data, message.data_length_code, value)) continue;
      if (!sessionKnown || currentSession != value.bootId) {
        Serial.printf("#session,%lu,%lu,%s\n", now, value.bootId,
                      sessionKnown ? "CHANGED" : "INITIAL");
      }
      currentSession = value.bootId;
      sessionKnown = true;
      lastSessionMs = now;
      printSourceLabels(value.flags);
    } else if (message.identifier == config::kIdHeartbeat) {
      protocol::Heartbeat value;
      if (!protocol::decode(message.data, message.data_length_code, value)) continue;
      markReceived(heartbeatStatus, now);
      printSourceLabels(value.flags);
      Serial.printf("heartbeat,%lu,%u,%u,%u,%lu", now, value.protocolVersion,
                    value.role, value.flags, value.uptimeSeconds);
      printSequenceSuffix(value.sequence, now);
    } else if (message.identifier == config::kIdCondition) {
      protocol::Condition value;
      if (!protocol::decode(message.data, message.data_length_code, value)) continue;
      markReceived(conditionStatus, now);
      printTemperatureQuality(value.warnings);
      const float temperatureC = value.temperatureDeciC == protocol::kTemperatureUnavailable
          ? NAN : value.temperatureDeciC / 10.0f;
      Serial.printf("condition,%lu,%s,%u,%u,%.2f,%.1f", now,
                    stateName(value.state), value.warnings, value.faults,
                    value.conditionCentiPercent / 100.0f,
                    temperatureC);
      printSequenceSuffix(value.sequence, now);
    } else if (message.identifier == config::kIdMotionPower) {
      protocol::MotionPower value;
      if (!protocol::decode(message.data, message.data_length_code, value)) continue;
      const bool hardwareMotion = sensorFlagsKnown && (sensorFlags & protocol::kFlagMpuMotion);
      if (hardwareMotion && !protocol::validHardwareMotionEncoding(value.quality, value.vibrationMg))
        continue;
      const bool motionMissing = hardwareMotion && value.vibrationMg == protocol::kMotionUnavailable;
      markReceived(motionPowerStatus, now);
      const bool missing = protocol::powerMissing(value.busMillivolts, value.currentDeciMa);
      printPowerQuality(missing);
      printMotionQuality(motionMissing);
      Serial.printf("motion_power,%lu,%u,%.0f,%.3f,%.1f", now, value.quality,
                    motionMissing ? NAN : static_cast<float>(value.vibrationMg),
                    missing ? NAN : value.busMillivolts / 1000.0f,
                    missing ? NAN : value.currentDeciMa / 10.0f);
      printSequenceSuffix(value.sequence, now);
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
  // Diagnostic epoch only: not an authenticated or guaranteed-unique identity.
  sensorBootId = esp_random();
  Serial.printf("#role=SENSOR,simulation=%u\n", SENSOR_SIMULATION);
  Serial.printf("#sources,condition=SIM,temperature=%s,motion=%s,power=%s\n",
                SENSOR_DHT11 ? "DHT11" : "SIM", SENSOR_MPU6050 ? "MPU6050" : "SIM",
                SENSOR_INA219 ? "INA219" : "SIM");
  if (SENSOR_DHT11) Serial.println("#dht11,pin=4,period_ms=2000,qualification_reads=2");
  if (SENSOR_INA219) Serial.println("#ina219,sda=21,scl=22,address=0x40,pause_ms=100,freshness_ms=300,qualification_reads=2");
#else
  Serial.println("#role=DASHBOARD");
  Serial.println("#telemetry_schema=0.5,sequence_and_session_are_final_two_columns");
  Serial.println("#sequence_analysis=host,session_freshness_ms=250");
  dashboardStartMs = millis();
#endif
  canReady = startCan();
#if ECU_ROLE_SENSOR && SENSOR_DHT11
#if BENCH_DHT11_DELAY_MS > 0
  temperatureDelayStartedMs = millis();
  Serial.printf("#bench,dht11_acquisition=delayed,delay_ms=%lu\n",
                static_cast<unsigned long>(BENCH_DHT11_DELAY_MS));
#else
  if (canReady) startTemperature();
#endif
#endif
#if ECU_ROLE_SENSOR && SENSOR_INA219
  if (canReady && startSensorBus()) {
    if (BENCH_INA219_IDLE) {
      Serial.println("#bench,ina219_acquisition=disabled");
    } else {
      startPower();
    }
#if SENSOR_MPU6050
    startMotion();
#endif
  }
#endif
#if SERIALIZE_CAN_I2C && !BENCH_SERIALIZE_CAN_I2C
  Serial.println("#can_i2c_serialization=enabled");
#endif
#if ECU_ROLE_SENSOR && BENCH_CAN_DELAY_MS > 0
#if BENCH_SERIALIZE_CAN_I2C
  Serial.println("#bench,can_i2c_serialization=enabled");
#endif
  txDelayStartedMs = millis();
  Serial.printf("#bench,can_tx=delayed,delay_ms=%lu\n",
                static_cast<unsigned long>(BENCH_CAN_DELAY_MS));
#endif
}

void loop() {
  if (!canReady) {
    delay(1000);
    return;
  }
  runRole();
}
