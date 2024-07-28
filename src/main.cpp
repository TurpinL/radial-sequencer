#include <Arduino.h>
#include <TFT_eSPI.h>
#include "utils.h"
#include "SineCosinePot.h"
#include "Sequence.h"
#include "Vec2.h"

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite screen = TFT_eSprite(&tft);
uint16_t* screenPtr;

const Vec2 screenCenter = Vec2(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);

Sequence sequence;

SineCosinePot endlessPot = SineCosinePot(0, 1);
float cursorAngle = 0;
float a = 0;
float b = 0;
float c = 0;

float lastHighlightedStageIndicatorAngle = 0;
float highlightedStageIndex = 0;

int32_t lastFrameMillis = 0;
float fps = 0;

void render();
void processInput();

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

  for (int i = 0; i < 16; i++) {
    sequence.addStage();
  }
}

void loop() {
  processInput();

  if (!tft.dmaBusy()) {
    render();

    int32_t deltaMillis = millis() - lastFrameMillis;
    fps = 1000 / deltaMillis;

    lastFrameMillis = millis();
  }
}

void processInput() {
  endlessPot.update();

  if (gpio_get(21)) {
    a += endlessPot.getAngleDelta();
  } else if (gpio_get(22)) {
    b += endlessPot.getAngleDelta();
    sequence.getStage(highlightedStageIndex).voltage += endlessPot.getAngleDelta() / 180.f;
  } else if (gpio_get(4)) {
    c += endlessPot.getAngleDelta();
  } else {
    cursorAngle += endlessPot.getAngleDelta();
    cursorAngle = fwrap(cursorAngle, 0, 360);
  }
}

void render() {
  screen.fillSprite(TFT_BLACK);

  float degreesPerStage = 360 / (float)sequence.stagesCount();

  // Update selected stage
  float highlightedStageIndexAngle = highlightedStageIndex * degreesPerStage;
  if (abs(degBetweenAngles(highlightedStageIndexAngle, cursorAngle)) > degreesPerStage * 0.66f) {
    highlightedStageIndex = (int)roundf(cursorAngle / degreesPerStage) % sequence.stagesCount();
    highlightedStageIndexAngle = highlightedStageIndex * degreesPerStage;
  }

  // Stages
  for (size_t i = 0; i < sequence.stagesCount(); i++) {
    float angle = i * degreesPerStage;
    Vec2 stagePos = Vec2::fromPolar(90, angle) + screenCenter;
  
    Stage curStage = sequence.getStage(i);

    if (highlightedStageIndex == i) {
      screen.setTextColor(TFT_WHITE);
    } else {
      screen.setTextColor(TFT_DARKGREY);
    }
    int noteIndex = round(fwrap(curStage.voltage, 0, 1) * 11);
    screen.drawString(
      indexToNote[noteIndex], 
      stagePos.x - 5, 
      stagePos.y + 2,
      4
    );

    if (noteIndex == 1 || noteIndex == 3 || noteIndex == 6 || noteIndex == 8 || noteIndex == 10) {
      screen.drawString(
        "#", 
        stagePos.x + 9, 
        stagePos.y - 8, 
        1
      );
    }

    screen.drawNumber(
      curStage.voltage, 
      stagePos.x + 9, 
      stagePos.y + 8, 
      1
    );
  }

  // Cursor
  Vec2 cursorPos = Vec2::fromPolar(SCREEN_WIDTH / 2, cursorAngle) + screenCenter;
  screen.fillCircle(cursorPos.x, cursorPos.y, 10, TFT_SKYBLUE);

  // FPS
  screen.setTextColor(TFT_WHITE);
  screen.drawNumber(fps, screenCenter.x, screenCenter.y - 6, 2);
  screen.drawString("fps", screenCenter.x, screenCenter.y + 6, 2);

  tft.pushImageDMA(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, screenPtr);
}