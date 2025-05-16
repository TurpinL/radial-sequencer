#include <Arduino.h>
#include <TFT_eSPI.h>
#include "Utils.h"
#include "SineCosinePot.h"
#include "Sequence.h"
#include "Vec2.h"
#include "Button.h"
#include "Multiplexer.h"
#include "ButtonHandlers/GateModeButtonHandler.hpp"
#include "ButtonHandlers/PitchButtonHandler.hpp"
#include <set>
#include <algorithm>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
#include "SelectionState.hpp"
#include "ButtonHandlers/SelectButtonHandler.hpp"

#define SCREEN_WIDTH 240
#define SCREEN_HALF_WIDTH 120
#define SCREEN_HEIGHT 240
#define SCREEN_HALF_HEIGHT 120

const uint16_t COLOUR_BG =        0x0000;
const uint16_t COLOUR_BEAT =      0x8224;
const uint16_t COLOUR_USER =      0x96cd;
const uint16_t COLOUR_ACTIVE =    0xfd4f;
const uint16_t COLOUR_INACTIVE =  0xaa21;
const uint16_t COLOUR_SKIPPED =   0x5180;

// USB MIDI object
Adafruit_USBD_MIDI usb_midi;

// Create a new instance of the Arduino MIDI Library,
// and attach usb_midi as the transport.
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite screen = TFT_eSprite(&tft);
uint16_t* screenPtr;

const Vec2 screenCenter = Vec2(SCREEN_HALF_WIDTH, SCREEN_HALF_HEIGHT);

UndoRedoManager undoRedoManager;
Sequence *sequence;
StageDrawInfo renderableStages[MAX_STAGES];

SineCosinePot endlessPot = SineCosinePot(0, 1);
float cursorAngle = 0;

uint8_t gate1Pin = D14;
uint8_t gate2Pin = D15;
uint8_t pitchPin = D22;
uint8_t cvPin = D21;
uint8_t switchMultPin = D9;
uint8_t ledMultPin = D4;

Button pitchBtn = Button(PITCH);
Button deleteBtn = Button(DELETE);
Button gatemodeBtn = Button(GATEMODE);
Button selectBtn = Button(SELECT);
Button pulsesBtn = Button(PULSES);
Button moveBtn = Button(MOVE);
Button undoBtn = Button(UNDO);
Button redoBtn = Button(REDO);
std::vector<Button*> buttons = {
  nullptr, nullptr, nullptr, nullptr, 
  nullptr, nullptr, &selectBtn, &pitchBtn,
  nullptr,  nullptr, &moveBtn, nullptr,
  &redoBtn, &pulsesBtn, &gatemodeBtn, &undoBtn
};
std::vector<Button*> activeButtons; // Buttons that are held, or fallingEdge == true.
GateModeButtonHandler gateModeButtonHandler;
SelectButtonHandler selectButtonHandler;
PitchButtonHandler pitchButtonHandler;

Multiplexer switchMult = Multiplexer(D13, D12, D11, D10);
Multiplexer switchLedMult = Multiplexer(D8, D7, D6, D5);

float lastHighlightedStageIndicatorAngle = 0;
uint8_t highlightedStageIndex = 0;
bool lastSelectToggleState = true;
bool isEditingPosition = false;

int32_t lastFrameMillis = 0;
float fps = 0;

int32_t lastAnimationTickMillis = 0;

bool lastGateValue = false;

void render();
void processInput();
void updateAnimations();

void setup() {
  sequence = undoRedoManager.getSequence();
  // Initialize MIDI, and listen to all MIDI channels
  // This will also call usb_midi's begin()
  MIDI.begin(MIDI_CHANNEL_OMNI);

  Serial.begin(115200);

  tft.init();
  tft.initDMA();
  tft.setRotation(1);
  tft.fillScreen(COLOUR_BG);
  screenPtr = (uint16_t*)screen.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);
  screen.setTextDatum(MC_DATUM);
  tft.startWrite(); // TFT chip select held low permanently

  // Gate LED
  pinMode(gate1Pin, OUTPUT);
  pinMode(gate2Pin, OUTPUT);

  pinMode(switchMultPin, INPUT_PULLUP);
  pinMode(ledMultPin, OUTPUT);
}

uint8_t currentNote = 0;

void loop() {
  processInput();
  sequence->update(micros());

  if (sequence->getGate() != lastGateValue) {
    if (sequence->getGate()) {
      currentNote = 60 + round(sequence->getOutput() * 24);
      MIDI.sendNoteOn(currentNote, 255, 1);
    } else {
      MIDI.sendNoteOff(currentNote, 0, 1);
    }
    lastGateValue = sequence->getGate();
  }

  updateAnimations();

  // Gate LED
  digitalWrite(gate1Pin, sequence->getGate());
  // digitalWrite(gate2Pin, sequence->getGate());
  // Pitch LED
  analogWrite(pitchPin, powf((sequence->getOutput() + 1) / 2, 2) * 255);
  // analogWrite(cvPin, powf((sequence->getOutput() + 1) / 2, 2) * 255);

  analogWrite(ledMultPin, 30);
  
  if (!tft.dmaBusy()) {
    render();

    int32_t deltaMillis = millis() - lastFrameMillis;
    fps = 100000 / deltaMillis;

    lastFrameMillis = millis();
  }
}

uint nextLedIndex = 0;
void loop1() {
  if (buttons[nextLedIndex] != nullptr) {
    switchLedMult.select(nextLedIndex);
  } else {
    switchLedMult.select(0);
  }

  nextLedIndex += 1;
  nextLedIndex %= 16;

  delayMicroseconds(100);
}

uint nextButtonIndex = 0;

// Updates the state of one of the buttons, and progresses
// the mux to the next button.
// We update buttons one by one so we don't have to wait for the 
// mux to settle.
void updateButtons() {
  Button *buttonToUpdate = buttons[nextButtonIndex];

  // TODO: Rethink 
  for (Button *x: buttons) {
    if ( x != nullptr ) {
      x->stabilizeState();
    }
  }

  if (buttonToUpdate != nullptr) {
    buttonToUpdate->update(!gpio_get(D9));
  }

  nextButtonIndex += 1;
  nextButtonIndex %= 16;
  switchMult.select(nextButtonIndex);

  activeButtons.clear();
    
  // Add all held buttons to the list. 
  // Or buttons that just stopped being held. 
  for (Button *x: buttons) {
    if ( x != nullptr && x->held() || x->fallingEdge() ) { 
      activeButtons.push_back(x);
    }
  }

  // Sort them by timestamp
  std::sort(activeButtons.begin(), activeButtons.end(), 
    [](Button *a, Button *b) { 
      return a->getLastActivation() < b->getLastActivation(); 
    }
  );
}

float hiddenValue = 0;
float mutationCanary = 0;

void processInput() {
  endlessPot.update();
  updateButtons();

  auto newBpm = (analogRead(A2) / 1024.f) * 100 + 60;
  if (abs(sequence->getBpm() - newBpm) > 2) {
    sequence->setBpm(newBpm);
  }
  
  // Hidden value used for... things?
  bool shouldResetHiddenValue = true;

  // Update highlighted stage
  float degreesPerStage = 360 / (float)sequence->stageCount();

  if (!isEditingPosition) {
    highlightedStageIndex = (int)roundf(cursorAngle / degreesPerStage) % sequence->stageCount();
  }

  SelectionState selectionState = SelectionState(*sequence, highlightedStageIndex); 
  
  Button *baseButton = activeButtons.size() > 0 ? activeButtons[0] : nullptr;
  Command baseCommand = baseButton != nullptr ? baseButton->_command : NOTHING;

  Button *modifierButton = activeButtons.size() > 1 ? activeButtons[1] : nullptr;
  Command modifier = modifierButton != nullptr ? modifierButton->_command : NOTHING;

  bool isShiftHeld = false;
  bool shouldSupressCursorRotation = false;
  isEditingPosition = false;

  if (baseCommand == UNDO) {
    if (baseButton->risingEdge()) {
      undoRedoManager.undo();
    } 
  } else if (baseCommand == REDO) {
    if (baseButton->risingEdge()) {
      undoRedoManager.redo();
    } 
  } else if (baseCommand == PITCH) {
    pitchButtonHandler.handle(*baseButton, endlessPot, undoRedoManager, selectionState);
    shouldSupressCursorRotation = pitchButtonHandler.shouldSuppressCursorRotation();
  } else if (baseCommand == SKIP) {
    if (baseButton->risingEdge()) {
      // Toggle isSkipped
      for (auto stage : selectionState.getAffectedStages()) {
        stage->isSkipped = !stage->isSkipped;
      }
  
      sequence->updateNextStageIndex();

      undoRedoManager.saveUndoRedoSnapshot();
    }
  } else if (baseCommand == PULSES) {
    shouldSupressCursorRotation = true;
    shouldResetHiddenValue = false;

    hiddenValue += endlessPot.getAngleDelta();

    if (abs(hiddenValue) > 20) {
      for (auto stage : selectionState.getAffectedStages()) {
        stage->pulseCount = coerceInRange(stage->pulseCount + (hiddenValue > 0 ? 1 : -1), 1, 8);
      }
      mutationCanary += (hiddenValue > 0 ? 1 : -1);

      hiddenValue = 0;
    }

    if (baseButton->fallingEdge() && mutationCanary != 0) {
      undoRedoManager.saveUndoRedoSnapshot();
      mutationCanary = 0;
    }
  } else if (baseCommand == GATEMODE) {
    gateModeButtonHandler.handle(*baseButton, endlessPot, undoRedoManager, selectionState);
    shouldSupressCursorRotation = gateModeButtonHandler.shouldSuppressCursorRotation();
  } else if (baseCommand == SELECT) {
    selectButtonHandler.handle(*baseButton, modifierButton, endlessPot, undoRedoManager, selectionState);
    shouldSupressCursorRotation = selectButtonHandler.shouldSuppressCursorRotation();
  } else if (baseCommand == SLIDE) {
    if (baseButton->risingEdge()) {
      for (auto stage : selectionState.getAffectedStages()) {
        stage->shouldSlideIn = !stage->shouldSlideIn;
      }

      undoRedoManager.saveUndoRedoSnapshot();
    }
  } else if (baseCommand == CLONE) {
    if (baseButton->risingEdge()) {
      // Iterate from the end to the beginning because deleting 
      // stages moves around the data in the stages vector causing 
      // the pointers in affected stages to point to the wrong stage. 
      // Iterating in reverse is a workaround to that issue
      auto rit = selectionState.getAffectedStages().rbegin();
      while (rit != selectionState.getAffectedStages().rend()) {
        // Copy everything about the stage, except deselect it
        Stage newStage = **rit;
        newStage.isSelected = false;
        newStage.id = sequence->getNewStageId();

        sequence->insertStage(sequence->indexOfStage(*rit) + 1, newStage);
        
        ++rit;
      }

      undoRedoManager.saveUndoRedoSnapshot();
    }
  } else if (baseCommand == DELETE) {
    if (baseButton->risingEdge()) {
      // Iterate from the end to the beginning because deleting 
      // stages moves around the data in the stages vector causing 
      // the pointers in affected stages to point to the wrong stage. 
      // Iterating in reverse is a workaround to that issue
      auto rit = selectionState.getAffectedStages().rbegin();
      while (rit != selectionState.getAffectedStages().rend()) {
        sequence->deleteStage(sequence->indexOfStage(*rit));
        ++rit;
      }

      undoRedoManager.saveUndoRedoSnapshot();
    }
  } else if (baseCommand == MOVE) {
    shouldResetHiddenValue = false;
    isEditingPosition = true;

    hiddenValue += endlessPot.getAngleDelta();
    float degreesPerStage = 360 / (float)sequence->stageCount();

    // When we move the stages far enough rearrange the sequence
    if (abs(hiddenValue) > degreesPerStage) {
      int direction = (hiddenValue > 0) ? 1 : -1;
      mutationCanary += direction;
      
      std::vector<size_t> indexesOfStagesToMove;
      for (Stage* stage : selectionState.getAffectedStages()) {
        size_t stageIndex = sequence->indexOfStage(stage);
        indexesOfStagesToMove.push_back(stageIndex);
      }

      sequence->moveStages(indexesOfStagesToMove, direction);

      // The highlightedStage will be moved by this action, so update the index
      highlightedStageIndex = wrap((int)highlightedStageIndex + direction, 0, sequence->stageCount());

      hiddenValue = 0;
    }

    if (baseButton->fallingEdge() && mutationCanary != 0) {
      undoRedoManager.saveUndoRedoSnapshot();
      mutationCanary = 0;
    }
  }

  if (!shouldSupressCursorRotation) {
    // Cusor
    cursorAngle += endlessPot.getAngleDelta();
    cursorAngle = fwrap(cursorAngle, 0, 360); 
  }

  if (shouldResetHiddenValue) {
    hiddenValue = 0;
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
      screen.fillRect(
        pipPos.x - 1.5, pipPos.y - 1.5, // Position
        3, 3,
        colour
      );
    }
  }
}

void drawHeldPulses(Stage& stage, float angle, Vec2 pos) {
  bool isStageActive = &stage == &sequence->getActiveStage();

  // 0 to pulseCount
  float progress;
  if (isStageActive) {
    progress = sequence->getCurrentPulseInStage() + sequence->getPulseAnticipation();
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

void drawPulses(Stage& stage, float angle, Vec2 pos, int8_t currentPulseInStage, GateMode gateMode) {
  if (gateMode == HELD) {
    drawHeldPulses(stage, angle, pos);
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

float degreesPerStage() {
  return 360 / (float)sequence->stageCount();
}

void updateAnimations() {
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

      float targetAngle = i * degreesPerStage();
      if (isEditingPosition && isHighlighted) {
        stageDrawInfo.angle = targetAngle + hiddenValue;
      }

      bool isEditingGateModeOfThisStage = gateModeButtonHandler.isEditingGateMode() && (i == highlightedStageIndex || stage.isSelected);
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
      
      if (pitchBtn.held()) {
        stageDrawInfo.output = stage.output;
      } else {
        stageDrawInfo.output = lerp(stageDrawInfo.output, stage.output, 0.1);
      }
    }
  }
}

////////////////////////////////////////////////////
///                   Render                    ////
////////////////////////////////////////////////////
void render() {
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

    bool isEditingGateModeOfThisStage = gateModeButtonHandler.isEditingGateMode() && (i == highlightedStageIndex || curStage.isSelected);
    bool arePulsePipsAnimating = abs(degBetweenAngles(stageDrawInfo.pulsePipsAngle, curStage.pulsePipsAngle)) > 10;

    if (curStage.gateMode == EACH || isEditingGateModeOfThisStage || arePulsePipsAnimating) {
      drawPulses(curStage, stageDrawInfo.angle + stageDrawInfo.pulsePipsAngle, stagePos, isActive ? sequence->getCurrentPulseInStage() : -1, EACH);
    }

    if (curStage.gateMode == HELD || isEditingGateModeOfThisStage || arePulsePipsAnimating) {
      drawPulses(curStage, stageDrawInfo.angle - 90 + stageDrawInfo.pulsePipsAngle, stagePos, isActive ? sequence->getCurrentPulseInStage() : -1, HELD);
    }

    if (curStage.gateMode == FIRST || isEditingGateModeOfThisStage || arePulsePipsAnimating) {
      drawPulses(curStage, stageDrawInfo.angle - 180 + stageDrawInfo.pulsePipsAngle, stagePos, isActive ? sequence->getCurrentPulseInStage() : -1, FIRST);
    }

    if (curStage.gateMode == NONE || isEditingGateModeOfThisStage || arePulsePipsAnimating) {
      drawPulses(curStage, stageDrawInfo.angle - 270 + stageDrawInfo.pulsePipsAngle, stagePos, isActive ? sequence->getCurrentPulseInStage() : -1, NONE);
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