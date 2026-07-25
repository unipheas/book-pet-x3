#pragma once

#include <Arduino.h>

enum class PetAction : uint8_t { Feed, Play, Sleep };

struct PetState {
  uint32_t version = 1;
  uint32_t activeMinutes = 0;
  uint16_t interactions = 0;
  uint8_t hunger = 22;
  uint8_t happiness = 72;
  uint8_t energy = 85;
  bool sleeping = false;
};

class PetEngine {
 public:
  void begin();
  const PetState& state() const { return pet; }
  bool tick(uint32_t nowMs);
  void apply(PetAction action);
  void save() const;

 private:
  static uint8_t clampAdd(uint8_t value, int delta);
  PetState pet;
  uint32_t lastTickMs = 0;
  uint8_t unsavedTicks = 0;
};

