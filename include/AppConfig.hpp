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

static_assert((ECU_ROLE_SENSOR + ECU_ROLE_DASHBOARD) == 1,
              "Select exactly one ECU role");

namespace config {

constexpr char kProjectName[] = "ESP32 CAN Condition Monitor Network";
constexpr char kFirmwareVersion[] = "0.1.0";
constexpr uint8_t kProtocolVersion = 1;

constexpr gpio_num_t kCanTxPin = GPIO_NUM_17;
constexpr gpio_num_t kCanRxPin = GPIO_NUM_16;

constexpr uint32_t kHeartbeatPeriodMs = 1000;
constexpr uint32_t kConditionPeriodMs = 200;
constexpr uint32_t kMotionPowerPeriodMs = 100;
constexpr uint32_t kHeartbeatTimeoutMs = 2500;
constexpr uint32_t kDataTimeoutMs = 1000;

constexpr uint32_t kIdHeartbeat = 0x18FF0001;
constexpr uint32_t kIdCondition = 0x18FF1001;
constexpr uint32_t kIdMotionPower = 0x18FF1101;

constexpr uint8_t kRoleSensor = 1;
constexpr uint8_t kRoleDashboard = 2;

}  // namespace config

