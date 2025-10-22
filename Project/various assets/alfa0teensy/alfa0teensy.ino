#include <ArduinoJson.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// =================== TUNABLES ===================
// Volume from gY
static const float VOL_CENTER     = 0.5f;
static const float VOL_MIN        = 0.05f;
static const float VOL_MAX        = 1.0f;

// Rotation -> filter
static const float HPF_IDLE       = 300.0f;   // wider & more audible at rest
static const float HPF_MAX        = 6000.0f;  // high-pass top
static const float LPF_IDLE       = 6000.0f;  // wide-open top at rest
static const float LPF_MAX        = 12000.0f; // low-pass top
static const float HPF_Q0         = 0.9f;
static const float HPF_Q1         = 1.0f;
static const float LPF_Q          = 0.7f;

// Octave trigger (gravity-removed X)
static const float ACC_X_UP_THR   = 0.6f;
static const float ACC_X_DN_THR   = -0.6f;
static const uint32_t ACC_DEBOUNCE_MS = 350;

// Mixer levels
static const float MIX_ACTIVE = 0.3f;  // each of 3 channels
static const float MIX_SILENT = 0.0f;

// Ramp (anti-click)
static const uint32_t RAMP_TOTAL_MS = 5;   // ~5 ms
static const uint8_t  RAMP_STEPS    = 5;   // 1 ms per step

// =================== AUDIO GRAPH ===================
AudioSynthWaveform waveform1;
AudioSynthWaveform waveform2;
AudioSynthWaveform waveform3;
AudioMixer4        mixer1;
AudioFilterBiquad  biquad1;   // stages 0..3
AudioEffectFreeverb freeverb1;
AudioAmplifier     amp1;
AudioOutputI2S     i2s1;

AudioConnection patchCord1(waveform1, 0, mixer1, 0);
AudioConnection patchCord2(waveform2, 0, mixer1, 1);
AudioConnection patchCord3(waveform3, 0, mixer1, 2);
AudioConnection patchCord4(mixer1, biquad1);
AudioConnection patchCord5(biquad1, freeverb1);
AudioConnection patchCord6(freeverb1, amp1);
AudioConnection patchCord7(amp1, 0, i2s1, 0);
AudioConnection patchCord8(amp1, 0, i2s1, 1);
AudioControlSGTL5000 sgtl5000_1;

// =================== STATE ===================
float x, y, z;
float gx, gy, gz;
int cap;
int face;
int lastFace = -999;

// Triad: base notes (stable) and voiced notes (applied to synth)
float triadBase[3] = {220.0f, 277.18f, 329.63f};
float triadF[3]    = {220.0f, 277.18f, 329.63f};
float baseFreq     = 220.0f;

int   chord_num = 1;

unsigned long lastShiftTime = 0;
bool shifted = false; // while octave ×2 or ÷2 applied

// Gate (touch-driven)
enum GateState { SILENCE, PLAY } gateState = SILENCE;
float mixCurrent = MIX_SILENT;
float mixTarget  = MIX_SILENT;
unsigned long rampStepAt = 0;
uint8_t rampStep = 0;

// Chords
float chord1[] = {130.81f,146.83f,164.81f,220.00f,246.94f};
float chord2[] = {261.63f,293.66f,329.63f,440.00f,493.88f};
float chord3[] = {523.25f,587.33f,659.25f,880.00f,987.77f};

// Gravity removal for X accel (hi-pass)
float xLow = 0.0f;
const float xLowAlpha = 0.98f;

// =================== HELPERS ===================
inline float hiPassX(float xRaw){
  xLow = xLowAlpha * xLow + (1.0f - xLowAlpha) * xRaw;
  return xRaw - xLow;
}

float mapExp(float v,float inMin,float inMax,float outMin,float outMax,float expFactor=2.0f){
  v = constrain(v, inMin, inMax);
  float norm   = (v - inMin) / (inMax - inMin);
  float shaped = powf(norm, expFactor);
  return outMin + shaped * (outMax - outMin);
}

float mapf(float v,float inMin,float inMax,float outMin,float outMax){
  v = constrain(v, inMin, inMax);
  return outMin + (outMax - outMin) * ((v - inMin) / (inMax - inMin));
}

float* currentChordArray(){
  if(chord_num==1) return chord1;
  if(chord_num==2) return chord2;
  return chord3;
}

inline void applyTriad(float f1, float f2, float f3){
  waveform1.frequency(f1);
  waveform2.frequency(f2);
  waveform3.frequency(f3);
}

inline void applyTriadScaled(float scale){
  applyTriad(triadF[0]*scale, triadF[1]*scale, triadF[2]*scale);
}

// --- Voicing transform: update triadF[] from triadBase[] based on face, then apply
void applyVoicingFromFace(){
  triadF[0] = triadBase[0];
  triadF[1] = triadBase[1];
  triadF[2] = triadBase[2];

  switch(face){
    case 0: case 1:                       // normal triad
      break;
    case 2: case 3:                       // widen top
      triadF[2] *= 2.0f;
      break;
    case 4: case 5:                       // inversion (root down)
      triadF[0] *= 0.5f;
      break;
    default:
      break;
  }

  baseFreq = triadF[0];
  applyTriad(triadF[0], triadF[1], triadF[2]);
}

void changeChordRandom(){
  int prev=chord_num;
  chord_num=random(1,4);
  if(chord_num==prev) chord_num=(prev%3)+1;
}

// Pick new random base notes; voicing applied separately
void pickTriadBaseFromChord(){
  float* chord = currentChordArray();
  int i1 = random(0,5);
  int i2, i3;
  do { i2 = random(0,5); } while(i2 == i1);
  do { i3 = random(0,5); } while(i3 == i1 || i3 == i2);

  triadBase[0] = chord[i1];
  triadBase[1] = chord[i2];
  triadBase[2] = chord[i3];
}

inline void setMixerAll(float g){
  mixer1.gain(0, g);
  mixer1.gain(1, g);
  mixer1.gain(2, g);
}

inline void startRamp(float fromG, float toG){
  setMixerAll(fromG);
  mixCurrent = fromG;
  mixTarget  = toG;
  rampStep   = 0;
  rampStepAt = millis();
}

void serviceRamp(){
  if (rampStep >= RAMP_STEPS) return;
  unsigned long now = millis();
  unsigned long perStep = RAMP_TOTAL_MS / (RAMP_STEPS ? RAMP_STEPS : 1);
  if ((long)(now - rampStepAt) >= 0) {
    float t = (float)(rampStep + 1) / (float)RAMP_STEPS;
    float g = mixCurrent + (mixTarget - mixCurrent) * t;
    setMixerAll(g);
    rampStep++;
    rampStepAt = now + perStep;
  }
}

void enterSilence(){
  startRamp(MIX_ACTIVE, MIX_SILENT);
  gateState = SILENCE;
}

void enterPlay(){
  changeChordRandom();
  pickTriadBaseFromChord();
  applyVoicingFromFace();      // voice immediately for current face
  startRamp(MIX_SILENT, MIX_ACTIVE);
  gateState = PLAY;
}

// =================== SETUP ===================
void setup(){
  Serial.begin(115200);
  Serial1.begin(115200);
  delay(200);

  randomSeed(analogRead(A0));
  AudioMemory(24);
  sgtl5000_1.enable();
  sgtl5000_1.volume(0.6);

  waveform1.begin(WAVEFORM_SINE);
  waveform2.begin(WAVEFORM_SINE);
  waveform3.begin(WAVEFORM_SINE);
  waveform1.amplitude(0.8);
  waveform2.amplitude(0.8);
  waveform3.amplitude(0.8);

  // start silent; gate opens on touch
  setMixerAll(MIX_SILENT);

  // Initialize filters at idle (wider band)
  biquad1.setHighpass(0, HPF_IDLE,            HPF_Q0);
  biquad1.setHighpass(1, HPF_IDLE * 1.8f,     HPF_Q1);
  biquad1.setLowpass (2, LPF_IDLE,            LPF_Q);
  biquad1.setLowpass (3, LPF_IDLE * 0.8f,     LPF_Q);

  freeverb1.roomsize(0.7);
  freeverb1.damping(0.3);
  amp1.gain(0.5);

  changeChordRandom();
  pickTriadBaseFromChord();
  applyVoicingFromFace();      // set initial voicing
  lastFace = face;
  gateState = SILENCE;         // touch enables sound
}

// =================== LOOP ===================
void loop(){
  // Parse JSON from ESP32
  if(Serial1.available()){
    String line=Serial1.readStringUntil('\n');
    StaticJsonDocument<256> doc;
    if(deserializeJson(doc,line)==DeserializationError::Ok){
      x=doc["x"]; y=doc["y"]; z=doc["z"];
      gx=doc["gx"]; gy=doc["gy"]; gz=doc["gz"];
      cap=doc["cap"]; face=doc["face"];
    }
  }

  // Instant re-voicing on face change (same base notes)
  if(face != lastFace){
    applyVoicingFromFace();
    lastFace = face;
  }

  // Touch = gate
  if(cap == 1 && gateState == SILENCE){
    enterPlay();
  } else if(cap == 0 && gateState == PLAY){
    enterSilence();
  }

  // Volume from gY (more sensitive range)
  float vol;
  if (gy < 0)
    vol = mapExp(-gy, 0.0f, 30.0f, VOL_CENTER, VOL_MIN, 1.1f);
  else
    vol = mapExp(gy,  0.0f, 30.0f, VOL_CENTER, VOL_MAX, 1.1f);
  amp1.gain(constrain(vol, VOL_MIN, VOL_MAX));

  // Strong rotation response (gZ) with wider idle band
  float gzAbs = fabs(gz);
  float t     = constrain(gzAbs, 0.0f, 60.0f) / 60.0f;
  float cutoffHP = HPF_IDLE + t * (HPF_MAX - HPF_IDLE);   // 300 → 6000
  float cutoffLP = LPF_IDLE + t * (LPF_MAX - LPF_IDLE);   // 6000 → 12000

  biquad1.setHighpass(0, cutoffHP,          HPF_Q0);
  biquad1.setHighpass(1, cutoffHP * 1.8f,   HPF_Q1);
  biquad1.setLowpass (2, cutoffLP,          LPF_Q);
  biquad1.setLowpass (3, cutoffLP * 0.8f,   LPF_Q);

  // Octave (accel X, gravity removed)
  unsigned long now = millis();
  float linX = hiPassX(x);

  if(now - lastShiftTime > ACC_DEBOUNCE_MS){
    if(linX > ACC_X_UP_THR){
      applyTriadScaled(2.0f);
      lastShiftTime = now; shifted = true;
    } else if(linX < ACC_X_DN_THR){
      applyTriadScaled(0.5f);
      lastShiftTime = now; shifted = true;
    }
  }
  if(shifted && (now - lastShiftTime > ACC_DEBOUNCE_MS)){
    applyVoicingFromFace();   // restore current voicing of same base notes
    shifted = false;
  }

  // Anti-click ramp service
  serviceRamp();
}
