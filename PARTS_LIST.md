# Backgammon Match Clock Parts List

This bill of materials matches the current ESP32-S3 firmware and enclosure
model. Quantities are for one complete clock. Amazon links intentionally open
search results where possible, since individual marketplace listings and seller
specifications change frequently.

## Core Electronics

| Qty | Part | What to look for | Amazon link | Status |
|---:|---|---|---|---|
| 1 | Elecrow CrowPanel ESP32-S3 5-inch HMI | Original 5-inch, 800x480 capacitive-touch CrowPanel with 8 MB PSRAM and battery connector | [Amazon search](https://www.amazon.com/s?k=Elecrow+CrowPanel+ESP32+5+inch+800x480) | [ ] |
| 1 | PCF8575 I2C 16-bit I/O expander | PCF8575 module, 2.5-5.5 V supply, address-select pads or jumpers | [Example module](https://www.amazon.com/dp/B0F2M5Y2LK) | [ ] |
| 1 | MAX17048 fuel-gauge breakout | MAX17048 module for one-cell LiPo/Li-ion; 3.3 V I2C-compatible | [Amazon search](https://www.amazon.com/s?k=MAX17048+LiPo+fuel+gauge+module) | [ ] |
| 1 | DS3231 RTC module | DS3231 module with I2C and backup-cell holder; use a 3.3 V-compatible board | [Amazon search](https://www.amazon.com/s?k=DS3231+RTC+module+3.3V) | [ ] |
| 1 | RTC backup cell | Match the RTC module holder, usually CR2032 | [Amazon search](https://www.amazon.com/s?k=CR2032+battery) | [ ] |

The current firmware expects these I2C addresses:

| Module | Expected address |
|---|---:|
| CrowPanel PCA9557 | `0x18`-`0x1F` |
| PCF8575 | `0x20`-`0x27` |
| MAX17048 | `0x36` |
| GT911 touch controller | Usually `0x5D` |
| DS3231 RTC | `0x68` |

## Buttons and Power Control

| Qty | Part | What to look for | Amazon link | Status |
|---:|---|---|---|---|
| 3 | Normally-open tactile switches | Momentary, through-hole, approximately 12 x 12 mm body; verify height before ordering | [Amazon search](https://www.amazon.com/s?k=12x12mm+momentary+tactile+push+button+through+hole) | [ ] |
| 1 | SPST slide power switch | Latching on/off switch; enclosure opening is currently 6 x 3 mm, so measure the selected switch before printing | [Amazon search](https://www.amazon.com/s?k=mini+SPST+slide+switch+6mm+3mm) | [ ] |

Physical-button assignments:

| Function | PCF8575 port |
|---|---:|
| Play/Pause | P00 |
| Player A | P01 |
| Player B | P07 |

Each button connects between its assigned PCF8575 port and GND. No external
resistor is required by the current active-low input arrangement.

The printable enclosure supplies custom button caps. Its current reference
geometry assumes approximately 12.5 x 12.5 x 7.3 mm switch bodies, 9.8 mm round
player-button openings, and a 34 x 12 mm opening beneath the rectangular
Play/Pause cap. Measure the switches you receive before printing the final case.

## Battery and Charging

| Qty | Part | What to look for | Amazon link | Status |
|---:|---|---|---|---|
| 1 | Protected one-cell LiPo battery | 3.7 V nominal, 4.2 V maximum, protection circuit, JST-PH 2-pin plug; maximum modeled envelope is about 55 x 55 x 8 mm | [Amazon search](https://www.amazon.com/s?k=3.7V+protected+LiPo+battery+JST+PH+2.0) | [ ] |
| 1 | USB-C power supply | Regulated 5 V, 2 A or greater, from a reputable manufacturer | [Amazon search](https://www.amazon.com/s?k=5V+2A+USB+C+power+adapter) | [ ] |
| 1 | USB-C data cable | Data-capable cable for programming and external power | [Amazon search](https://www.amazon.com/s?k=USB+C+data+cable) | [ ] |

The specified CrowPanel has a BAT input and onboard one-cell charging circuit;
Elecrow specifies a 3.7-4.2 V battery and 5 V/2 A external supply. Do not add a
second charger in parallel unless its power-path design has been reviewed.
The MAX17048 only measures battery voltage/state of charge and does not charge
the battery.

**Before connecting a battery:** check the connector polarity with a multimeter.
JST cable colors and connector orientation are not guaranteed between sellers.
BAT+ must reach battery positive and BAT- must reach battery negative.

## Wiring and Assembly

| Qty | Part | What to look for | Amazon link | Status |
|---:|---|---|---|---|
| 1 | I2C splitter/hub | Passive 4-pin I2C hub compatible with 3.3 V; it does not create additional buses | [Amazon search](https://www.amazon.com/s?k=Qwiic+I2C+hub+splitter) | [ ] |
| 1 set | 4-pin I2C cables | Match the connectors on the selected modules; confirm pin order for GND/VCC/SDA/SCL | [Amazon search](https://www.amazon.com/s?k=JST+SH+4+pin+Qwiic+cable+kit) | [ ] |
| 1 | Prototype PCB | Solderable perfboard or stripboard sized for the enclosure | [Amazon search](https://www.amazon.com/s?k=solderable+prototype+PCB+perfboard) | [ ] |
| 1 kit | Hook-up wire | 22-26 AWG stranded wire for short internal connections | [Amazon search](https://www.amazon.com/s?k=22+24+26+AWG+stranded+hookup+wire+kit) | [ ] |
| 1 kit | Pin headers | 2.54 mm male/female breakaway headers | [Amazon search](https://www.amazon.com/s?k=2.54mm+breakaway+pin+header+kit) | [ ] |
| 1 kit | Heat-shrink tubing | Assorted small diameters for switch and power joints | [Amazon search](https://www.amazon.com/s?k=heat+shrink+tubing+assortment+electronics) | [ ] |
| 1 roll | Foam mounting tape | Thin, strong double-sided tape for the battery and modules | [Amazon search](https://www.amazon.com/s?k=thin+double+sided+foam+tape+electronics) | [ ] |
| 1 | Inline fuse or resettable fuse | Optional protection in the battery/load path; size for the final measured current | [Amazon search](https://www.amazon.com/s?k=inline+resettable+fuse+holder+low+voltage) | [ ] |

## Enclosure Hardware

| Qty | Part | What to look for | Amazon link | Status |
|---:|---|---|---|---|
| 1 set | M3 screws | Assorted M3 machine screws for the display and case | [Amazon search](https://www.amazon.com/s?k=M3+machine+screw+assortment) | [ ] |
| 1 set | M3 heat-set inserts | Brass inserts sized for the chosen print material and screw length | [Amazon search](https://www.amazon.com/s?k=M3+heat+set+inserts+3D+printing) | [ ] |
| 4 | Rubber feet | Small self-adhesive feet for the enclosure base | [Amazon search](https://www.amazon.com/s?k=small+self+adhesive+rubber+feet+electronics) | [ ] |
| 1 spool | PETG or PLA+ filament | PETG is preferable if the clock may be exposed to a warm car or direct sunlight | [Amazon search](https://www.amazon.com/s?k=1.75mm+PETG+filament) | [ ] |

## Tools and Consumables

These are workshop items rather than parts installed in the clock:

- [Temperature-controlled soldering iron](https://www.amazon.com/s?k=temperature+controlled+soldering+iron+electronics)
- [Lead-free or 63/37 electronics solder](https://www.amazon.com/s?k=electronics+solder+small+diameter)
- [No-clean flux pen](https://www.amazon.com/s?k=no+clean+flux+pen)
- [Digital multimeter](https://www.amazon.com/s?k=digital+multimeter+electronics)
- [Wire stripper and flush cutters](https://www.amazon.com/s?k=electronics+wire+stripper+flush+cutter)
- [Heat-set insert soldering tips](https://www.amazon.com/s?k=M3+heat+set+insert+soldering+tips)

## Recommended Purchase Order

1. Confirm the exact CrowPanel model and its battery connector polarity.
2. Buy and bench-test the PCF8575, MAX17048, and DS3231 on the shared I2C bus.
3. Buy switches and measure their bodies, stems, and mounting openings.
4. Update the enclosure dimensions if the purchased parts differ from the CAD
   reference geometry.
5. Select a protected LiPo that fits the measured enclosure and verify polarity.
6. Print the case only after a complete dry fit of the electronics.

## Compatibility Notes

- Power all shared I2C logic from 3.3 V and connect every module to common GND.
- Avoid modules with fixed 5 V I2C pull-ups; they can expose the ESP32-S3 pins to
  unsafe voltage.
- A passive I2C hub only provides duplicate connectors. It does not resolve
  address conflicts.
- Some inexpensive DS3231 boards contain a charging circuit intended for an
  LIR2032 rechargeable cell. Do not install a normal CR2032 in a module that
  actively charges its backup cell; inspect the board design or buy a module
  explicitly described as CR2032-safe.
- Marketplace titles are not reliable specifications. Confirm the chip marking,
  voltage range, dimensions, connector type, and pin order on the actual listing
  before purchasing.

