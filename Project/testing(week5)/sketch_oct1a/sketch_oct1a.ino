//made with AI help, this demo allow for connecting to the computer using WebBluetooth. See the html file to have the website.

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_NeoPixel.h>
#include <NimBLEDevice.h>


NimBLEServer* pServer;
NimBLECharacteristic* pCharacteristic;
bool deviceConnected = false;

Adafruit_LIS3DH lis = Adafruit_LIS3DH(&Wire1);//use "&Wire1" when using the stemma qt port connection, different pins for I2C
Adafruit_NeoPixel pixels(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

#define CLICKTHRESHHOLD 100


void setup(void) {
  Serial.begin(115200);
  delay(200);

  // --- BLE init ---
  NimBLEDevice::init("");                   // empty here; we set the name in adv data below
  // Optional but helpful: crank TX power for easier discovery
  // If this line errors on your version, just comment it out.
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);   // or ESP_PWR_LVL_P7

  pServer = NimBLEDevice::createServer();

  // Nordic UART service + TX characteristic (notify)
  NimBLEService* pService = pServer->createService("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
  pCharacteristic = pService->createCharacteristic(
      "6E400003-B5A3-F393-E0A9-E50E24DCCA9E",
      NIMBLE_PROPERTY::NOTIFY
  );

  pService->start();

  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(pService->getUUID());  // advertise the service UUID

  // Build explicit advertising payload with a name + flags.
  NimBLEAdvertisementData advData;
  advData.setName("TapSensor");             // <— shows up in scanners/UI
  advData.setFlags(0x06);                   // General Disc + BR/EDR Not Supported
  pAdvertising->setAdvertisementData(advData);



  pAdvertising->start();
  Serial.println("Advertising as TapSensor...");

  // --- your existing setup below ---
  pixels.begin();
  pixels.setBrightness(20);

  Serial.println("Adafruit LIS3DH Tap Test!");
  if (!lis.begin(0x18)) {
    Serial.println("Couldnt start LIS3DH");
    // BLE is already advertising, so you can still connect even if LIS3DH fails.
    while (1) yield();
  }
  Serial.println("LIS3DH found!");
  lis.setRange(LIS3DH_RANGE_2_G);
  lis.setClick(2, CLICKTHRESHHOLD);
  delay(100);
}


void loop() {
  // --- Read tap/click events ---
  uint8_t click = lis.getClick();

  if (click & 0x20) {   // double tap
    Serial.println("double click");
    pixels.fill(0x000000);
    pixels.show();
    pCharacteristic->setValue("double");
    pCharacteristic->notify();
  } 
  else if (click & 0x10) {   // single tap
    Serial.println("single click");
    pixels.fill(0xFF0000);
    pixels.show();
    pCharacteristic->setValue("single");
    pCharacteristic->notify();
  }

  // --- Read live acceleration ---
  sensors_event_t accelEvent;
  lis.getEvent(&accelEvent);

  char buffer[64];
  snprintf(buffer, sizeof(buffer),
           "X: %.2f Y: %.2f Z: %.2f",
           accelEvent.acceleration.x,
           accelEvent.acceleration.y,
           accelEvent.acceleration.z);

 
  pCharacteristic->setValue((uint8_t*)buffer, strlen(buffer));
  pCharacteristic->notify();

  delay(100);  // update rate ~10 Hz
}
