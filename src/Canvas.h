#pragma once

#include <Arduino.h>

class Canvas {
 public:
  enum class Rotation : uint8_t { None, Clockwise, CounterClockwise };

  Canvas(uint8_t* buffer, uint16_t physicalWidth, uint16_t physicalHeight,
         Rotation rotation = Rotation::None)
      : data(buffer),
        physicalW(physicalWidth),
        physicalH(physicalHeight),
        w(rotation == Rotation::None ? physicalWidth : physicalHeight),
        h(rotation == Rotation::None ? physicalHeight : physicalWidth),
        stride(physicalWidth / 8),
        orientation(rotation) {}

  void pixel(int x, int y, bool black = true);
  void line(int x0, int y0, int x1, int y1);
  void rect(int x, int y, int width, int height, bool fill = false);
  void text(int x, int y, const char* value, uint8_t scale = 1);
  int textWidth(const char* value, uint8_t scale = 1) const;
  uint16_t width() const { return w; }
  uint16_t height() const { return h; }

 private:
  void character(int x, int y, char c, uint8_t scale);
  uint8_t* data;
  uint16_t physicalW;
  uint16_t physicalH;
  uint16_t w;
  uint16_t h;
  uint16_t stride;
  Rotation orientation;
};
