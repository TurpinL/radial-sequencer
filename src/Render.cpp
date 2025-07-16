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
TFT_eSprite screens[2] = {TFT_eSprite(&tft), TFT_eSprite(&tft)};
TFT_eSprite* curScreen = &screens[0];
uint16_t* screenPtrs[2];
uint8_t curScreenIndex = 0;

StageDrawInfo renderableStagesById[MAX_STAGES]; // TODO: move to somewhere Sequence specific
int32_t lastFrameMillis = 0;
float fps = 0;
int32_t lastAnimationTickMillis = 0;
float msPerFrame = 0;

void drawStageOutput(float output, uint16_t colour, Vec2 pos);
void drawPulses(Stage& stage, float angle, Vec2 pos, int8_t currentPulseInStage, GateMode gateMode, float pulseAnticipation);
void drawHeldPulses(Stage& stage, float angle, Vec2 pos, float pulseAnticipation);
void drawPulsePips(Stage& stage, float angle, Vec2 pos, int8_t currentPulseInStage, GateMode gateMode);
void drawStageStrikethrough(Vec2 pos);
void drawPiano(Vec2 pos);
void drawPianoPip(Vec2 pos, float highlightedPitch);

void initScreen() {
  tft.init();
  tft.initDMA();
  tft.setRotation(1);
  tft.fillScreen(COLOUR_BG);
  screenPtrs[0] = (uint16_t*)screens[0].createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);
  screenPtrs[1] = (uint16_t*)screens[1].createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);
  screens[0].setTextDatum(MC_DATUM);
  screens[1].setTextDatum(MC_DATUM);
  tft.startWrite(); // TFT chip select held low permanently
}

float degreesPerStage(float stageCount) {
  return 360 / stageCount;
}

void updateAnimations(
  UndoRedoManager &undoRedoManager,
  InteractionManager &interactionManager
) {
  Sequence *sequence = undoRedoManager.getSequence();

  if ((millis() - lastAnimationTickMillis) > 16 || lastAnimationTickMillis == 0) {
    lastAnimationTickMillis += 16;

    uint defaultStagePositionRadius = 48 + 3 * sequence->stageCount();

    for (size_t i = 0; i < sequence->stageCount(); i++) {
      Stage& stage = sequence->getStage(i);
      StageDrawInfo& stageDrawInfo = renderableStagesById[stage.id];
      bool isHighlighted = interactionManager._highlightedStageIndex == i || stage.isSelected;

      // Update position
      float targetRadius = defaultStagePositionRadius;
      targetRadius += (isHighlighted ? 8 : 0);
      if (interactionManager._isEditingPosition) {
        targetRadius += (isHighlighted ? 4 : -8);
      }

      float targetAngle = i * degreesPerStage(sequence->stageCount());
      if (interactionManager._isEditingPosition && isHighlighted) {
        stageDrawInfo.angle = targetAngle + interactionManager._hiddenValue;
      }

      bool isEditingGateModeOfThisStage = interactionManager.gateModeButtonHandler.isEditingGateMode() && (i == interactionManager._highlightedStageIndex || stage.isSelected);
      if (isEditingGateModeOfThisStage) {
        stageDrawInfo.pulsePipsAngle = stage.pulsePipsAngle;
      } else {
        stage.pulsePipsAngle = stage.gateMode * 90;
        stageDrawInfo.pulsePipsAngle = stageDrawInfo.pulsePipsAngle + degBetweenAngles(stageDrawInfo.pulsePipsAngle, stage.pulsePipsAngle) * 0.1;
      }

      if (interactionManager._isEditingPosition && isHighlighted) {
        stageDrawInfo.angle = targetAngle + interactionManager._hiddenValue;
      } else {
        stageDrawInfo.angle = stageDrawInfo.angle + degBetweenAngles(stageDrawInfo.angle, targetAngle) * 0.1;
      }

      stageDrawInfo.radius = lerp(stageDrawInfo.radius, targetRadius, 0.1);

      uint curStagePulseTally = undoRedoManager.stagePulseTallyById[stage.id];
      if (interactionManager.pitchButtonHandler.isEditingPitch()) {
        stageDrawInfo.output = stage.getOutput(curStagePulseTally);
      } else {
        stageDrawInfo.output = lerp(stageDrawInfo.output, stage.getOutput(curStagePulseTally), 0.1);
      }
    }
  }
}

void swapScreenBuffer() {
  curScreenIndex = !curScreenIndex;
  curScreen = &screens[curScreenIndex];
}

void renderIfDmaIsReady(
  UndoRedoManager &undoRedoManager,
  InteractionManager &interactionManager,
  const std::vector<Button*> &activeButtons
) {
  if (!tft.dmaBusy()) {
    int32_t frameStartMillis = millis();

    tft.pushImageDMA(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, screenPtrs[curScreenIndex]);
    swapScreenBuffer();
    render(
      undoRedoManager, 
      interactionManager,
      activeButtons
    );

    msPerFrame = msPerFrame * 0.9 + (millis() - frameStartMillis) * 0.1;

    int32_t deltaMillis = millis() - lastFrameMillis;
    fps = fps * 0.9 + (100000 / deltaMillis * 0.1);

    lastFrameMillis = millis();
  }
}

void render(
    UndoRedoManager &undoRedoManager,
    InteractionManager &interactionManager,
    const std::vector<Button*> &activeButtons
) {
  Sequence *sequence = undoRedoManager.getSequence();

  curScreen->fillSprite(COLOUR_BG);

  float targetDegreesPerStage = 360 / (float)sequence->stageCount();

  // Beat Indicator
  if (true) {
    size_t nextStageIndex = sequence->getNextStageIndex();
    Stage &activeStage = sequence->getActiveStage();
    StageDrawInfo& activeStageDrawInfo = renderableStagesById[activeStage.id];
    Stage &nextStage = sequence->getStage(nextStageIndex);
    StageDrawInfo& nextStageDrawInfo = renderableStagesById[nextStage.id];
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

    curScreen->fillCircle(pos.x, pos.y, radius, COLOUR_BEAT);
  }

  // Stages
  for (size_t i = 0; i < sequence->stageCount(); i++) {
    Stage& curStage = sequence->getStage(i);
    StageDrawInfo& stageDrawInfo = renderableStagesById[curStage.id];

    bool isActive = sequence->indexOfActiveStage() == i;
    bool isHighlighted = interactionManager._highlightedStageIndex == i || curStage.isSelected;

    Vec2 stagePos = Vec2::fromPolar(stageDrawInfo.radius, stageDrawInfo.angle) + screenCenter;

    uint16_t colour;
    if (isActive) {
      colour = COLOUR_ACTIVE;
    } else if (isHighlighted) {
      colour = COLOUR_USER; 
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

      curScreen->drawArc(
        screenCenter.x, screenCenter.y, // Position
        stageDrawInfo.radius + 1, stageDrawInfo.radius - 1, // Radius, Inner Radius
        startAngle, endAngle, // Arc start & end 
        COLOUR_SKIPPED, COLOUR_BG, // Colour, AA Colour
        false // Smoothing
      );

      if (isActive && sequence->isSliding()) {
        curScreen->drawArc(
          screenCenter.x, screenCenter.y, // Position
          stageDrawInfo.radius + 1, stageDrawInfo.radius - 1, // Radius, Inner Radius
          startAngle, wrapDeg(startAngle - degToStartAngle * sequence->getPulseAnticipation()), // Arc start & end 
          COLOUR_ACTIVE, COLOUR_BG, // Colour, AA Colour
          false // Smoothing
        );
      }
    }

    drawStageOutput(stageDrawInfo.output, colour, stagePos);

    bool isEditingGateModeOfThisStage = interactionManager.gateModeButtonHandler.isEditingGateMode() && (i == interactionManager._highlightedStageIndex || curStage.isSelected);
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

      curScreen->fillCircle(
        selectionPipPos.x, selectionPipPos.y, // Position,
        2,
        COLOUR_USER
      );
    }
  }

  // Cursor
  curScreen->drawArc(
    SCREEN_HALF_WIDTH, SCREEN_HALF_HEIGHT, // Position
    SCREEN_HALF_WIDTH + 2, SCREEN_HALF_WIDTH - 5, // Radius, Inner Radius
    fwrap(interactionManager._cursorAngle + 180 - 4, 0, 360), fwrap(interactionManager._cursorAngle + 180 + 4, 0, 360), // Arc start & end 
    COLOUR_USER, COLOUR_BG, // Colour, AA Colour
    false // Smoothing
  );
  
  // if (gpio_get(4)) {
  //   // BPM
  //   curScreen->setTextColor(COLOUR_INACTIVE);
  //   curScreen->drawNumber(sequence->getBpm(), screenCenter.x, screenCenter.y - 6, 2);
  //   curScreen->drawString("bpm", screenCenter.x, screenCenter.y + 6, 2);
  // }

  if (interactionManager.pitchButtonHandler.isEditingPitch()) {
    drawPiano(screenCenter);
    
    for (size_t i = 0; i < sequence->stageCount(); i++) {
      Stage& curStage = sequence->getStage(i);
      bool isHighlighted = interactionManager._highlightedStageIndex == i || curStage.isSelected;

      if (isHighlighted) {
        drawPianoPip(screenCenter, curStage.getBaseOutput() * 12);
      }
    }
  }
  
  // FPS
  curScreen->setTextColor(COLOUR_INACTIVE);
  curScreen->drawNumber(fps/100, screenCenter.x, 12, 2);
  curScreen->drawString("fps", screenCenter.x, 24, 2);

  // ms/frame
  curScreen->setTextColor(COLOUR_INACTIVE);
  curScreen->drawNumber(msPerFrame, screenCenter.x, SCREEN_HEIGHT - 12, 2);
  curScreen->drawString("ms/frame", screenCenter.x, SCREEN_HEIGHT - 24, 2);

  // Debug
  for (int i = 0; i < activeButtons.size(); i++) {
    Vec2 pos = Vec2::fromPolar(SCREEN_HALF_WIDTH - 25, 290 - i * 10) + screenCenter;

    curScreen->setTextColor(COLOUR_INACTIVE);
    curScreen->drawString(toString(activeButtons[i]->_command), pos.x, pos.y, 2);
  }
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
      curScreen->fillRect(
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

    curScreen->drawArc(
      pos.x, pos.y, // Position
      rowRadius + 2, rowRadius - 1, // Radius, Inner Radius
      startAngle, endAngle, // Arc start & end 
      (stage.isSkipped) ? COLOUR_SKIPPED : COLOUR_INACTIVE, COLOUR_BG, // Colour, AA Colour
      false // Smoothing
    );

    if (progress > row * 4) { 
      float rowProgress = min(1, (progress - row * 4) / pulsesInRow);

      curScreen->drawArc(
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
    curScreen->fillSmoothCircle(
      pos.x, pos.y,
      semitone * 8 + 2,
      colour, COLOUR_BG
    );
  } else {
    curScreen->drawArc(
      pos.x, pos.y, // Position
      semitone * 8 + 2, (semitone - 0.5) * 2 * 11, // Radius, Inner Radius
      0, 359, // Arc start & end 
      colour, COLOUR_BG, // Colour, AA Colour
      true // Smoothing
    );
  }

  for (int i = 0; i < octave; i++) {
    float extraSize = max(0, semitone * 8 - 8 + 3);

    curScreen->drawCircle(
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

  curScreen->fillTriangle(
    cornerB1.x, cornerB1.y,
    cornerB2.x, cornerB2.y,
    cornerB4.x, cornerB4.y,
    COLOUR_BG
  );

  curScreen->fillTriangle(
    cornerB4.x, cornerB4.y,
    cornerB2.x, cornerB2.y,
    cornerB3.x, cornerB3.y,
    COLOUR_BG
  );

  curScreen->fillTriangle(
    corner1.x, corner1.y,
    corner2.x, corner2.y,
    corner4.x, corner4.y,
    COLOUR_SKIPPED
  );

  curScreen->fillTriangle(
    corner4.x, corner4.y,
    corner2.x, corner2.y,
    corner3.x, corner3.y,
    COLOUR_SKIPPED
  );
}

Vec2 unscaledPianoPositions[] = {
  Vec2(-6,  1), // C
  Vec2(-5, -1), // C#
  Vec2(-4,  1), // D
  Vec2(-3, -1), // D#
  Vec2(-2,  1), // E
  Vec2(0,   1), // F
  Vec2(1,  -1), // F#
  Vec2(2,   1), // G
  Vec2(3,  -1), // G#
  Vec2(4,   1), // A
  Vec2(5,  -1), // A#
  Vec2(6,   1)  // B
};

int pianoPipSpacing = 5;
int pianoPipSize = 4;

void drawPiano(Vec2 pos) {
  for (Vec2 unscaledPos : unscaledPianoPositions) {
    Vec2 finalPos = unscaledPos * pianoPipSpacing + pos;

    curScreen->drawSmoothCircle(
      finalPos.x, finalPos.y,
      pianoPipSize,
      COLOUR_INACTIVE,
      COLOUR_BG
    );
  }
}

void drawPianoPip(Vec2 pos, float highlightedPitch) {
  float foo = fwrap(highlightedPitch, 0, 12);
  int a = floor(foo);
  int b = (int)ceil(foo) % 12;
  float remainder = foo - a;

  if (a == 11) {
    Vec2 startExtension = Vec2(-8, 1);
    Vec2 endExtension = Vec2(8, 1);

    Vec2 userPos1 = lerp(unscaledPianoPositions[11], endExtension, remainder) * pianoPipSpacing + pos;
    Vec2 userPos2 = lerp(startExtension, unscaledPianoPositions[0], remainder) * pianoPipSpacing + pos;

    curScreen->drawSpot(
      userPos1.x, userPos1.y,
      pianoPipSize - 1,
      lerpColour(COLOUR_USER, COLOUR_BG, remainder),
      COLOUR_BG
    );

    curScreen->drawSpot(
      userPos2.x, userPos2.y,
      pianoPipSize - 1,
      lerpColour(COLOUR_USER, COLOUR_BG, 1 - remainder),
      COLOUR_BG
    );
  } else {
    Vec2 userPos = lerp(unscaledPianoPositions[a], unscaledPianoPositions[b], remainder) * pianoPipSpacing + pos;
    curScreen->drawSpot(
      userPos.x, userPos.y,
      pianoPipSize - 1,
      COLOUR_USER,
      COLOUR_BG
    );
  }
}