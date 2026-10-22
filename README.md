# Automated Terrarium 🦎

An Arduino-based control system for a leopard gecko terrarium. It keeps temperature, humidity, lighting and air quality in the right range on its own, and shows live status on a small TFT screen.

Built as my final-year project (maturitetna naloga) in the Mechatronics Technician program at Tehniški šolski center Maribor, 2026.

<!-- Add photos: put them in an images/ folder and link them like this: -->
<!-- ![Finished terrarium](images/terrarium.jpg) -->

## Features

- **Heating**: a 230 V heat cable switched by a relay, with hysteresis (on at 29 °C, off at 32 °C) so it doesn't switch on and off constantly
- **Humidity**: an ultrasonic mister turns on for 5 s when humidity drops below 30 %
- **Ventilation**: a 12 V fan driven by a MOSFET, which runs:
  - automatically when CO₂ goes above 1800 ppm
  - periodically, for 1 minute every 30 minutes
- **Day/night cycle**: a lamp on a relay, on from 8:00 to 20:00 based on a real-time clock
- **Status display**: a 1.8" TFT shows time, temperature, humidity, CO₂ and the state of every output. A button turns the screen and backlight on or off.
- **Serial log**: full status printed every 2 s at 115200 baud

## Hardware

| Component | Model |
|---|---|
| Microcontroller | Arduino Mega 2560 Pro |
| Temperature and humidity sensor | SHT31 (I²C, 0x44) |
| CO₂ sensor | MH-Z19C (UART) |
| Real-time clock | DS1307 |
| Display | 1.8" TFT 128×160 (ST7735) |
| Relays | JQC-3FF-S-Z 5 V relay modules |
| Fan | DFS122512L 12 V |
| MOSFET | IRLZ44N (logic level) |
| Heater | Exo Terra heat cable, 15 W |
| Lamp | Exo Terra heat bulb, 50 W |
| Mister | Mini ultrasonic mist maker, 5 V |
| Power supply | ATX computer PSU (5 V and 12 V rails) |

## Wiring

| Function | Arduino pin |
|---|---|
| Fan (MOSFET gate) | D35 |
| Heat cable relay | D41 |
| Lamp relay | D40 |
| Mister | D11 |
| Screen button (to GND, internal pull-up) | D5 |
| TFT backlight | D6 |
| TFT CS / DC / RST | D53 / D49 / D48 |
| TFT SDA (MOSI) / SCK | D51 / D52 |
| SHT31 and DS1307 SDA / SCL | D20 / D21 |
| MH-Z19C RX / TX | D18 (TX1) / D19 (RX1) |

The 230 V side (heater and lamp) is physically separated from the low-voltage electronics and only switched through relays. All low-voltage parts share a common ground.

## Software

Required libraries, all installable from the Arduino Library Manager:

- Adafruit GFX Library
- Adafruit ST7735 and ST7789 Library
- Adafruit SHT31 Library
- RTClib

The MH-Z19C is read directly over `Serial1` with a checksum check, so it needs no extra library.

All thresholds and timings are constants at the top of the sketch (`HEAT_ON_TEMP`, `HUMIDITY_THRESHOLD`, `CO2_LIMIT`, `FAN_INTERVAL`, and so on), so they're easy to change. Relay and fan polarity can be flipped with `RELAY_ACTIVE_LOW`, `MISTER_ACTIVE_LOW` and `FAN_ACTIVE_HIGH`.

If the RTC isn't running, the sketch sets it to the compile time on startup.

## Build

The enclosure is 700 × 355 × 606 mm. The bottom section is wood and holds the electronics. The living space has plexiglass sides and a sliding plexiglass door, a wooden back wall, and a metal mesh roof for airflow. The fan sits above the mesh.

The heater and lamp are on the left side to create a warm basking zone and a cooler side. The CO₂ sensor is mounted low, since CO₂ is heavier than air. The humidity sensor is at mid-height, away from the mister.

The sensor housing and the screen/button mount were designed in SolidWorks and 3D printed.

Total material cost was about 295 €, and the project took about 77 hours.

## Lessons learned

- **CO₂ sensor returned nothing for a week**: I had swapped the wrong pins instead of RX and TX. A simple mistake that was hard to spot.
- **TFT looked dead**: the backlight pin wasn't connected. The screen was working, just not visible.
- **Lamp did the opposite of the code**: it was wired to the relay's NC contact instead of NO.
- **The mister only worked with a button press**: I bridged its button contact so it runs as soon as it gets power.
- **Destroyed a soldering iron**: it was a 110 V model plugged into 230 V. Always check the specs first.
- **Plexiglass melted back together while cutting**: switching to a coarser jigsaw blade and cooling with water fixed it.

## Author

Žak Godler. Mentor: Željko Oberški
