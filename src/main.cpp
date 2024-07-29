#include <Arduino.h>
#include <TFT_eSPI.h>
#include "utils.h"
#include "colourUtils.h"
#include "SineCosinePot.h"
#include "Sequence.h"
#include "Vec2.h"

#define SCREEN_WIDTH 240
#define SCREEN_HALF_WIDTH 120
#define SCREEN_HEIGHT 240
#define SCREEN_HALF_HEIGHT 120

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite screen = TFT_eSprite(&tft);
uint16_t* screenPtr;

const Vec2 screenCenter = Vec2(SCREEN_HALF_WIDTH, SCREEN_HEIGHT / 2);

Sequence sequence = Sequence(8);

SineCosinePot endlessPot = SineCosinePot(0, 1);
float cursorAngle = 0;
float a = 0;
float b = 0;
float c = 250;

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
}

void loop() {
  processInput();
  sequence.update(micros());

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
    sequence.setBpm(sequence.getBpm() + endlessPot.getAngleDelta() / 2.f);
  } else {
    cursorAngle += endlessPot.getAngleDelta();
    cursorAngle = fwrap(cursorAngle, 0, 360);
  }
}

void render() {
  screen.fillSprite(TFT_BLACK);

  float degreesPerStage = 360 / (float)sequence.stageCount();
  float stagePositionRadius = 90;

  // Update selected stage
  float highlightedStageIndexAngle = highlightedStageIndex * degreesPerStage;
  if (abs(degBetweenAngles(highlightedStageIndexAngle, cursorAngle)) > degreesPerStage * 0.66f) {
    highlightedStageIndex = (int)roundf(cursorAngle / degreesPerStage) % sequence.stageCount();
    highlightedStageIndexAngle = highlightedStageIndex * degreesPerStage;
  }
  
  // Beat tracker
  if (true) {
    auto progress = powf(sequence.getPulseAnticipation(), 2);
    auto stage = sequence.indexOfActiveStage();
    auto nextStage = (stage + 1) % sequence.stageCount();
    auto angle = stage * degreesPerStage;

    auto degToNextStage = degBetweenAngles(angle, nextStage * degreesPerStage);
    angle += degToNextStage * powf(progress, 2);

    auto pos = Vec2::fromPolar(stagePositionRadius, angle) + screenCenter;
    auto radius = 2 + 14 * powf(1 - progress, 3);

    screen.fillCircle(pos.x, pos.y, radius, rainbow(angle * 191 / 360, 0.5f));
  }

  // Stages
  for (size_t i = 0; i < sequence.stageCount(); i++) {
    float angle = i * degreesPerStage;
    Vec2 stagePos = Vec2::fromPolar(stagePositionRadius, angle) + screenCenter;
  
    Stage curStage = sequence.getStage(i);

    if (sequence.indexOfActiveStage() == i) {
      screen.setTextColor(TFT_WHITE);
    } else if (highlightedStageIndex == i) {
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
  screen.drawArc(
    SCREEN_HALF_WIDTH, SCREEN_HALF_HEIGHT, // Position
    SCREEN_HALF_WIDTH + 2, SCREEN_HALF_WIDTH - 5, // Radius, Inner Radius
    fwrap(cursorAngle + 180 - 4, 0, 360), fwrap(cursorAngle + 180 + 4, 0, 360), // Arc start & end 
    rainbow(cursorAngle * 191 / 360, 0.3f), TFT_BLACK, // Colour, AA Colour
    false // Smoothing
  );
  
  if (gpio_get(4)) {
    // BPM
    screen.setTextColor(TFT_WHITE);
    screen.drawNumber(sequence.getBpm(), screenCenter.x, screenCenter.y - 6, 2);
    screen.drawString("bpm", screenCenter.x, screenCenter.y + 6, 2);
  } else {
    // FPS
    screen.setTextColor(TFT_WHITE);
    screen.drawNumber(fps, screenCenter.x, screenCenter.y - 6, 2);
    screen.drawString("fps", screenCenter.x, screenCenter.y + 6, 2);
  }
  

  tft.pushImageDMA(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, screenPtr);
}