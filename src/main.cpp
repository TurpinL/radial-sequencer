#include <Arduino.h>
#include <TFT_eSPI.h>
#include "utils.h"
#include "colourUtils.h"
#include "SineCosinePot.h"
#include "Sequence.h"
#include "Vec2.h"
#include "Button.h"

#define SCREEN_WIDTH 240
#define SCREEN_HALF_WIDTH 120
#define SCREEN_HEIGHT 240
#define SCREEN_HALF_HEIGHT 120

const uint16_t COLOUR_BG =        0x0000;
const uint16_t COLOUR_BEAT =      0xfc48;
const uint16_t COLOUR_ACTIVE =    0xf614;
const uint16_t COLOUR_INACTIVE =  0xaa21;
const uint16_t COLOUR_SKIPPED =   0x5180;

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite screen = TFT_eSprite(&tft);
uint16_t* screenPtr;

const Vec2 screenCenter = Vec2(SCREEN_HALF_WIDTH, SCREEN_HALF_HEIGHT);

Sequence sequence = Sequence(6);

SineCosinePot endlessPot = SineCosinePot(0, 1);
float cursorAngle = 0;

Button buttonA = Button(21);
Button buttonB = Button(22);
Button buttonC = Button(4);

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
  tft.fillScreen(COLOUR_BG);
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
  buttonA.update();
  buttonB.update();
  buttonC.update();

  if (buttonA.fallingEdge()) {
    sequence.getStage(highlightedStageIndex).isSkipped = !sequence.getStage(highlightedStageIndex).isSkipped;
    sequence.updateNextStageIndex();
  } else if (buttonB.held()) {
    float newVoltage = sequence.getStage(highlightedStageIndex).voltage + endlessPot.getAngleDelta() / 180.f;
    sequence.getStage(highlightedStageIndex).voltage = coerceInRange(newVoltage, -1, 1);
  } else if (buttonC.held()) {
    sequence.setBpm(sequence.getBpm() + endlessPot.getAngleDelta() / 2.f);
  } else {
    cursorAngle += endlessPot.getAngleDelta();
    cursorAngle = fwrap(cursorAngle, 0, 360);
  }
}

void drawPulsePips(Stage& stage, float angle, Vec2 pos, int8_t currentPulseInStage) {
  // TODO: Gate Modes
  int rowCount = stage.pulseCount / 4 + (stage.pulseCount % 4 > 0);
  int pxPerPip = 7;

  for (int row = 0; row < rowCount; row++) {
    int pulsesInRow = min(4, stage.pulseCount - 4 * row);

    // Rows 2 and 4 are in the outer shell
    int rowRadius = 20 + ((row == 0 || row == 2) ? 0 : 6);
    float degPerPip = degsPerPixel[rowRadius] * pxPerPip;

    float startAngle = (pulsesInRow - 1) * -0.5f * degPerPip + 180;

    for (int rowIndex = 0; rowIndex < pulsesInRow; rowIndex++) {
      float pulseAngle = angle + startAngle + (pulsesInRow - rowIndex - 1) * degPerPip;
      uint8_t pulseIndex = row * 4 + rowIndex;

      uint16_t colour;
      if (!stage.isPulseActive(pulseIndex)) {
        colour = COLOUR_SKIPPED;
      } else if (pulseIndex <= currentPulseInStage) {
        colour = COLOUR_ACTIVE;
      } else {
        colour = COLOUR_INACTIVE;
      }

      Vec2 pipPos = pos + Vec2::fromPolar(rowRadius, pulseAngle);
      screen.fillRect(
        pipPos.x - 1.5, pipPos.y - 1.5, // Position
        3, 3,
        colour
      );
    }
  }
}

void drawHeldPulses(Stage& stage, float angle, Vec2 pos) {
  bool isStageActive = &stage == &(sequence.getActiveStage());

  // 0 to pulseCount
  float progress;
  if (isStageActive) {
    progress = sequence.getCurrentPulseInStage() + sequence.getPulseAnticipation();
  } else {
    progress = 0;
  } 

  // TODO: Gate Modes
  int rowCount = stage.pulseCount / 4 + (stage.pulseCount % 4 > 0);
  int pxPerPip = 7;

  for (int row = 0; row < rowCount; row++) {
    int pulsesInRow = min(4, stage.pulseCount - 4 * row);

    // Rows 2 and 4 are in the outer shell
    int rowRadius = 20 + ((row == 0 || row == 2) ? 0 : 6);
    float degPerPip = degsPerPixel[rowRadius] * pxPerPip;

    float degsInArc = pulsesInRow * degPerPip;
    float startAngle = wrapDeg(angle - degsInArc * 0.5f);
    float endAngle = wrapDeg(angle + degsInArc * 0.5f);

    screen.drawArc(
      pos.x, pos.y, // Position
      rowRadius + 2, rowRadius - 1, // Radius, Inner Radius
      startAngle, endAngle, // Arc start & end 
      (stage.isSkipped) ? COLOUR_SKIPPED : COLOUR_INACTIVE, COLOUR_BG, // Colour, AA Colour
      false // Smoothing
    );

    if (progress > row * 4) { 
      float rowProgress = min(1, (progress - row * 4) / pulsesInRow);

      screen.drawArc(
        pos.x, pos.y, // Position
        rowRadius + 2, rowRadius - 1, // Radius, Inner Radius
        wrapDeg(endAngle - degsInArc * rowProgress), endAngle, // Arc start & end 
        COLOUR_ACTIVE, COLOUR_BG, // Colour, AA Colour
        false // Smoothing
      );
    }
  }
}

void drawPulses(Stage& stage, float angle, Vec2 pos, int8_t currentPulseInStage) {
  if (stage.gateMode == HELD) {
    drawHeldPulses(stage, angle, pos);
  } else {
    drawPulsePips(stage, angle, pos, currentPulseInStage);
  }
}

void drawStageStrikethrough(Vec2 pos) {
  Vec2 d = (pos - screenCenter).normalized();
  Vec2 n = d.normal();

  float halfWidth = 1.25;
  Vec2 corner1 = pos + d * -32 + n * halfWidth;
  Vec2 corner2 = pos + d * 16 + n * halfWidth;
  Vec2 corner3 = pos + d * 16 + n * -halfWidth;
  Vec2 corner4 = pos + d * -32 + n * -halfWidth;

  Vec2 cornerB1 = pos + d * -33.5 + n * (halfWidth+1.5f);
  Vec2 cornerB2 = pos + d * 17.5 + n * (halfWidth+1.5f);
  Vec2 cornerB3 = pos + d * 17.5 + n * -(halfWidth+1.5f);
  Vec2 cornerB4 = pos + d * -33.5 + n * -(halfWidth+1.5f);

  screen.fillTriangle(
    cornerB1.x, cornerB1.y,
    cornerB2.x, cornerB2.y,
    cornerB4.x, cornerB4.y,
    COLOUR_BG
  );

  screen.fillTriangle(
    cornerB4.x, cornerB4.y,
    cornerB2.x, cornerB2.y,
    cornerB3.x, cornerB3.y,
    COLOUR_BG
  );

  screen.fillTriangle(
    corner1.x, corner1.y,
    corner2.x, corner2.y,
    corner4.x, corner4.y,
    COLOUR_SKIPPED
  );

  screen.fillTriangle(
    corner4.x, corner4.y,
    corner2.x, corner2.y,
    corner3.x, corner3.y,
    COLOUR_SKIPPED
  );
}

void render() {
  screen.fillSprite(COLOUR_BG);

  float degreesPerStage = 360 / (float)sequence.stageCount();
  uint stagePositionRadius = 48 + 3 * sequence.stageCount();


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
    auto nextStage = sequence.getNextStageIndex();
    auto angle = stage * degreesPerStage;

    if (sequence.isLastPulseOfStage()) {
      auto degToNextStage = degBetweenAngles(angle, nextStage * degreesPerStage);
      angle += degToNextStage * powf(progress, 2);
    }

    auto pos = Vec2::fromPolar(stagePositionRadius, angle) + screenCenter;
    auto radius = 2 + 16 * powf(1 - progress, 3);

    screen.fillCircle(pos.x, pos.y, radius, COLOUR_BEAT);
  }

  // Stages
  for (size_t i = 0; i < sequence.stageCount(); i++) {
    bool isActive = sequence.indexOfActiveStage() == i;
    float angle = i * degreesPerStage;
    Vec2 stagePos = Vec2::fromPolar(stagePositionRadius, angle) + screenCenter;
  
    Stage& curStage = sequence.getStage(i);

    uint16_t colour;
    if (isActive || highlightedStageIndex == i) {
      colour = COLOUR_ACTIVE;
    } else if (curStage.isSkipped) {
      colour = COLOUR_SKIPPED; 
    } else {
      colour = COLOUR_INACTIVE; 
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
        colour, COLOUR_BG, // Colour, AA Colour
        false // Smoothing
      );
    }
  
    drawPulses(curStage, angle, stagePos, isActive ? sequence.getCurrentPulseInStage() : -1);

    if (curStage.isSkipped) {
      drawStageStrikethrough(stagePos);
    }
  }

  // Cursor
  screen.drawArc(
    SCREEN_HALF_WIDTH, SCREEN_HALF_HEIGHT, // Position
    SCREEN_HALF_WIDTH + 2, SCREEN_HALF_WIDTH - 5, // Radius, Inner Radius
    fwrap(cursorAngle + 180 - 4, 0, 360), fwrap(cursorAngle + 180 + 4, 0, 360), // Arc start & end 
    COLOUR_ACTIVE, COLOUR_BG, // Colour, AA Colour
    false // Smoothing
  );
  
  if (gpio_get(4)) {
    // BPM
    screen.setTextColor(COLOUR_INACTIVE);
    screen.drawNumber(sequence.getBpm(), screenCenter.x, screenCenter.y - 6, 2);
    screen.drawString("bpm", screenCenter.x, screenCenter.y + 6, 2);
  } else {
    // FPS
    screen.setTextColor(COLOUR_INACTIVE);
    screen.drawNumber(fps, screenCenter.x, screenCenter.y - 6, 2);
    screen.drawString("fps", screenCenter.x, screenCenter.y + 6, 2);
  }
  

  tft.pushImageDMA(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, screenPtr);
}