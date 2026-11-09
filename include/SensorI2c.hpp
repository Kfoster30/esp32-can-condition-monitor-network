#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace sensor_i2c {
class Bus {
 public:
  void begin() {
    mutex_ = xSemaphoreCreateMutex();
    ready_ = mutex_ != nullptr && Wire.begin(21, 22, 100000);
    if (ready_) Wire.setTimeOut(25);
  }

  bool ready() const { return ready_; }

 private:
  friend class Lock;
  SemaphoreHandle_t mutex_ = nullptr;
  bool ready_ = false;
};

// Keep Wire's shared receive buffer protected through the final Wire.read().
// Readers acquire this only after checking that Bus::begin succeeded.
class Lock {
 public:
  explicit Lock(Bus& bus, TickType_t wait = portMAX_DELAY) : mutex_(bus.mutex_) {
    if (xSemaphoreTake(mutex_, wait) != pdTRUE) mutex_ = nullptr;
  }
  ~Lock() {
    if (mutex_ != nullptr) xSemaphoreGive(mutex_);
  }
  bool acquired() const { return mutex_ != nullptr; }

  Lock(const Lock&) = delete;
  Lock& operator=(const Lock&) = delete;

 private:
  SemaphoreHandle_t mutex_;
};
}  // namespace sensor_i2c
