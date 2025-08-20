#include <Arduino.h>
#include "utils.h"
#include "SineCosinePot.h"
#include "Sequence.h"
#include "Vec2.h"
#include "Button.h"
#include "Multiplexer.h"
#include "ButtonHandlers/GateModeButtonHandler.hpp"
#include "ButtonHandlers/PitchButtonHandler.hpp"
#include "ButtonHandlers/SelectButtonHandler.hpp"
#include "ButtonHandlers/IButtonHandler.hpp"
#include <set>
#include <algorithm>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
#include "SelectionState.hpp"
#include "UserInputState.hpp"
#include "Render.hpp"
#include "InteractionManager.hpp"

// USB MIDI object
Adafruit_USBD_MIDI usb_midi;

// Create a new instance of the Arduino MIDI Library,
// and attach usb_midi as the transport.
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

Sequence *sequence;
UndoRedoManager undoRedoManager;
InteractionManager interactionManager;

SineCosinePot endlessPot = SineCosinePot(0, 1);

uint8_t gate1Pin = D14;
uint8_t gate2Pin = D15;
uint8_t pitchPin = D22;
uint8_t cvPin = D21;
uint8_t switchMultPin = D9;
uint8_t ledMultPin = D4;

Button buttons[BUTTON_COUNT] = {
  Button(), Button(), Button(), Button(), 
  Button(), Button(), Button(), Button(),
  Button(), Button(), Button(), Button(),
  Button(), Button(), Button(), Button()
};

Command defaultButtonMapping[BUTTON_COUNT] = {
  DELETE, NOTHING, CLONE, NOTHING, 
  QUANTIZER, NOTHING, SELECT, PITCH,
  NOTHING,  ARP, MOVE, RANDOMIZE,
  REDO, LENGTH, MUTE, UNDO
};

Multiplexer switchMux = Multiplexer(D13, D12, D11, D10);
Multiplexer switchLedMux = Multiplexer(D8, D7, D6, D5);

float lastHighlightedStageIndicatorAngle = 0;
bool lastSelectToggleState = true;
bool lastGateValue = false;

void processInput();

void setup() {
  sequence = undoRedoManager.getSequence();
  MIDI.begin(MIDI_CHANNEL_OMNI);
  Serial.begin(115200);
  initScreen();

  // Initialize GPIO
  pinMode(gate1Pin, OUTPUT);
  pinMode(gate2Pin, OUTPUT);
  pinMode(switchMultPin, INPUT_PULLUP);
  pinMode(ledMultPin, OUTPUT);
}

uint8_t currentNote = 0;

void loop() {
  updateAnimations(
    undoRedoManager,
    interactionManager
  );
  
  // Gate LED
  digitalWrite(gate1Pin, sequence->getGate());
  // digitalWrite(gate2Pin, sequence->getGate());
  // Pitch LED
  analogWrite(pitchPin, powf((sequence->getOutput() + 1) / 2, 2) * 255);
  // analogWrite(cvPin, powf((sequence->getOutput() + 1) / 2, 2) * 255);

  analogWrite(ledMultPin, 30);
  
  renderIfDmaIsReady(
    undoRedoManager,
    interactionManager
  );
}

void loop1() {
  processInput();
  sequence->update(micros());

  if (sequence->getGate() != lastGateValue) {
    if (sequence->getGate()) {
      currentNote = sequence->getMidiNote();
      MIDI.sendNoteOn(currentNote, 255, 1);
    } else {
      MIDI.sendNoteOff(currentNote, 0, 1);
    }

    lastGateValue = sequence->getGate();
  }
}

int8_t activeButtonIndexes[2] = {-1, -1};

// Updates the state of one of the buttons, and progresses
// the mux to the next button.
// We update buttons one by one so we don't have to wait for the 
// mux to settle.
void updateButtons() {
  for (int i = 0; i < BUTTON_COUNT; i++) {
    if (i == switchMux.getChannel()) {
      //
      buttons[i].update(!gpio_get(D9));
    } else {
      // 
      buttons[i].stabilizeState();
    }
  }

  switchMux.select((switchMux.getChannel() + 1) % BUTTON_COUNT);

  // Find the primary command: active button with the earliest activation time
  activeButtonIndexes[0] = -1;

  for (int i = 0; i < BUTTON_COUNT; i++) {
    Button &curButton = buttons[i];
    bool isHeldOrFalling = curButton.held() || curButton.fallingEdge();
    unsigned long lastActivation = curButton.getLastActivation();

    if (
      isHeldOrFalling 
      && (
        activeButtonIndexes[0] == -1
        || lastActivation < buttons[activeButtonIndexes[0]].getLastActivation()
      )
    ) { 
      activeButtonIndexes[0] = i;
    }
  }

  // Find the modifier command: active button with the second earliest activation time
  activeButtonIndexes[1] = -1;

  for (int i = 0; i < BUTTON_COUNT; i++) {
    Button &curButton = buttons[i];
    bool isHeldOrFalling = curButton.held() || curButton.fallingEdge();
    unsigned long lastActivation = curButton.getLastActivation();

    if (
      isHeldOrFalling
      && i != activeButtonIndexes[0]
      && (
        activeButtonIndexes[1] == -1
        || lastActivation < buttons[activeButtonIndexes[1]].getLastActivation()
      )
    ) { 
      activeButtonIndexes[1] = i;
    }
  }
}

float lastBpmPotState = 0;
int32_t lastUpdateBpmMillis = 0;

void processInput() {
  endlessPot.update();
  updateButtons();

  UserInputState userInputState = UserInputState(&endlessPot, activeButtonIndexes, buttons, defaultButtonMapping);

  // Limit jittering by slowing the rate we update the BPM
  if (millis() - lastUpdateBpmMillis > 0) {
    lastUpdateBpmMillis = millis();

    int newBpmPotState = lerp(lastBpmPotState, analogRead(A2), 0.1);
    lastBpmPotState = newBpmPotState;

    auto newBpm = (newBpmPotState / 1024.f) * 100 + 60;
    if (abs(sequence->getBpm() - newBpm) > 2) {
      sequence->setBpm(newBpm);
    }
  }
  
  interactionManager.processInput(undoRedoManager, userInputState);
}

