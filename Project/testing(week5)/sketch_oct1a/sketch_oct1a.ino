// Basic demo for tap/doubletap readings from Adafruit LIS3DH modified by me to work with the built in LED of my mictrocontroller

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

   // Init BLE
  NimBLEDevice::init("TapSensor");
  pServer = NimBLEDevice::createServer();

  // BLE UART service UUIDs (standard Nordic UART)
  NimBLEService* pService = pServer->createService("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
  pCharacteristic = pService->createCharacteristic(
      "6E400003-B5A3-F393-E0A9-E50E24DCCA9E",
      NIMBLE_PROPERTY::NOTIFY);
  
  pService->start();
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(pService->getUUID());
  NimBLEDevice::startAdvertising();


  pixels.begin(); 
  pixels.setBrightness(20);
  Serial.begin(9600);
    Serial.println("Adafruit LIS3DH Tap Test!");
  
  if (! lis.begin(0x18)) {   
    Serial.println("Couldnt start");
    while (1) yield();
  }
  Serial.println("LIS3DH found!");
  
  lis.setRange(LIS3DH_RANGE_2_G);

  // 0 = turn off click detection & interrupt
  // 1 = single click only interrupt output
  // 2 = double click only interrupt output, detect single click
  // Adjust threshhold, higher numbers are less sensitive
  lis.setClick(2, CLICKTHRESHHOLD);
  delay(100);
}


void loop() {
  uint8_t click = lis.getClick();
  if (click == 0) return;

  if (click & 0x20) {
    Serial.println("double click");
    pixels.fill(0x000000);
    pixels.show();
    pCharacteristic->setValue("double");
    pCharacteristic->notify();
  } else if (click & 0x10) {
    Serial.println("single click");
    pixels.fill(0xFF0000);
    pixels.show();
    pCharacteristic->setValue("single");
    pCharacteristic->notify();
  }

  delay(50);
}
