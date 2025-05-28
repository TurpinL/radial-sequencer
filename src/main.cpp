#include <Arduino.h>
#include "Utils.h"
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

Multiplexer switchMult = Multiplexer(D13, D12, D11, D10);
Multiplexer switchLedMult = Multiplexer(D8, D7, D6, D5);

float lastHighlightedStageIndicatorAngle = 0;
bool lastSelectToggleState = true;
bool lastGateValue = false;

void processInput();

void setup() {
  sequence = undoRedoManager.getSequence();
  // Initialize MIDI, and listen to all MIDI channels
  // This will also call usb_midi's begin()
  MIDI.begin(MIDI_CHANNEL_OMNI);

  Serial.begin(115200);

  initScreen();

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

  updateAnimations(
    sequence,
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
    sequence,
    interactionManager, 
    activeButtons
  );
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

void processInput() {
  endlessPot.update();
  updateButtons();

  UserInputState userInputState = UserInputState(&endlessPot, activeButtons);

  auto newBpm = (analogRead(A2) / 1024.f) * 100 + 60;
  if (abs(sequence->getBpm() - newBpm) > 2) {
    sequence->setBpm(newBpm);
  }
  
  interactionManager.processInput(undoRedoManager, userInputState);
}

