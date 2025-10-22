#include <Adafruit_LIS3DH.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_Sensor.h>
#include <NimBLEDevice.h>
#include <SPI.h>
#include <Wire.h>

NimBLEServer* pServer;
NimBLECharacteristic* pCharacteristic;

bool deviceConnected = false;
int cap = 0;
int capPin = A0;

Adafruit_LIS3DH lis(&Wire1);  // using Stemma QT port
Adafruit_NeoPixel pixels(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

#define CLICKTHRESHHOLD 100

void setup() {
  Serial.begin(115200);    // USB debug
  Serial1.begin(115200);   // HW UART for Teensy link (TX pin on ESP → RX on Teensy)
  delay(200);

  // --- BLE init ---
  NimBLEDevice::init(""); 
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);  

  pServer = NimBLEDevice::createServer();
  NimBLEService* pService = pServer->createService("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
  pCharacteristic = pService->createCharacteristic(
      "6E400003-B5A3-F393-E0A9-E50E24DCCA9E", NIMBLE_PROPERTY::NOTIFY);
  pService->start();

  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(pService->getUUID());
  NimBLEAdvertisementData advData;
  advData.setName("TapSensor");
  advData.setFlags(0x06);
  pAdvertising->setAdvertisementData(advData);
  pAdvertising->start();
  Serial.println("Advertising as TapSensor...");

  // --- LIS3DH init ---
  pixels.begin();
  pixels.setBrightness(20);

  if (!lis.begin(0x18)) {
    Serial.println("Couldnt start LIS3DH");
    while (1) yield();
  }
  Serial.println("LIS3DH found!");
  lis.setRange(LIS3DH_RANGE_2_G);
  lis.setClick(2, CLICKTHRESHHOLD);
  delay(100);
}

void loop() {
  // --- Tap detection ---
  uint8_t click = lis.getClick();
  const char* tapStr = "";

  if (click & 0x20) {      // double tap
    pixels.fill(0x000000);
    pixels.show();
    tapStr = "double";
  } else if (click & 0x10) {  // single tap
    pixels.fill(0xFF0000);
    pixels.show();
    tapStr = "single";
  }

  // --- Acceleration ---
  sensors_event_t accelEvent;
  lis.getEvent(&accelEvent);

  // --- Capacitive input ---
  int cap = digitalRead(capPin);

  // --- Build JSON ---
  char packet[128];
  snprintf(packet, sizeof(packet),
           "{\"tap\":\"%s\",\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"cap\":%d}",
           tapStr,
           accelEvent.acceleration.x,
           accelEvent.acceleration.y,
           accelEvent.acceleration.z,
           cap);

  // Send via BLE
  pCharacteristic->setValue((uint8_t*)packet, strlen(packet));
  pCharacteristic->notify();

  // Send via UART to Teensy
  Serial1.println(packet);   // Teensy reads this line

  // Also print to USB debug
  Serial.println(packet);

  delay(100);  // ~10 Hz
}
