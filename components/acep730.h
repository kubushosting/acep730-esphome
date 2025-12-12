#pragma once
#include "esphome.h"
#include <SPI.h>

// ACeP730Display: minimal driver with text drawing helpers
class ACeP730Display : public PollingComponent {
 public:
  ACeP730Display(int cs_pin, int dc_pin, int reset_pin, int busy_pin, int rail_en_pin = -1);

  void set_spi_frequency(uint32_t hz);
  void set_power_off_after_update(bool v);

  // send packed image buffer (4-bit indices, 2 pixels per byte)
  void display_packed_image(const uint8_t *image, size_t len);

  // convenience displays
  void show_6_stripes_test();
  void show_colored_text_lines(); // draws the 4 requested lines

  // PollingComponent override
  void setup() override;
  void update() override;

 protected:
  int cs_pin_, dc_pin_, reset_pin_, busy_pin_, rail_en_pin_;
  uint32_t spi_hz_;
  bool power_off_after_update_;

  // low-level helpers
  void cs_low();
  void cs_high();
  void dc_command();
  void dc_data();
  void send_command(uint8_t cmd);
  void send_data(uint8_t data);
  void send_data(const uint8_t *data, size_t len);
  void wait_busy_idle();

  // drawing helpers
  uint8_t pack_two(uint8_t left_idx, uint8_t right_idx);
  void draw_pixel_index(uint8_t *buf, int x, int y, uint8_t idx); // sets one pixel index in packed buffer
  void draw_char(uint8_t *buf, int x, int y, char c, uint8_t idx, int scale);
  void draw_text(uint8_t *buf, int x, int y, const char *s, uint8_t idx, int scale);

  // init sequence (copied from Waveshare)
  void run_init_sequence();
};
