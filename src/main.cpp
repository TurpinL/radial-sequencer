#include <Arduino.h>
#include <TFT_eSPI.h>
#include "utils.h"
#include "sineCosinePot.h"
#include "Vec2.h"

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite screen = TFT_eSprite(&tft);
uint16_t* screenPtr;

const Vec2 screenCenter = Vec2(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);

SineCosinePot endlessPot = SineCosinePot(0, 1);
float a = 0;
float cursorAngle = 0;

int32_t lastFrameMillis = 0;
uint16_t fps = 0;

void render();

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.initDMA();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  screenPtr = (uint16_t*)screen.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);
  screen.setTextDatum(MC_DATUM);
  tft.startWrite(); // TFT chip select held low permanently

  adc_init();
  endlessPot.init();
}

void loop() {
  endlessPot.update();

  a += endlessPot.getAngleDelta();

  if (!tft.dmaBusy()) {
    render();

    int32_t deltaMillis = millis() - lastFrameMillis;
    fps = 1000 / deltaMillis;

    lastFrameMillis = millis();
  }
}

void render() {
  screen.fillSprite(TFT_BLACK);

  // Cursor
  Vec2 cursorPos = Vec2::fromPolar(90, endlessPot.getAngle()) + screenCenter;
  screen.fillCircle(cursorPos.x, cursorPos.y, 8, TFT_SKYBLUE);

  // FPS
  screen.setTextColor(TFT_WHITE);
  screen.drawNumber(fps, screenCenter.x, screenCenter.y - 6, 2);
  screen.drawString("fps", screenCenter.x, screenCenter.y + 6, 2);

  tft.pushImageDMA(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, screenPtr);
}