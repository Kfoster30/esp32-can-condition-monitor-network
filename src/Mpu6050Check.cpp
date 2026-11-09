#if defined(MPU6050_WIRING_CHECK) && MPU6050_WIRING_CHECK

#include <Arduino.h>
#include <Wire.h>

// Temporary sensor diagnostic. Register-pointer selection writes only;
// no wake/reset/configuration/calibration writes, CAN, DHT, or motion samples.
namespace {
constexpr uint8_t kMpuAddress = 0x68;  // AD0 externally tied to sensor GND.
constexpr uint8_t kWhoAmIRegister = 0x75;
constexpr uint8_t kExpectedWhoAmI = 0x68;
constexpr uint8_t kInaAddress = 0x40;
constexpr uint8_t kInaConfigRegister = 0x00;
bool i2cReady = false;

bool readRegister(const char* device, uint8_t address, uint8_t reg,
                  uint8_t* bytes, size_t length) {
  Wire.beginTransmission(address);
  if (Wire.write(reg) != 1) {
    Wire.endTransmission(true);
    Serial.printf("#%s,address=0x%02X,status=POINTER_BUFFER_FAILED\n",
                  device, static_cast<unsigned int>(address));
    return false;
  }
  const uint8_t error = Wire.endTransmission(false);  // Repeated-start read.
  if (error != 0) {
    Serial.printf("#%s,address=0x%02X,status=REGISTER_SELECT_FAILED,code=%u\n",
                  device, static_cast<unsigned int>(address),
                  static_cast<unsigned int>(error));
    return false;
  }
  const size_t received = Wire.requestFrom(address, length, true);
  if (received != length || Wire.available() != static_cast<int>(length)) {
    while (Wire.available() > 0) Wire.read();
    Serial.printf("#%s,address=0x%02X,status=SHORT_READ,bytes=%u\n",
                  device, static_cast<unsigned int>(address),
                  static_cast<unsigned int>(received));
    return false;
  }
  for (size_t index = 0; index < length; ++index) {
    const int value = Wire.read();
    if (value < 0) {
      Serial.printf("#%s,address=0x%02X,status=READ_FAILED\n",
                    device, static_cast<unsigned int>(address));
      return false;
    }
    bytes[index] = static_cast<uint8_t>(value);
  }
  return true;
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("#diagnostic=MPU6050_SHARED_I2C_CHECK,revision=1");
  Serial.println("#role=SENSOR_DIAGNOSTIC,can_enabled=0,dht_enabled=0");
  Serial.println("#validation=COMMUNICATION_ONLY,motion_validation=NOT_PERFORMED");
  Serial.println("#writes=REGISTER_POINTER_ONLY,device_settings=UNCHANGED");
  Serial.println("#i2c,sda=21,scl=22,clock_hz=100000,timeout_ms=25,mpu_ad0=GND");
  i2cReady = Wire.begin(21, 22, 100000);
  if (i2cReady) Wire.setTimeOut(25);
}

void loop() {
  if (!i2cReady) {
    Serial.println("#mpu6050_check,status=I2C_INIT_FAILED");
    delay(2000);
    return;
  }
  uint8_t who = 0;
  if (readRegister("mpu6050_check", kMpuAddress, kWhoAmIRegister, &who, 1)) {
    // A matching byte supports register compatibility, not genuine-chip proof.
    Serial.printf("#mpu6050_check,address=0x68,status=%s,who_am_i=0x%02X\n",
                  who == kExpectedWhoAmI ? "ID_MATCH" : "ID_MISMATCH",
                  static_cast<unsigned int>(who));
  }
  uint8_t config[2] = {0, 0};
  if (readRegister("ina219_check", kInaAddress, kInaConfigRegister, config, 2)) {
    const uint16_t value = static_cast<uint16_t>(
        (static_cast<uint16_t>(config[0]) << 8) | config[1]);
    Serial.printf("#ina219_check,address=0x40,status=REGISTER_READ,config=0x%04X\n",
                  static_cast<unsigned int>(value));
  }
  delay(2000);
}

#endif  // MPU6050_WIRING_CHECK
