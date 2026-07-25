#include "PetState.h"

#include <Preferences.h>

#include "PetRules.h"

namespace {
constexpr char kNamespace[] = "bookpet";
constexpr char kStateKey[] = "state";
constexpr uint32_t kTickMs = 60'000;

struct PetStateV2 {
  uint32_t version;
  uint32_t activeMinutes;
  uint16_t interactions;
  uint16_t experience;
  uint16_t pageBites;
  uint8_t level;
  uint8_t fullness;
  uint8_t happiness;
  uint8_t energy;
  uint8_t food;
  uint8_t species;
  uint8_t lastEvent;
  bool sleeping;
};
static_assert(sizeof(PetStateV2) == 24);
}

void PetEngine::begin() {
  bool migrated = false;
  Preferences prefs;
  prefs.begin(kNamespace, true);
  const size_t savedSize = prefs.getBytesLength(kStateKey);
  if (savedSize == sizeof(PetState)) {
    PetState saved;
    prefs.getBytes(kStateKey, &saved, sizeof(saved));
    if (saved.version == 3) pet = saved;
  } else if (savedSize == sizeof(PetStateV2)) {
    PetStateV2 old;
    prefs.getBytes(kStateKey, &old, sizeof(old));
    if (old.version == 2) {
      pet.activeMinutes = old.activeMinutes;
      pet.interactions = old.interactions;
      pet.experience = old.experience;
      pet.pageBites = old.pageBites;
      pet.level = old.level;
      pet.fullness = old.fullness;
      pet.happiness = old.happiness;
      pet.energy = old.energy;
      pet.food = old.food;
      pet.species = old.species;
      pet.sleeping = old.sleeping;
      recordEvent(11);
      migrated = true;
    }
  }
  prefs.end();
  if (migrated) save();
  lastTickMs = millis();
}

bool PetEngine::tick(uint32_t nowMs) {
  if (nowMs - lastTickMs < kTickMs) return false;
  const uint32_t minutes = (nowMs - lastTickMs) / kTickMs;
  lastTickMs += minutes * kTickMs;
  const uint32_t previousMinutes = pet.activeMinutes;
  const uint8_t oldFullness = pet.fullness;
  const uint8_t oldHappiness = pet.happiness;
  const uint8_t oldEnergy = pet.energy;
  const uint8_t oldCleanliness = pet.cleanliness;
  const uint8_t oldCuriosity = pet.curiosity;
  pet.activeMinutes += minutes;

  if (pet.sleeping) {
    pet.energy = clampNeed(pet.energy + static_cast<int>(minutes));
    const uint32_t hungerSteps =
        intervalsCrossed(previousMinutes, pet.activeMinutes, 6);
    pet.fullness = clampNeed(pet.fullness - static_cast<int>(hungerSteps));
  } else {
    const uint32_t energySteps =
        intervalsCrossed(previousMinutes, pet.activeMinutes, 4);
    const uint32_t hungerSteps =
        intervalsCrossed(previousMinutes, pet.activeMinutes, 2);
    const uint32_t cleanSteps =
        intervalsCrossed(previousMinutes, pet.activeMinutes, 3);
    const uint32_t curiositySteps =
        intervalsCrossed(previousMinutes, pet.activeMinutes, 5);
    pet.energy = clampNeed(pet.energy - static_cast<int>(energySteps));
    pet.fullness = clampNeed(pet.fullness - static_cast<int>(hungerSteps));
    pet.cleanliness = clampNeed(pet.cleanliness - static_cast<int>(cleanSteps));
    pet.curiosity = clampNeed(pet.curiosity + static_cast<int>(curiositySteps));
    if (pet.fullness < 25 || pet.cleanliness < 25) {
      const uint32_t unhappySteps =
          intervalsCrossed(previousMinutes, pet.activeMinutes, 4);
      pet.happiness =
          clampNeed(pet.happiness - static_cast<int>(unhappySteps));
    }
  }

  unsavedTicks++;
  if (unsavedTicks >= 5) {
    save();
    unsavedTicks = 0;
  }
  return pet.fullness != oldFullness || pet.happiness != oldHappiness ||
         pet.energy != oldEnergy || pet.cleanliness != oldCleanliness ||
         pet.curiosity != oldCuriosity;
}

void PetEngine::apply(PetAction action) {
  pet.interactions++;
  switch (action) {
    case PetAction::Feed:
      pet.sleeping = false;
      if (pet.food == 0) {
        recordEvent(3);
        break;
      }
      pet.food--;
      pet.feedCount++;
      pet.fullness = clampNeed(pet.fullness + 30);
      pet.energy = clampNeed(pet.energy + 4);
      pet.cleanliness = clampNeed(pet.cleanliness - 4);
      recordEvent(1);
      addExperience(4);
      break;
    case PetAction::Play:
      // The Page Catch screen completes this action.
      pet.sleeping = false;
      break;
    case PetAction::Clean:
      pet.sleeping = false;
      pet.cleanCount++;
      pet.cleanliness = clampNeed(pet.cleanliness + 45);
      pet.happiness = clampNeed(pet.happiness + 5);
      recordEvent(12);
      addExperience(3);
      break;
    case PetAction::Rest:
      pet.restCount++;
      pet.sleeping = true;
      recordEvent(5);
      break;
  }
  save();
}

void PetEngine::completePageCatch(bool caught, FragmentKind kind) {
  pet.interactions++;
  pet.playCount++;
  pet.sleeping = false;
  pet.energy = clampNeed(pet.energy - 8);
  pet.fullness = clampNeed(pet.fullness - 5);
  pet.curiosity = clampNeed(pet.curiosity - (caught ? 18 : 8));
  if (caught) {
    pet.fragments[static_cast<uint8_t>(kind)]++;
    pet.pageBites += 2;
    pet.happiness = clampNeed(pet.happiness + 18);
    recordEvent(13 + static_cast<uint8_t>(kind));
    addExperience(10);
  } else {
    pet.happiness = clampNeed(pet.happiness + 4);
    recordEvent(17);
    addExperience(2);
  }
  save();
}

bool PetEngine::buyFood() {
  if (pet.pageBites < foodCostPages() || pet.food >= 99) {
    recordEvent(6);
    return false;
  }
  pet.pageBites -= foodCostPages();
  pet.food++;
  recordEvent(7);
  addExperience(2);
  save();
  return true;
}

void PetEngine::addExperience(uint16_t amount) {
  pet.experience += amount;
  while (pet.experience >= nextLevelXp()) {
    pet.experience -= nextLevelXp();
    pet.level++;
    pet.food = static_cast<uint8_t>(min(99, pet.food + 2));
    pet.pageBites += 2;
    recordEvent(8);
  }
}

void PetEngine::recordEvent(uint8_t event) {
  pet.lastEvent = event;
  pet.diary[2] = pet.diary[1];
  pet.diary[1] = pet.diary[0];
  pet.diary[0] = event;
}

void PetEngine::wake() {
  if (!pet.sleeping) return;
  pet.sleeping = false;
  pet.energy = clampNeed(pet.energy + 12);
  recordEvent(9);
  save();
}

PetMood PetEngine::mood() const {
  if (pet.sleeping) return PetMood::Sleeping;
  if (pet.cleanliness < 25) return PetMood::Dirty;
  if (pet.fullness < 25) return PetMood::Hungry;
  if (pet.energy < 25) return PetMood::Tired;
  if (pet.happiness < 30) return PetMood::Lonely;
  if (pet.happiness > 82) return PetMood::Happy;
  return PetMood::Content;
}

Personality PetEngine::personality() const {
  if (pet.restCount > pet.playCount + 3) return Personality::Cozy;
  if (pet.playCount > pet.feedCount + pet.cleanCount) return Personality::Bold;
  return Personality::Curious;
}

LifeStage PetEngine::lifeStage() const {
  if (pet.level < 3) return LifeStage::Hatchling;
  if (pet.level < 6) return LifeStage::Sprout;
  return LifeStage::Familiar;
}

uint16_t PetEngine::nextLevelXp() const {
  return xpForNextLevel(pet.level);
}

const char* PetEngine::thought() const {
  switch (pet.lastEvent) {
    case 1: return "Crunch! That page had good words.";
    case 3: return "The snack shelf is empty.";
    case 5: return "Saving my place... zzz";
    case 6: return "I need three Page Bites.";
    case 7: return "Fresh page snack, baked!";
    case 8: return "Level up! I feel different.";
    case 9: return "Oh! You're back.";
    case 11: return "I remember our last chapter.";
    case 12: return "My pixels are squeaky clean!";
    case 13: return "A Story fragment! Tell me more.";
    case 14: return "A Mystery fragment... curious.";
    case 15: return "Science! My brain is buzzing.";
    case 16: return "Adventure! Let's go farther.";
    case 17: return "It escaped. One more try?";
    default: break;
  }
  switch (mood()) {
    case PetMood::Dirty: return "I have crumbs in my pixels.";
    case PetMood::Hungry: return "Could I nibble a little story?";
    case PetMood::Tired: return "My pixels feel sleepy.";
    case PetMood::Lonely: return "Let's catch a page together.";
    case PetMood::Happy: return "Today has excellent margins.";
    case PetMood::Sleeping: return "Dreaming between the lines...";
    case PetMood::Content: break;
  }
  if (pet.curiosity > 80) return "I hear a wild page nearby!";
  switch (personality()) {
    case Personality::Bold: return "Bet I can catch the next one.";
    case Personality::Cozy: return "A quiet corner sounds perfect.";
    case Personality::Curious: return "What's beyond the next page?";
  }
  return "";
}

const char* PetEngine::diaryLine(uint8_t index) const {
  if (index > 2) return "";
  switch (pet.diary[index]) {
    case 1: return "Ate a warm page snack.";
    case 5: return "Curled up for a long dream.";
    case 7: return "Baked something in the pantry.";
    case 8: return "Grew into a new level.";
    case 9: return "Woke up when you returned.";
    case 10: return "Hatched into the pocket world.";
    case 11: return "Carried memories into v0.3.";
    case 12: return "Had a very bubbly cleanup.";
    case 13: return "Caught a Story fragment.";
    case 14: return "Caught a Mystery fragment.";
    case 15: return "Caught a Science fragment.";
    case 16: return "Caught an Adventure fragment.";
    case 17: return "Chased a page that escaped.";
    default: return "Watched the loose pages drift.";
  }
}

void PetEngine::save() const {
  Preferences prefs;
  prefs.begin(kNamespace, false);
  prefs.putBytes(kStateKey, &pet, sizeof(pet));
  prefs.end();
}
