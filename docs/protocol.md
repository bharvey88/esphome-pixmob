# The PixMob waveband protocol

What the component sends, and why it sends it that way. Everything here is
implemented in [`components/pixmob/pixmob_light.cpp`](../components/pixmob/pixmob_light.cpp)
and was verified against a Cement V1.1 board (Waveband, Product# W2 Gen3).

The protocol itself is the reverse-engineering work of
[sueppchen/PixMob_waveband](https://github.com/sueppchen/PixMob_waveband),
specifically `Pixmob_cement/pixmob_cement.cpp`, BSD licensed. This document
records it because a second written source is useful, not because any of it
was discovered here.

## Radio

| | |
| --- | --- |
| Frequency | 915.33 MHz (NA) or 868.41 MHz (EU), both the band's crystal times 35 |
| Modulation | ASK/OOK |
| CC1101 mode | Asynchronous serial, not FIFO or packet mode |
| Bit time | 500 us, NRZ, no Manchester coding |
| Receiver in the band | CMT2210LH, sub-GHz OOK |

Neither frequency is a round number. Setting 915.00 MHz does not work.

## Frame

90 bits total, 45 ms on air:

1. Preamble `0x55`, twice
2. Resync bits: one `0`, then one `1`
3. Nine payload bytes

Every byte goes out **LSB first**.

## Payload

Before line coding, the payload is seven bytes:

```
[ mode, green>>2, red>>2, blue>>2, (attack<<3)|random, (release<<3)|hold, group&0x1F ]
```

- `mode` is `0x00` for "display this color", the only mode this component
  uses. The upstream library implements others.
- Color channels are 6 bits each, hence the `>>2`. That is why the bottom
  few steps of an 8-bit brightness slider produce no visible light.
- `attack`, `release`, `hold` and `random` are 0 to 7 and shape the band's
  own envelope. For a color that holds steady under continuous refresh,
  use `attack: 0` and `hold: 7`.
- `group` is 0 to 31. Bands respond only to their own group, which is how a
  venue lights sections independently.

Each of those seven bytes is run through a 64-entry **6b8b line code table**
and lands in `TXbuffer[1..7]`.

## CRC

A reversed CRC-12, polynomial `0x8F3`, initial value `0xC69`, computed over
**the already line-coded bytes** `TXbuffer[1..7]`, not over the plain
payload. The low 6 bits become `TXbuffer[0]` and the high 6 bits become
`TXbuffer[8]`, both themselves line coded.

Computing the CRC over the plain bytes instead is the easiest way to
produce a frame that looks correct on a scope and that the band silently
ignores.

## Refresh

The band forgets its color if it does not hear a frame within about 120 ms.
Sending a single frame gives roughly 3.4 seconds of light followed by the
release fade, which is what the protocol is designed for at a concert.

This component instead retransmits every 60 ms for as long as the light is
on, which is what turns a burst protocol into a steady lamp. Turning the
light off sends a few explicit black frames so the band cuts out promptly
rather than fading, then puts the radio back in idle.

## ESPHome specifics

- **Omit `gdo0_pin` from the `cc1101:` block.** Setting it triggers
  `pin_mode()` calls that steal the pad back from the RMT peripheral, and
  the waveform never reaches the pin even though the chip reports it
  entered TX. See [esphome/esphome#16876](https://github.com/esphome/esphome/issues/16876).
- **`non_blocking: true` on `remote_transmitter`.** The component paces its
  own frames, so it must never block the main loop.
- **`gamma_correct: 1.0` on the light.** The band applies its own response
  curve; ESPHome's default 2.8 gamma on top crushes dim colors to nothing.
- On an ESP32-C6, RMT memory blocks are 48 symbols, so `rmt_symbols: 96`
  covers a frame comfortably.

## Diagnosing a radio that will not talk

If the boot log reports a chip ID other than `0x0014`, SPI is not working.
`0x0000` specifically means the ESP is clocking data out but reading zeros
back, which is usually a wrong CS or MISO connection rather than a dead
chip.

[`tools/pin-fingerprint.yaml`](../tools/pin-fingerprint.yaml) identifies
which wire is on which chip pin without a multimeter. It samples all four
data lines while toggling chip select, and the giveaway is GDO0: a
freshly-reset CC1101 idles that pin outputting a clock at XOSC/192, about
13.5 MHz, which shows up as a line that reads randomly rather than steadily.
Every other pin reads steady. Once you know where GDO0 is, the rest of the
header follows from the pin order.

That same clock has a nasty side effect worth knowing: an ESPHome
`binary_sensor` watching that pin with interrupts enabled generates
millions of interrupts a second and hangs the device in an Interrupt-WDT
boot loop. Set `use_interrupt: false` on any GPIO sensor pointed at GDO0.
