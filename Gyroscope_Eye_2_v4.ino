// ESP32 Guide: https://RandomNerdTutorials.com/esp32-mpu-6050-accelerometer-gyroscope-arduino/
// ESP8266 Guide: https://RandomNerdTutorials.com/esp8266-nodemcu-mpu-6050-accelerometer-gyroscope-arduino/
// Arduino Guide: https://RandomNerdTutorials.com/arduino-mpu-6050-accelerometer-gyroscope-arduino/

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <TFT_eSPI.h>
#include "iris.h"

Adafruit_MPU6050 mpu;
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite eyeballSprite = TFT_eSprite(&tft);
TFT_eSprite pupilSprite   = TFT_eSprite(&tft);

// position stored 0..100, mapped to sprite placement (0..(240-140)=100 px)
int positionZ = 50; // horizontal (maps to x)
int positionX = 50; // vertical   (maps to y)

// gyro offsets from calibration
float gx_off = 0.0f;
float gy_off = 0.0f;
float gz_off = 0.0f;

void setup(void) {
  Serial.begin(115200);
  delay(100);

  // Start I2C. Change pins if your wiring differs (ESP32 default SDA=21, SCL=22).
  Wire.begin(21, 22);

  // Try to initialize MPU6050
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) { delay(10); }
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_5_HZ);

  // TFT init
  tft.init();
  tft.setRotation(2);
  tft.setSwapBytes(true);
  tft.fillScreen(TFT_WHITE);

  // Create sprites
  eyeballSprite.createSprite(240, 240);
  eyeballSprite.fillSprite(TFT_WHITE);

  pupilSprite.createSprite(140,140);
  pupilSprite.setSwapBytes(true);
  pupilSprite.pushImage(0,0,140,140,iris);

  // initial draw: pupil centered
  pupilSprite.pushToSprite(&eyeballSprite, 50, 50, TFT_WHITE);
  eyeballSprite.pushSprite(0,0);

  // Calibrate gyro offsets (keep board still during this)
  calibrateGyro();
}

void loop() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Subtract offsets to get zero-centered gyro (degrees/sec)
  float gx = g.gyro.x - gx_off;
  float gy = g.gyro.y - gy_off;
  float gz = g.gyro.z - gz_off;

  plotPupil(gx, gy, gz);

  delay(20); // ~50 Hz
}

// Calibrate gyro by averaging samples while stationary
void calibrateGyro(uint16_t samples = 300, uint16_t delayMs = 5) {
  Serial.println("Calibrating gyro... keep board still");
  double sx = 0, sy = 0, sz = 0;
  for (uint16_t i = 0; i < samples; ++i) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    sx += g.gyro.x;
    sy += g.gyro.y;
    sz += g.gyro.z;
    delay(delayMs);
  }
  gx_off = float(sx / samples);
  gy_off = float(sy / samples);
  gz_off = float(sz / samples);
  Serial.print("Gyro offsets: ");
  Serial.print(gx_off, 4); Serial.print(", ");
  Serial.print(gy_off, 4); Serial.print(", ");
  Serial.println(gz_off, 4);
}

// Move the pupil. gyro values in degrees/sec (float)
void plotPupil(float gyro_x, float gyro_y, float gyro_z)
{
  // Debug
  Serial.print("Gx:"); Serial.print(gyro_x, 3);
  Serial.print(" Gy:"); Serial.print(gyro_y, 3);
  Serial.print(" Gz:"); Serial.println(gyro_z, 3);

  // Invert axes to match desired direction (adjust if needed)
  gyro_z = -gyro_z; // horizontal
  gyro_x = -gyro_x; // vertical

  // Tunable params
  const float deadzone = 0.08f;     // degrees/sec considered as zero
  const float zScale   = 18.0f;     // how much gz moves pupil horizontally
  const float xScale   = 20.0f;     // how much gx moves pupil vertically
  const float relax    = 0.75f;     // return-to-center smoothing when no movement

  // Update horizontal (Z axis -> X position on screen)
  if (fabs(gyro_z) < deadzone) {
    positionZ = 50 + int((positionZ - 50) * relax);
  } else {
    // scale down because gyro values are degrees/sec
    positionZ = positionZ + int(gyro_z * zScale / 100.0f);
    positionZ = constrain(positionZ, 0, 100);
  }

  // Update vertical (X axis -> Y position on screen)
  if (fabs(gyro_x) < deadzone) {
    positionX = 50 + int((positionX - 50) * relax);
  } else {
    positionX = positionX + int(gyro_x * xScale / 100.0f);
    positionX = constrain(positionX, 0, 100);
  }

  // Redraw
  eyeballSprite.fillSprite(TFT_WHITE);

  // Ensure pupil image is current and byte order set
  pupilSprite.setSwapBytes(true);
  pupilSprite.pushImage(0, 0, 140, 140, iris);

  // Map 0..100 to 0..100 pixel offset inside eyeball sprite (240-140=100)
  int px = map(positionZ, 0, 100, 0, 100);
  int py = map(positionX, 0, 100, 0, 100);

  pupilSprite.pushToSprite(&eyeballSprite, px, py, TFT_WHITE);

  eyeballSprite.pushSprite(0, 0);
}
