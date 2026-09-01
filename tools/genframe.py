#!/usr/bin/env python3
"""Generate ESPHome remote_transmitter raw timings for a PixMob Cement v1.1 RF frame.

Port of sueppchen/PixMob_waveband Pixmob_cement (BSD). Bit-exact with
pixmob_cement.cpp: lineCode(), setCRC(), generateTXbuffer(), refresh().
"""

DICT = [
    0x21, 0x35, 0x2c, 0x34, 0x66, 0x26, 0xac, 0x24, 0x46, 0x56, 0x44, 0x54, 0x64, 0x6d, 0x4c, 0x6c,
    0x92, 0xb2, 0xa6, 0xa2, 0xb4, 0x94, 0x86, 0x96, 0x42, 0x62, 0x2a, 0x6a, 0xb6, 0x36, 0x22, 0x32,
    0x31, 0xB1, 0x95, 0xB5, 0x91, 0x99, 0x85, 0x89, 0xa5, 0xa4, 0x8c, 0x84, 0xa1, 0xa9, 0x8d, 0xad,
    0x9a, 0x8a, 0x5a, 0x4a, 0x49, 0x59, 0x52, 0x51, 0x25, 0x2d, 0x69, 0x29, 0x4D, 0x45, 0x61, 0x65,
]

POLYR = 0x8F3   # reversed CRC12 polynomial
INITR = 0xC69   # reversed CRC12 init
BIT_TIME = 500  # us
PREAMBLE = 0x55
MODE_RX = 0x00


def line_code(b):
    return DICT[b & 0x3F]


def build_txbuffer(message):
    """message = 7 plain bytes -> 9-byte line-coded TX buffer with CRC."""
    assert len(message) == 7
    tx = [0] * 9
    for i in range(7):
        tx[i + 1] = line_code(message[i])

    reg = INITR
    for i in range(1, 8):          # CRC over the LINE-CODED bytes 1..7
        reg ^= tx[i]
        for _ in range(8):
            if reg & 1:
                reg = (reg >> 1) ^ POLYR
            else:
                reg >>= 1
    tx[0] = line_code(reg & 0x3F)
    tx[8] = line_code(reg >> 6)
    return tx


def color_message(red, green, blue, attack=1, hold=2, release=2, random=0, group=0):
    return [
        MODE_RX,
        green >> 2,
        red >> 2,
        blue >> 2,
        ((attack & 7) << 3) + (random & 7),
        ((release & 7) << 3) + (hold & 7),
        group & 0x1F,
    ]


def frame_bits(tx):
    """Exact on-air bit order from refresh(): preamble x2, sync 0,1, then 9 bytes LSB-first."""
    bits = []
    for byte in (PREAMBLE, PREAMBLE):
        bits += [(byte >> i) & 1 for i in range(8)]
    bits += [0, 1]                                  # SYNC1, SYNC2
    for byte in tx:
        bits += [(byte >> i) & 1 for i in range(8)]
    return bits


def rle(bits):
    """Run-length encode into ESPHome raw timings: +us for high, -us for low."""
    out = []
    cur, run = bits[0], 1
    for b in bits[1:]:
        if b == cur:
            run += 1
        else:
            out.append(run * BIT_TIME * (1 if cur else -1))
            cur, run = b, 1
    out.append(run * BIT_TIME * (1 if cur else -1))
    if out[-1] > 0:                 # must end low; refresh() drives the pin to 0
        out.append(-BIT_TIME)
    return out


def emit(name, red, green, blue, **kw):
    tx = build_txbuffer(color_message(red, green, blue, **kw))
    bits = frame_bits(tx)
    timings = rle(bits)
    print(f"# {name}: rgb({red},{green},{blue}) {kw}")
    print(f"#   txbuffer = {' '.join(f'{b:02X}' for b in tx)}")
    print(f"#   {len(bits)} bits, {len(timings)} timings, {sum(abs(t) for t in timings)/1000:.1f} ms")
    print(f"      - remote_transmitter.transmit_raw:")
    print(f"          code: [{', '.join(str(t) for t in timings)}]")
    print()


if __name__ == "__main__":
    emit("RED", 255, 0, 0)
    emit("GREEN", 0, 255, 0)
    emit("BLUE", 0, 0, 255)
    emit("WHITE", 255, 255, 255)
    emit("OFF", 0, 0, 0, attack=0, hold=0, release=0, random=0)
