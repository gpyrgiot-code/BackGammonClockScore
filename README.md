# Backgammon Match Clock

A portable ESP32-S3 backgammon match clock and scoreboard built for an Elecrow
CrowPanel 5-inch 800x480 RGB touchscreen. The firmware combines tournament
scoring, three clock systems, Crawford tracking, physical controls, battery
monitoring, an RTC, player customization, match history, and day/night themes.

## Highlights

- 800x480 capacitive touchscreen interface
- Two player clocks and touch-adjustable scores
- Physical Player A, Player B, and Play/Pause buttons
- Match targets from 1 through 21 points (odd values)
- Timer by game or timer for the complete match
- Simple delay, Bronstein delay, and Fischer increment
- Crawford and post-Crawford tracking
- Standard score or Points to Win display
- Editable player names and checker-color scorecards
- Persistent settings stored in ESP32 preferences
- Match history with names, score, and RTC date
- MAX17048 battery monitoring and DS3231 date/time
- Day and night display themes
- Light-red timer warning at 1:30 or less
- Optimized custom GFX fonts and startup splash image

## Target Hardware

The firmware targets the Elecrow ESP32-S3 CrowPanel HMI with:

- ESP32-S3
- 8 MB OPI PSRAM
- 800x480 RGB-parallel display
- GT911 capacitive touch controller
- PCA9557 onboard I/O expander

External hardware used by the complete build:

- PCF8575 16-bit I2C GPIO expander
- MAX17048 LiPo fuel gauge
- DS3231 real-time clock
- Three normally-open momentary push buttons
- One-cell LiPo battery and a suitable protected charging/power circuit

The MAX17048 measures the battery but does not charge it. Use a charging and
power-management circuit designed for the battery chemistry and load. Verify
battery polarity before applying power; reversing BAT+ and BAT- can damage the
module, battery, or ESP32.

## Project Layout

```text
BackgammonMatchClock/
   |-- BackgammonMatchClock.ino
   |-- splash_image.h
   |-- splash4.png
   |-- 7segment48pt7b_numbers.h
   |-- collegeb96pt7b_numbers.h
   |-- FreeSansBold48pt7b_numbers.h
   |-- FreeSans12pt7b.h
```

The firmware uses a custom RGB565 renderer built on the ESP-IDF RGB LCD
peripheral. It does not require LVGL or an external TFT library.

## I2C Bus

The shared I2C bus uses:

| Signal | ESP32-S3 pin |
|---|---:|
| SDA | GPIO 19 |
| SCL | GPIO 20 |
| Logic supply | 3.3 V |
| Ground | GND |

Configured devices:

| Device | Address | Purpose |
|---|---:|---|
| PCA9557 | `0x18`-`0x1F` | CrowPanel initialization/reset control |
| PCF8575 | `0x20`-`0x27` | Physical clock buttons |
| MAX17048 | `0x36` | Battery voltage and state of charge |
| GT911 | Usually `0x5D` | Capacitive touch |
| DS3231 | `0x68` | Date and time |

All I2C modules share SDA, SCL, 3.3 V, and GND. An I2C splitter is only a
parallel connection hub; each device remains on the same bus and needs a unique
address.

## Physical Buttons

The PCF8575 ports are active-low inputs. Connect one terminal of each
normally-open button to its PCF8575 port and the other terminal to GND.

| Function | PCF8575 port |
|---|---:|
| Play/Pause | P00 |
| Player A clock | P01 |
| Player B clock | P07 |

Four-leg tactile switches contain two internally connected pairs. Use one leg
from each opposite electrical side. Confirm continuity with a multimeter before
soldering.

The firmware also supports a direct button on GPIO 38 for compatibility with the
earlier one-button prototype.

## Display Connections

The built-in RGB panel uses these fixed signals:

| Signal | GPIO |
|---|---|
| Backlight | 2 |
| D0-D15 | 8, 3, 46, 9, 1, 5, 6, 7, 15, 16, 4, 45, 48, 47, 21, 14 |
| DE | 40 |
| VSYNC | 41 |
| HSYNC | 39 |
| PCLK | 0 |

Do not reuse these pins for external controls.

## User Interface

### Main screen

The main screen contains:

- Battery status, elapsed match time, date, and time
- Player names and clock values
- Player scorecards
- Play Game/Pause Game control
- End Game and End Match controls
- Match target, game counter, and Crawford state
- Log and Settings buttons

The name bars use each player's selected checker color. When either timer reaches
1:30, its background becomes light red as a low-time warning.

### Touch controls

- Tap Player A's timer to finish A's turn and start Player B's clock.
- Tap Player B's timer to finish B's turn and start Player A's clock.
- Tap Play Game to start or resume; tap Pause Game to pause.
- Swipe up on a scorecard to add one point.
- Swipe down on a scorecard to remove one point.
- Tap a player name while paused to edit the name and checker color.
- Tap End Game to stop the current game without resetting the match clocks.
- Tap End Match to confirm and record the result.
- After a match ends, End Match becomes Reset.

Scores can only be changed while the clock is paused or after a game ends.

### Settings

Settings includes:

- Match target: 1, 3, 5, ... 21
- Time per game in 30-second steps
- Bonus or delay duration
- Timer scope: Game or Match
- Timing mode: Simple, Bronstein, or Fischer
- Appearance
- RTC clock/date setup
- Start Match and End Match

### Appearance

Appearance currently provides:

- Night or Day color theme
- Score or Points to Win display

Points to Win is a display-only calculation:

```text
points to win = match target - actual player score
```

Stored scores, Crawford logic, match winner detection, and history remain based
on the actual score.

### Match log

The Log screen stores up to eight completed matches. Each entry includes the RTC
date, both player names, and the final score. The log is stored in nonvolatile
ESP32 preferences and can be cleared from the Log screen.

## Timing Modes

### Simple delay

The reserve clock does not visibly decrease during the delay. Only time used
beyond the configured delay is deducted.

```text
deduction = max(0, elapsed turn time - delay)
```

Unused delay is not added to the reserve.

### Bronstein delay

The clock counts down immediately. When the player presses the clock, elapsed
time up to the configured delay is refunded. The final reserve deduction is the
same as Simple delay, but the live display behaves differently.

```text
refund = min(elapsed turn time, delay)
```

### Fischer increment

The clock counts down during the turn. Pressing the clock adds the configured
increment to that player's reserve.

```text
new reserve = old reserve - elapsed turn time + increment
```

Players can gain reserve time by moving faster than the increment.

### Timer scope

- **Game:** each player's timer resets when a new game begins.
- **Match:** each player's initial reserve equals time per game multiplied by
  the match target and carries across games.

If a player reaches zero, the timeout screen identifies that player. Applying
the penalty point remains a manual scoring action.

## Crawford Rule

When either player reaches one-away and Crawford has not already been used, the
next game is marked `CRAWFORD` and the doubling cube is disabled. After that
game, the display changes to `POST CRAWFORD` and the cube is enabled for all
remaining games.

## Fonts and Splash Image

The sketch uses trimmed GFX font headers to reduce flash use:

- `7segment48pt7b_numbers.h`: timer digits and colon
- `collegeb96pt7b_numbers.h`: score digits
- `FreeSansBold48pt7b_numbers.h`: match target digits
- `FreeSans12pt7b.h`: interface text

The splash image is stored as RGB565 data in `splash_image.h`. Its source is
`splash4.png`.

## Building with Arduino IDE

### Requirements

- Arduino IDE 2.x
- Espressif Systems ESP32 board package
- USB data cable

Open:

```text
BackgammonMatchClock/BackgammonMatchClock.ino
```

Recommended board settings:

| Setting | Value |
|---|---|
| Board | ESP32S3 Dev Module |
| PSRAM | OPI PSRAM |
| USB CDC On Boot | Disabled or Default |
| USB Mode | Hardware CDC/JTAG or Default |
| Upload Speed | 115200 |

PSRAM is mandatory. An 800x480 RGB565 framebuffer requires more memory than the
ESP32-S3's internal RAM can provide. If Serial Monitor reports `PSRAM found: no`,
select OPI PSRAM and upload again.

### Upload

1. Connect the CrowPanel over USB.
2. Select its `/dev/cu.usbserial...` port.
3. Compile and upload.
4. Open Serial Monitor at 115200 baud.

If automatic upload does not begin, hold BOOT, start the upload, and release BOOT
when `Connecting...` appears. Press RESET after a successful upload if needed.

Uploads at 921600 can connect successfully and then fail with `The chip stopped
responding`. Use 115200 for reliable flashing.

## Arduino CLI

From the repository directory:

```sh
sh scripts/compile-arduino.sh
sh scripts/upload-arduino.sh /dev/cu.usbserial-XXXXX
```

Replace the port with the result from:

```sh
arduino-cli board list
```

The equivalent compile target is:

```sh
arduino-cli compile \
  --fqbn esp32:esp32:esp32s3:PSRAM=opi,CDCOnBoot=default,USBMode=default \
  BackgammonMatchClock
```

## PlatformIO

The repository includes `platformio.ini`:

```sh
pio run
pio run -t upload
pio device monitor -b 115200
```

Arduino IDE/CLI is the primary tested workflow for the CrowPanel hardware.

## Wokwi

`WokwiMatchClock` is a generated compatibility target. Wokwi does not model the
CrowPanel RGB panel, GT911, PCA9557, PCF8575, MAX17048, or DS3231 exactly, so it
uses display and input substitutions.

After firmware changes, regenerate the Wokwi copy with:

```sh
node scripts/sync-wokwi.mjs
```

Do not place both the generated `sketch.ino` and hardware `.ino` in one Wokwi
project because Arduino will compile both and report duplicate definitions.

The ESP32 binary cannot run directly on macOS because it contains ESP32-S3
machine code and hardware drivers. A native desktop simulator is not yet
included.

## Serial Diagnostics

Use Serial Monitor at 115200 baud. A healthy boot should report:

- `PSRAM found: yes`
- PCA9557 found and initialized
- PCF8575 found, if connected
- MAX17048 found at `0x36`, if connected
- DS3231 found at `0x68`, if connected
- GT911 found, normally at `0x5D`
- `esp_lcd_new_rgb_panel: 0`
- `esp_lcd_panel_reset: 0`
- `esp_lcd_panel_init: 0`

P00 Play/Pause presses also print a diagnostic line with the current mode and
clock state.

## Troubleshooting

### White or blank screen

- Confirm PSRAM is set to OPI PSRAM.
- Verify Serial reports `PSRAM found: yes`.
- Confirm the RGB panel initialization calls return `0`.
- Use a stable USB power source and cable.
- Check that the PCA9557 appears on the I2C scan.

### Touch is not detected

- Confirm the GT911 appears in the I2C scan.
- Check SDA, SCL, power, and ground.
- Make sure no external device is holding the shared I2C bus low.

### PCF8575 buttons are not detected

- Confirm the expander appears between `0x20` and `0x27`.
- Connect each button between its assigned P port and GND.
- Solder module headers; loose unsoldered pins are not electrically reliable.
- Verify address jumpers do not conflict with another device.

### Battery percentage is missing or unstable

- Confirm the MAX17048 appears at `0x36`.
- Verify battery polarity and solder joints.
- Allow the gauge time to settle after reconnecting the cell.
- The firmware performs a MAX17048 quick-start during initialization.
- Charging may be inferred from a rising voltage/state-of-charge trend unless a
  dedicated charger status pin is added.

### RTC is missing or the log says DATE UNSET

- Confirm the DS3231 appears at `0x68`.
- Set date and time through Settings > Clock.
- Check the RTC backup battery if time is lost when power is removed.

### Upload fails after changing baud rate

Select 115200 upload speed, reconnect USB, and retry. Temporarily disconnecting
external modules can help diagnose power or bus issues while flashing.

### Duplicate or unreadable Serial output

The firmware writes diagnostics to both available serial interfaces for board
compatibility. Use 115200 baud. Boot ROM characters may briefly appear garbled
if the monitor opens during reset.

## Enclosure

The `BackgammonMatchClock/case` directory contains enclosure work for a sloped
desktop clock body with a front display opening and top-mounted player and
Play/Pause buttons. Verify dimensions against the physical hardware before
printing or machining.

## Safety

- Use a protected single-cell LiPo setup designed for charging and load sharing
  if the clock operates while charging.
- Never connect a LiPo directly to a 5 V input unless the board documentation
  explicitly permits it.
- Do not reverse battery polarity.
- Do not power I2C signal pull-ups above 3.3 V.
- Place a physical power switch in the battery or regulated load path according
  to the charger/power-path design.

## Current Status

The firmware compiles for ESP32-S3 with OPI PSRAM and currently uses about 47%
of program flash and 7% of internal dynamic memory. The project remains under
active development, with Appearance prepared for additional layouts and themes.
