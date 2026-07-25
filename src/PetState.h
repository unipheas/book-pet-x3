#pragma once

#include <Arduino.h>

enum class PetAction : uint8_t { Feed, Play, Clean, Rest };
enum class PetMood : uint8_t {
  Content,
  Happy,
  Hungry,
  Tired,
  Lonely,
  Dirty,
  Sleeping
};
enum class Personality : uint8_t { Curious, Cozy, Bold };
enum class LifeStage : uint8_t { Hatchling, Sprout, Familiar };
enum class FragmentKind : uint8_t { Story, Mystery, Science, Adventure };

struct PetState {
  uint32_t version = 3;
  uint32_t activeMinutes = 0;
  uint16_t interactions = 0;
  uint16_t experience = 0;
  uint16_t pageBites = 0;
  uint16_t fragments[4] = {0, 0, 0, 0};
  uint16_t feedCount = 0;
  uint16_t playCount = 0;
  uint16_t cleanCount = 0;
  uint16_t restCount = 0;
  uint8_t level = 1;
  uint8_t fullness = 72;
  uint8_t happiness = 72;
  uint8_t energy = 82;
  uint8_t cleanliness = 78;
  uint8_t curiosity = 55;
  uint8_t food = 5;
  uint8_t species = 0;
  uint8_t lastEvent = 0;
  uint8_t diary[3] = {10, 0, 0};
  bool sleeping = false;
};
static_assert(sizeof(PetState) == 44);

class PetEngine {
 public:
  void begin();
  const PetState& state() const { return pet; }
  bool tick(uint32_t nowMs);
  void apply(PetAction action);
  void completePageCatch(bool caught, FragmentKind kind);
  bool buyFood();
  void wake();
  void save() const;
  PetMood mood() const;
  Personality personality() const;
  LifeStage lifeStage() const;
  const char* thought() const;
  const char* diaryLine(uint8_t index) const;
  uint16_t nextLevelXp() const;

 private:
  void addExperience(uint16_t amount);
  void recordEvent(uint8_t event);
  PetState pet;
  uint32_t lastTickMs = 0;
  uint8_t unsavedTicks = 0;
};
