//Cant get this to work - working with ai to get audio working and jsut wont work via the audioShield

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// GUItool: begin automatically generated code
AudioSynthWaveformSine   sine1;          //xy=105,318
AudioOutputI2S           i2s1;           //xy=472,270
AudioConnection          patchCord1(sine1, 0, i2s1, 0);
AudioConnection          patchCord2(sine1, 0, i2s1, 1);
AudioControlSGTL5000     sgtl5000_1;     //xy=234,175
// GUItool: end automatically generated code

void setup() {
  // Reserve memory for audio processing
  AudioMemory(12);

  // Initialize the SGTL5000 audio codec on the shield
  sgtl5000_1.enable();
  sgtl5000_1.volume(1);       // headphone volume (0.0–1.0)
  sgtl5000_1.lineOutLevel(13);  // line-out gain (0–15)

  // Configure the sine wave generator
  sine1.frequency(440);         // A4 pitch
  sine1.amplitude(0.5);         // 50% amplitude
}

void loop() {
  // Nothing needed — sine runs continuously
}
