#include <Adafruit_LSM6DSO32.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_Sensor.h>
#include <NimBLEDevice.h>
#include <Wire.h>
#include <math.h>

// BLE
NimBLEServer* pServer;
NimBLECharacteristic* pCharacteristic;

// IO
const int capPin = A0;
Adafruit_NeoPixel pixels(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// IMU
Adafruit_LSM6DSO32 imu;
#define IMU_ADDR 0x6A

// Faces
enum Face : uint8_t { X_NEG=0, X_POS=1, Y_NEG=2, Y_POS=3, Z_NEG=4, Z_POS=5, UNKNOWN=255 };

const char* faceName(Face f) {
  switch (f) {
    case X_NEG: return "X-"; case X_POS: return "X+";
    case Y_NEG: return "Y-"; case Y_POS: return "Y+";
    case Z_NEG: return "Z-"; case Z_POS: return "Z+";
    default:    return "Unknown";
  }
}

uint32_t faceColor(Face f) {
  switch (f) {
    case X_NEG: return 0xFF0000;  // red
    case X_POS: return 0x0000FF;  // blue
    case Y_NEG: return 0x00FF00;  // green
    case Y_POS: return 0xFFFF00;  // yellow
    case Z_NEG: return 0xFF00FF;  // magenta
    case Z_POS: return 0x00FFFF;  // cyan
    default:    return 0x000000;  // off
  }
}

// Parameters
static const float HP_ALPHA = 0.97f;     // motion high-pass
static const float LP_ALPHA = 0.10f;     // gravity low-pass
static const uint32_t FACE_DWELL_MS = 150;
static const float FACE_MARGIN = 2.0f;

Face currentFace = UNKNOWN;
Face pendingFace = UNKNOWN;
uint32_t faceStamp = 0;

// High-pass helper
struct HighPass {
  float xin = 0, yout = 0;
  float a;
  HighPass(float alpha=0.97f): a(alpha) {}
  float update(float x) { float y = a * (yout + x - xin); xin = x; yout = y; return y; }
};

HighPass hpx(HP_ALPHA), hpy(HP_ALPHA), hpz(HP_ALPHA);

// Low-pass helper
struct LowPass {
  float y = 0, inited = false;
  float a;
  LowPass(float alpha=0.10f): a(alpha) {}
  float update(float x) {
    if (!inited) { y = x; inited = true; return y; }
    y += a * (x - y);
    return y;
  }
};

LowPass gxx(LP_ALPHA), gyy(LP_ALPHA), gzz(LP_ALPHA);

// BLE timing
uint32_t lastSendMs = 0;
char packet[256];

// Face detection from smoothed gravity
Face faceFromGravity(float gx, float gy, float gz) {
  float ax = fabsf(gx), ay = fabsf(gy), az = fabsf(gz);

  // Compare axes with a margin to avoid jitter
  float vals[3] = {ax, ay, az};
  float max1 = ax, max2 = ay, max3 = az;
  if (max2 > max1) { float t=max1; max1=max2; max2=t; }
  if (max3 > max1) { float t=max1; max1=max3; max3=t; }
  if (max3 > max2) { float t=max2; max2=max3; max3=t; }

  if ((max1 - max2) < FACE_MARGIN) return UNKNOWN;

  if (ax > ay && ax > az) return (gx > 0 ? X_POS : X_NEG);
  if (ay > ax && ay > az) return (gy > 0 ? Y_POS : Y_NEG);
  return (gz > 0 ? Z_POS : Z_NEG);
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);
  delay(150);

  // BLE
  NimBLEDevice::init("");
  NimBLEDevice::setPower(ESP_PWR_LVL_P3);
  pServer = NimBLEDevice::createServer();
  NimBLEService* pService = pServer->createService("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
  pCharacteristic = pService->createCharacteristic("6E400003-B5A3-F393-E0A9-E50E24DCCA9E", NIMBLE_PROPERTY::NOTIFY);
  pService->start();
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(pService->getUUID());
  NimBLEAdvertisementData advData;
  advData.setName("MotionSensor");
  advData.setFlags(0x06);
  adv->setAdvertisementData(advData);
  adv->start();

  // LED
  pixels.begin();
  pixels.setBrightness(20);
  pixels.clear();
  pixels.show();

  // IMU
  Wire1.begin();
  if (!imu.begin_I2C(IMU_ADDR, &Wire1)) {
    if (!imu.begin_I2C(0x6B, &Wire1)) {
      Serial.println("No LSM6DSO32.");
      while (1) delay(10);
    }
  }
  imu.setAccelRange(LSM6DSO32_ACCEL_RANGE_8_G);
  imu.setGyroRange(LSM6DS_GYRO_RANGE_500_DPS);
  imu.setAccelDataRate(LSM6DS_RATE_104_HZ);
  imu.setGyroDataRate(LSM6DS_RATE_104_HZ);

  pinMode(capPin, INPUT_PULLUP);
}

void loop() {
  sensors_event_t accel, gyro, temp;
  imu.getEvent(&accel, &gyro, &temp);

  int cap = digitalRead(capPin);
  uint32_t now = millis();

  // Raw accel (with gravity)
  float rawAx = accel.acceleration.x;
  float rawAy = accel.acceleration.y;
  float rawAz = accel.acceleration.z;

  // Smoothed gravity for face detection
  float gX = gxx.update(rawAx);
  float gY = gyy.update(rawAy);
  float gZ = gzz.update(rawAz);

  // Motion-only accel for Teensy
  float ax = hpx.update(rawAx);
  float ay = hpy.update(rawAy);
  float az = hpz.update(rawAz);

  // Face logic
  Face candidate = faceFromGravity(gX, gY, gZ);

  if (candidate != UNKNOWN) {
    if (candidate != pendingFace) {
      pendingFace = candidate;
      faceStamp = now;
    } else if ((now - faceStamp) >= FACE_DWELL_MS && candidate != currentFace) {
      currentFace = candidate;
      pixels.fill(faceColor(currentFace));
      pixels.show();
    }
  }

  // BLE data out (5 Hz)
  if ((now - lastSendMs) >= 200) {
    int n = snprintf(packet, sizeof(packet),
                     "{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,"
                     "\"gx\":%.2f,\"gy\":%.2f,\"gz\":%.2f,"
                     "\"cap\":%d,\"face\":%u}",
                     ax, ay, az,
                     gyro.gyro.x, gyro.gyro.y, gyro.gyro.z,
                     cap, (unsigned)currentFace);
    if (n > 0) {
      pCharacteristic->setValue((uint8_t*)packet, (size_t)n);
      pCharacteristic->notify();
      Serial1.println(packet);
    }
    lastSendMs = now;
  }

  delay(1);
}
