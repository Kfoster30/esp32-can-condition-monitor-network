#if defined(MPU6050_MEASUREMENT_CHECK) && MPU6050_MEASUREMENT_CHECK

#include "Mpu6050Acquisition.hpp"

namespace {
sensor_i2c::Bus bus;
mpu6050::Reader reader(bus);
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
#if defined(MPU6050_MOTION_CHECK) && MPU6050_MOTION_CHECK
  Serial.println("#diagnostic=MPU6050_MOTION_CHECK,revision=1");
#else
  Serial.println("#diagnostic=MPU6050_ACCELERATION_CHECK,revision=1");
#endif
  Serial.println("#role=SENSOR_DIAGNOSTIC,can_enabled=0,dht_enabled=0,ina_reads=0");
  Serial.println("#profile,address=0x68,range_g=4,lsb_per_g=8192,dlpf_cfg=3,sample_div=9");
#if defined(MPU6050_MOTION_CHECK) && MPU6050_MOTION_CHECK
  Serial.println("#metric=RMS_ABOUT_WINDOW_MEAN,units=mg,changing_tilt_included=1");
  Serial.println("type,t_ms,status,sample_count,span_ms,motion_rms_mg");
#else
  Serial.println("#units=g,gravity_included=1,calibration=NOT_VERIFIED,vibration_rms=NOT_COMPUTED");
  Serial.println("#acquisition=FRESH_DATA_READY_BURST,min_pause_ms=200,ina_settings=UNCHANGED");
  Serial.println("type,t_ms,status,ax_g,ay_g,az_g,norm_g");
#endif
  bus.begin();
}

void loop() {
  const bool wasConfigured = reader.configured();
#if defined(MPU6050_MOTION_CHECK) && MPU6050_MOTION_CHECK
  mpu6050::Window window = {};
  const char* reason = reader.window(window);
#else
  float axes[3] = {};
  const char* reason = reader.sample(axes);
#endif
  if (!wasConfigured && reader.configured()) {
    Serial.println("#mpu6050_measure,status=CONFIGURED");
  }
  if (reason != nullptr) reader.handleFailure(reason);
#if defined(MPU6050_MOTION_CHECK) && MPU6050_MOTION_CHECK
  if (reason == nullptr) {
    Serial.printf("mpu6050_motion,%lu,HW_WINDOW,%u,%lu,%.2f\n",
                  millis(), static_cast<unsigned>(window.sampleCount),
                  static_cast<unsigned long>(window.spanMs), window.rmsMg);
  } else {
    Serial.printf("mpu6050_motion,%lu,%s,0,0,nan\n", millis(), reason);
    delay(1000);
  }
#else
  if (reason == nullptr) {
    const float magnitude = sqrtf(axes[0] * axes[0] + axes[1] * axes[1] +
                                  axes[2] * axes[2]);
    Serial.printf("mpu6050_measure,%lu,HW_SAMPLE,%.4f,%.4f,%.4f,%.4f\n",
                  millis(), axes[0], axes[1], axes[2], magnitude);
  } else {
    Serial.printf("mpu6050_measure,%lu,%s,nan,nan,nan,nan\n", millis(), reason);
  }
  delay(reader.configured() ? 200 : 1000);
#endif
}

#endif  // MPU6050_MEASUREMENT_CHECK
