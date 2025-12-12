#include "acep730.h"

// Constructor
ACeP730Display::ACeP730Display(int cs_pin, int dc_pin, int reset_pin, int busy_pin, int rail_en_pin)
  : PollingComponent(60000),
    cs_pin_(cs_pin),
    dc_pin_(dc_pin),
    reset_pin_(reset_pin),
    busy_pin_(busy_pin),
    rail_en_pin_(rail_en_pin),
    spi_hz_(2000000),
    power_off_after_update_(false) { }

void ACeP730Display::set_spi_frequency(uint32_t hz) { spi_hz_ = hz; }
void ACeP730Display::set_power_off_after_update(bool v) { power_off_after_update_ = v; }

void ACeP730Display::cs_low() { digitalWrite(cs_pin_, LOW); }
void ACeP730Display::cs_high() { digitalWrite(cs_pin_, HIGH); }
void ACeP730Display::dc_command() { digitalWrite(dc_pin_, LOW); }
void ACeP730Display::dc_data() { digitalWrite(dc_pin_, HIGH); }

void ACeP730Display::send_command(uint8_t cmd) {
  cs_low(); dc_command();
  SPI.transfer(cmd);
  cs_high();
}

void ACeP730Display::send_data(uint8_t data) {
  cs_low(); dc_data();
  SPI.transfer(data);
  cs_high();
}

void ACeP730Display::send_data(const uint8_t *data, size_t len) {
  const size_t CHUNK = 256;
  size_t sent = 0;
  while (sent < len) {
    size_t to_send = (len - sent) > CHUNK ? CHUNK : (len - sent);
    cs_low(); dc_data();
    for (size_t i = 0; i < to_send; i++) SPI.transfer(data[sent + i]);
    cs_high();
    sent += to_send;
  }
}

void ACeP730Display::wait_busy_idle() {
  int timeout = 15000;
  // Waveshare: LOW = busy, HIGH = idle
  while (!digitalRead(busy_pin_) && timeout > 0) {
    delay(1);
    timeout--;
  }
  if (timeout <= 0) {
    ESP_LOGW("acep", "wait_busy_idle timeout waiting for BUSY HIGH");
  }
}

// display_packed_image: send buffer to panel
void ACeP730Display::display_packed_image(const uint8_t *image, size_t len) {
  const int W = 800;
  const int H = 480;
  const size_t expected = (W / 2) * H;
  if (len != expected) {
    ESP_LOGW("acep", "display_packed_image: unexpected length %u (expected %u)", (unsigned)len, (unsigned)expected);
  }

  send_command(0x10); // Data Start
  // send in chunks
  const size_t CHUNK = 1024;
  size_t sent = 0;
  while (sent < len) {
    size_t to_send = (len - sent) > CHUNK ? CHUNK : (len - sent);
    cs_low(); dc_data();
    for (size_t i = 0; i < to_send; i++) SPI.transfer(image[sent + i]);
    cs_high();
    sent += to_send;
  }

  send_command(0x12); // Display Refresh
  send_data(0x00);
  wait_busy_idle();

  // Power off sequence
  send_command(0x02); send_data(0x00);
  wait_busy_idle();

  if (power_off_after_update_ && rail_en_pin_ >= 0) {
    delay(50);
    digitalWrite(rail_en_pin_, LOW);
    ESP_LOGD("acep", "rails powered off after update");
  }
}

// pack two 4-bit indices into one byte
uint8_t ACeP730Display::pack_two(uint8_t left_idx, uint8_t right_idx) {
  return (uint8_t)(((left_idx & 0x0F) << 4) | (right_idx & 0x0F));
}

// draw_pixel_index: set one pixel in packed buffer (high nibble = left pixel)
void ACeP730Display::draw_pixel_index(uint8_t *buf, int x, int y, uint8_t idx) {
  if (x < 0 || x >= 800 || y < 0 || y >= 480) return;
  int wb = 800 / 2;
  int byte_index = y * wb + (x / 2);
  uint8_t cur = buf[byte_index];
  if ((x & 1) == 0) {
    // left pixel -> high nibble
    cur = (uint8_t)((cur & 0x0F) | ((idx & 0x0F) << 4));
  } else {
    // right pixel -> low nibble
    cur = (uint8_t)((cur & 0xF0) | (idx & 0x0F));
  }
  buf[byte_index] = cur;
}

// 5x7 font for ASCII letters (A-Z, a-z mapped to same), digits and space and parentheses.
// Each char is 5 columns, LSB at top. We'll include only characters we need and a fallback.
static const uint8_t font5x7[][5] = {
  // space (32)
  {0x00,0x00,0x00,0x00,0x00}, // ' '
  // 'A' (65)
  {0x7C,0x12,0x11,0x12,0x7C}, // A
  {0x7F,0x49,0x49,0x49,0x36}, // B
  {0x3E,0x41,0x41,0x41,0x22}, // C
  {0x7F,0x41,0x41,0x22,0x1C}, // D
  {0x7F,0x49,0x49,0x49,0x41}, // E
  {0x7F,0x09,0x09,0x09,0x01}, // F
  {0x3E,0x41,0x49,0x49,0x7A}, // G
  {0x7F,0x08,0x08,0x08,0x7F}, // H
  {0x00,0x41,0x7F,0x41,0x00}, // I
  {0x20,0x40,0x41,0x3F,0x01}, // J
  {0x7F,0x08,0x14,0x22,0x41}, // K
  {0x7F,0x40,0x40,0x40,0x40}, // L
  {0x7F,0x02,0x0C,0x02,0x7F}, // M
  {0x7F,0x04,0x08,0x10,0x7F}, // N
  {0x3E,0x41,0x41,0x41,0x3E}, // O
  {0x7F,0x09,0x09,0x09,0x06}, // P
  {0x3E,0x41,0x51,0x21,0x5E}, // Q
  {0x7F,0x09,0x19,0x29,0x46}, // R
  {0x46,0x49,0x49,0x49,0x31}, // S
  {0x01,0x01,0x7F,0x01,0x01}, // T
  {0x3F,0x40,0x40,0x40,0x3F}, // U
  {0x1F,0x20,0x40,0x20,0x1F}, // V
  {0x3F,0x40,0x38,0x40,0x3F}, // W
  {0x63,0x14,0x08,0x14,0x63}, // X
  {0x07,0x08,0x70,0x08,0x07}, // Y
  {0x61,0x51,0x49,0x45,0x43}, // Z
  // '(' (40)
  {0x00,0x1C,0x22,0x41,0x00}, // (
  // ')' (41)
  {0x00,0x41,0x22,0x1C,0x00}  // )
};

// helper to get font index for char
static const uint8_t* get_char_bitmap(char c) {
  if (c == ' ') return font5x7[0];
  if (c >= 'A' && c <= 'Z') return font5x7[1 + (c - 'A')];
  if (c >= 'a' && c <= 'z') return font5x7[1 + (c - 'a')]; // map lowercase to uppercase
  if (c == '(') return font5x7[27];
  if (c == ')') return font5x7[28];
  // fallback: return space
  return font5x7[0];
}

// draw_char: draws a 5x7 char scaled by 'scale' at pixel position (x,y)
void ACeP730Display::draw_char(uint8_t *buf, int x, int y, char c, uint8_t idx, int scale) {
  const uint8_t *bm = get_char_bitmap(c);
  for (int col = 0; col < 5; col++) {
    uint8_t colbits = bm[col];
    for (int row = 0; row < 7; row++) {
      bool pixel_on = (colbits >> row) & 0x01;
      if (pixel_on) {
        // draw scaled block of size scale x scale
        for (int sx = 0; sx < scale; sx++) {
          for (int sy = 0; sy < scale; sy++) {
            draw_pixel_index(buf, x + col * scale + sx, y + row * scale + sy, idx);
          }
        }
      }
    }
  }
  // one column spacing after char
  // leave blank column of width scale (already blank by default)
}

// draw_text: draws a null-terminated string at (x,y)
void ACeP730Display::draw_text(uint8_t *buf, int x, int y, const char *s, uint8_t idx, int scale) {
  int cursor_x = x;
  while (*s) {
    char c = *s++;
    draw_char(buf, cursor_x, y, c, idx, scale);
    cursor_x += (5 * scale) + scale; // char width + spacing
  }
}

// show_colored_text_lines: builds buffer and draws the 4 requested lines
void ACeP730Display::show_colored_text_lines() {
  const int W = 800;
  const int H = 480;
  const int WB = W / 2;
  const size_t BUF_SIZE = WB * H;

  uint8_t *buf = (uint8_t*)malloc(BUF_SIZE);
  if (!buf) { ESP_LOGE("acep", "malloc failed for text buffer"); return; }

  // fill white background (WHITE index = 0x1)
  uint8_t white_packed = pack_two(0x1, 0x1);
  for (size_t i = 0; i < BUF_SIZE; ++i) buf[i] = white_packed;

  // color indices (Waveshare defines)
  const uint8_t IDX_BLACK  = 0x0;
  const uint8_t IDX_WHITE  = 0x1;
  const uint8_t IDX_YELLOW = 0x2;
  const uint8_t IDX_RED    = 0x3;
  const uint8_t IDX_BLUE   = 0x5;
  const uint8_t IDX_GREEN  = 0x6;

  // Lines to draw (Dutch text)
  const char *line1 = "Dit is rood (in het rood)";
  const char *line2 = "Dit is blauw (in het blauw)";
  const char *line3 = "Dit is groen (in het groen)";
  const char *line4 = "Dit is geel (in het geel)";

  // choose scale so text fits: 5x7 font, scale 6 -> char width ~36 px incl spacing
  int scale = 6;
  // compute vertical positions for 4 lines centered vertically
  int line_height = 7 * scale + scale; // char height + spacing
  int total_text_height = line_height * 4;
  int start_y = (H - total_text_height) / 2;

  // center each line horizontally: compute text pixel width
  auto text_pixel_width = [&](const char *s)->int {
    int len = 0;
    for (const char *p = s; *p; ++p) len++;
    return len * (5 * scale + scale);
  };

  int x1 = (W - text_pixel_width(line1)) / 2;
  int x2 = (W - text_pixel_width(line2)) / 2;
  int x3 = (W - text_pixel_width(line3)) / 2;
  int x4 = (W - text_pixel_width(line4)) / 2;

  // draw lines with their colors
  draw_text(buf, x1, start_y + 0 * line_height, line1, IDX_RED, scale);
  draw_text(buf, x2, start_y + 1 * line_height, line2, IDX_BLUE, scale);
  draw_text(buf, x3, start_y + 2 * line_height, line3, IDX_GREEN, scale);
  draw_text(buf, x4, start_y + 3 * line_height, line4, IDX_YELLOW, scale);

  // send to display
  display_packed_image(buf, BUF_SIZE);
  free(buf);
}

// show_6_stripes_test: convenience test
void ACeP730Display::show_6_stripes_test() {
  const int W = 800;
  const int H = 480;
  const int WB = W / 2;
  const size_t BUF_SIZE = WB * H;

  uint8_t *buf = (uint8_t*)malloc(BUF_SIZE);
  if (!buf) { ESP_LOGE("acep", "malloc failed for test buffer"); return; }

  const uint8_t Color_seven[6] = {0x0, 0x2, 0x3, 0x5, 0x6, 0x1};

  for (int y = 0; y < H; y++) {
    for (int bx = 0; bx < WB; bx++) {
      int stripe = (bx * 6) / WB;
      uint8_t idx = Color_seven[stripe] & 0x0F;
      buf[y * WB + bx] = (uint8_t)((idx << 4) | idx);
    }
  }

  display_packed_image(buf, BUF_SIZE);
  free(buf);
}

// run_init_sequence: copied from Waveshare EPD_7in3e.c (used in setup)
void ACeP730Display::run_init_sequence() {
  send_command(0xAA); send_data(0x49); send_data(0x55); send_data(0x20); send_data(0x08); send_data(0x09); send_data(0x18);
  send_command(0x01); send_data(0x3F);
  send_command(0x00); send_data(0x5F); send_data(0x69);
  send_command(0x03); send_data(0x00); send_data(0x54); send_data(0x00); send_data(0x44);
  send_command(0x05); send_data(0x40); send_data(0x1F); send_data(0x1F); send_data(0x2C);
  send_command(0x06); send_data(0x6F); send_data(0x1F); send_data(0x17); send_data(0x49);
  send_command(0x08); send_data(0x6F); send_data(0x1F); send_data(0x1F); send_data(0x22);
  send_command(0x30); send_data(0x03);
  send_command(0x50); send_data(0x3F);
  send_command(0x60); send_data(0x02); send_data(0x00);
  send_command(0x61); send_data(0x03); send_data(0x20); send_data(0x01); send_data(0xE0);
  send_command(0x84); send_data(0x01);
  send_command(0xE3); send_data(0x2F);
}

// setup: configure pins, rails, reset, init
void ACeP730Display::setup() {
  pinMode(cs_pin_, OUTPUT);
  pinMode(dc_pin_, OUTPUT);
  pinMode(reset_pin_, OUTPUT);
  pinMode(busy_pin_, INPUT);

  if (rail_en_pin_ >= 0) {
    pinMode(rail_en_pin_, OUTPUT);
    digitalWrite(rail_en_pin_, LOW);
  }

  SPI.begin();
  if (spi_hz_ > 0) SPI.setFrequency(spi_hz_);
  else SPI.setFrequency(2000000);

  // Power-on rails
  if (rail_en_pin_ >= 0) {
    digitalWrite(rail_en_pin_, HIGH);
    delay(120);
  } else {
    delay(50);
  }

  // Hardware reset
  digitalWrite(reset_pin_, HIGH);
  delay(20);
  digitalWrite(reset_pin_, LOW);
  delay(2);
  digitalWrite(reset_pin_, HIGH);
  delay(20);

  wait_busy_idle();
  delay(30);

  // init sequence
  run_init_sequence();

  // power on and wait
  send_command(0x04);
  wait_busy_idle();

  ESP_LOGD("acep", "ACeP730 setup complete");
}

// update: called periodically by ESPHome; show the colored text once
void ACeP730Display::update() {
  ESP_LOGD("acep", "ACeP730 update() - drawing colored text lines");
  show_colored_text_lines();
  // After first draw, increase interval by setting PollingComponent interval if desired.
  // For simplicity we keep periodic updates (every minute by default).
}
