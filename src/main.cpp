#include <Arduino.h>
#include <BoardConfig.h>
#include <EInkDisplay.h>
#include <InputManager.h>
#include <PowerManager.h>
#include <SDCardManager.h>
#include <SPI.h>
#include <XteinkDetect.h>
#include <driver/gpio.h>
#include <esp_sleep.h>
#include <esp_system.h>

#include "Canvas.h"
#include "BookPetVersion.h"
#include "FirmwareUpdater.h"
#include "PetRules.h"
#include "PetSprite.h"
#include "PetState.h"
#ifdef FILE_READ
#undef FILE_READ
#endif
#ifdef FILE_WRITE
#undef FILE_WRITE
#endif
#include "UpdatePortal.h"

extern "C" bool verifyRollbackLater() {
  return true;
}

namespace {
// GPIO 13 is the X3's proven soft-power latch. FreeInk does not currently
// describe this latch in its X3 BoardProfile, so keep this one board-specific
// detail here until it can be represented upstream.
constexpr gpio_num_t X3_POWER_LATCH = GPIO_NUM_13;
constexpr uint32_t AWAKE_MOMENT_MS = 15'000;
constexpr uint32_t DROWSY_AFTER_MS = 2 * 60'000;
constexpr uint32_t NATURAL_SLEEP_MS = 3 * 60'000;
constexpr uint64_t AUTONOMY_WAKE_US = 15ULL * 60ULL * 1'000'000ULL;
constexpr uint32_t OTA_HEALTHY_RUNTIME_MS = 5'000;
constexpr uint8_t FULL_REFRESH_EVERY = 12;
constexpr uint8_t MENU_ITEM_COUNT = 10;
constexpr uint8_t UPDATE_ITEM_COUNT = 4;

enum class Screen : uint8_t {
  Home,
  Menu,
  Pantry,
  Fragments,
  Diary,
  Reading,
  Toys,
  Behavior,
  Stats,
  Pets,
  Updates,
  PageCatch
};

enum class UpdateView : uint8_t {
  Menu,
  Status,
  RollbackConfirm
};

EInkDisplay display(
    BoardConfig::ACTIVE.display.sclk, BoardConfig::ACTIVE.display.mosi,
    BoardConfig::ACTIVE.display.cs, BoardConfig::ACTIVE.display.dc,
    BoardConfig::ACTIVE.display.rst, BoardConfig::ACTIVE.display.busy);
InputManager buttons;
PetEngine pet;
Screen screen = Screen::Home;
PetAction selectedAction = PetAction::Feed;
uint8_t menuIndex = 0;
uint8_t readingIndex = 0;
uint8_t readingPhase = 0;
uint16_t pendingPages = 0;
uint8_t petCursor = 0;
uint8_t toyCursor = 0;
uint8_t updateIndex = 0;
UpdateView updateView = UpdateView::Menu;
char updateStatusTitle[48] = "READY";
char updateStatusDetail[160] = "Choose an update method";
uint8_t updateProgress = 0;
bool recoveryBoot = false;
bool displayReady = false;
bool otaConfirmPending = false;
uint32_t otaConfirmAfterMs = 0;
bool drowsyShown = false;
uint8_t gamePhase = 0;
uint8_t playerLane = 1;
uint8_t targetLane = 1;
FragmentKind gameFragment = FragmentKind::Story;
bool gameCaught = false;
uint32_t lastInputMs = 0;
uint32_t lastAmbientMs = 0;
uint8_t fastRefreshes = 0;

void render(bool forceFull = false);

void confirmHealthyUpdateIfDue(bool force = false) {
  if (!otaConfirmPending) return;
  if (!force &&
      static_cast<int32_t>(millis() - otaConfirmAfterMs) < 0) {
    return;
  }
  if (bookpet::FirmwareUpdater::confirmRunningImage()) {
    otaConfirmPending = false;
    Serial.println("[bookpet] OTA image confirmed after healthy runtime");
  } else {
    otaConfirmAfterMs = millis() + OTA_HEALTHY_RUNTIME_MS;
    Serial.println("[bookpet] OTA image confirmation failed; will retry");
  }
}

// The X3 display and SD card share SCLK/MOSI, but only the card uses MISO.
// Arduino ignores new SPI pins after the bus has started, so MISO must be
// attached before FreeInk starts the display or later SD mounts read 0xFF.
void beginDisplayHardware() {
  SPI.end();
  SPI.begin(BoardConfig::ACTIVE.display.sclk, BoardConfig::ACTIVE.sd.miso,
            BoardConfig::ACTIVE.display.mosi, BoardConfig::ACTIVE.display.cs);
  display.begin();
}

const char* actionName(PetAction action) {
  switch (action) {
    case PetAction::Feed: return "FEED";
    case PetAction::Play: return "PLAY";
    case PetAction::Clean: return "CLEAN";
    case PetAction::Rest: return "REST";
  }
  return "";
}

const char* moodName(PetMood mood) {
  switch (mood) {
    case PetMood::Happy: return "SPARKY";
    case PetMood::Hungry: return "HUNGRY";
    case PetMood::Tired: return "DROWSY";
    case PetMood::Lonely: return "LONELY";
    case PetMood::Dirty: return "MESSY";
    case PetMood::Sleeping: return "DREAMING";
    case PetMood::Content:
    default: return "CONTENT";
  }
}

void centeredText(Canvas& c, int y, const char* text, uint8_t scale) {
  c.text((c.width() - c.textWidth(text, scale)) / 2, y, text, scale);
}

void meter(Canvas& c, int x, int y, const char* label, uint8_t value) {
  c.text(x, y, label, 2);
  c.rect(x + 112, y - 3, 340, 24);
  const int fill = (334 * value) / 100;
  if (fill > 0) c.rect(x + 115, y, fill, 18, true);
}

void drawThought(Canvas& c, const char* thought) {
  c.rect(28, 82, 472, 65);
  c.line(118, 147, 105, 164);
  c.line(105, 164, 139, 147);
  const int maxChars = 36;
  char first[37] = {};
  char second[37] = {};
  const size_t length = strlen(thought);
  if (length <= maxChars) {
    strncpy(first, thought, sizeof(first) - 1);
  } else {
    int split = maxChars;
    while (split > 0 && thought[split] != ' ') split--;
    strncpy(first, thought, split);
    strncpy(second, thought + split + 1, sizeof(second) - 1);
  }
  centeredText(c, second[0] ? 94 : 105, first, 2);
  if (second[0]) centeredText(c, 119, second, 2);
}

const char* const* spriteFor(const PetState& state, PetMood mood) {
  if (state.species == static_cast<uint8_t>(PetSpecies::Mote)) {
    if (mood == PetMood::Sleeping || state.ambientPose == 5) {
      return PetSprite::MOTE_SLEEP;
    }
    if (mood == PetMood::Happy || state.idleVariant % 3 == 1) {
      return PetSprite::MOTE_HAPPY;
    }
    return PetSprite::MOTE_IDLE;
  }
  if (state.species == static_cast<uint8_t>(PetSpecies::Pip)) {
    if (mood == PetMood::Sleeping || state.ambientPose == 5) {
      return PetSprite::PIP_SLEEP;
    }
    if (mood == PetMood::Happy || state.idleVariant % 3 == 1) {
      return PetSprite::PIP_HAPPY;
    }
    return PetSprite::PIP_IDLE;
  }
  if (mood == PetMood::Sleeping || state.ambientPose == 5) {
    return PetSprite::BYTE_SLEEP;
  }
  if (mood == PetMood::Happy || state.idleVariant % 3 == 1) {
    return PetSprite::BYTE_HAPPY;
  }
  if (mood == PetMood::Hungry) return PetSprite::BYTE_HUNGRY;
  if (mood == PetMood::Tired) return PetSprite::BYTE_TIRED;
  if (mood == PetMood::Dirty) return PetSprite::BYTE_DIRTY;
  return PetSprite::BYTE_IDLE;
}

void drawRoom(Canvas& c, const PetState& state, PetMood mood) {
  c.rect(22, 72, 484, 355);
  drawThought(c, pet.thought());
  c.line(23, 390, 505, 390);
  for (int x = 42; x < 505; x += 48) c.line(x, 390, x - 20, 426);

  const char* const* sprite = spriteFor(state, mood);
  const uint8_t scale = pet.lifeStage() == LifeStage::Hatchling ? 6
                        : pet.lifeStage() == LifeStage::Sprout ? 7 : 8;
  const int spriteSize = 24 * scale;
  int petX = (c.width() - spriteSize) / 2;
  if (state.ambientPose == 1) petX -= 55;
  if (state.ambientPose == 2) petX += 55;
  if (state.ambientPose == 4) petX -= 28;
  const int petY = state.ambientPose == 3 ? 176 : 188;
  PetSprite::draw(c, sprite, petX, petY, scale);

  if (state.equippedToy != 0xFF) {
    const int toyX = state.ambientPose == 2 ? 72 : 410;
    switch (static_cast<ToyKind>(state.equippedToy)) {
      case ToyKind::Ball:
        c.rect(toyX, 336, 32, 32);
        c.rect(toyX + 8, 344, 16, 16, true);
        break;
      case ToyKind::Bell:
        c.line(toyX + 16, 330, toyX + 4, 362);
        c.line(toyX + 16, 330, toyX + 28, 362);
        c.line(toyX + 4, 362, toyX + 28, 362);
        c.rect(toyX + 13, 365, 7, 7, true);
        break;
      case ToyKind::Blocks:
        c.rect(toyX, 340, 28, 28);
        c.text(toyX + 9, 349, "A", 1);
        break;
      case ToyKind::Kite:
        c.line(toyX + 16, 330, toyX, 350);
        c.line(toyX, 350, toyX + 16, 370);
        c.line(toyX + 16, 370, toyX + 32, 350);
        c.line(toyX + 32, 350, toyX + 16, 330);
        break;
    }
  }

  if (mood == PetMood::Happy) {
    c.text(76, 208, "*", 4);
    c.text(412, 238, "*", 3);
  } else if (mood == PetMood::Hungry) {
    c.text(398, 270, "?", 5);
  } else if (mood == PetMood::Tired) {
    c.text(392, 216, "z", 5);
  } else if (mood == PetMood::Dirty) {
    c.text(84, 238, ".", 5);
    c.text(406, 294, ".", 4);
  } else if (mood == PetMood::Sleeping) {
    c.text(396, 215, "z", 5);
    c.text(438, 190, "z", 3);
    if (state.ambientPose >= 7) {
      c.text(72, 220, state.ambientPose == 7 ? "*" : "o", 4);
      c.text(105, 195, state.ambientPose == 7 ? "o" : "*", 2);
    }
  } else if (state.ambientPose == 1 || state.ambientPose == 2) {
    const int trailX = state.ambientPose == 1 ? 395 : 92;
    c.text(trailX, 352, ". .", 2);
  } else if (state.ambientPose == 3) {
    c.text(78, 205, "!", 4);
  }
}

void drawHeader(Canvas& c, const PetState& state) {
  c.text(22, 18, pet.speciesName(), 3);
  char value[32];
  snprintf(value, sizeof(value), "LV %u", state.level);
  c.text(205, 22, value, 2);
  snprintf(value, sizeof(value), "PG %u", state.pageBites);
  c.text(322, 22, value, 2);
  snprintf(value, sizeof(value), "FOOD %u", state.food);
  c.text(424, 22, value, 1);
  c.line(22, 56, 506, 56);
}

void drawActionDock(Canvas& c) {
  const PetAction actions[] = {
      PetAction::Feed, PetAction::Play, PetAction::Clean, PetAction::Rest};
  for (int i = 0; i < 4; ++i) {
    const int x = 14 + i * 128;
    const bool selected = actions[i] == selectedAction;
    c.rect(x, 612, 116, 60, selected);
    if (selected) {
      for (int yy = 620; yy < 663; ++yy)
        for (int xx = x + 7; xx < x + 109; ++xx) c.pixel(xx, yy, false);
    }
    const char* label = actionName(actions[i]);
    c.text(x + (116 - c.textWidth(label, 1)) / 2, 636, label, 1);
  }
}

void drawHome(Canvas& c) {
  const PetState& state = pet.state();
  const PetMood mood = pet.mood();
  drawHeader(c, state);
  drawRoom(c, state, mood);
  meter(c, 36, 444, "FULL", state.fullness);
  meter(c, 36, 482, "JOY", state.happiness);
  meter(c, 36, 520, "REST", state.energy);
  meter(c, 36, 558, "CLEAN", state.cleanliness);
  drawActionDock(c);
  centeredText(c, 692, moodName(mood), 2);
  c.line(22, 722, 506, 722);
  c.text(30, 744, "BACK MENU", 1);
  c.text(206, 744, "<  CHOOSE  >", 1);
  c.text(420, 744, "OK DO", 1);
}

void drawTitle(Canvas& c, const char* title, const char* subtitle) {
  centeredText(c, 28, title, 3);
  centeredText(c, 66, subtitle, 1);
  c.line(22, 92, 506, 92);
}

void drawMenu(Canvas& c) {
  static constexpr const char* items[] = {
      "HOME", "READING", "PANTRY", "TOYS", "PET LIFE",
      "FRAGMENTS", "DIARY", "STATS", "PETS", "UPDATES"};
  drawTitle(c, "PET MENU", "SIDE BUTTONS MOVE  /  OK SELECT");
  for (int i = 0; i < MENU_ITEM_COUNT; ++i) {
    const int y = 108 + i * 60;
    if (menuIndex == i) c.rect(42, y - 10, 444, 46, true);
    if (menuIndex == i) {
      for (int yy = y - 5; yy < y + 27; ++yy)
        for (int xx = 54; xx < 474; ++xx) c.pixel(xx, yy, false);
    }
    c.text(72, y, items[i], 2);
    c.text(438, y, menuIndex == i ? ">" : "-", 2);
  }
  c.text(34, 744, "BACK HOME", 1);
}

void drawReading(Canvas& c) {
  const PetState& state = pet.state();
  drawTitle(c, "READING REWARDS", "REAL PAGES FEED YOUR PET");
  char value[48];
  if (readingPhase == 1) {
    centeredText(c, 145, "HOW MANY PAGES DID YOU READ?", 1);
    snprintf(value, sizeof(value), "%u", pendingPages);
    centeredText(c, 235, value, 7);
    centeredText(c, 355, "LEFT / RIGHT: 1 PAGE", 2);
    centeredText(c, 400, "SIDE UP / DOWN: 10 PAGES", 2);
    centeredText(c, 475, "EVERY 10 PAGES EARNS 1 FOOD", 1);
    c.text(34, 744, "BACK CANCEL", 1);
    c.text(410, 744, "OK SAVE", 1);
    return;
  }

  snprintf(value, sizeof(value), "CURRENT BOOK   %u PAGES",
           state.currentBookPages);
  c.text(54, 140, value, 2);
  snprintf(value, sizeof(value), "LIFETIME       %lu PAGES",
           static_cast<unsigned long>(state.lifetimePages));
  c.text(54, 185, value, 2);
  snprintf(value, sizeof(value), "BOOKS FINISHED %u", state.booksFinished);
  c.text(54, 230, value, 2);

  static constexpr const char* choices[] = {"LOG PAGES", "FINISH THIS BOOK"};
  for (int i = 0; i < 2; ++i) {
    const int y = 330 + i * 105;
    if (readingIndex == i) c.rect(45, y - 17, 438, 62, true);
    if (readingIndex == i) {
      for (int yy = y - 8; yy < y + 35; ++yy)
        for (int xx = 56; xx < 472; ++xx) c.pixel(xx, yy, false);
    }
    c.text(70, y, choices[i], 2);
  }
  centeredText(c, 610, "10 PAGES = 1 FOOD", 2);
  centeredText(c, 655, "FINISHED BOOKS UNLOCK TOYS + PETS", 1);
  c.text(34, 744, "BACK MENU", 1);
  c.text(416, 744, "OK", 1);
}

void drawToys(Canvas& c) {
  const PetState& state = pet.state();
  drawTitle(c, "TOY BOX", "FINISH BOOKS TO FIND TOYS");
  for (int i = 0; i < 4; ++i) {
    const int y = 145 + i * 105;
    const bool unlocked = pet.toyUnlocked(i);
    const bool selected = toyCursor == i;
    if (selected) c.rect(42, y - 20, 444, 68, true);
    if (selected) {
      for (int yy = y - 9; yy < y + 36; ++yy)
        for (int xx = 54; xx < 474; ++xx) c.pixel(xx, yy, false);
    }
    c.text(70, y, unlocked ? PetEngine::toyName(i) : "LOCKED", 2);
    if (unlocked && state.equippedToy == i) c.text(420, y, "*", 2);
  }
  centeredText(c, 625, "LEFT / RIGHT CHOOSE  /  OK EQUIP", 1);
  c.text(34, 744, "BACK MENU", 1);
}

void drawBehavior(Canvas& c) {
  const PetState& state = pet.state();
  drawTitle(c, "PET LIFE",
            state.autonomousEnabled ? "ALIVE MODE IS ON" : "ALIVE MODE IS OFF");
  c.rect(48, 135, 432, 110, state.autonomousEnabled);
  if (state.autonomousEnabled) {
    for (int y = 148; y < 231; ++y)
      for (int x = 60; x < 468; ++x) c.pixel(x, y, false);
  }
  centeredText(c, 164, state.autonomousEnabled ? "TURN OFF" : "TURN ON", 3);
  centeredText(c, 205, "OK TO CHANGE", 1);

  c.text(62, 310, "WHILE AWAKE", 2);
  c.text(62, 350, "WANDERS, LOOKS, AND PLAYS", 1);
  c.text(62, 430, "WHEN TIRED", 2);
  c.text(62, 470, "GETS DROWSY AND FALLS ASLEEP", 1);
  c.text(62, 550, "WHILE ASLEEP", 2);
  c.text(62, 590, "DREAMS, THEN WAKES BY ITSELF", 1);
  centeredText(c, 670, "E-PAPER MOTION HAPPENS IN STEPS", 1);
  c.text(34, 744, "BACK MENU", 1);
}

void drawFragments(Canvas& c) {
  const PetState& state = pet.state();
  static constexpr const char* names[] = {
      "STORY", "MYSTERY", "SCIENCE", "ADVENTURE"};
  drawTitle(c, "PAGE FRAGMENTS", "CATCH PAGES TO BUILD A COLLECTION");
  for (int i = 0; i < 4; ++i) {
    const int y = 145 + i * 105;
    c.rect(48, y - 20, 432, 68);
    c.text(72, y, names[i], 2);
    char count[16];
    snprintf(count, sizeof(count), "x %u", state.fragments[i]);
    c.text(390, y, count, 2);
  }
  centeredText(c, 610, "PLAY FROM HOME TO FIND MORE", 1);
  c.text(34, 744, "BACK MENU", 1);
}

void drawDiary(Canvas& c) {
  char title[28];
  snprintf(title, sizeof(title), "%s'S DIARY", pet.speciesName());
  drawTitle(c, title, "THE LAST THREE MOMENTS");
  for (int i = 0; i < 3; ++i) {
    const int y = 145 + i * 150;
    c.rect(42, y - 22, 444, 105);
    char chapter[20];
    snprintf(chapter, sizeof(chapter), "CHAPTER %d", i + 1);
    c.text(62, y, chapter, 2);
    c.text(62, y + 42, pet.diaryLine(i), 1);
  }
  centeredText(c, 650, "YOUR PET REMEMBERS WHAT MATTERS", 1);
  c.text(34, 744, "BACK MENU", 1);
}

void drawPantry(Canvas& c) {
  const PetState& state = pet.state();
  drawTitle(c, "PANTRY", "TURN PAGE BITES INTO FOOD");
  char value[40];
  snprintf(value, sizeof(value), "PAGE BITES   %u", state.pageBites);
  centeredText(c, 160, value, 2);
  snprintf(value, sizeof(value), "FOOD         %u", state.food);
  centeredText(c, 205, value, 2);
  c.rect(55, 300, 418, 120);
  centeredText(c, 326, "BAKE 1 FOOD", 3);
  centeredText(c, 372, "COST: 3 PAGE BITES", 2);
  centeredText(c, 480, pet.thought(), 1);
  c.text(34, 744, "BACK MENU", 1);
  c.text(404, 744, "OK BAKE", 1);
}

void drawStats(Canvas& c) {
  const PetState& state = pet.state();
  char title[28];
  snprintf(title, sizeof(title), "%s'S STORY", pet.speciesName());
  drawTitle(c, title, "LIFE SO FAR");
  char value[48];
  snprintf(value, sizeof(value), "LEVEL          %u", state.level);
  c.text(62, 150, value, 2);
  snprintf(value, sizeof(value), "XP             %u / %u",
           state.experience, pet.nextLevelXp());
  c.text(62, 205, value, 2);
  snprintf(value, sizeof(value), "ACTIVE MINUTES %lu",
           static_cast<unsigned long>(state.activeMinutes));
  c.text(62, 260, value, 2);
  snprintf(value, sizeof(value), "CARE MOMENTS   %u", state.interactions);
  c.text(62, 315, value, 2);
  snprintf(value, sizeof(value), "PAGE BITES     %u", state.pageBites);
  c.text(62, 370, value, 2);
  const char* trait = pet.personality() == Personality::Bold ? "BOLD"
                      : pet.personality() == Personality::Cozy ? "COZY"
                                                               : "CURIOUS";
  snprintf(value, sizeof(value), "PERSONALITY    %s", trait);
  c.text(62, 425, value, 2);
  c.rect(54, 500, 420, 125);
  centeredText(c, 525, "NEXT LEVEL REWARD", 2);
  centeredText(c, 570, "+2 FOOD  +2 PAGE BITES", 2);
  c.text(34, 744, "BACK MENU", 1);
}

void drawPets(Canvas& c) {
  drawTitle(c, "PET FAMILY", "READ BOOKS TO MEET NEW FRIENDS");
  const bool unlocked = pet.speciesUnlocked(petCursor);
  const char* const* sprite = petCursor == 1 ? PetSprite::MOTE_IDLE
                              : petCursor == 2 ? PetSprite::PIP_IDLE
                                               : PetSprite::BYTE_IDLE;
  c.rect(46, 132, 436, 300);
  PetSprite::draw(c, sprite, 168, 165, 8);
  centeredText(c, 375,
               unlocked ? PetEngine::speciesName(petCursor) : "LOCKED", 3);
  if (!unlocked) {
    centeredText(c, 465,
                 petCursor == 1 ? "FINISH 1 BOOK" : "FINISH 3 BOOKS", 2);
  } else {
    centeredText(c, 465,
                 pet.state().species == petCursor ? "LIVING WITH YOU"
                                                  : "OK TO CHOOSE",
                 2);
  }
  centeredText(c, 565, "<  BROWSE PETS  >", 2);
  centeredText(c, 625, "BYTE  /  MOTE  /  PIP", 1);
  c.text(34, 744, "BACK MENU", 1);
}

void drawWrappedCentered(Canvas& c, int y, const char* text, uint8_t scale,
                         size_t maxChars, int lineHeight,
                         uint8_t maxLines = 4) {
  if (!text || !text[0]) return;
  const char* cursor = text;
  for (uint8_t line = 0; line < maxLines && *cursor; ++line) {
    while (*cursor == ' ') ++cursor;
    const size_t remaining = strlen(cursor);
    size_t take = remaining < maxChars ? remaining : maxChars;
    if (take < remaining) {
      size_t split = take;
      while (split > 0 && cursor[split] != ' ') --split;
      if (split > 0) take = split;
    }
    char buffer[48] = {};
    const size_t copy =
        take < sizeof(buffer) - 1 ? take : sizeof(buffer) - 1;
    memcpy(buffer, cursor, copy);
    centeredText(c, y + line * lineHeight, buffer, scale);
    cursor += take;
  }
}

void drawUpdateMenu(Canvas& c) {
  static constexpr const char* items[] = {
      "UPDATE FROM SD", "PHONE / BROWSER", "RESTORE PREVIOUS", "ABOUT"};
  static constexpr const char* help[] = {
      "COPY UPDATE.BIN TO /BOOKPET ON THE SD CARD",
      "CONNECT A PHONE TO BOOK PET - NO APP NEEDED",
      "RETURN TO THE LAST WORKING FIRMWARE",
      "VERSION, SIGNATURE, AND RECOVERY DETAILS"};
  drawTitle(c, recoveryBoot ? "RECOVERY & UPDATES" : "UPDATE BOOK PET",
            "YOUR PET DATA STAYS SAFE");
  for (int i = 0; i < UPDATE_ITEM_COUNT; ++i) {
    const int y = 140 + i * 105;
    if (updateIndex == i) c.rect(42, y - 17, 444, 62, true);
    if (updateIndex == i) {
      for (int yy = y - 8; yy < y + 35; ++yy)
        for (int xx = 55; xx < 473; ++xx) c.pixel(xx, yy, false);
    }
    c.text(68, y, items[i], 2);
  }
  drawWrappedCentered(c, 600, help[updateIndex], 1, 42, 22, 2);
  c.text(34, 744, "BACK MENU", 1);
  c.text(420, 744, "OK", 1);
}

void drawUpdateStatus(Canvas& c) {
  drawTitle(c, updateStatusTitle, "FIRMWARE UPDATE");
  drawWrappedCentered(c, 145, updateStatusDetail, 2, 34, 34, 5);
  if (updateProgress > 0) {
    c.rect(62, 350, 404, 34);
    const int fill = (396 * updateProgress) / 100;
    if (fill > 0) c.rect(66, 354, fill, 26, true);
    char progress[16];
    snprintf(progress, sizeof(progress), "%u%%", updateProgress);
    centeredText(c, 404, progress, 2);
  }
  char version[40];
  snprintf(version, sizeof(version), "CURRENT VERSION %s", BOOKPET_VERSION);
  centeredText(c, 530, version, 2);
  centeredText(c, 575,
               bookpet::FirmwareUpdater::requiresSignature()
                   ? "OFFICIAL SIGNED UPDATES ONLY"
                   : "DEVELOPER BUILD - UNSIGNED ALLOWED",
               1);
  centeredText(c, 650, "OK OR BACK TO RETURN", 1);
  c.text(34, 744, "BACK", 1);
}

void drawPhoneUpdate(Canvas& c) {
  drawTitle(c, "PHONE / BROWSER", "BOOK PET MADE A PRIVATE WI-FI NETWORK");
  c.text(48, 135, "1. JOIN THIS WI-FI", 2);
  centeredText(c, 185, bookpet::updatePortal.ssid().c_str(), 3);
  c.text(48, 255, "2. PASSWORD", 2);
  centeredText(c, 305, bookpet::updatePortal.password().c_str(), 3);
  c.text(48, 375, "3. OPEN IN YOUR BROWSER", 2);
  centeredText(c, 425, "192.168.4.1", 3);
  drawWrappedCentered(c, 505, updateStatusDetail, 1, 42, 22, 3);
  centeredText(c, 625, "KEEP BOOK PET POWERED DURING INSTALL", 1);
  c.text(34, 744, "BACK STOP", 1);
}

void drawRollbackConfirm(Canvas& c) {
  drawTitle(c, "RESTORE PREVIOUS?", "RECOVERY ROLLBACK");
  centeredText(c, 165, "THIS RESTARTS BOOK PET", 3);
  centeredText(c, 225, "USING THE LAST FIRMWARE", 2);
  centeredText(c, 285, "YOUR PET DATA WILL STAY", 2);
  c.rect(62, 390, 404, 110);
  centeredText(c, 420, "PRESS OK AGAIN", 3);
  centeredText(c, 465, "TO RESTORE", 2);
  c.text(34, 744, "BACK CANCEL", 1);
  c.text(420, 744, "OK", 1);
}

void drawUpdates(Canvas& c) {
  if (bookpet::updatePortal.active()) {
    drawPhoneUpdate(c);
  } else if (updateView == UpdateView::RollbackConfirm) {
    drawRollbackConfirm(c);
  } else if (updateView == UpdateView::Status) {
    drawUpdateStatus(c);
  } else {
    drawUpdateMenu(c);
  }
}

void setUpdateStatus(const char* title, const char* detail,
                     uint8_t progress = 0, bool redraw = true) {
  snprintf(updateStatusTitle, sizeof(updateStatusTitle), "%s",
           title ? title : "UPDATE");
  snprintf(updateStatusDetail, sizeof(updateStatusDetail), "%s",
           detail ? detail : "");
  updateProgress = progress;
  updateView = UpdateView::Status;
  if (redraw && displayReady && screen == Screen::Updates) render();
}

void onPortalStatus(const char* title, const char* detail, uint8_t progress) {
  snprintf(updateStatusTitle, sizeof(updateStatusTitle), "%s",
           title ? title : "PHONE UPDATE");
  snprintf(updateStatusDetail, sizeof(updateStatusDetail), "%s",
           detail ? detail : "");
  updateProgress = progress;
  if (displayReady && screen == Screen::Updates) render();
}

void restoreDisplayAfterSd() {
  beginDisplayHardware();
  display.requestResync();
}

String readSdSha256(const char* firmwarePath) {
  String sidecarPath = String(firmwarePath) + ".sha256";
  String value = SdMan.readFile(sidecarPath.c_str());
  if (value.isEmpty()) value = SdMan.readFile("/BOOKPET/UPDATE.SHA256");
  value.trim();
  const int separator = value.indexOf(' ');
  if (separator > 0) value = value.substring(0, separator);
  value.toLowerCase();
  return value.length() == 64 ? value : String();
}

void performSdUpdate() {
  setUpdateStatus("CHECKING SD CARD",
                  "Looking for /BOOKPET/UPDATE.BIN", 0);
  if (!SdMan.begin()) {
    restoreDisplayAfterSd();
    setUpdateStatus("SD CARD NOT FOUND",
                    "Insert a FAT32 or exFAT card and try again");
    return;
  }

  static constexpr const char* paths[] = {
      "/BOOKPET/UPDATE.BIN", "/BOOKPET/book-pet-x3-update.bin",
      "/book-pet-x3-update.bin", "/update.bin"};
  const char* selectedPath = nullptr;
  for (const char* path : paths) {
    if (SdMan.exists(path)) {
      selectedPath = path;
      break;
    }
  }
  if (!selectedPath) {
    restoreDisplayAfterSd();
    setUpdateStatus("UPDATE FILE NOT FOUND",
                    "Copy UPDATE.BIN into a BOOKPET folder on the SD card");
    return;
  }

  FsFile firmware = SdMan.open(selectedPath, O_RDONLY);
  if (!firmware) {
    restoreDisplayAfterSd();
    setUpdateStatus("SD CARD ERROR", "The update file could not be opened");
    return;
  }
  const uint64_t fileSize = firmware.fileSize();
  if (fileSize > SIZE_MAX) {
    firmware.close();
    restoreDisplayAfterSd();
    setUpdateStatus("UPDATE FILE TOO LARGE",
                    "This file is not Book Pet firmware");
    return;
  }
  const String sha256 = readSdSha256(selectedPath);
  pet.save();
  setUpdateStatus("INSTALLING FROM SD",
                  "Verifying and writing the inactive recovery slot", 1);
  const bool installed = bookpet::firmwareUpdater.install(
      firmware, static_cast<size_t>(fileSize),
      sha256.isEmpty() ? nullptr : sha256.c_str());
  firmware.close();
  restoreDisplayAfterSd();
  if (!installed) {
    setUpdateStatus("SD UPDATE FAILED", bookpet::firmwareUpdater.error());
    return;
  }

  setUpdateStatus("UPDATE VERIFIED",
                  "Your pet is safe. Restarting into the new version", 100);
  delay(2500);
  ESP.restart();
}

const char* fragmentName(FragmentKind kind) {
  switch (kind) {
    case FragmentKind::Story: return "STORY";
    case FragmentKind::Mystery: return "MYSTERY";
    case FragmentKind::Science: return "SCIENCE";
    case FragmentKind::Adventure: return "ADVENTURE";
  }
  return "";
}

void startPageCatch() {
  const PetState& state = pet.state();
  targetLane = (state.interactions + state.activeMinutes + state.level) % 3;
  gameFragment = static_cast<FragmentKind>(
      (state.interactions + state.level + state.pageBites) % 4);
  playerLane = 1;
  gamePhase = 0;
  gameCaught = false;
  screen = Screen::PageCatch;
}

void drawPageCatch(Canvas& c) {
  drawTitle(c, "CATCH THE PAGE", fragmentName(gameFragment));
  c.line(176, 145, 176, 610);
  c.line(352, 145, 352, 610);
  if (gamePhase == 0) {
    centeredText(c, 132, "MEMORIZE ITS LANE", 2);
    const int x = 60 + targetLane * 176;
    c.rect(x, 235, 56, 76);
    c.line(x + 8, 247, x + 46, 247);
    c.line(x + 8, 263, x + 39, 263);
    c.line(x + 8, 279, x + 46, 279);
    centeredText(c, 650, "OK: HIDE THE PAGE", 2);
  } else if (gamePhase == 1) {
    centeredText(c, 132, "WHERE WAS IT?", 2);
    const int x = 46 + playerLane * 176;
    c.text(x, 420, "^", 5);
    centeredText(c, 650, "<  PICK A LANE  >   OK", 2);
  } else {
    centeredText(c, 170, gameCaught ? "CAUGHT!" : "IT ESCAPED!", 4);
    const PetState& state = pet.state();
    PetSprite::draw(
        c, spriteFor(state, gameCaught ? PetMood::Happy : PetMood::Hungry),
        168, 260, 8);
    centeredText(c, 535,
                 gameCaught ? "+2 PAGE BITES  +10 XP" : "+2 XP", 2);
    centeredText(c, 650, "OK: GO HOME", 2);
  }
  c.text(34, 744, "BACK HOME", 1);
}

void render(bool forceFull) {
  display.clearScreen();
  Canvas canvas(display.getFrameBuffer(), display.getDisplayWidth(),
                display.getDisplayHeight(), Canvas::Rotation::CounterClockwise);
  switch (screen) {
    case Screen::Home: drawHome(canvas); break;
    case Screen::Menu: drawMenu(canvas); break;
    case Screen::Pantry: drawPantry(canvas); break;
    case Screen::Reading: drawReading(canvas); break;
    case Screen::Toys: drawToys(canvas); break;
    case Screen::Behavior: drawBehavior(canvas); break;
    case Screen::Fragments: drawFragments(canvas); break;
    case Screen::Diary: drawDiary(canvas); break;
    case Screen::Stats: drawStats(canvas); break;
    case Screen::Pets: drawPets(canvas); break;
    case Screen::Updates: drawUpdates(canvas); break;
    case Screen::PageCatch: drawPageCatch(canvas); break;
  }
  const bool doFull = forceFull || fastRefreshes >= FULL_REFRESH_EVERY;
  display.displayBuffer(doFull ? EInkDisplay::FULL_REFRESH
                               : EInkDisplay::FAST_REFRESH, true);
  fastRefreshes = doFull ? 0 : fastRefreshes + 1;
}

void enterDeepSleep() {
  confirmHealthyUpdateIfDue(true);
  Serial.printf("[bookpet] deep sleep pet_sleeping=%u\n",
                pet.state().sleeping);
  Serial.flush();
  pet.save();
  display.deepSleep();
  freeink::PowerManager::waitForPowerButtonRelease();
  freeink::PowerManager::armPowerButtonWakeup();
  freeink::PowerManager::powerDownRailsForSleep();
  gpio_set_direction(X3_POWER_LATCH, GPIO_MODE_OUTPUT);
  gpio_set_level(X3_POWER_LATCH, 0);
  gpio_hold_en(X3_POWER_LATCH);
  freeink::PowerManager::deepSleep();
}

void enterDreamSleep() {
  const gpio_num_t powerPin =
      static_cast<gpio_num_t>(BoardConfig::ACTIVE.input.power);
  const bool powerActiveHigh =
      BoardConfig::ACTIVE.input.powerActiveHigh;
  while (pet.state().sleeping && pet.state().autonomousEnabled) {
    pet.save();
    display.deepSleep();
    while (buttons.isPressed(InputManager::BTN_POWER)) {
      buttons.update();
      delay(20);
    }

    pinMode(powerPin,
            powerActiveHigh ? INPUT_PULLDOWN : INPUT_PULLUP);
    gpio_wakeup_enable(powerPin, powerActiveHigh ? GPIO_INTR_HIGH_LEVEL
                                                 : GPIO_INTR_LOW_LEVEL);
    const esp_err_t gpioResult = esp_sleep_enable_gpio_wakeup();
    const esp_err_t timerResult =
        esp_sleep_enable_timer_wakeup(AUTONOMY_WAKE_US);
    Serial.printf(
        "[bookpet] light sleep gpio=%d timer=%d interval_us=%llu\n",
        static_cast<int>(gpioResult), static_cast<int>(timerResult),
        static_cast<unsigned long long>(AUTONOMY_WAKE_US));
    Serial.flush();

    const esp_err_t sleepResult = esp_light_sleep_start();
    const esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
    Serial.begin(115200);
    delay(100);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
    gpio_wakeup_disable(powerPin);
    Serial.printf("[bookpet] light wake result=%d cause=%d\n",
                  static_cast<int>(sleepResult),
                  static_cast<int>(wakeCause));

    if (sleepResult != ESP_OK ||
        wakeCause == ESP_SLEEP_WAKEUP_GPIO) {
      pet.wake();
    } else if (wakeCause == ESP_SLEEP_WAKEUP_TIMER) {
      pet.dreamMoment();
    } else {
      pet.wake();
    }

    beginDisplayHardware();
    display.requestResync();
    screen = Screen::Home;
    const bool cleanupRefresh =
        (pet.state().autonomousSteps + pet.state().sleepCycles) %
            FULL_REFRESH_EVERY ==
        0;
    render(cleanupRefresh);
    if (pet.state().sleeping) delay(400);
  }

  lastInputMs = millis();
  lastAmbientMs = lastInputMs;
  drowsyShown = false;
  pet.resetTickClock();
}

void goBack() {
  if (screen == Screen::Updates && bookpet::updatePortal.active()) {
    if (!bookpet::updatePortal.safeToStop()) {
      onPortalStatus("UPDATE IN PROGRESS",
                     "Please wait until Book Pet finishes safely",
                     bookpet::updatePortal.progress());
      return;
    }
    bookpet::updatePortal.stop();
    updateView = UpdateView::Menu;
    return;
  }
  if (screen == Screen::Updates && updateView != UpdateView::Menu) {
    updateView = UpdateView::Menu;
    return;
  }
  if (screen == Screen::Home) {
    screen = Screen::Menu;
  } else if (screen == Screen::Menu) {
    screen = Screen::Home;
  } else if (screen == Screen::PageCatch) {
    screen = Screen::Home;
  } else if (screen == Screen::Reading && readingPhase == 1) {
    readingPhase = 0;
    pendingPages = 0;
  } else {
    screen = Screen::Menu;
  }
}

void selectMenuItem() {
  switch (menuIndex) {
    case 0: screen = Screen::Home; break;
    case 1: screen = Screen::Reading; break;
    case 2: screen = Screen::Pantry; break;
    case 3: screen = Screen::Toys; break;
    case 4: screen = Screen::Behavior; break;
    case 5: screen = Screen::Fragments; break;
    case 6: screen = Screen::Diary; break;
    case 7: screen = Screen::Stats; break;
    case 8: screen = Screen::Pets; break;
    case 9:
      updateView = UpdateView::Menu;
      screen = Screen::Updates;
      break;
  }
}

void handleInput() {
  bool changed = false;
  if (buttons.wasPressed(InputManager::BTN_BACK)) {
    goBack();
    changed = true;
  }
  if (screen == Screen::Home) {
    if (buttons.wasPressed(InputManager::BTN_LEFT)) {
      selectedAction = static_cast<PetAction>(
          (static_cast<uint8_t>(selectedAction) + 3) % 4);
      changed = true;
    }
    if (buttons.wasPressed(InputManager::BTN_RIGHT)) {
      selectedAction = static_cast<PetAction>(
          (static_cast<uint8_t>(selectedAction) + 1) % 4);
      changed = true;
    }
    if (buttons.wasPressed(InputManager::BTN_CONFIRM)) {
      if (selectedAction == PetAction::Play) {
        startPageCatch();
      } else {
        pet.apply(selectedAction);
      }
      if (selectedAction == PetAction::Rest) {
        render();
        delay(300);
        enterDeepSleep();
      }
      changed = true;
    }
  } else if (screen == Screen::Menu) {
    if (buttons.wasPressed(InputManager::BTN_UP)) {
      menuIndex = (menuIndex + MENU_ITEM_COUNT - 1) % MENU_ITEM_COUNT;
      changed = true;
    }
    if (buttons.wasPressed(InputManager::BTN_DOWN)) {
      menuIndex = (menuIndex + 1) % MENU_ITEM_COUNT;
      changed = true;
    }
    if (buttons.wasPressed(InputManager::BTN_CONFIRM)) {
      selectMenuItem();
      changed = true;
    }
  } else if (screen == Screen::Pantry &&
             buttons.wasPressed(InputManager::BTN_CONFIRM)) {
    pet.buyFood();
    changed = true;
  } else if (screen == Screen::PageCatch) {
    if (buttons.wasPressed(InputManager::BTN_LEFT) && gamePhase == 1) {
      playerLane = (playerLane + 2) % 3;
      changed = true;
    }
    if (buttons.wasPressed(InputManager::BTN_RIGHT) && gamePhase == 1) {
      playerLane = (playerLane + 1) % 3;
      changed = true;
    }
    if (buttons.wasPressed(InputManager::BTN_CONFIRM)) {
      if (gamePhase == 0) {
        gamePhase = 1;
      } else if (gamePhase == 1) {
        gameCaught = playerLane == targetLane;
        pet.completePageCatch(gameCaught, gameFragment);
        gamePhase = 2;
      } else {
        screen = Screen::Home;
      }
      changed = true;
    }
  } else if (screen == Screen::Reading) {
    if (readingPhase == 0) {
      if (buttons.wasPressed(InputManager::BTN_UP)) {
        readingIndex = (readingIndex + 1) % 2;
        changed = true;
      }
      if (buttons.wasPressed(InputManager::BTN_DOWN)) {
        readingIndex = (readingIndex + 1) % 2;
        changed = true;
      }
      if (buttons.wasPressed(InputManager::BTN_CONFIRM)) {
        if (readingIndex == 0) {
          readingPhase = 1;
          pendingPages = 0;
        } else {
          pet.finishBook();
        }
        changed = true;
      }
    } else {
      if (buttons.wasPressed(InputManager::BTN_LEFT)) {
        if (pendingPages > 0) pendingPages--;
        changed = true;
      }
      if (buttons.wasPressed(InputManager::BTN_RIGHT)) {
        if (pendingPages < 999) pendingPages++;
        changed = true;
      }
      if (buttons.wasPressed(InputManager::BTN_UP)) {
        pendingPages = min<uint16_t>(999, pendingPages + 10);
        changed = true;
      }
      if (buttons.wasPressed(InputManager::BTN_DOWN)) {
        pendingPages = pendingPages >= 10 ? pendingPages - 10 : 0;
        changed = true;
      }
      if (buttons.wasPressed(InputManager::BTN_CONFIRM)) {
        pet.logPages(pendingPages);
        pendingPages = 0;
        readingPhase = 0;
        changed = true;
      }
    }
  } else if (screen == Screen::Pets) {
    if (buttons.wasPressed(InputManager::BTN_LEFT)) {
      petCursor = (petCursor + 2) % 3;
      changed = true;
    }
    if (buttons.wasPressed(InputManager::BTN_RIGHT)) {
      petCursor = (petCursor + 1) % 3;
      changed = true;
    }
    if (buttons.wasPressed(InputManager::BTN_CONFIRM)) {
      pet.selectSpecies(petCursor);
      changed = true;
    }
  } else if (screen == Screen::Toys) {
    if (buttons.wasPressed(InputManager::BTN_LEFT)) {
      toyCursor = (toyCursor + 3) % 4;
      changed = true;
    }
    if (buttons.wasPressed(InputManager::BTN_RIGHT)) {
      toyCursor = (toyCursor + 1) % 4;
      changed = true;
    }
    if (buttons.wasPressed(InputManager::BTN_CONFIRM)) {
      pet.equipToy(toyCursor);
      changed = true;
    }
  } else if (screen == Screen::Behavior &&
             buttons.wasPressed(InputManager::BTN_CONFIRM)) {
    pet.toggleAutonomy();
    changed = true;
  } else if (screen == Screen::Updates) {
    if (updateView == UpdateView::Status) {
      if (buttons.wasPressed(InputManager::BTN_CONFIRM)) {
        updateView = UpdateView::Menu;
        changed = true;
      }
    } else if (updateView == UpdateView::RollbackConfirm) {
      if (buttons.wasPressed(InputManager::BTN_CONFIRM)) {
        setUpdateStatus("RESTORING PREVIOUS",
                        "Restarting with the last working firmware", 0);
        delay(1200);
        if (!bookpet::FirmwareUpdater::rollbackAndReboot()) {
          setUpdateStatus("RESTORE FAILED",
                          "The previous firmware could not be selected");
        }
        changed = true;
      }
    } else {
      if (buttons.wasPressed(InputManager::BTN_UP)) {
        updateIndex =
            (updateIndex + UPDATE_ITEM_COUNT - 1) % UPDATE_ITEM_COUNT;
        changed = true;
      }
      if (buttons.wasPressed(InputManager::BTN_DOWN)) {
        updateIndex = (updateIndex + 1) % UPDATE_ITEM_COUNT;
        changed = true;
      }
      if (buttons.wasPressed(InputManager::BTN_CONFIRM)) {
        if (updateIndex == 0) {
          performSdUpdate();
        } else if (updateIndex == 1) {
          updateView = UpdateView::Status;
          if (!bookpet::updatePortal.start(onPortalStatus)) {
            setUpdateStatus("PHONE UPDATE FAILED",
                            bookpet::updatePortal.detail());
          }
        } else if (updateIndex == 2) {
          if (bookpet::FirmwareUpdater::rollbackAvailable()) {
            updateView = UpdateView::RollbackConfirm;
          } else {
            setUpdateStatus("NO PREVIOUS VERSION",
                            "Install one update before using rollback");
          }
        } else {
          char detail[144];
          snprintf(detail, sizeof(detail),
                   "Book Pet %s. %s Hold Back while powering on for recovery.",
                   BOOKPET_VERSION,
                   bookpet::FirmwareUpdater::requiresSignature()
                       ? "Official updates require a valid signature."
                       : "This developer build accepts unsigned firmware.");
          setUpdateStatus("ABOUT BOOK PET", detail);
        }
        changed = true;
      }
    }
  }
  if (changed) {
    lastInputMs = millis();
    lastAmbientMs = lastInputMs;
    drowsyShown = false;
    render();
  }
}
}  // namespace

void setup() {
  delay(250);
  gpio_deep_sleep_hold_dis();
  gpio_hold_dis(X3_POWER_LATCH);
  pinMode(X3_POWER_LATCH, OUTPUT);
  digitalWrite(X3_POWER_LATCH, HIGH);
  Serial.begin(115200);
  delay(150);
  Serial.printf("[bookpet] boot reset=%d wake=%d\n",
                static_cast<int>(esp_reset_reason()),
                static_cast<int>(esp_sleep_get_wakeup_cause()));
  const freeink::X3DisplayVerdict panelVerdict =
      freeink::detectX3DisplayController();
  const bool hasUc8279 =
      panelVerdict == freeink::X3DisplayVerdict::Uc8279Confirmed;
  BoardConfig::selectDevice(
      hasUc8279 ? BoardConfig::Board::XteinkX3Uc8279
                : BoardConfig::Board::XteinkX3);
  display.setDisplayX3();
  Serial.printf("[bookpet] freeink board=%s panel_probe=%u\n",
                BoardConfig::ACTIVE.name,
                static_cast<unsigned>(panelVerdict));
  BoardConfig::releaseSdRail();
  buttons.begin();
  for (uint8_t sample = 0; sample < 4; ++sample) {
    buttons.update();
    delay(25);
  }
  recoveryBoot = buttons.isPressed(InputManager::BTN_BACK);
  if (recoveryBoot) {
    Serial.println("[bookpet] recovery mode requested by Back button");
    screen = Screen::Updates;
    updateView = UpdateView::Menu;
  }
  pet.begin();
  pet.wake();
  beginDisplayHardware();
  display.requestResync();
  lastInputMs = millis();
  lastAmbientMs = lastInputMs;
  render(true);
  displayReady = true;
  // A power-button wake can still be physically held after the first screen
  // appears. Consume that wake gesture before loop() treats it as a new
  // long-press and immediately sends recovery (or the pet) back to sleep.
  freeink::PowerManager::waitForPowerButtonRelease();
  buttons.update();
  otaConfirmPending =
      bookpet::FirmwareUpdater::runningImagePendingVerify();
  if (otaConfirmPending) {
    otaConfirmAfterMs = millis() + OTA_HEALTHY_RUNTIME_MS;
    Serial.println(
        "[bookpet] OTA image pending five-second health confirmation");
  }
}

void loop() {
  buttons.update();
  confirmHealthyUpdateIfDue();
  if (bookpet::updatePortal.active()) {
    bookpet::updatePortal.handle();
    handleInput();
    delay(10);
    return;
  }
  if (pet.tick(millis()) && screen == Screen::Home) render();
  handleInput();

  if (buttons.isPressed(InputManager::BTN_POWER) &&
      buttons.getPowerButtonHeldTime() > 1200) {
    pet.apply(PetAction::Rest);
    screen = Screen::Home;
    render();
    delay(300);
    enterDeepSleep();
  }

  const uint32_t now = millis();
  const uint32_t idleFor = now - lastInputMs;
  const bool livingAtHome =
      pet.state().autonomousEnabled && !pet.state().sleeping &&
      screen == Screen::Home;
  const bool naturallyTired = pet.state().energy < 25;
  const uint32_t drowsyAfter =
      naturallyTired ? DROWSY_AFTER_MS / 2 : DROWSY_AFTER_MS;
  const uint32_t sleepAfter =
      naturallyTired ? NATURAL_SLEEP_MS / 2 : NATURAL_SLEEP_MS;
  if (livingAtHome && !drowsyShown && idleFor >= drowsyAfter) {
    pet.awakeMoment(true);
    Serial.println("[bookpet] visible moment: drowsy");
    drowsyShown = true;
    lastAmbientMs = now;
    render();
  } else if (livingAtHome && !drowsyShown &&
             now - lastAmbientMs >= AWAKE_MOMENT_MS) {
    pet.awakeMoment();
    Serial.printf("[bookpet] visible moment: pose=%u\n",
                  pet.state().ambientPose);
    lastAmbientMs = now;
    render();
  }

  if (idleFor >= sleepAfter) {
    if (pet.state().autonomousEnabled) {
      pet.beginNaturalSleep();
      Serial.println("[bookpet] natural sleep");
      screen = Screen::Home;
      render();
      delay(1200);
      enterDreamSleep();
    } else {
      enterDeepSleep();
    }
  }
  delay(10);
}
