#include <Arduino.h>
#include <TFT_eSPI.h>
#include "utils.h"
#include "colourUtils.h"
#include "SineCosinePot.h"
#include "Sequence.h"
#include "Vec2.h"
#include "Button.h"
#include <set>
#include <algorithm>

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

Sequence sequence = Sequence(8);

SineCosinePot endlessPot = SineCosinePot(0, 1);
float cursorAngle = 0;

Button buttonA = Button(6, SKIP);
Button buttonB = Button(8, PITCH);
Button buttonC = Button(7, SELECT);
Button buttonD = Button(21, SLIDE);
Button buttonE = Button(22, NOTHING);
Button buttonF = Button(4, NOTHING);
std::vector<Button*> buttons =        {&buttonA, &buttonB, &buttonC, &buttonD, &buttonE, &buttonF};
std::vector<Button*> heldButtons;

float lastHighlightedStageIndicatorAngle = 0;
uint8_t highlightedStageIndex = 0;
bool lastSelectToggleState = true;

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

  // Gate LED
  pinMode(9, OUTPUT);

  adc_init();
  endlessPot.init();
}

void loop() {
  processInput();
  sequence.update(micros());

  // Gate LED
  digitalWrite(9, sequence.getGate());
  // Pitch LED
  float output = sequence.getOutput();
  if (output > 0) {
    analogWrite(10, powf(output, 2) * 255);
    analogWrite(11, 0);
  } else {
    analogWrite(10, 0);
    analogWrite(11, powf(output, 2) * 255);
  }
  

  if (!tft.dmaBusy()) {
    render();

    int32_t deltaMillis = millis() - lastFrameMillis;
    fps = 100000 / deltaMillis;

    lastFrameMillis = millis();
  }
}

void processInput() {
  endlessPot.update();

  // Update highlighted stage
  float degreesPerStage = 360 / (float)sequence.stageCount();
  uint8_t lastHighlightedStageIndex = highlightedStageIndex;
  float highlightedStageIndexAngle = highlightedStageIndex * degreesPerStage;
  if (abs(degBetweenAngles(highlightedStageIndexAngle, cursorAngle)) > degreesPerStage * 0.66f) {
    highlightedStageIndex = (int)roundf(cursorAngle / degreesPerStage) % sequence.stageCount();
    highlightedStageIndexAngle = highlightedStageIndex * degreesPerStage;
  }

  Stage &highlightedStage = sequence.getStage(highlightedStageIndex);
  
  // Update buttons
  for (auto button : buttons) {
    button->update();

    heldButtons.clear();
    
    // Add all held buttons to the list
    for (Button *x: buttons) {
      if ( x->held() ) { 
        heldButtons.push_back(x);
      }
    }

    // Sort them by timestamp
    std::sort(heldButtons.begin(), heldButtons.end(), 
      [](Button *a, Button *b) { 
        return a->getLastActivation() < b->getLastActivation(); 
      }
    );
  }

  Button *baseButton = heldButtons.size() > 0 ? heldButtons[0] : nullptr;
  Command baseCommand = baseButton != nullptr ? baseButton->_command : NOTHING;

  Button *modifierButton = heldButtons.size() > 1 ? heldButtons[1] : nullptr;
  Command modifier = modifierButton != nullptr ? modifierButton->_command : NOTHING;

  bool isShiftHeld = false;

  uint8_t selectedStagesCount = sequence.selectedStagesCount();
  bool areAnyStagesSelected = selectedStagesCount > 0;
  bool shouldSupressCursorRotation = false;

  if (baseCommand == PITCH) {
    shouldSupressCursorRotation = true;

    if (areAnyStagesSelected) {
      for (size_t i = 0; i < sequence.stageCount(); i++) {
        Stage& curStage = sequence.getStage(i);
        if (curStage.isSelected) {
          float newVoltage = curStage.output + endlessPot.getAngleDelta() / 180.f;
          curStage.output = coerceInRange(newVoltage, -1, 1);
        }
      }
    } else {
      float newVoltage = highlightedStage.output + endlessPot.getAngleDelta() / 180.f;
      highlightedStage.output = coerceInRange(newVoltage, -1, 1);
    }
  } else if (baseCommand == SKIP) {
    if (baseButton->risingEdge()) {
      if (areAnyStagesSelected) {
        for (size_t i = 0; i < sequence.stageCount(); i++) {
          Stage& curStage = sequence.getStage(i);
          if (curStage.isSelected) {
            curStage.isSkipped = !curStage.isSkipped;
          }
        }
      } else {
        highlightedStage.isSkipped = !highlightedStage.isSkipped;
      }
      sequence.updateNextStageIndex();
    }
  } else if (baseCommand == SELECT) {
    if (modifier == NOTHING) {
      if (baseButton->risingEdge()) {
        if (baseButton->doubleTapped()) { 
          // Select all or clear selection on double tap
          bool isHighlightedStageTheOnlySelectedStage = (selectedStagesCount == 1 && highlightedStage.isSelected);
          bool shouldSelectStages = isHighlightedStageTheOnlySelectedStage || selectedStagesCount == 0;

          for (size_t i = 0; i < sequence.stageCount(); i++) {
              sequence.getStage(i).isSelected = shouldSelectStages;
          }
        } else {
          highlightedStage.isSelected = !highlightedStage.isSelected;
          lastSelectToggleState = highlightedStage.isSelected;
        }
      }

      if (lastHighlightedStageIndex != highlightedStageIndex) {
        highlightedStage.isSelected = lastSelectToggleState;
      }
    } else if (modifier == SLIDE) {
      if (modifierButton->risingEdge()) {
        for (size_t i = 0; i < sequence.stageCount(); i++) {
          sequence.getStage(i).isSelected = sequence.getStage(i).shouldSlideIn;
        }
      }
    }
  } else if (baseCommand == SLIDE) {
    if (baseButton->risingEdge()) {
      if (areAnyStagesSelected) {
        for (size_t i = 0; i < sequence.stageCount(); i++) {
          Stage& curStage = sequence.getStage(i);
          if (curStage.isSelected) {
            curStage.shouldSlideIn = !curStage.shouldSlideIn;
          }
        }
      } else {
        highlightedStage.shouldSlideIn = !highlightedStage.shouldSlideIn;
      }
    }
  }

  if (!shouldSupressCursorRotation) {
    // Cusor
    cursorAngle += endlessPot.getAngleDelta();
    cursorAngle = fwrap(cursorAngle, 0, 360); 
  }
}

void drawPulsePips(Stage& stage, float angle, Vec2 pos, int8_t currentPulseInStage) {
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
      bool isPulseActive = stage.isPulseActive(pulseIndex);
      if (pulseIndex <= currentPulseInStage) {
        colour = (isPulseActive) ? COLOUR_ACTIVE :  COLOUR_INACTIVE;
      } else {
        colour = (isPulseActive) ? COLOUR_INACTIVE :  COLOUR_SKIPPED;
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

void drawStageOutput(float output, uint16_t colour, Vec2 pos) {
  const float minVoltageSizeThing = 0.5f;

  if (output <= -minVoltageSizeThing) {
    screen.drawCircle(
      pos.x, pos.y, // Position,
      -output * 10,
      colour
    );
  } else if (output >= minVoltageSizeThing) {
    screen.fillCircle(
      pos.x, pos.y, // Position,
      output * 10,
      colour
    );
  } else {
    screen.drawArc(
      pos.x, pos.y, // Position
      minVoltageSizeThing * 10, 5 - (output + minVoltageSizeThing) * 5, // Radius, Inner Radius
      0, 359, // Arc start & end 
      colour, COLOUR_BG, // Colour, AA Colour
      false // Smoothing
    );
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

////////////////////////////////////////////////////
///                   Render                    ////
////////////////////////////////////////////////////
void render() {
  screen.fillSprite(COLOUR_BG);

  float degreesPerStage = 360 / (float)sequence.stageCount();
  uint stagePositionRadius = 48 + 3 * sequence.stageCount();

  // Beat Indicator
  if (true) {
    size_t stageIndex = sequence.indexOfActiveStage();
    size_t nextStageIndex = sequence.getNextStageIndex();
    Stage &activeStage = sequence.getActiveStage();
    Stage &nextStage = sequence.getStage(nextStageIndex);
    float angle = stageIndex * degreesPerStage;
    float progress = powf(sequence.getPulseAnticipation(), 2);

    if (sequence.isLastPulseOfStage()) {
      auto degToNextStage = degBetweenAngles(angle, nextStageIndex * degreesPerStage);

      if (degToNextStage < 0) {
        degToNextStage += 360; 
      }

      if (nextStage.shouldSlideIn) {
        degToNextStage -= degreesPerStage * 0.75f;
      }

      angle += degToNextStage * powf(progress, 2);
    } 
    
    if (sequence.isSliding()) {
      auto degToPrevStage = wrapDeg(degreesPerStage * 0.75f);

      angle -= degToPrevStage * (1 - progress);
    }

    auto pos = Vec2::fromPolar(stagePositionRadius, angle) + screenCenter;
    uint32_t radius;

    if (activeStage.gateMode == HELD) {
      // Held note
      radius = 12 + 4 * powf(1 - progress, 3);
    } else if (activeStage.isPulseActive(sequence.getCurrentPulseInStage())) {
      // Active pulse
      radius = 2 + 16 * (1 - progress);
    } else {
      // Inactive pulse
      radius = 10 + 2 * (1 - progress);
    }

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
    
    // Slide indicator
    if (curStage.shouldSlideIn) {
      float endAngle = wrapDeg(180 + angle);
      float degToStartAngle = -degreesPerStage * 0.75f;
      float startAngle = wrapDeg(endAngle + degToStartAngle);

      screen.drawArc(
        screenCenter.x, screenCenter.y, // Position
        stagePositionRadius + 1, stagePositionRadius - 1, // Radius, Inner Radius
        startAngle, endAngle, // Arc start & end 
        COLOUR_SKIPPED, COLOUR_BG, // Colour, AA Colour
        false // Smoothing
      );

      if (isActive && sequence.isSliding()) {
        screen.drawArc(
          screenCenter.x, screenCenter.y, // Position
          stagePositionRadius + 1, stagePositionRadius - 1, // Radius, Inner Radius
          startAngle, wrapDeg(startAngle - degToStartAngle * sequence.getPulseAnticipation()), // Arc start & end 
          COLOUR_ACTIVE, COLOUR_BG, // Colour, AA Colour
          false // Smoothing
        );
      }
    }

    drawStageOutput(curStage.output, colour, stagePos);
    drawPulses(curStage, angle, stagePos, isActive ? sequence.getCurrentPulseInStage() : -1);
    if (curStage.isSkipped) { drawStageStrikethrough(stagePos); }

    // Selected indicator
    if (curStage.isSelected) {
      Vec2 selectionPipPos = Vec2::fromPolar(stagePositionRadius + 16, angle) + screenCenter;

      screen.fillCircle(
        selectionPipPos.x, selectionPipPos.y, // Position,
        2,
        COLOUR_ACTIVE
      );
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
  
  // if (gpio_get(4)) {
  //   // BPM
  //   screen.setTextColor(COLOUR_INACTIVE);
  //   screen.drawNumber(sequence.getBpm(), screenCenter.x, screenCenter.y - 6, 2);
  //   screen.drawString("bpm", screenCenter.x, screenCenter.y + 6, 2);
  // }

  // FPS
  screen.setTextColor(COLOUR_INACTIVE);
  screen.drawNumber(fps, screenCenter.x, 12, 2);
  screen.drawString("fps", screenCenter.x, 24, 2);

  // Debug
  for (int i = 0; i < heldButtons.size(); i++) {
    Vec2 pos = Vec2::fromPolar(SCREEN_HALF_WIDTH - 25, 290 - i * 10) + screenCenter;

    screen.setTextColor(COLOUR_INACTIVE);
    screen.drawString(toString(heldButtons[i]->_command), pos.x, pos.y, 2);
  }

  // Gate
  drawStageOutput(sequence.getOutput(), sequence.getGate() ? COLOUR_ACTIVE : COLOUR_SKIPPED, screenCenter);
  
  tft.pushImageDMA(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, screenPtr);
}