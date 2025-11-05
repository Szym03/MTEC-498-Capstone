#include <ArduinoJson.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SerialFlash.h>

// GUItool: begin automatically generated code
AudioSynthWaveform       waveform1;      //xy=653,521
AudioSynthWaveformSine   sine1;          //xy=656,608
AudioSynthWaveformSine   sine2;          //xy=657,647
AudioEffectEnvelope      envelope2;      //xy=803,610
AudioEffectEnvelope      envelope1; //xy=804,520
AudioEffectEnvelope      envelope3;      //xy=807,655
AudioEffectFreeverb      freeverb1;      //xy=999,463
AudioMixer4              mixer1;         //xy=1173,523
AudioOutputI2S           i2s2; //xy=1307,524
AudioConnection          patchCord1(waveform1, envelope1);
AudioConnection          patchCord2(sine1, envelope2);
AudioConnection          patchCord3(sine2, envelope3);
AudioConnection          patchCord4(envelope2, 0, mixer1, 2);
AudioConnection          patchCord5(envelope1, freeverb1);
AudioConnection          patchCord6(envelope1, 0, mixer1, 1);
AudioConnection          patchCord7(envelope3, 0, mixer1, 3);
AudioConnection          patchCord8(freeverb1, 0, mixer1, 0);
AudioConnection          patchCord9(mixer1, 0, i2s2, 0);
AudioConnection          patchCord10(mixer1, 0, i2s2, 1);
AudioControlSGTL5000     sgtl5000_1;     //xy=671,443
// GUItool: end automatically generated code




float x, y, z;       // accel 
float gx, gy, gz;    // gyro
int cap;             // capacitive touch
int face;            // current face ID (0–5)

int lastCap = 0;

const float freqs[6] = {
  440.00f,  // A4 
  493.88f,  // B4
  523.25f,  // C5 
  293.66f,  // D4
  329.63f,  // E4
  392.00f   // G4
};

unsigned long lastChordChange = 0;   
const unsigned long interval = 2000;  


//FACES:
//0: Cap on top
//1: Cap on bottom
//2: Speaker on bottom
//3: Speaker on top
//4,5 cap on sides. 


void setup() {
  Serial.begin(115200);      // USB serial for debugging
  Serial1.begin(115200);     // Incoming data from ESP32
  delay(200);

  AudioMemory(12);
  sgtl5000_1.enable();
  sgtl5000_1.volume(0.7);

  waveform1.begin(WAVEFORM_TRIANGLE);
  waveform1.amplitude(0.5);
  mixer1.gain(0, 0.6);
  mixer1.gain(1, 0.4);
  mixer1.gain(2,0.4);
  mixer1.gain(3,0.4);

  sine1.amplitude(0.2);
  sine2.amplitude(0.2);

  sine1.frequency(440.0f);
  sine2.frequency(329.62f);

  envelope2.sustain(1.0f);
  envelope2.release(1500);

  envelope3.delay(100);
  envelope3.sustain(1.0f);
  envelope3.release(1500);
}

void loop() {
  // data collection
  if (Serial1.available()) {
    String line = Serial1.readStringUntil('\n');
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, line);

    if (!err) {
      x    = doc["x"];
      y    = doc["y"];
      z    = doc["z"];
      gx   = doc["gx"];
      gy   = doc["gy"];
      gz   = doc["gz"];
      cap  = doc["cap"];
      face = doc["face"];
    }
  }

  unsigned long currentTime = millis();

  if (currentTime - lastChordChange >= interval) {
    if(envelope2.isActive() == false && envelope2.isActive() == false){
      envelope2.noteOn();
      envelope3.noteOn();
    }else{
      envelope2.noteOff();
      envelope3.noteOff();
    }
    lastChordChange = currentTime;
  }


  waveform1.frequency(freqs[face]);
  // note on/off logic from cap touch
  if (cap == 1 && lastCap == 0){
    envelope1.noteOn();
  }
  if (cap == 0 && lastCap == 1) {
    envelope1.noteOff();
  }
  lastCap = cap;


}

