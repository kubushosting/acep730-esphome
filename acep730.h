esphome:
  name: epaper-acp
  includes:
    - acep730.h

esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

logger:
  level: DEBUG

spi:
  clk_pin: GPIO10
  mosi_pin: GPIO11

api:
ota:

# Maak de driver instance via lambda. Pas pins aan naar jouw wiring:
# cs, dc, reset, busy, rail_en (gebruik -1 als je geen rail enable pin hebt)
custom_component:
  - lambda: |-
      auto *disp = new ACeP730Display(9, 8, 12, 13, 4);
      disp->set_spi_frequency(4000000); // 4 MHz
      disp->set_power_off_after_update(false);
      App.register_component(disp);
      return {disp};

# Optioneel: tijd of sensoren
time:
  - platform: homeassistant
    id: ha_time
