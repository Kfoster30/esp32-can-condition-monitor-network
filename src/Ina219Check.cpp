#if defined(INA219_WIRING_CHECK) && INA219_WIRING_CHECK

#include <Arduino.h>
#include <Wire.h>

// Isolated wiring diagnostic: no CAN, DHT task, current calculation, or load.
// This file is otherwise empty in the normal 0.3.0 application builds.
namespace {
constexpr uint8_t kAddress = 0x40;
constexpr uint8_t kConfigRegister = 0x00;
constexpr int kSdaPin = 21;
constexpr int kSclPin = 22;
constexpr uint32_t kClockHz = 100000;
constexpr uint16_t kTimeoutMs = 25;
bool i2cReady = false;

void probeConfiguration() {
  if (!i2cReady) {
    Serial.println("#ina219_check,status=I2C_INIT_FAILED");
    return;
  }

  // Select a register pointer only. Do not write configuration/calibration data.
  Wire.beginTransmission(kAddress);
  if (Wire.write(kConfigRegister) != 1) {
    Wire.endTransmission(true);
    Serial.println("#ina219_check,address=0x40,status=POINTER_BUFFER_FAILED");
    return;
  }
  const uint8_t error = Wire.endTransmission(true);
  if (error != 0) {
    Serial.printf("#ina219_check,address=0x40,status=REGISTER_SELECT_FAILED,code=%u\n",
                  error);
    return;
  }

  const size_t received = Wire.requestFrom(kAddress, static_cast<size_t>(2), true);
  if (received != 2 || Wire.available() < 2) {
    while (Wire.available() > 0) Wire.read();
    Serial.printf("#ina219_check,address=0x40,status=SHORT_READ,bytes=%u\n",
                  static_cast<unsigned int>(received));
    return;
  }
  const uint8_t high = static_cast<uint8_t>(Wire.read());
  const uint8_t low = static_cast<uint8_t>(Wire.read());
  const uint16_t configuration = static_cast<uint16_t>(
      (static_cast<uint16_t>(high) << 8) | low);
  // A complete register read is communication evidence, not a unique chip ID
  // or proof of correct voltage/current measurement or shunt calibration.
  Serial.printf("#ina219_check,address=0x40,status=REGISTER_READ,config=0x%04X\n",
                static_cast<unsigned int>(configuration));
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("#diagnostic=INA219_WIRING_CHECK,revision=1");
  Serial.println("#role=SENSOR_DIAGNOSTIC,can_enabled=0,dht_enabled=0");
  Serial.println("#measurement_validation=NOT_PERFORMED,vin_terminals=LEAVE_DISCONNECTED");
  Serial.println("#i2c,sda=21,scl=22,clock_hz=100000,timeout_ms=25,address=0x40");
  i2cReady = Wire.begin(kSdaPin, kSclPin, kClockHz);
  if (i2cReady) Wire.setTimeOut(kTimeoutMs);
}

void loop() {
  probeConfiguration();
  delay(2000);
}

#endif  // INA219_WIRING_CHECK
