#include <Arduino.h>
#include <EInkDisplay.h>
#include <InputManager.h>
#include <SPI.h>
#include <esp_sleep.h>

#include "Canvas.h"
#include "InputRouter.h"
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

EInkDisplay display(EPD_SCLK, EPD_MOSI, EPD_CS, EPD_DC, EPD_RST, EPD_BUSY);
InputManager buttons;
PetEngine pet;
PetAction selected = PetAction::Feed;
uint32_t lastInputMs = 0;
uint8_t fastRefreshes = 0;

const char* actionName(PetAction action) {
  switch (action) {
    case PetAction::Feed: return "FEED";
    case PetAction::Play: return "PLAY";
    case PetAction::Sleep: return "SLEEP";
  }
  return "";
}

void meter(Canvas& c, int x, int y, const char* label, uint8_t value) {
  c.text(x, y, label, 2);
  c.rect(x + 150, y - 2, 260, 23);
  const int fill = (254 * value) / 100;
  if (fill > 0) c.rect(x + 153, y + 1, fill, 17, true);
}

void drawPet(Canvas& c, const PetState& state) {
  const int ox = 85, oy = 105;
  // A friendly book-shaped creature.
  c.rect(ox, oy, 250, 220);
  c.line(ox + 125, oy + 15, ox + 125, oy + 205);
  c.line(ox + 18, oy + 30, ox + 125, oy + 15);
  c.line(ox + 232, oy + 30, ox + 125, oy + 15);
  if (state.sleeping) {
    c.line(ox + 45, oy + 92, ox + 85, oy + 92);
    c.line(ox + 165, oy + 92, ox + 205, oy + 92);
    c.text(ox + 82, oy + 140, "Z z z", 3);
  } else {
    c.rect(ox + 55, oy + 78, 20, 28, true);
    c.rect(ox + 175, oy + 78, 20, 28, true);
    if (state.hunger > 75) {
      c.line(ox + 90, oy + 160, ox + 125, oy + 145);
      c.line(ox + 125, oy + 145, ox + 160, oy + 160);
    } else {
      c.line(ox + 90, oy + 145, ox + 125, oy + 165);
      c.line(ox + 125, oy + 165, ox + 160, oy + 145);
    }
  }
  c.line(ox + 45, oy + 220, ox + 25, oy + 245);
  c.line(ox + 205, oy + 220, ox + 225, oy + 245);
}

void render(bool forceFull = false) {
  display.clearScreen();
  Canvas canvas(display.getFrameBuffer(), display.getDisplayWidth(), display.getDisplayHeight());
  const PetState& state = pet.state();

  canvas.text(38, 28, "BOOK PET", 4);
  canvas.line(38, 66, 754, 66);
  drawPet(canvas, state);

  canvas.text(402, 112, state.sleeping ? "DREAMING" : (state.hunger > 75 ? "HUNGRY" : "CONTENT"), 3);
  meter(canvas, 402, 175, "FULL", 100 - state.hunger);
  meter(canvas, 402, 225, "JOY", state.happiness);
  meter(canvas, 402, 275, "REST", state.energy);

  char line[32];
  snprintf(line, sizeof(line), "TIME %lu MIN", static_cast<unsigned long>(state.activeMinutes));
  canvas.text(402, 335, line, 2);
  snprintf(line, sizeof(line), "HELLOS %u", state.interactions);
  canvas.text(402, 370, line, 2);

  canvas.line(38, 438, 754, 438);
  canvas.text(48, 462, "UP: CHOOSE", 2);
  canvas.text(286, 462, "DOWN: DO", 2);
  canvas.rect(557, 451, 178, 39, true);
  // White lettering by clearing pixels inside the selected black button.
  const char* label = actionName(selected);
  const int tx = 557 + (178 - canvas.textWidth(label, 2)) / 2;
  for (int y = 460; y < 478; ++y)
    for (int x = tx - 4; x < tx + canvas.textWidth(label, 2) + 4; ++x) canvas.pixel(x, y, false);
  canvas.text(tx, 462, label, 2);

  const bool doFull = forceFull || fastRefreshes >= FULL_REFRESH_EVERY;
  display.displayBuffer(doFull ? EInkDisplay::FULL_REFRESH : EInkDisplay::FAST_REFRESH, true);
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
  esp_deep_sleep_enable_gpio_wakeup(1ULL << InputManager::POWER_BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_deep_sleep_start();
}
}

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
  bool changed = pet.tick(millis());

  for (uint8_t button = InputManager::BTN_BACK;
       button <= InputManager::BTN_DOWN; ++button) {
    if (!buttons.wasPressed(button)) continue;
    const InputIntent intent = routeButton(button);
    if (intent == InputIntent::Choose) {
      selected = static_cast<PetAction>((static_cast<uint8_t>(selected) + 1) % 3);
      lastInputMs = millis();
      changed = true;
    } else if (intent == InputIntent::Act) {
      pet.apply(selected);
      lastInputMs = millis();
      if (selected == PetAction::Sleep) {
        render();
        delay(300);
        enterDeepSleep();
      }
      changed = true;
    }
  }
  if (buttons.isPressed(InputManager::BTN_POWER) &&
      buttons.getPowerButtonHeldTime() > 1200) {
    pet.apply(PetAction::Sleep);
    render();
    delay(300);
    enterDeepSleep();
  }

  if (changed) render();
  if (millis() - lastInputMs > AUTO_SLEEP_MS) enterDeepSleep();
  delay(10);
}
