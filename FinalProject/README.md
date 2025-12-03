# SonoCube

A handheld interactive sound-object that responds to touch, motion, and orientation. [Slides available here](https://gamma.app/docs/MTEC-498FinalPresentationpdf-ry76ryofz4b5aca).

## Overview

SonoCube is a self-contained sonic device that creates ambient, droneish synthesis controlled entirely through physical interaction. Touch triggers melodies, while shaking, rotation, and movement modulate delay, reverb, and filters. LEDs respond to the cube's orientation, providing visual feedback.

This project is not a toy or an instrument—it's a study and exercise in combining physical interaction with sound design, created as a stepping stone toward building instruments and installations.

**Project Type:** Hardware device with bespoke embedded software

**Target Audience:** Anyone curious about playful sound interaction



## Features

- **Touch-triggered melodies** via capacitive touch embedded in one cube wall
- **Motion-modulated effects** (delay, reverb, filtering) responding to shake, rotation, and acceleration
- **Orientation-responsive LEDs** that shift based on cube position
- **Fully self-contained** — runs on internal LiPo battery with built-in speaker
- **Bluetooth debugging** — streams sensor data to a simple web app for visualization


## Motivation

I aspire to create instruments and installations, and SonoCube is an exercise toward that goal. The project explores the intersection of physical interaction and sound design, aiming for smooth interactions and a fun, tactile sonic experience.

### Creative Influences

- **Teenage Engineering** — Playful yet refined hardware design that shows small devices can feel premium and fun
- **Kenya Hara** — Minimalist design philosophy that informed the object's simplicity


## Technical Architecture

### Hardware

| Component | Purpose |
|-----------|---------|
| Teensy microcontroller | Audio DSP and synthesis |
| Adafruit QTpy | Sensing (IMU, capacitive touch) and Bluetooth |
| IMU (gyro + accelerometer) | Motion and orientation detection |
| Capacitive touch sensor | Touch interaction |
| LEDs | Visual feedback |
| Small amplifier + speaker | Audio output |
| LiPo battery | Power |
| 3D-printed enclosure | Housing |

### Software

- **Arduino (C++)** for Teensy and QTpy
- **JavaScript + HTML** for Bluetooth debugging web app

### Libraries

- Teensy Audio Library for DSP
- Adafruit libraries for IMU, capacitive touch, and BLE

### Data Flow

```
QTpy (sensing + filtering)
    ↓ JSON over Serial
Teensy (parsing + audio synthesis + LED control)
    ↓
Speaker + LEDs
```

The QTpy handles all sensing, filters out gravity, detects shakes, and sends cleaned data as JSON over Serial to the Teensy. The Teensy parses this data and drives the audio synthesis and LED feedback.

### DSP Approach

- Ambient, pulsing, droneish synthesis (no samples)
- Reverb, delay, and filtering modulated by motion data


## User Experience

1. **Pick up the cube** — sound plays immediately
2. **Touch the capacitive wall** — triggers melodies
3. **Shake, tilt, and rotate** — modulates delay, reverb, and filters
4. **Observe LEDs** — they shift with the cube's orientation


## Development Challenges

The main constraint was working within the Teensy Audio Library's limitations. In hindsight, using a **Daisy Seed** would have provided access to regular C++ or Max/MSP Gen~ for more flexible DSP development.

### New Skills Learned

- 3D printing and Fusion360 (completely new)
- Bluetooth/BLE implementation
- Embedded systems integration
- Serial JSON communication between microcontrollers

## Future Development

- **Companion visual app** — The BLE data pipeline already exists; next step is programming visuals that respond to cube movements
- **Migrate to Daisy Seed** — For more advanced DSP capabilities and C++/Gen~ workflows



## Resources

- [PJRC Teensy Audio Library](https://www.pjrc.com/teensy/td_libs_Audio.html)
- [Teensy Audio System Design Tool](https://www.pjrc.com/teensy/gui/)
- Adafruit documentation for QTpy



