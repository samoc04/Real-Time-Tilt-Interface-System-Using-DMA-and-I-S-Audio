# Specialist Embedded Audio and Tilt-Controlled Warning System

STM32 embedded system combining tilt detection, WS2812 LED control, DMA-based ADC sampling, timer interrupts, and digital I²S audio output.

![Working Project](working_project.jpeg)

---

## Overview

This project expands the original Project 1 tilt-controlled LED ring system into a more advanced specialist embedded system by integrating digital audio generation, DMA transfers, timer interrupts, and I²S audio communication using the STM32L432 microcontroller.

The final system combines:

- tilt detection using a BMI160 accelerometer
- WS2812B LED ring control
- DMA-based ADC sampling
- timer interrupt scheduling
- UART debugging
- SAI/I²S digital audio output
- MAX98357A speaker amplifier integration
- tilt-triggered audio warning tones

The project demonstrates integration of multiple embedded peripherals operating simultaneously while maintaining stable real-time behaviour.

---

## Objectives

The objectives of this project were:

- Extend the original Project 1 embedded system
- Configure SAI/I²S digital audio output
- Interface with a MAX98357A audio amplifier
- Implement DMA-based audio streaming
- Implement DMA-based ADC sampling
- Implement timer interrupt-driven events
- Generate waveform lookup tables using MATLAB
- Integrate audio response with tilt detection
- Maintain stable LED ring behaviour alongside audio playback
- Apply structured embedded testing and debugging methods
- Use GitHub for version control and documentation

---

## Final System Features

### Key Features

- Tilt-controlled WS2812 LED ring pointer
- 3-LED directional pointer
- Low-pass filtered accelerometer readings
- Push button colour selection
- Potentiometer brightness and save-position control
- UART debugging output
- Timer interrupt system
- DMA-based ADC sampling
- SAI/I²S digital audio output
- MAX98357A speaker amplifier integration
- Tilt-activated 300 Hz warning tone

---

## Hardware Used

- STM32L432 Nucleo board
- BMI160 accelerometer
- WS2812B 16-LED ring
- MAX98357A I²S mono amplifier
- 8 Ω speaker
- Potentiometer
- Push button
- Breadboard and jumper wires
- Oscilloscope
- USB serial monitor

---

## Peripheral Usage

| Peripheral | Purpose |
|---|---|
| GPIO | LED control, button input, debug heartbeat |
| I2C | BMI160 accelerometer communication |
| ADC | Potentiometer analogue input |
| DMA | Continuous ADC and audio transfers |
| SAI / I²S | Digital audio transmission |
| TIM2 | Interrupt timing and PWM |
| UART | Serial debugging output |
| DWT cycle counter | Precise WS2812 timing |

---

## System Description

The system operates as several integrated embedded subsystems running simultaneously.

### Tilt Detection System

The BMI160 accelerometer provides X, Y and Z acceleration data over I²C. The system filters the acceleration values and determines the tilt direction.

### LED Ring Display System

The WS2812B LED ring displays the current tilt direction using a 3-LED pointer. A dead zone prevents jitter when the board is flat.

### Audio System

The MAX98357A amplifier receives digital audio data from the STM32 using the I²S protocol.

Different stages of the project used:

- DMA audio streaming through SAI
- waveform lookup tables generated in MATLAB
- software-generated warning tones

### ADC Input System

A potentiometer connected to ADC1 provides analogue user input. DMA continuously transfers ADC samples into memory while timer interrupts periodically average the values.

### Interrupt and Timing System

TIM2 generates periodic interrupts while DMA handles continuous peripheral transfers independently of the CPU.

---

## Project Development Procedure (LO5)

The system was developed incrementally, with each subsystem tested independently before full integration.

---

## Task 1 – Re-establishing the Project 1 System

### Goal

Restore the fully working tilt-controlled LED ring system from Project 1.

### Functionality Restored

- accelerometer communication
- WS2812 LED ring control
- tilt direction mapping
- colour selection
- PWM brightness control
- saved-position mode
- UART debugging

### Result

The original Project 1 system was successfully restored with stable operation and smooth tilt-controlled LED movement.

---

## Task 2 – Configuring the I²S Audio System

### Goal

Configure the STM32L432 SAI peripheral and MAX98357A amplifier for digital audio output.

### Wiring Added

| STM32 Pin | MAX98357A |
|---|---|
| PA8 | BCLK |
| PA9 | LRC |
| PA10 | DIN |
| 3V3 | VIN |
| GND | GND |

Speaker connections:

- OUTP → speaker positive
- OUTN → speaker negative

### Result

The STM32 successfully transmitted digital audio signals to the MAX98357A amplifier using the I²S protocol.

---

## Task 3 – MATLAB Audio Waveform Generation

### Goal

Generate digital audio waveform lookup tables for embedded playback.

### MATLAB Implementation

MATLAB was used to generate sine wave sample arrays.

```matlab
fs = 16000;
duration = 0.01;

t = 0:1/fs:duration;

x = 32767 * sin(2*pi*300*t);

x = int16(x);
