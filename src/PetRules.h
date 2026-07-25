#pragma once

#include <stdint.h>

constexpr uint8_t clampNeed(int value) {
  return value < 0 ? 0 : (value > 100 ? 100 : static_cast<uint8_t>(value));
}

constexpr uint16_t xpForNextLevel(uint8_t level) {
  return static_cast<uint16_t>(25 + (level - 1) * 15);
}

constexpr uint8_t foodCostPages() { return 3; }

static_assert(clampNeed(-1) == 0);
static_assert(clampNeed(101) == 100);
static_assert(xpForNextLevel(1) == 25);
static_assert(xpForNextLevel(5) == 85);
static_assert(foodCostPages() == 3);
