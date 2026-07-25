#pragma once

#include <stdint.h>

enum class InputIntent : uint8_t { None, Choose, Act };

// X3 units expose page controls through more than one physical button ladder.
// Pair every "previous/upper" control with Choose and every "next/lower"
// control with Act so the pet works with either side or front page buttons.
constexpr InputIntent routeButton(uint8_t button) {
  switch (button) {
    case 0:  // Back
    case 2:  // Left
    case 4:  // Up
      return InputIntent::Choose;
    case 1:  // Confirm
    case 3:  // Right
    case 5:  // Down
      return InputIntent::Act;
    default:
      return InputIntent::None;
  }
}

static_assert(routeButton(0) == InputIntent::Choose);
static_assert(routeButton(2) == InputIntent::Choose);
static_assert(routeButton(4) == InputIntent::Choose);
static_assert(routeButton(1) == InputIntent::Act);
static_assert(routeButton(3) == InputIntent::Act);
static_assert(routeButton(5) == InputIntent::Act);
static_assert(routeButton(6) == InputIntent::None);

