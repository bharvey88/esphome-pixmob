#include "pixmob_light.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::pixmob {

static const char *const TAG = "pixmob";

// PixMob Cement waveband protocol, ported from sueppchen/PixMob_waveband
// (Pixmob_cement, BSD license). Bit-exact with pixmob_cement.cpp.

static const uint32_t BIT_TIME_US = 500;
// A frame is 45ms on air; never start a new one before the last has cleared
static const uint32_t MIN_FRAME_GAP_MS = 50;
static const uint8_t PREAMBLE = 0x55;
static const uint8_t MODE_RX = 0x00;
static const uint16_t CRC_POLY_REVERSED = 0x8F3;
static const uint16_t CRC_INIT_REVERSED = 0xC69;

// 6b8b line code dictionary
static const uint8_t LINE_CODE_DICT[64] = {
    0x21, 0x35, 0x2C, 0x34, 0x66, 0x26, 0xAC, 0x24, 0x46, 0x56, 0x44, 0x54, 0x64, 0x6D, 0x4C, 0x6C,
    0x92, 0xB2, 0xA6, 0xA2, 0xB4, 0x94, 0x86, 0x96, 0x42, 0x62, 0x2A, 0x6A, 0xB6, 0x36, 0x22, 0x32,
    0x31, 0xB1, 0x95, 0xB5, 0x91, 0x99, 0x85, 0x89, 0xA5, 0xA4, 0x8C, 0x84, 0xA1, 0xA9, 0x8D, 0xAD,
    0x9A, 0x8A, 0x5A, 0x4A, 0x49, 0x59, 0x52, 0x51, 0x25, 0x2D, 0x69, 0x29, 0x4D, 0x45, 0x61, 0x65,
};

static inline uint8_t line_code(uint8_t value) { return LINE_CODE_DICT[value & 0x3F]; }

void PixMobLight::send_color_(uint8_t red, uint8_t green, uint8_t blue, uint8_t attack, uint8_t hold,
                              uint8_t release) {
  // 7-byte plain message
  uint8_t message[7] = {
      MODE_RX,
      static_cast<uint8_t>(green >> 2),
      static_cast<uint8_t>(red >> 2),
      static_cast<uint8_t>(blue >> 2),
      static_cast<uint8_t>(((attack & 7) << 3) | (this->random_ & 7)),
      static_cast<uint8_t>(((release & 7) << 3) | (hold & 7)),
      static_cast<uint8_t>(this->group_ & 0x1F),
  };

  // Line code into tx[1..7], then CRC-12 over the line-coded bytes
  uint8_t tx[9];
  for (int i = 0; i < 7; i++)
    tx[i + 1] = line_code(message[i]);
  uint16_t reg = CRC_INIT_REVERSED;
  for (int i = 1; i <= 7; i++) {
    reg ^= tx[i];
    for (int j = 0; j < 8; j++)
      reg = (reg & 1) ? (reg >> 1) ^ CRC_POLY_REVERSED : reg >> 1;
  }
  tx[0] = line_code(reg & 0x3F);
  tx[8] = line_code(reg >> 6);

  // On-air bit stream: preamble x2, sync bits 0 and 1, then tx LSB-first
  uint8_t bits[90];
  int n = 0;
  for (int p = 0; p < 2; p++)
    for (int i = 0; i < 8; i++)
      bits[n++] = (PREAMBLE >> i) & 1;
  bits[n++] = 0;
  bits[n++] = 1;
  for (int b = 0; b < 9; b++)
    for (int i = 0; i < 8; i++)
      bits[n++] = (tx[b] >> i) & 1;

  // Run-length encode into RMT marks and spaces
  auto call = this->transmitter_->transmit();
  auto *data = call.get_data();
  uint8_t cur = bits[0];
  uint32_t run = 1;
  for (int i = 1; i < n; i++) {
    if (bits[i] == cur) {
      run++;
      continue;
    }
    if (cur) {
      data->mark(run * BIT_TIME_US);
    } else {
      data->space(run * BIT_TIME_US);
    }
    cur = bits[i];
    run = 1;
  }
  if (cur) {
    data->mark(run * BIT_TIME_US);
    data->space(BIT_TIME_US);  // the reference driver parks the pin low
  } else {
    data->space(run * BIT_TIME_US);
  }
  call.perform();
  this->last_send_ = millis();
}

void PixMobLight::send_current_() {
  float red, green, blue;
  this->state_->current_values_as_rgb(&red, &green, &blue);
  this->send_color_(static_cast<uint8_t>(red * 255.0f), static_cast<uint8_t>(green * 255.0f),
                    static_cast<uint8_t>(blue * 255.0f), this->attack_, this->hold_, this->release_);
}

void PixMobLight::write_state(light::LightState *state) {
  float red, green, blue;
  state->current_values_as_rgb(&red, &green, &blue);
  bool is_on = state->current_values.is_on() && (red > 0.0f || green > 0.0f || blue > 0.0f);

  if (is_on) {
    if (!this->on_) {
      if (!this->radio_active_) {
        this->radio_->begin_tx();
        this->radio_active_ = true;
      }
      this->on_ = true;
      this->pending_off_ = 0;
    }
    // Air the new color as soon as the previous frame has cleared
    if (millis() - this->last_send_ >= MIN_FRAME_GAP_MS) {
      this->send_current_();
      this->dirty_ = false;
    } else {
      this->dirty_ = true;
    }
  } else if (this->on_) {
    this->on_ = false;
    // Queue a few explicit black frames so the band drops out immediately
    // instead of waiting for its 120ms memory to lapse into the release fade
    this->pending_off_ = this->off_repeats_;
  }
}

void PixMobLight::loop() {
  uint32_t now = millis();
  if (now - this->last_send_ < MIN_FRAME_GAP_MS)
    return;
  if (this->pending_off_ > 0) {
    this->send_color_(0, 0, 0, 0, 0, 0);
    this->pending_off_--;
    return;
  }
  if (this->on_) {
    if (this->dirty_ || now - this->last_send_ >= this->refresh_ms_) {
      this->send_current_();
      this->dirty_ = false;
    }
  } else if (now - this->last_send_ >= this->refresh_ms_ && this->radio_active_) {
    // One refresh period after the last frame, so a queued non-blocking
    // transmission finishes before the radio leaves TX
    this->radio_->set_idle();
    this->radio_active_ = false;
  }
}

void PixMobLight::dump_config() {
  ESP_LOGCONFIG(TAG,
                "PixMob light:\n"
                "  Group: %u\n"
                "  Attack/Hold/Release: %u/%u/%u\n"
                "  Random: %u\n"
                "  Refresh interval: %ums",
                this->group_, this->attack_, this->hold_, this->release_, this->random_,
                (unsigned) this->refresh_ms_);
}

}  // namespace esphome::pixmob
