#include "PetState.h"

#include <Preferences.h>

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
    if (saved.version == 1) pet = saved;
  }
  prefs.end();
  lastTickMs = millis();
}

uint8_t PetEngine::clampAdd(uint8_t value, int delta) {
  return static_cast<uint8_t>(constrain(static_cast<int>(value) + delta, 0, 100));
}

bool PetEngine::tick(uint32_t nowMs) {
  if (nowMs - lastTickMs < kTickMs) return false;
  const uint32_t minutes = (nowMs - lastTickMs) / kTickMs;
  lastTickMs += minutes * kTickMs;
  pet.activeMinutes += minutes;

  if (pet.sleeping) {
    pet.energy = clampAdd(pet.energy, static_cast<int>(minutes));
    pet.hunger = clampAdd(pet.hunger, static_cast<int>(minutes / 4));
  } else {
    pet.energy = clampAdd(pet.energy, -static_cast<int>(minutes / 3));
    pet.hunger = clampAdd(pet.hunger, static_cast<int>(minutes / 2));
    if (pet.hunger > 75) pet.happiness = clampAdd(pet.happiness, -static_cast<int>(minutes / 4));
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
      pet.hunger = clampAdd(pet.hunger, -28);
      pet.energy = clampAdd(pet.energy, 3);
      break;
    case PetAction::Play:
      pet.sleeping = false;
      pet.happiness = clampAdd(pet.happiness, 22);
      pet.energy = clampAdd(pet.energy, -12);
      pet.hunger = clampAdd(pet.hunger, 8);
      break;
    case PetAction::Sleep:
      pet.sleeping = true;
      break;
  }
  save();
}

void PetEngine::wake() {
  if (!pet.sleeping) return;
  pet.sleeping = false;
  save();
}

void PetEngine::save() const {
  Preferences prefs;
  prefs.begin(kNamespace, false);
  prefs.putBytes(kStateKey, &pet, sizeof(pet));
  prefs.end();
}
