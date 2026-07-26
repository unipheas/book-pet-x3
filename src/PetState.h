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
enum class PetSpecies : uint8_t { Byte, Mote, Pip };
enum class ToyKind : uint8_t { Ball, Bell, Blocks, Kite };

struct PetState {
  uint32_t version = 6;
  uint32_t activeMinutes = 0;
  uint32_t lifetimePages = 0;
  uint16_t interactions = 0;
  uint16_t experience = 0;
  uint16_t pageBites = 0;
  // v6 reuses the retired fragment bytes for an idempotent reading-reward
  // transaction. The legacy view keeps old saves easy to migrate.
  union {
    uint16_t fragments[4] = {0, 0, 0, 0};
    struct __attribute__((packed)) {
      uint32_t lastReadingTransaction;
      uint32_t readingReserved;
    };
  };
  uint16_t feedCount = 0;
  uint16_t playCount = 0;
  uint16_t cleanCount = 0;
  uint16_t restCount = 0;
  uint16_t currentBookPages = 0;
  uint16_t booksFinished = 0;
  uint16_t readingSessions = 0;
  uint16_t autonomousSteps = 0;
  uint8_t level = 1;
  uint8_t fullness = 72;
  uint8_t happiness = 72;
  uint8_t energy = 82;
  uint8_t cleanliness = 78;
  uint8_t curiosity = 55;
  uint8_t food = 5;
  uint8_t species = 0;
  uint8_t unlockedPets = 0x01;
  uint8_t toys = 0;
  uint8_t equippedToy = 0xFF;
  uint8_t ambientPose = 0;
  uint8_t lastEvent = 0;
  uint8_t diary[3] = {10, 0, 0};
  uint8_t sleepCycles = 0;
  uint8_t idleVariant = 0;
  bool autonomousEnabled = true;
  bool sleeping = false;
};
static_assert(sizeof(PetState) == 64);

class PetEngine {
 public:
  void begin();
  const PetState& state() const { return pet; }
  bool tick(uint32_t nowMs);
  void apply(PetAction action);
  void completePageCatch(bool caught);
  bool completeReadingTransaction(uint32_t transaction, bool rewardsPage,
                                  bool finishesBook);
  void startReadingSession();
  bool selectSpecies(uint8_t species);
  bool equipToy(uint8_t toy);
  void awakeMoment(bool drowsy = false);
  void beginNaturalSleep();
  bool dreamMoment();
  void toggleAutonomy();
  bool buyFood();
  void wake();
  void resetTickClock();
  bool save() const;
  PetMood mood() const;
  Personality personality() const;
  LifeStage lifeStage() const;
  const char* thought() const;
  const char* diaryLine(uint8_t index) const;
  const char* speciesName() const;
  static const char* speciesName(uint8_t species);
  static const char* toyName(uint8_t toy);
  bool speciesUnlocked(uint8_t species) const;
  bool toyUnlocked(uint8_t toy) const;
  uint16_t nextLevelXp() const;

 private:
  void addExperience(uint16_t amount);
  void recordEvent(uint8_t event);
  PetState pet;
  uint32_t lastTickMs = 0;
  uint8_t unsavedTicks = 0;
};
