#include "PetState.h"

#include <Preferences.h>

#include "PetRules.h"

namespace {
constexpr char kNamespace[] = "bookpet";
constexpr char kStateKey[] = "state";
constexpr uint32_t kTickMs = 60'000;
}

void PetEngine::begin() {
  Preferences prefs;
  prefs.begin(kNamespace, true);
  if (prefs.getBytesLength(kStateKey) == sizeof(PetState)) {
    PetState saved;
    prefs.getBytes(kStateKey, &saved, sizeof(saved));
    if (saved.version == 2) pet = saved;
  }
  prefs.end();
  lastTickMs = millis();
}

bool PetEngine::tick(uint32_t nowMs) {
  if (nowMs - lastTickMs < kTickMs) return false;
  const uint32_t minutes = (nowMs - lastTickMs) / kTickMs;
  lastTickMs += minutes * kTickMs;
  pet.activeMinutes += minutes;

  if (pet.sleeping) {
    pet.energy = clampNeed(pet.energy + static_cast<int>(minutes));
    pet.fullness = clampNeed(pet.fullness - static_cast<int>(minutes / 5));
  } else {
    pet.energy = clampNeed(pet.energy - static_cast<int>(minutes / 3));
    pet.fullness = clampNeed(pet.fullness - static_cast<int>(minutes / 2));
    if (pet.fullness < 25) {
      pet.happiness = clampNeed(pet.happiness - static_cast<int>(minutes / 4));
    }
    const uint32_t previousPageWindow = (pet.activeMinutes - minutes) / 10;
    const uint32_t currentPageWindow = pet.activeMinutes / 10;
    if (currentPageWindow > previousPageWindow) {
      pet.pageBites += static_cast<uint16_t>(currentPageWindow - previousPageWindow);
      pet.lastEvent = 4;
    }
  }

  unsavedTicks++;
  if (unsavedTicks >= 5) {
    save();
    unsavedTicks = 0;
  }
  return true;
}

void PetEngine::apply(PetAction action) {
  pet.interactions++;
  switch (action) {
    case PetAction::Feed:
      pet.sleeping = false;
      if (pet.food == 0) {
        pet.lastEvent = 3;
        break;
      }
      pet.food--;
      pet.fullness = clampNeed(pet.fullness + 30);
      pet.energy = clampNeed(pet.energy + 4);
      pet.lastEvent = 1;
      addExperience(4);
      break;
    case PetAction::Play:
      pet.sleeping = false;
      pet.happiness = clampNeed(pet.happiness + 22);
      pet.energy = clampNeed(pet.energy - 10);
      pet.fullness = clampNeed(pet.fullness - 6);
      pet.pageBites++;
      pet.lastEvent = 2;
      addExperience(8);
      break;
    case PetAction::Rest:
      pet.sleeping = true;
      pet.lastEvent = 5;
      break;
  }
  save();
}

bool PetEngine::buyFood() {
  if (pet.pageBites < foodCostPages() || pet.food >= 99) {
    pet.lastEvent = 6;
    return false;
  }
  pet.pageBites -= foodCostPages();
  pet.food++;
  pet.lastEvent = 7;
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
    pet.lastEvent = 8;
  }
}

void PetEngine::wake() {
  if (!pet.sleeping) return;
  pet.sleeping = false;
  pet.lastEvent = 9;
  save();
}

PetMood PetEngine::mood() const {
  if (pet.sleeping) return PetMood::Sleeping;
  if (pet.fullness < 25) return PetMood::Hungry;
  if (pet.energy < 25) return PetMood::Tired;
  if (pet.happiness < 30) return PetMood::Lonely;
  if (pet.happiness > 82) return PetMood::Happy;
  return PetMood::Content;
}

uint16_t PetEngine::nextLevelXp() const {
  return xpForNextLevel(pet.level);
}

const char* PetEngine::thought() const {
  switch (pet.lastEvent) {
    case 1: return "Crunchy! Tastes like chapter three.";
    case 2: return "I found a Page Bite!";
    case 3: return "The snack shelf is empty.";
    case 4: return "A loose page drifted by.";
    case 5: return "Saving my place... zzz";
    case 6: return "I need three Page Bites.";
    case 7: return "Fresh page snack, baked!";
    case 8: return "Level up! My thoughts feel bigger.";
    case 9: return "Oh! You're back.";
    default: break;
  }
  switch (mood()) {
    case PetMood::Hungry: return "Could I nibble a little story?";
    case PetMood::Tired: return "My pixels feel sleepy.";
    case PetMood::Lonely: return "Maybe we can play a chapter?";
    case PetMood::Happy: return "Today has excellent margins.";
    case PetMood::Sleeping: return "Dreaming between the lines...";
    case PetMood::Content:
    default:
      break;
  }
  static constexpr const char* thoughts[] = {
      "I wonder what's after this page.",
      "Quiet pages have loud secrets.",
      "I saved you a good sentence.",
      "Do bookmarks dream of chapters?",
  };
  return thoughts[(pet.activeMinutes + pet.interactions + pet.level) % 4];
}

void PetEngine::save() const {
  Preferences prefs;
  prefs.begin(kNamespace, false);
  prefs.putBytes(kStateKey, &pet, sizeof(pet));
  prefs.end();
}
