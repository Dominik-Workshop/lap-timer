# Project assumptions

## Functional assumptions

* The system measures **time between two IR beam interruptions** (start and finish).
* When the first beam is broken, the timer starts; when the second is broken, it stops.
* The measured time is displayed on the OLED screen.

![Block diagram](./block-diagram.drawio.svg)

## Design assumptions

* **STM32** as the brain of the timer
* **TSSP4038 IR Sensor Module + IR LEDS controlled by a `555` timer IC** for the detector ligh barier
* **0.96" SSD1306 OLED** for displaying time measurement
