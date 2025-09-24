// PWM audio demo created with aid from AI

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

// Audio objects
AudioSynthWaveform waveform;
AudioOutputPWM pwmOut;
AudioConnection patchCord1(waveform, pwmOut);

// Define a simple arpeggio (C major pentatonic)
float notes[] = {
  261.63, // C4
  293.66, // D4
  329.63, // E4
  392.00, // G4
  440.00, // A4
  523.25  // C5
};
const int numNotes = sizeof(notes) / sizeof(notes[0]);

void setup() {
  AudioMemory(10);
  waveform.begin(WAVEFORM_SINE);
  waveform.amplitude(0.5);
}

void loop() {
  for (int i = 0; i < numNotes; i++) {
    waveform.frequency(notes[i]);
    delay(250);  
  }
}
