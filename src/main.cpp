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

Sequence sequence = Sequence(16);

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
    fps = 100000 / deltaMillis;

    lastFrameMillis = millis();
  }
}

void processInput() {
  endlessPot.update();

  if (gpio_get(21)) {
    a += endlessPot.getAngleDelta();
  } else if (gpio_get(22)) {
    b += endlessPot.getAngleDelta();
    sequence.getStage(highlightedStageIndex).voltage += endlessPot.getAngleDelta() / 360.f;
  } else if (gpio_get(4)) {
    sequence.setBpm(sequence.getBpm() + endlessPot.getAngleDelta() / 2.f);
  } else {
    cursorAngle += endlessPot.getAngleDelta();
    cursorAngle = fwrap(cursorAngle, 0, 360);
  }
}

void drawPulsePips(Stage& stage, float angle, Vec2 pos) {
  // TODO: Gate Modes
  int rowCount = stage.pulseCount / 4 + (stage.pulseCount % 4 > 0);
  int pxPerPip = 7;

  for (int row = 0; row < rowCount; row++) {
    bool isLastRow = row == rowCount - 1;
    int pulsesInRow = min(4, stage.pulseCount - 4 * row);

    // Rows 2 and 4 are in the outer shell
    int rowRadius = 20 + ((row == 0 || row == 2) ? 0 : 6);
    float degPerPip = degsPerPixel[rowRadius] * pxPerPip;

    float startAngle = (pulsesInRow - 1) * -0.5f * degPerPip + 180;

    for (int pulse = 0; pulse < pulsesInRow; pulse++) {
      float pulseAngle = angle + startAngle + (pulsesInRow - pulse - 1) * degPerPip;
     
      Vec2 pipPos = pos + Vec2::fromPolar(rowRadius, pulseAngle);
      screen.fillRect(
        pipPos.x - 1.5, pipPos.y - 1.5, // Position
        3, 3,
        TFT_WHITE
      );
    }
  }
}

void drawHeldPulses(Stage& stage, float angle, Vec2 pos) {
  // TODO: Gate Modes
  int rowCount = stage.pulseCount / 4 + (stage.pulseCount % 4 > 0);
  int pxPerPip = 7;

  for (int row = 0; row < rowCount; row++) {
    bool isLastRow = row == rowCount - 1;
    int pulsesInRow = min(4, stage.pulseCount - 4 * row);

    // Rows 2 and 4 are in the outer shell
    int rowRadius = 20 + ((row == 0 || row == 2) ? 0 : 6);
    float degsPerPixelAtRadius = degsPerPixel[rowRadius];
    float degPerPip = degsPerPixel[rowRadius] * pxPerPip;

    float startAngle = wrapDeg(angle + (pulsesInRow - 1) * -0.5f * degPerPip - degsPerPixelAtRadius * 2);
    float endAngle = wrapDeg(angle + (pulsesInRow - 1) * 0.5f * degPerPip + degsPerPixelAtRadius * 2);

    screen.drawArc(
      pos.x, pos.y, // Position
      rowRadius + 2, rowRadius - 1, // Radius, Inner Radius
      startAngle, endAngle, // Arc start & end 
      TFT_WHITE, TFT_BLACK, // Colour, AA Colour
      false // Smoothing
    );
  }
}

void render() {
  screen.fillSprite(TFT_BLACK);

  float degreesPerStage = 360 / (float)sequence.stageCount();
  uint stagePositionRadius = 32 + 4 * sequence.stageCount();

  // Update selected stage
  float highlightedStageIndexAngle = highlightedStageIndex * degreesPerStage;
  if (abs(degBetweenAngles(highlightedStageIndexAngle, cursorAngle)) > degreesPerStage * 0.66f) {
    highlightedStageIndex = (int)roundf(cursorAngle / degreesPerStage) % sequence.stageCount();
    highlightedStageIndexAngle = highlightedStageIndex * degreesPerStage;
  }
  
  // Beat Indicator
  if (true) {
    auto progress = powf(sequence.getPulseAnticipation(), 2);
    auto stage = sequence.indexOfActiveStage();
    auto nextStage = (stage + 1) % sequence.stageCount();
    auto angle = stage * degreesPerStage;

    if (sequence.isLastPulseOfStage()) {
      auto degToNextStage = degBetweenAngles(angle, nextStage * degreesPerStage);
      angle += degToNextStage * powf(progress, 2);
    }

    auto pos = Vec2::fromPolar(stagePositionRadius, angle) + screenCenter;
    auto radius = 2 + 16 * powf(1 - progress, 3);

    screen.fillCircle(pos.x, pos.y, radius, rainbow(angle * 191 / 360, 0.5f));
  }

  // Stages
  for (size_t i = 0; i < sequence.stageCount(); i++) {
    float angle = i * degreesPerStage;
    Vec2 stagePos = Vec2::fromPolar(stagePositionRadius, angle) + screenCenter;
  
    Stage curStage = sequence.getStage(i);

    uint16_t colour;
    if (sequence.indexOfActiveStage() == i || highlightedStageIndex == i) {
      colour = TFT_WHITE;
    } else {
      colour = TFT_DARKGREY;
    }

    const float minVoltageSizeThing = 0.5f;

    if (curStage.voltage <= -minVoltageSizeThing) {
      screen.drawCircle(
        stagePos.x, stagePos.y, // Position,
        wrapDeg(-curStage.voltage * 10),
        colour
      );
    } else if (curStage.voltage >= minVoltageSizeThing) {
      screen.fillCircle(
        stagePos.x, stagePos.y, // Position,
        wrapDeg(curStage.voltage * 10),
        colour
      );
    } else {
      screen.drawArc(
        stagePos.x, stagePos.y, // Position
        minVoltageSizeThing * 10, 5 - (curStage.voltage + minVoltageSizeThing) * 5, // Radius, Inner Radius
        0, 359, // Arc start & end 
        colour, TFT_BLACK, // Colour, AA Colour
        false // Smoothing
      );
    }
  
    (i % 2) ? drawHeldPulses(curStage, angle, stagePos) : drawPulsePips(curStage, angle, stagePos);
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