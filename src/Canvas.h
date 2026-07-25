#pragma once

#include <Arduino.h>

class Canvas {
 public:
  Canvas(uint8_t* buffer, uint16_t width, uint16_t height)
      : data(buffer), w(width), h(height), stride(width / 8) {}

  void pixel(int x, int y, bool black = true);
  void line(int x0, int y0, int x1, int y1);
  void rect(int x, int y, int width, int height, bool fill = false);
  void text(int x, int y, const char* value, uint8_t scale = 1);
  int textWidth(const char* value, uint8_t scale = 1) const;

 private:
  void character(int x, int y, char c, uint8_t scale);
  uint8_t* data;
  uint16_t w;
  uint16_t h;
  uint16_t stride;
};

