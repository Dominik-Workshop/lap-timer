# Lap Timer

[![License](https://img.shields.io/github/license/Dominik-Workshop/lap-timer)](https://github.com/Dominik-Workshop/lap-timer/blob/main/LICENSE) ![Repo Size](https://img.shields.io/github/repo-size/Dominik-Workshop/lap-timer)

<!-- <p align="center">
  <img src="img/renders/hero-image.png" width=80% alt="Hero image">
</p> -->

## Overview

Compact and portable **lap timer**, capable of measuring time on:

- **Closed-loop tracks** (1-gate mode)
- **Start/finish tracks** (2-gate mode)

Infrared light barriers are used as optical triggers to detect when an object crosses the start or finish line.

### Key Features

- **High accuracy** using STM32 hardware timers
- **OLED display** for showing lap times
- **Two operating modes:** 1-gate or 2-gate
- **Modulated IR system (38 kHz)** for ambient light immunity

## Project Status

**Design:** ⌛ --> **Fabrication & Assembly:** ❌ --> **Bring-up:** ❌

## Electronics

Simplified arhitecture of the **Lap Timer** is shown on the block diagram bellow:
![Block diagram](./block-diagram.drawio.svg)

This project contains 3 custom PCBs:

- [**Timer board**](/pcb/timer-board/): main PCB of the system, responsible for time measurement and displaying results

<!-- <p align="center">
  <img src="images/renders/timer-board.png" width=50% alt="Timer board render"> 
</p> -->

- [**Transmitter board**](/pcb/transmitter-board/): generates the modulated IR light beam for the optical barrier

<!-- <p align="center">
  <img src="images/renders/transmitter-board.png" width=50% alt="Transmitter board render"> 
</p> -->

- [**Reveiver board**](/pcb/reveiver-board/): (used only in 2-gate mode) detects the IR signal and triggers the timer

<!-- <p align="center">
  <img src="images/renders/reveiver-board.png" width=50% alt="Reveiver board render"> 
</p> -->

## Mechanical

Each board has a custom-designed **3D printed enclosure**.

## Used Tools

<img src="img/logos/KiCad.png" align="center" height="64">
