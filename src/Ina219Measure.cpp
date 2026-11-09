#if defined(INA219_MEASUREMENT_CHECK) && INA219_MEASUREMENT_CHECK

#include <Arduino.h>
#include <Wire.h>

#include "Ina219Diagnostic.hpp"

namespace {
constexpr uint8_t kAddress = 0x40;
constexpr uint8_t kConfigRegister = 0;
constexpr uint8_t kShuntRegister = 1;
constexpr uint8_t kBusRegister = 2;
constexpr uint8_t kCurrentRegister = 4;
constexpr uint8_t kCalibrationRegister = 5;
bool i2cReady = false;

bool writeRegister(uint8_t reg, uint16_t value) {
  Wire.beginTransmission(kAddress);
  const size_t pointerBytes = Wire.write(reg);
  const size_t highBytes = Wire.write(static_cast<uint8_t>(value >> 8));
  const size_t lowBytes = Wire.write(static_cast<uint8_t>(value & 0xFFu));
  const uint8_t error = Wire.endTransmission(true);
  return pointerBytes == 1 && highBytes == 1 && lowBytes == 1 && error == 0;
}

bool readRegister(uint8_t reg, uint16_t& value) {
  Wire.beginTransmission(kAddress);
  const size_t written = Wire.write(reg);
  const uint8_t error = Wire.endTransmission(true);
  if (written != 1 || error != 0) return false;
  const size_t received = Wire.requestFrom(kAddress, static_cast<size_t>(2), true);
  if (received != 2 || Wire.available() < 2) {
    while (Wire.available() > 0) Wire.read();
    return false;
  }
  const int high = Wire.read();
  const int low = Wire.read();
  if (high < 0 || low < 0) return false;
  value = static_cast<uint16_t>((static_cast<uint16_t>(high) << 8) | low);
  return true;
}

void unavailable(const char* reason) {
  // Never retain or substitute a numeric value after a failed acquisition.
  Serial.printf("ina219_measure,%lu,%s,nan,nan,nan\n", millis(), reason);
}

void measure() {
  if (!i2cReady) {
    unavailable("I2C_INIT_FAILED");
    return;
  }
  // Program a known scale and trigger a new paired acquisition on EVERY sample.
  if (!writeRegister(kCalibrationRegister, ina219diag::kCalibration) ||
      !writeRegister(kConfigRegister, ina219diag::kConfiguration)) {
    unavailable("WRITE_FAILED");
    return;
  }

  uint16_t bus = 0;
  const uint32_t started = millis();
  do {
    delay(2);
    if (!readRegister(kBusRegister, bus)) {
      unavailable("READ_FAILED");
      return;
    }
    if (ina219diag::conversionReady(bus)) break;
  } while (millis() - started < ina219diag::kConversionTimeoutMs);
  if (!ina219diag::conversionReady(bus)) {
    unavailable("CONVERSION_TIMEOUT");
    return;
  }

  uint16_t shunt = 0, current = 0, configuration = 0, calibration = 0;
  if (!readRegister(kShuntRegister, shunt) ||
      !readRegister(kCurrentRegister, current) ||
      !readRegister(kConfigRegister, configuration) ||
      !readRegister(kCalibrationRegister, calibration)) {
    unavailable("READ_FAILED");
    return;
  }

  switch (ina219diag::classify(bus, current, shunt, configuration, calibration)) {
    case ina219diag::Status::kSettingsMismatch:
      unavailable("SETTINGS_MISMATCH"); return;
    case ina219diag::Status::kOverflow:
      unavailable("OVERFLOW"); return;
    case ina219diag::Status::kNotReady:
      unavailable("CONVERSION_NOT_READY"); return;
    case ina219diag::Status::kRange:
      unavailable("OUT_OF_BENCH_RANGE"); return;
    case ina219diag::Status::kSample:
      break;
  }

  // HW_SAMPLE means checked register acquisition, NOT reference-accuracy PASS.
  // Bus voltage is VIN- relative to GND; current is only the measured LED branch.
  Serial.printf("ina219_measure,%lu,HW_SAMPLE,%.3f,%.3f,%.3f\n", millis(),
                ina219diag::busMillivolts(bus) / 1000.0,
                ina219diag::currentMicroamps(current) / 1000.0,
                ina219diag::shuntMicrovolts(shunt) / 1000.0);
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("#diagnostic=INA219_MEASUREMENT_CHECK,revision=1");
  Serial.println("#role=SENSOR_DIAGNOSTIC,can_enabled=0,dht_enabled=0");
  Serial.println("#profile,r_shunt_ohm=0.1,calibration=8192,configuration=0x019B,triggered=1");
  Serial.println("#reference_accuracy=NOT_VERIFIED,bench_supply=3V3,resistor_ohm=331");
  Serial.println("type,t_ms,status,bus_V,current_mA,shunt_mV");
  i2cReady = Wire.begin(21, 22, 100000);
  if (i2cReady) Wire.setTimeOut(25);
}

void loop() {
  measure();
  delay(1000);
}

#endif  // INA219_MEASUREMENT_CHECK
