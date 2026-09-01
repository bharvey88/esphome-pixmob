# esphome-pixmob

Control PixMob RF wristbands (the "waveband" generation, Cement family) from
ESPHome. The band shows up in Home Assistant as a normal RGB light: color
picker, transitions, effects, automations. A background refresh keeps the
band lit for as long as the light is on, so you get steady colors instead of
the 3.4 second bursts the protocol gives you out of the box.

Only works with the RF bands (915 MHz US / 868 MHz EU, CMT2210LH receiver
inside). The far more common IR PixMob bands are a different protocol; for
those see [pixmob-ir-reverse-engineering](https://github.com/danielweidman/pixmob-ir-reverse-engineering).

## Hardware

- Any ESP32 board ESPHome supports (developed on an ESP32-C6)
- A CC1101 radio module built for 868/915 MHz. This matters: the common
  433 MHz modules share the chip but not the matching network, and lose
  10 to 20 dB at 915 MHz. Listings titled "315/433/868/915MHz" are quoting
  the chip datasheet, not the board. Check the silkscreen.
- An antenna for your band's frequency

Wire the CC1101's SPI pins (SCK, MOSI, MISO, CSN) plus GDO0 to free GPIOs.

This component was developed and tested with the
[Rabbit-Labs CC1101 900MHz module](https://www.tindie.com/products/tehrabbitt/rabbit-labstm-cc1101-900mhz-module/),
which has a proper 915 MHz front end and measures about +10 dBm in this
band. Its 2x4 header is unlabeled, and its layout does not match the
diagram printed on the seller's Flipper carrier board. The real layout,
verified electrically, is the Ebyte E07-M1101D order in pairs from the
power end:

| Pair (from power end) | Board-edge row | Inner row |
|---|---|---|
| 1 | GND | VCC (3.3V) |
| 2 | GDO0 | CSN |
| 3 | SCK | MOSI |
| 4 | MISO | GDO2 (unused) |

To orient yourself on any unlabeled module: GND is the edge-row pin that
has continuity to the antenna connector's shield. Never feed the module
5V, the CC1101 is not 5V tolerant.

## Configuration

```yaml
external_components:
  - source: github://bharvey88/esphome-pixmob

spi:
  clk_pin: GPIO3
  miso_pin: GPIO14
  mosi_pin: GPIO6

cc1101:
  id: radio
  cs_pin: GPIO2
  frequency: 915.33MHz     # US band. EU bands use 868.41MHz.
  modulation_type: ASK/OOK
  output_power: 10
  packet_mode: false

remote_transmitter:
  id: tx
  pin: GPIO7               # CC1101 GDO0
  carrier_duty_percent: 100%
  rmt_symbols: 96
  non_blocking: true

light:
  - platform: pixmob
    name: "PixMob Band"
    transmitter_id: tx
    cc1101_id: radio
    gamma_correct: 1.0
```

See [example.yaml](example.yaml) for a complete config.

### Options

| Option | Default | Range | What it does |
|---|---|---|---|
| `transmitter_id` | required | | The `remote_transmitter` driving GDO0 |
| `cc1101_id` | required | | The `cc1101` radio; the light switches it between TX and idle |
| `group` | `0` | 0-31 | Which bands listen. Bands store a group at the show; 0 is the common default |
| `attack` | `1` | 0-7 | Fade-in speed applied by the band |
| `hold` | `2` | 0-7 | How long the band holds the color per frame |
| `release` | `2` | 0-7 | Fade-out speed when frames stop |
| `random` | `0` | 0-7 | The protocol's sparkle/randomize field |
| `refresh_interval` | `90ms` | | Rebroadcast period while on. Must stay under the band's 120ms memory |
| `off_repeats` | `5` | 1-20 | Black frames sent on turn-off so the band cuts out instead of fading |

Multiple `light:` entries with different `group` values give independent
zones from one transmitter.

`gamma_correct: 1.0` is recommended: the band applies its own brightness
curve, and ESPHome's default 2.8 gamma on top of it crushes dim colors to
black.

## Frequency

915.33 MHz (US) and 868.41 MHz (EU) exactly, both 35 times the band's
crystal. Which one you need depends on where the band was deployed, not
where you live. If a band stays dark at one frequency, try the other.

## Credit

The protocol (6b8b line coding, reversed CRC-12, frame format, timing) is
the reverse-engineering work of
[sueppchen/PixMob_waveband](https://github.com/sueppchen/PixMob_waveband),
ported here under its BSD license. The broader PixMob hacking ecosystem is
indexed at
[danielweidman/pixmob-ir-reverse-engineering](https://github.com/danielweidman/pixmob-ir-reverse-engineering).

This project is not affiliated with or endorsed by PixMob. The PixMob name
is used only to describe compatibility.
