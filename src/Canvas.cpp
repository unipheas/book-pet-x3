#include "Canvas.h"

#include "Font5x7.h"

#include <cstdlib>

void Canvas::pixel(int x, int y, bool black) {
  if (x < 0 || y < 0 || x >= w || y >= h) return;
  int physicalX = x;
  int physicalY = y;
  if (orientation == Rotation::Clockwise) {
    physicalX = physicalW - 1 - y;
    physicalY = x;
  } else if (orientation == Rotation::CounterClockwise) {
    physicalX = y;
    physicalY = physicalH - 1 - x;
  }
  if (physicalX < 0 || physicalY < 0 ||
      physicalX >= physicalW || physicalY >= physicalH) return;
  uint8_t& cell = data[physicalY * stride + physicalX / 8];
  const uint8_t mask = 0x80 >> (physicalX % 8);
  if (black) cell &= ~mask;
  else cell |= mask;
}

void Canvas::line(int x0, int y0, int x1, int y1) {
  const int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  const int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int error = dx + dy;
  while (true) {
    pixel(x0, y0);
    if (x0 == x1 && y0 == y1) break;
    const int twice = 2 * error;
    if (twice >= dy) { error += dy; x0 += sx; }
    if (twice <= dx) { error += dx; y0 += sy; }
  }
}

void Canvas::rect(int x, int y, int width, int height, bool fill) {
  if (fill) {
    for (int yy = y; yy < y + height; ++yy)
      for (int xx = x; xx < x + width; ++xx) pixel(xx, yy);
    return;
  }
  line(x, y, x + width - 1, y);
  line(x, y + height - 1, x + width - 1, y + height - 1);
  line(x, y, x, y + height - 1);
  line(x + width - 1, y, x + width - 1, y + height - 1);
}

void Canvas::character(int x, int y, char c, uint8_t scale) {
  if (c < 32 || c > 126) c = '?';
  const uint8_t* glyph = FONT_5X7[c - 32];
  for (int column = 0; column < 5; ++column)
    for (int row = 0; row < 7; ++row)
      if (glyph[column] & (1 << row))
        rect(x + column * scale, y + row * scale, scale, scale, true);
}

void Canvas::text(int x, int y, const char* value, uint8_t scale) {
  while (*value) {
    character(x, y, *value++, scale);
    x += 6 * scale;
  }
}

int Canvas::textWidth(const char* value, uint8_t scale) const {
  return strlen(value) * 6 * scale;
}
