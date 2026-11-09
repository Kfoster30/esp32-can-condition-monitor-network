#pragma once

#include <Arduino.h>

#ifndef ECU_ROLE_SENSOR
#define ECU_ROLE_SENSOR 0
#endif

#ifndef ECU_ROLE_DASHBOARD
#define ECU_ROLE_DASHBOARD 0
#endif

#ifndef SENSOR_SIMULATION
#define SENSOR_SIMULATION 1
#endif

#ifndef SENSOR_DHT11
#define SENSOR_DHT11 0
#endif

#ifndef SENSOR_INA219
#define SENSOR_INA219 0
#endif

#ifndef SENSOR_MPU6050
#define SENSOR_MPU6050 0
#endif

#ifndef BENCH_INA219_IDLE
#define BENCH_INA219_IDLE 0
#endif

#ifndef BENCH_DHT11_DELAY_MS
#define BENCH_DHT11_DELAY_MS 0
#endif

#ifndef BENCH_CAN_DELAY_MS
#define BENCH_CAN_DELAY_MS 0
#endif

#ifndef BENCH_SERIALIZE_CAN_I2C
#define BENCH_SERIALIZE_CAN_I2C 0
#endif

// Keep the original isolation targets available for comparison.
#define SERIALIZE_CAN_I2C (SENSOR_MPU6050 && \
    (BENCH_SERIALIZE_CAN_I2C || (!BENCH_INA219_IDLE && !BENCH_CAN_DELAY_MS)))

static_assert((ECU_ROLE_SENSOR + ECU_ROLE_DASHBOARD) == 1,
              "Select exactly one ECU role");
static_assert(SENSOR_SIMULATION == 1,
              "Condition percentage still requires labeled simulation");
static_assert(!SENSOR_MPU6050 || (ECU_ROLE_SENSOR && SENSOR_INA219),
              "MPU6050 target uses the combined sensor I2C setup");
static_assert(!BENCH_INA219_IDLE || SENSOR_MPU6050,
              "INA219 isolation requires the combined motion build");
static_assert(!BENCH_DHT11_DELAY_MS || (BENCH_INA219_IDLE && SENSOR_DHT11),
              "DHT isolation requires the INA219-idle bench build");
static_assert(!BENCH_CAN_DELAY_MS || (SENSOR_MPU6050 && !BENCH_INA219_IDLE && !BENCH_DHT11_DELAY_MS),
              "CAN isolation requires all sensors active");
static_assert(!BENCH_SERIALIZE_CAN_I2C || (SENSOR_MPU6050 && BENCH_CAN_DELAY_MS),
              "Bench CAN/I2C serialization requires the CAN delay target");

namespace config {

constexpr char kProjectName[] = "ESP32 CAN Condition Monitor Network";
constexpr char kFirmwareVersion[] = "0.5.0";
constexpr uint8_t kProtocolVersion = 1;

constexpr gpio_num_t kCanTxPin = GPIO_NUM_17;
constexpr gpio_num_t kCanRxPin = GPIO_NUM_16;
constexpr uint8_t kDhtDataPin = 4;
constexpr uint32_t kDhtReadPeriodMs = 2000;
// A minimum pause AFTER each blocking acquisition; not an exact sampling period.
constexpr uint32_t kInaReadPauseMs = 100;

constexpr uint32_t kHeartbeatPeriodMs = 1000;
constexpr uint32_t kConditionPeriodMs = 200;
constexpr uint32_t kMotionPowerPeriodMs = 100;
constexpr uint32_t kHeartbeatTimeoutMs = 2500;
constexpr uint32_t kDataTimeoutMs = 1000;
// Less than the fixed 500 ms setup delay, so pre-reset session markers expire.
constexpr uint32_t kSessionFreshnessMs = 250;

constexpr uint32_t kIdSession = 0x18FEFF01;
constexpr uint32_t kIdHeartbeat = 0x18FF0001;
constexpr uint32_t kIdCondition = 0x18FF1001;
constexpr uint32_t kIdMotionPower = 0x18FF1101;

constexpr uint8_t kRoleSensor = 1;
constexpr uint8_t kRoleDashboard = 2;

}  // namespace config

