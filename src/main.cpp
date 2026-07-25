#include <Arduino.h>
#include <EInkDisplay.h>
#include <InputManager.h>
#include <SPI.h>
#include <esp_sleep.h>

#include "Canvas.h"
#include "PetRules.h"
#include "PetSprite.h"
#include "PetState.h"

namespace {
constexpr int EPD_SCLK = 8;
constexpr int EPD_MOSI = 10;
constexpr int EPD_CS = 21;
constexpr int EPD_DC = 4;
constexpr int EPD_RST = 5;
constexpr int EPD_BUSY = 6;
constexpr int SPI_MISO = 7;
constexpr gpio_num_t POWER_LATCH = GPIO_NUM_13;
constexpr uint32_t AUTO_SLEEP_MS = 120'000;
constexpr uint8_t FULL_REFRESH_EVERY = 12;

enum class Screen : uint8_t { Home, Menu, Pantry, Stats, Pets };

EInkDisplay display(EPD_SCLK, EPD_MOSI, EPD_CS, EPD_DC, EPD_RST, EPD_BUSY);
InputManager buttons;
PetEngine pet;
Screen screen = Screen::Home;
PetAction selectedAction = PetAction::Feed;
uint8_t menuIndex = 0;
uint32_t lastInputMs = 0;
uint8_t fastRefreshes = 0;

const char* actionName(PetAction action) {
  switch (action) {
    case PetAction::Feed: return "FEED";
    case PetAction::Play: return "PLAY";
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

void drawRoom(Canvas& c, const PetState& state, PetMood mood) {
  c.rect(22, 72, 484, 355);
  drawThought(c, pet.thought());
  c.line(23, 390, 505, 390);
  for (int x = 42; x < 505; x += 48) c.line(x, 390, x - 20, 426);

  const char* const* sprite =
      mood == PetMood::Sleeping ? PetSprite::BYTE_SLEEP
      : mood == PetMood::Happy ? PetSprite::BYTE_HAPPY
                               : PetSprite::BYTE_IDLE;
  PetSprite::draw(c, sprite, 168, 180, 8);

  if (mood == PetMood::Happy) {
    c.text(76, 208, "*", 4);
    c.text(412, 238, "*", 3);
  } else if (mood == PetMood::Hungry) {
    c.text(398, 270, "?", 5);
  } else if (mood == PetMood::Tired) {
    c.text(392, 216, "z", 5);
  }
}

void drawHeader(Canvas& c, const PetState& state) {
  c.text(22, 18, "BYTE", 3);
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
  const PetAction actions[] = {PetAction::Feed, PetAction::Play, PetAction::Rest};
  for (int i = 0; i < 3; ++i) {
    const int x = 22 + i * 164;
    const bool selected = actions[i] == selectedAction;
    c.rect(x, 590, 150, 70, selected);
    if (selected) {
      c.rect(x + 7, 597, 136, 56, false);
      // White knockout behind black text.
      c.rect(x + 22, 613, 106, 24, false);
      for (int yy = 608; yy < 643; ++yy)
        for (int xx = x + 14; xx < x + 136; ++xx) c.pixel(xx, yy, false);
    }
    const char* label = actionName(actions[i]);
    c.text(x + (150 - c.textWidth(label, 2)) / 2, 615, label, 2);
  }
}

void drawHome(Canvas& c) {
  const PetState& state = pet.state();
  const PetMood mood = pet.mood();
  drawHeader(c, state);
  drawRoom(c, state, mood);
  meter(c, 36, 452, "FULL", state.fullness);
  meter(c, 36, 492, "JOY", state.happiness);
  meter(c, 36, 532, "REST", state.energy);
  drawActionDock(c);
  centeredText(c, 684, moodName(mood), 2);
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
  static constexpr const char* items[] = {"HOME", "PANTRY", "STATS", "PETS"};
  drawTitle(c, "PET MENU", "SIDE BUTTONS MOVE  /  OK SELECT");
  for (int i = 0; i < 4; ++i) {
    const int y = 140 + i * 112;
    if (menuIndex == i) c.rect(42, y - 18, 444, 72, true);
    if (menuIndex == i) {
      for (int yy = y - 7; yy < y + 34; ++yy)
        for (int xx = 54; xx < 474; ++xx) c.pixel(xx, yy, false);
    }
    c.text(72, y, items[i], 3);
    c.text(438, y, menuIndex == i ? ">" : "-", 3);
  }
  c.text(34, 744, "BACK HOME", 1);
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
  drawTitle(c, "BYTE'S STORY", "LIFE SO FAR");
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
  c.rect(54, 455, 420, 145);
  centeredText(c, 480, "NEXT LEVEL REWARD", 2);
  centeredText(c, 525, "+2 FOOD  +2 PAGE BITES", 2);
  c.text(34, 744, "BACK MENU", 1);
}

void drawPets(Canvas& c) {
  drawTitle(c, "PETS", "NEW FRIENDS WILL HATCH HERE");
  c.rect(46, 132, 436, 180);
  PetSprite::draw(c, PetSprite::BYTE_IDLE, 72, 145, 6);
  c.text(250, 176, "BYTE", 3);
  c.text(250, 220, "PIXEL FAMILIAR", 1);
  c.text(250, 250, "SELECTED", 2);
  c.rect(46, 350, 436, 150);
  centeredText(c, 390, "? ? ?", 5);
  centeredText(c, 460, "FUTURE PETS", 2);
  centeredText(c, 570, "LEVEL UP TO GROW THE FAMILY", 1);
  c.text(34, 744, "BACK MENU", 1);
}

void render(bool forceFull = false) {
  display.clearScreen();
  Canvas canvas(display.getFrameBuffer(), display.getDisplayWidth(),
                display.getDisplayHeight(), Canvas::Rotation::CounterClockwise);
  switch (screen) {
    case Screen::Home: drawHome(canvas); break;
    case Screen::Menu: drawMenu(canvas); break;
    case Screen::Pantry: drawPantry(canvas); break;
    case Screen::Stats: drawStats(canvas); break;
    case Screen::Pets: drawPets(canvas); break;
  }
  const bool doFull = forceFull || fastRefreshes >= FULL_REFRESH_EVERY;
  display.displayBuffer(doFull ? EInkDisplay::FULL_REFRESH
                               : EInkDisplay::FAST_REFRESH, true);
  fastRefreshes = doFull ? 0 : fastRefreshes + 1;
}

void enterDeepSleep() {
  pet.save();
  display.deepSleep();
  while (buttons.isPressed(InputManager::BTN_POWER)) {
    buttons.update();
    delay(20);
  }
  gpio_set_direction(POWER_LATCH, GPIO_MODE_OUTPUT);
  gpio_set_level(POWER_LATCH, 0);
  esp_sleep_config_gpio_isolate();
  gpio_deep_sleep_hold_en();
  gpio_hold_en(POWER_LATCH);
  pinMode(InputManager::POWER_BUTTON_PIN, INPUT_PULLUP);
  esp_deep_sleep_enable_gpio_wakeup(
      1ULL << InputManager::POWER_BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_deep_sleep_start();
}

void goBack() {
  if (screen == Screen::Home) {
    screen = Screen::Menu;
  } else if (screen == Screen::Menu) {
    screen = Screen::Home;
  } else {
    screen = Screen::Menu;
  }
}

void selectMenuItem() {
  switch (menuIndex) {
    case 0: screen = Screen::Home; break;
    case 1: screen = Screen::Pantry; break;
    case 2: screen = Screen::Stats; break;
    case 3: screen = Screen::Pets; break;
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
          (static_cast<uint8_t>(selectedAction) + 2) % 3);
      changed = true;
    }
    if (buttons.wasPressed(InputManager::BTN_RIGHT)) {
      selectedAction = static_cast<PetAction>(
          (static_cast<uint8_t>(selectedAction) + 1) % 3);
      changed = true;
    }
    if (buttons.wasPressed(InputManager::BTN_CONFIRM)) {
      pet.apply(selectedAction);
      if (selectedAction == PetAction::Rest) {
        render();
        delay(300);
        enterDeepSleep();
      }
      changed = true;
    }
  } else if (screen == Screen::Menu) {
    if (buttons.wasPressed(InputManager::BTN_UP)) {
      menuIndex = (menuIndex + 3) % 4;
      changed = true;
    }
    if (buttons.wasPressed(InputManager::BTN_DOWN)) {
      menuIndex = (menuIndex + 1) % 4;
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
  }
  if (changed) {
    lastInputMs = millis();
    render();
  }
}
}  // namespace

void setup() {
  delay(250);
  Serial.begin(115200);
  SPI.begin(EPD_SCLK, SPI_MISO, EPD_MOSI, EPD_CS);
  buttons.begin();
  pet.begin();
  pet.wake();
  display.setDisplayX3();
  display.begin();
  display.requestResync();
  lastInputMs = millis();
  render(true);
}

void loop() {
  buttons.update();
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
  if (millis() - lastInputMs > AUTO_SLEEP_MS) enterDeepSleep();
  delay(10);
}
