#include <Adafruit_LSM6DSO32.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_Sensor.h>
#include <NimBLEDevice.h>
#include <Wire.h>
#include <math.h>

// ---------- BLE ----------
NimBLEServer* pServer;
NimBLECharacteristic* pCharacteristic;

// ---------- IO ----------
const int capPin = A0;
Adafruit_NeoPixel pixels(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// ---------- IMU ----------
Adafruit_LSM6DSO32 imu;
#define IMU_ADDR 0x6A

// ---------- Orientation detection ----------
enum Face : uint8_t { X_NEG=0, X_POS=1, Y_NEG=2, Y_POS=3, Z_NEG=4, Z_POS=5, UNKNOWN=255 };
const char* faceName(Face f) {
  switch(f) {
    case X_NEG: return "X-"; case X_POS: return "X+";
    case Y_NEG: return "Y-"; case Y_POS: return "Y+";
    case Z_NEG: return "Z-"; case Z_POS: return "Z+";
    default:    return "Unknown";
  }
}

// --- new: helper for colors ---
uint32_t faceColor(Face f) {
  switch (f) {
    case X_NEG: return 0x00FF66;  // bright green-cyan
    case X_POS: return 0x00A3FF;  // sky blue
    case Y_NEG: return 0x009999;  // deep teal
    case Y_POS: return 0x00FFF0;  // pale aqua-white
    case Z_NEG: return 0x007755;  // dark greenish teal
    case Z_POS: return 0x33FFFF;  // electric cyan
    default:    return 0x000000;  // off
  }
}

Face currentFace = UNKNOWN;
Face lastFace = UNKNOWN;
uint32_t faceSince = 0;

Face detectFace(float ax, float ay, float az) {
  float abx=fabsf(ax), aby=fabsf(ay), abz=fabsf(az);
  if (abx > aby && abx > abz) return (ax > 0 ? X_POS : X_NEG);
  if (aby > abx && aby > abz) return (ay > 0 ? Y_POS : Y_NEG);
  return (az > 0 ? Z_POS : Z_NEG);
}

// ---------- Timers ----------
uint32_t lastSendMs = 0;
char packet[256];

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);
  delay(150);

  // BLE
  NimBLEDevice::init("");
  NimBLEDevice::setPower(ESP_PWR_LVL_P3);
  pServer = NimBLEDevice::createServer();
  NimBLEService* pService = pServer->createService("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
  pCharacteristic = pService->createCharacteristic(
      "6E400003-B5A3-F393-E0A9-E50E24DCCA9E", NIMBLE_PROPERTY::NOTIFY);
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
      Serial.println("Failed to find LSM6DSO32.");
      while (1) delay(10);
    }
  }
  imu.setAccelRange(LSM6DSO32_ACCEL_RANGE_8_G);
  imu.setGyroRange(LSM6DS_GYRO_RANGE_500_DPS);
  imu.setAccelDataRate(LSM6DS_RATE_104_HZ);
  imu.setGyroDataRate(LSM6DS_RATE_104_HZ);

  pinMode(capPin, INPUT_PULLUP);
  Serial.println("IMU ready. Starting simple 6D detection...");
}

void loop() {
  sensors_event_t accel, gyro, temp;
  imu.getEvent(&accel, &gyro, &temp);
  yield();

  int cap = digitalRead(capPin);

  float ax = accel.acceleration.x;
  float ay = accel.acceleration.y;
  float az = accel.acceleration.z;

  Face f = detectFace(ax, ay, az);
  uint32_t now = millis();

  if (f != currentFace) {
    if (f != lastFace) {
      lastFace = f;
      faceSince = now;
    }
    if (now - faceSince > 200) { // dwell filter
      currentFace = f;
      Serial.print("[6D] Face = ");
      Serial.println(faceName(currentFace));

      // --- update LED color based on face ---
      pixels.fill(faceColor(currentFace));
      pixels.show();
    }
  }

  if (now - lastSendMs >= 200) { // 5 Hz
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
