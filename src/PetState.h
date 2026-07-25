#pragma once

#include <Arduino.h>

enum class PetAction : uint8_t { Feed, Play, Rest };
enum class PetMood : uint8_t { Content, Happy, Hungry, Tired, Lonely, Sleeping };

struct PetState {
  uint32_t version = 2;
  uint32_t activeMinutes = 0;
  uint16_t interactions = 0;
  uint16_t experience = 0;
  uint16_t pageBites = 0;
  uint8_t level = 1;
  uint8_t fullness = 72;
  uint8_t happiness = 72;
  uint8_t energy = 82;
  uint8_t food = 5;
  uint8_t species = 0;
  uint8_t lastEvent = 0;
  bool sleeping = false;
};

class PetEngine {
 public:
  void begin();
  const PetState& state() const { return pet; }
  bool tick(uint32_t nowMs);
  void apply(PetAction action);
  bool buyFood();
  void wake();
  void save() const;
  PetMood mood() const;
  const char* thought() const;
  uint16_t nextLevelXp() const;

 private:
  void addExperience(uint16_t amount);
  PetState pet;
  uint32_t lastTickMs = 0;
  uint8_t unsavedTicks = 0;
};
