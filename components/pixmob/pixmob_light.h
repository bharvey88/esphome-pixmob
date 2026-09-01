#pragma once

#include "esphome/core/component.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/light/light_traits.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/cc1101/cc1101.h"

namespace esphome::pixmob {

class PixMobLight : public Component, public light::LightOutput {
 public:
  void loop() override;
  void dump_config() override;

  light::LightTraits get_traits() override {
    light::LightTraits traits;
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  }
  void setup_state(light::LightState *state) override { this->state_ = state; }
  void write_state(light::LightState *state) override;

  void set_transmitter(remote_base::RemoteTransmitterBase *tx) { this->transmitter_ = tx; }
  void set_cc1101(cc1101::CC1101Component *radio) { this->radio_ = radio; }
  void set_group(uint8_t v) { this->group_ = v; }
  void set_attack(uint8_t v) { this->attack_ = v; }
  void set_hold(uint8_t v) { this->hold_ = v; }
  void set_release(uint8_t v) { this->release_ = v; }
  void set_random(uint8_t v) { this->random_ = v; }
  void set_refresh_interval(uint32_t ms) { this->refresh_ms_ = ms; }
  void set_off_repeats(uint8_t v) { this->off_repeats_ = v; }

 protected:
  void send_color_(uint8_t red, uint8_t green, uint8_t blue, uint8_t attack, uint8_t hold, uint8_t release);
  void send_current_();

  remote_base::RemoteTransmitterBase *transmitter_{nullptr};
  cc1101::CC1101Component *radio_{nullptr};
  light::LightState *state_{nullptr};

  uint8_t group_{0};
  uint8_t attack_{1};
  uint8_t hold_{2};
  uint8_t release_{2};
  uint8_t random_{0};
  uint32_t refresh_ms_{90};
  uint8_t off_repeats_{5};

  bool on_{false};
  bool radio_active_{false};
  bool dirty_{false};
  uint8_t pending_off_{0};
  uint32_t last_send_{0};
};

}  // namespace esphome::pixmob
