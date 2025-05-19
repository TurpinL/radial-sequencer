#include "Render.hpp"

#define SCREEN_WIDTH 240
#define SCREEN_HALF_WIDTH 120
#define SCREEN_HEIGHT 240
#define SCREEN_HALF_HEIGHT 120
const Vec2 screenCenter = Vec2(SCREEN_HALF_WIDTH, SCREEN_HALF_HEIGHT);

const uint16_t COLOUR_BG =        0x0000;
const uint16_t COLOUR_BEAT =      0x8224;
const uint16_t COLOUR_USER =      0x96cd;
const uint16_t COLOUR_ACTIVE =    0xfd4f;
const uint16_t COLOUR_INACTIVE =  0xaa21;
const uint16_t COLOUR_SKIPPED =   0x5180;

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite screen = TFT_eSprite(&tft);
uint16_t* screenPtr;

StageDrawInfo renderableStages[MAX_STAGES];
int32_t lastFrameMillis = 0;
float fps = 0;
int32_t lastAnimationTickMillis = 0;

void drawStageOutput(float output, uint16_t colour, Vec2 pos);
void drawPulses(Stage& stage, float angle, Vec2 pos, int8_t currentPulseInStage, GateMode gateMode, float pulseAnticipation);
void drawHeldPulses(Stage& stage, float angle, Vec2 pos, float pulseAnticipation);
void drawPulsePips(Stage& stage, float angle, Vec2 pos, int8_t currentPulseInStage, GateMode gateMode);
void drawStageStrikethrough(Vec2 pos);

void initScreen() {
  tft.init();
  tft.initDMA();
  tft.setRotation(1);
  tft.fillScreen(COLOUR_BG);
  screenPtr = (uint16_t*)screen.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);
  screen.setTextDatum(MC_DATUM);
  tft.startWrite(); // TFT chip select held low permanently
}

float degreesPerStage(float stageCount) {
  return 360 / stageCount;
}

void updateAnimations(
    Sequence *sequence,
    uint8_t highlightedStageIndex,
    bool isEditingPosition,
    bool isEditingGateMode,
    bool isEditingPitch,
    float hiddenValue
) {
  if ((millis() - lastAnimationTickMillis) > 16 || lastAnimationTickMillis == 0) {
    lastAnimationTickMillis += 16;

    uint defaultStagePositionRadius = 48 + 3 * sequence->stageCount();

    for (size_t i = 0; i < sequence->stageCount(); i++) {
      Stage& stage = sequence->getStage(i);
      StageDrawInfo& stageDrawInfo = renderableStages[stage.id];
      bool isHighlighted = highlightedStageIndex == i || stage.isSelected;

      // Update position
      float targetRadius = defaultStagePositionRadius;
      targetRadius += (isHighlighted ? 8 : 0);
      if (isEditingPosition) {
        targetRadius += (isHighlighted ? 4 : -8);
      }

      float targetAngle = i * degreesPerStage(sequence->stageCount());
      if (isEditingPosition && isHighlighted) {
        stageDrawInfo.angle = targetAngle + hiddenValue;
      }

      bool isEditingGateModeOfThisStage = isEditingGateMode && (i == highlightedStageIndex || stage.isSelected);
      if (isEditingGateModeOfThisStage) {
        stageDrawInfo.pulsePipsAngle = stage.pulsePipsAngle;
      } else {
        stage.pulsePipsAngle = stage.gateMode * 90;
        stageDrawInfo.pulsePipsAngle = stageDrawInfo.pulsePipsAngle + degBetweenAngles(stageDrawInfo.pulsePipsAngle, stage.pulsePipsAngle) * 0.1;
      }

      if (isEditingPosition && isHighlighted) {
        stageDrawInfo.angle = targetAngle + hiddenValue;
      } else {
        stageDrawInfo.angle = stageDrawInfo.angle + degBetweenAngles(stageDrawInfo.angle, targetAngle) * 0.1;
      }

      stageDrawInfo.radius = lerp(stageDrawInfo.radius, targetRadius, 0.1);
      
      if (isEditingPitch) {
        stageDrawInfo.output = stage.output;
      } else {
        stageDrawInfo.output = lerp(stageDrawInfo.output, stage.output, 0.1);
      }
    }
  }
}

void renderIfDmaIsReady(
    Sequence *sequence, 
    float cursorAngle, 
    uint8_t highlightedStageIndex, 
    const std::vector<Button*> &activeButtons,
    bool isEditingGateMode
) {
    if (!tft.dmaBusy()) {
        render(
            sequence, 
            cursorAngle, 
            highlightedStageIndex, 
            activeButtons,
            isEditingGateMode
        );

        int32_t deltaMillis = millis() - lastFrameMillis;
        fps = 100000 / deltaMillis;

        lastFrameMillis = millis();
    }
}

void render(
    Sequence *sequence, 
    float cursorAngle, 
    uint8_t highlightedStageIndex, 
    const std::vector<Button*> &activeButtons,
    bool isEditingGateMode
) {
  screen.fillSprite(COLOUR_BG);

  float targetDegreesPerStage = 360 / (float)sequence->stageCount();

  // Beat Indicator
  if (true) {
    size_t nextStageIndex = sequence->getNextStageIndex();
    Stage &activeStage = sequence->getActiveStage();
    StageDrawInfo& activeStageDrawInfo = renderableStages[activeStage.id];
    Stage &nextStage = sequence->getStage(nextStageIndex);
    StageDrawInfo& nextStageDrawInfo = renderableStages[nextStage.id];
    float angle = activeStageDrawInfo.angle;
    float polarRadius = nextStageDrawInfo.radius;
    float progress = powf(sequence->getPulseAnticipation(), 2);

    if (sequence->isLastPulseOfStage()) {
      auto degToNextStage = degBetweenAngles(angle, nextStageDrawInfo.angle);

      if (degToNextStage < 0) {
        degToNextStage += 360; 
      }

      angle += degToNextStage * powf(progress, 2);
      polarRadius = lerp(activeStageDrawInfo.radius, nextStageDrawInfo.radius, powf(progress, 2));
    } 
    
    auto pos = Vec2::fromPolar(polarRadius, angle) + screenCenter;
    uint32_t radius;

    if (activeStage.gateMode == HELD) {
      // Held note
      radius = 12 + 4 * powf(1 - progress, 3);
    } else if (activeStage.isPulseActive(sequence->getCurrentPulseInStage())) {
      // Active pulse
      radius = 2 + 16 * (1 - progress);
    } else {
      // Inactive pulse
      radius = 10 + 2 * (1 - progress);
    }

    screen.fillCircle(pos.x, pos.y, radius, COLOUR_BEAT);
  }

  // Stages
  for (size_t i = 0; i < sequence->stageCount(); i++) {
    Stage& curStage = sequence->getStage(i);
    StageDrawInfo& stageDrawInfo = renderableStages[curStage.id];

    bool isActive = sequence->indexOfActiveStage() == i;
    bool isHighlighted = highlightedStageIndex == i || curStage.isSelected;

    Vec2 stagePos = Vec2::fromPolar(stageDrawInfo.radius, stageDrawInfo.angle) + screenCenter;

    uint16_t colour;
    if (isHighlighted) {
      colour = COLOUR_USER;
    } else if (isActive) {
      colour = COLOUR_ACTIVE; 
    } else if (curStage.isSkipped) {
      colour = COLOUR_SKIPPED; 
    } else {
      colour = COLOUR_INACTIVE; 
    }
    
    // Slide indicator
    if (curStage.shouldSlideIn) {
      float endAngle = wrapDeg(180 + stageDrawInfo.angle);
      float degToStartAngle = -targetDegreesPerStage * 0.75f;
      float startAngle = wrapDeg(endAngle + degToStartAngle);

      screen.drawArc(
        screenCenter.x, screenCenter.y, // Position
        stageDrawInfo.radius + 1, stageDrawInfo.radius - 1, // Radius, Inner Radius
        startAngle, endAngle, // Arc start & end 
        COLOUR_SKIPPED, COLOUR_BG, // Colour, AA Colour
        false // Smoothing
      );

      if (isActive && sequence->isSliding()) {
        screen.drawArc(
          screenCenter.x, screenCenter.y, // Position
          stageDrawInfo.radius + 1, stageDrawInfo.radius - 1, // Radius, Inner Radius
          startAngle, wrapDeg(startAngle - degToStartAngle * sequence->getPulseAnticipation()), // Arc start & end 
          COLOUR_ACTIVE, COLOUR_BG, // Colour, AA Colour
          false // Smoothing
        );
      }
    }

    drawStageOutput(stageDrawInfo.output, colour, stagePos);

    bool isEditingGateModeOfThisStage = isEditingGateMode && (i == highlightedStageIndex || curStage.isSelected);
    bool arePulsePipsAnimating = abs(degBetweenAngles(stageDrawInfo.pulsePipsAngle, curStage.pulsePipsAngle)) > 10;

    if (curStage.gateMode == EACH || isEditingGateModeOfThisStage || arePulsePipsAnimating) {
      drawPulses(curStage, stageDrawInfo.angle + stageDrawInfo.pulsePipsAngle, stagePos, isActive ? sequence->getCurrentPulseInStage() : -1, EACH, sequence->getPulseAnticipation());
    }

    if (curStage.gateMode == HELD || isEditingGateModeOfThisStage || arePulsePipsAnimating) {
      drawPulses(curStage, stageDrawInfo.angle - 90 + stageDrawInfo.pulsePipsAngle, stagePos, isActive ? sequence->getCurrentPulseInStage() : -1, HELD, sequence->getPulseAnticipation());
    }

    if (curStage.gateMode == FIRST || isEditingGateModeOfThisStage || arePulsePipsAnimating) {
      drawPulses(curStage, stageDrawInfo.angle - 180 + stageDrawInfo.pulsePipsAngle, stagePos, isActive ? sequence->getCurrentPulseInStage() : -1, FIRST, sequence->getPulseAnticipation());
    }

    if (curStage.gateMode == NONE || isEditingGateModeOfThisStage || arePulsePipsAnimating) {
      drawPulses(curStage, stageDrawInfo.angle - 270 + stageDrawInfo.pulsePipsAngle, stagePos, isActive ? sequence->getCurrentPulseInStage() : -1, NONE, sequence->getPulseAnticipation());
    }

    if (curStage.isSkipped) { drawStageStrikethrough(stagePos); }

    // Selected indicator
    if (curStage.isSelected) {
      Vec2 selectionPipPos = Vec2::fromPolar(stageDrawInfo.radius + 16, stageDrawInfo.angle) + screenCenter;

      screen.fillCircle(
        selectionPipPos.x, selectionPipPos.y, // Position,
        2,
        COLOUR_USER
      );
    }
  }

  // Cursor
  screen.drawArc(
    SCREEN_HALF_WIDTH, SCREEN_HALF_HEIGHT, // Position
    SCREEN_HALF_WIDTH + 2, SCREEN_HALF_WIDTH - 5, // Radius, Inner Radius
    fwrap(cursorAngle + 180 - 4, 0, 360), fwrap(cursorAngle + 180 + 4, 0, 360), // Arc start & end 
    COLOUR_USER, COLOUR_BG, // Colour, AA Colour
    false // Smoothing
  );
  
  // if (gpio_get(4)) {
  //   // BPM
  //   screen.setTextColor(COLOUR_INACTIVE);
  //   screen.drawNumber(sequence->getBpm(), screenCenter.x, screenCenter.y - 6, 2);
  //   screen.drawString("bpm", screenCenter.x, screenCenter.y + 6, 2);
  // }

  // Gate
  drawStageOutput(sequence->getOutput(), sequence->getGate() ? COLOUR_ACTIVE : COLOUR_SKIPPED, screenCenter);
  
  // FPS
  screen.setTextColor(COLOUR_INACTIVE);
  screen.drawNumber(fps/100, screenCenter.x, 12, 2);
  screen.drawString("fps", screenCenter.x, 24, 2);

  // Debug
  for (int i = 0; i < activeButtons.size(); i++) {
    Vec2 pos = Vec2::fromPolar(SCREEN_HALF_WIDTH - 25, 290 - i * 10) + screenCenter;

    screen.setTextColor(COLOUR_INACTIVE);
    screen.drawString(toString(activeButtons[i]->_command), pos.x, pos.y, 2);
  }

  tft.pushImageDMA(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, screenPtr);
}

void drawPulsePips(Stage& stage, float angle, Vec2 pos, int8_t currentPulseInStage, GateMode gateMode) {
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
      bool isActive = isPulseActive(pulseIndex, gateMode) && !stage.isSkipped;
      if (pulseIndex <= currentPulseInStage) {
        colour = (isActive) ? COLOUR_ACTIVE :  COLOUR_INACTIVE;
      } else {
        colour = (isActive) ? COLOUR_INACTIVE :  COLOUR_SKIPPED;
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

void drawHeldPulses(Stage& stage, float angle, Vec2 pos, int8_t currentPulseInStage, float pulseAnticipation) {
  bool isStageActive = currentPulseInStage > 0;

  // 0 to pulseCount
  float progress;
  if (isStageActive) {
    progress = currentPulseInStage + pulseAnticipation;
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

void drawPulses(Stage& stage, float angle, Vec2 pos, int8_t currentPulseInStage, GateMode gateMode, float pulseAnticipation) {
  if (gateMode == HELD) {
    drawHeldPulses(stage, angle, pos, currentPulseInStage, pulseAnticipation);
  } else {
    drawPulsePips(stage, angle, pos, currentPulseInStage, gateMode);
  }
}

void drawStageOutput(float output, uint16_t colour, Vec2 pos) {
  int octave = (int)output;
  float semitone = output - octave;

  if (semitone < 0.5) {
    screen.fillSmoothCircle(
      pos.x, pos.y,
      semitone * 8 + 2,
      colour, COLOUR_BG
    );
  } else {
    screen.drawArc(
      pos.x, pos.y, // Position
      semitone * 8 + 2, (semitone - 0.5) * 2 * 11, // Radius, Inner Radius
      0, 359, // Arc start & end 
      colour, COLOUR_BG, // Colour, AA Colour
      true // Smoothing
    );
  }

  for (int i = 0; i < octave; i++) {
    float extraSize = max(0, semitone * 8 - 8 + 3);

    screen.drawCircle(
      pos.x, pos.y,
      10 + 2 * i + extraSize,
      colour
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
