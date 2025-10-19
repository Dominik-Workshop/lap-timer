# Project assumptions

## Functional assumptions

* The system measures the **time between one or two IR beam interruptions**, depending on the selected mode:
  * **1-gate mode:** both start and finish are detected by the same gate — the timer starts on the first pass and stops on the next.
  * **2-gate mode:** separate start and finish gates are used — the timer starts when the first beam is broken and stops when the second is interrupted.
* The system includes **3 buttons** for manual control:
  * Start/Stop
  * Reset
  * Mode selection (1-gate / 2-gate)
* IR light barriers are used to detect when an object passes a specific point on the track.
* The measured time is shown on the OLED display.

![Block diagram](./block-diagram.drawio.svg)

## Design assumptions

* **STM32** microcontroller as the main controller for precise time measurement.
* **TSSP4038 IR receiver modules + IR LEDs driven by a `555` timer IC** (38 kHz modulation) for the optical barriers.
* **0.96" SSD1306 OLED** display for presenting the measured time.
* **5 V USB C power supply** for the timer board.
* **Li-ion battery power** for transmitter boards.
* Detachable **receiver board**, used only in 2-gate mode.
