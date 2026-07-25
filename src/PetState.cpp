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

struct PetStateV3 {
  uint32_t version;
  uint32_t activeMinutes;
  uint16_t interactions;
  uint16_t experience;
  uint16_t pageBites;
  uint16_t fragments[4];
  uint16_t feedCount;
  uint16_t playCount;
  uint16_t cleanCount;
  uint16_t restCount;
  uint8_t level;
  uint8_t fullness;
  uint8_t happiness;
  uint8_t energy;
  uint8_t cleanliness;
  uint8_t curiosity;
  uint8_t food;
  uint8_t species;
  uint8_t lastEvent;
  uint8_t diary[3];
  bool sleeping;
};
static_assert(sizeof(PetStateV3) == 44);

struct PetStateV4 {
  uint32_t version;
  uint32_t activeMinutes;
  uint32_t lifetimePages;
  uint16_t interactions;
  uint16_t experience;
  uint16_t pageBites;
  uint16_t fragments[4];
  uint16_t feedCount;
  uint16_t playCount;
  uint16_t cleanCount;
  uint16_t restCount;
  uint16_t currentBookPages;
  uint16_t booksFinished;
  uint16_t readingSessions;
  uint16_t autonomousSteps;
  uint8_t level;
  uint8_t fullness;
  uint8_t happiness;
  uint8_t energy;
  uint8_t cleanliness;
  uint8_t curiosity;
  uint8_t food;
  uint8_t species;
  uint8_t unlockedPets;
  uint8_t toys;
  uint8_t equippedToy;
  uint8_t ambientPose;
  uint8_t lastEvent;
  uint8_t diary[3];
  bool autonomousEnabled;
  bool sleeping;
};
static_assert(sizeof(PetStateV4) == 60);
static_assert(offsetof(PetStateV4, autonomousEnabled) ==
              offsetof(PetState, sleepCycles));
}

void PetEngine::begin() {
  bool migrated = false;
  Preferences prefs;
  prefs.begin(kNamespace, true);
  const size_t savedSize = prefs.getBytesLength(kStateKey);
  if (savedSize == sizeof(PetState)) {
    PetState saved;
    prefs.getBytes(kStateKey, &saved, sizeof(saved));
    if (saved.version == 5) pet = saved;
  } else if (savedSize == sizeof(PetStateV4)) {
    PetStateV4 old;
    prefs.getBytes(kStateKey, &old, sizeof(old));
    if (old.version == 4) {
      memcpy(&pet, &old, offsetof(PetStateV4, autonomousEnabled));
      pet.version = 5;
      pet.autonomousEnabled = old.autonomousEnabled;
      pet.sleeping = old.sleeping;
      migrated = true;
    }
  } else if (savedSize == sizeof(PetStateV3)) {
    PetStateV3 old;
    prefs.getBytes(kStateKey, &old, sizeof(old));
    if (old.version == 3) {
      pet.activeMinutes = old.activeMinutes;
      pet.interactions = old.interactions;
      pet.experience = old.experience;
      pet.pageBites = old.pageBites;
      memcpy(pet.fragments, old.fragments, sizeof(old.fragments));
      pet.feedCount = old.feedCount;
      pet.playCount = old.playCount;
      pet.cleanCount = old.cleanCount;
      pet.restCount = old.restCount;
      pet.level = old.level;
      pet.fullness = old.fullness;
      pet.happiness = old.happiness;
      pet.energy = old.energy;
      pet.cleanliness = old.cleanliness;
      pet.curiosity = old.curiosity;
      pet.food = old.food;
      pet.species = old.species;
      pet.lastEvent = old.lastEvent;
      memcpy(pet.diary, old.diary, sizeof(old.diary));
      pet.sleeping = old.sleeping;
      migrated = true;
    }
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
      pet.sleepCycles = 0;
      pet.ambientPose = 6;
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

uint8_t PetEngine::logPages(uint16_t pages) {
  if (pages == 0) return 0;
  const uint32_t before = pet.lifetimePages;
  pet.lifetimePages += pages;
  pet.currentBookPages =
      static_cast<uint16_t>(min(65'535UL,
                                static_cast<unsigned long>(pet.currentBookPages) +
                                    pages));
  pet.readingSessions++;
  const uint32_t earned = pet.lifetimePages / 10 - before / 10;
  pet.food = static_cast<uint8_t>(
      min(99UL, static_cast<unsigned long>(pet.food) + earned));
  pet.curiosity = clampNeed(pet.curiosity - min(30, pages / 2));
  pet.happiness = clampNeed(pet.happiness + min(15, pages / 4));
  pet.interactions++;
  recordEvent(20);
  addExperience(min<uint16_t>(50, pages));
  save();
  return static_cast<uint8_t>(min(255UL, earned));
}

bool PetEngine::finishBook() {
  if (pet.currentBookPages == 0) return false;
  pet.booksFinished++;
  pet.currentBookPages = 0;
  const uint8_t toy = (pet.booksFinished - 1) % 4;
  pet.toys |= static_cast<uint8_t>(1U << toy);
  if (pet.equippedToy == 0xFF) pet.equippedToy = toy;
  if (pet.booksFinished >= 1) pet.unlockedPets |= 0x02;
  if (pet.booksFinished >= 3) pet.unlockedPets |= 0x04;
  pet.happiness = clampNeed(pet.happiness + 25);
  pet.pageBites += 5;
  recordEvent(21);
  addExperience(25);
  save();
  return true;
}

bool PetEngine::selectSpecies(uint8_t species) {
  if (!speciesUnlocked(species)) return false;
  pet.species = species;
  recordEvent(22);
  save();
  return true;
}

bool PetEngine::equipToy(uint8_t toy) {
  if (!toyUnlocked(toy)) return false;
  pet.equippedToy = toy;
  recordEvent(24);
  save();
  return true;
}

void PetEngine::awakeMoment(bool drowsy) {
  if (!pet.autonomousEnabled || pet.sleeping) return;
  pet.autonomousSteps++;
  pet.idleVariant = (pet.idleVariant + 1 + pet.level) % 4;
  if (drowsy) {
    pet.ambientPose = 5;
    pet.lastEvent = 25;
  } else if (pet.fullness < 25) {
    pet.ambientPose = 4;
    pet.lastEvent = 29;
  } else if (pet.cleanliness < 25) {
    pet.ambientPose = 1;
    pet.lastEvent = 30;
  } else if (pet.happiness < 40 && pet.equippedToy != 0xFF) {
    pet.ambientPose = 3;
    pet.lastEvent = 31;
  } else {
    static constexpr uint8_t kWalk[] = {1, 0, 2, 0, 3, 4, 0};
    pet.ambientPose = kWalk[pet.autonomousSteps % 7];
    pet.lastEvent = 23;
  }
  if (!drowsy && pet.ambientPose == 3 && pet.equippedToy != 0xFF) {
    pet.happiness = clampNeed(pet.happiness + 2);
    pet.energy = clampNeed(pet.energy - 1);
  } else if (pet.ambientPose == 4) {
    pet.curiosity = clampNeed(pet.curiosity - 2);
  }
}

void PetEngine::beginNaturalSleep() {
  if (pet.sleeping) return;
  pet.restCount++;
  pet.sleeping = true;
  pet.sleepCycles = 0;
  pet.ambientPose = 6;
  recordEvent(26);
  save();
}

bool PetEngine::dreamMoment() {
  if (!pet.autonomousEnabled) return false;
  if (!pet.sleeping) {
    awakeMoment();
    return false;
  }

  pet.sleepCycles++;
  pet.energy = clampNeed(pet.energy + 12);
  pet.fullness = clampNeed(pet.fullness - 1);
  if (pet.sleepCycles >= 2) {
    pet.sleeping = false;
    pet.sleepCycles = 0;
    pet.ambientPose = 0;
    recordEvent(28);
    save();
    return true;
  }

  pet.ambientPose = 7 + (pet.sleepCycles % 2);
  pet.lastEvent = 27;
  save();
  return false;
}

void PetEngine::toggleAutonomy() {
  pet.autonomousEnabled = !pet.autonomousEnabled;
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
  pet.sleepCycles = 0;
  pet.ambientPose = 0;
  pet.energy = clampNeed(pet.energy + 12);
  recordEvent(9);
  save();
}

void PetEngine::resetTickClock() {
  lastTickMs = millis();
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
    case 20: return "Those pages were delicious!";
    case 21: return "A whole book! I found a toy!";
    case 22: return "A new friend moved in.";
    case 23:
      switch (pet.ambientPose) {
        case 1: return "Something rustled over here.";
        case 2: return "This corner smells like stories.";
        case 3: return "My toy wants an adventure.";
        case 4: return "Just checking the room.";
        default: return "I had a tiny adventure.";
      }
    case 24: return "Can we play with this one?";
    case 25: return "My eyes are getting heavy...";
    case 26: return "I found the coziest spot.";
    case 27:
      return pet.sleepCycles % 2 ? "Dreaming of a paper moon..."
                                 : "Chasing commas in my sleep...";
    case 28: return "Good morning! I woke up myself.";
    case 29: return "I am checking for dropped snacks.";
    case 30: return "Maybe the clean corner is over here.";
    case 31: return "My toy keeps me company.";
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
    case 20: return "Read pages and earned a snack.";
    case 21: return "Finished a book and found a toy.";
    case 22: return "Welcomed a different pet home.";
    case 24: return "Picked a favorite toy.";
    case 26: return "Fell asleep in a cozy corner.";
    case 28: return "Woke up and explored alone.";
    default: return "Watched the loose pages drift.";
  }
}

const char* PetEngine::speciesName() const {
  return speciesName(pet.species);
}

const char* PetEngine::speciesName(uint8_t species) {
  switch (static_cast<PetSpecies>(species)) {
    case PetSpecies::Byte: return "BYTE";
    case PetSpecies::Mote: return "MOTE";
    case PetSpecies::Pip: return "PIP";
  }
  return "BYTE";
}

const char* PetEngine::toyName(uint8_t toy) {
  switch (static_cast<ToyKind>(toy)) {
    case ToyKind::Ball: return "PAGE BALL";
    case ToyKind::Bell: return "TINY BELL";
    case ToyKind::Blocks: return "LETTER BLOCKS";
    case ToyKind::Kite: return "PAPER KITE";
  }
  return "NONE";
}

bool PetEngine::speciesUnlocked(uint8_t species) const {
  return species < 3 && (pet.unlockedPets & (1U << species));
}

bool PetEngine::toyUnlocked(uint8_t toy) const {
  return toy < 4 && (pet.toys & (1U << toy));
}

void PetEngine::save() const {
  Preferences prefs;
  prefs.begin(kNamespace, false);
  prefs.putBytes(kStateKey, &pet, sizeof(pet));
  prefs.end();
}
