#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "Vec2.h"
#include "Sequence.h"
#include "Button.h"

void initScreen();

void updateAnimations(
    Sequence *sequence,
    uint8_t highlightedStageIndex,
    bool isEditingPosition,
    bool isEditingGateMode,
    bool isEditingPitch,
    float hiddenValue
);

void render(
    Sequence *sequence, 
    float cursorAngle, 
    uint8_t highlightedStageIndex, 
    const std::vector<Button*> &activeButtons,
    bool isEditingGateMode
);

void renderIfDmaIsReady(
    Sequence *sequence, 
    float cursorAngle, 
    uint8_t highlightedStageIndex, 
    const std::vector<Button*> &activeButtons,
    bool isEditingGateMode
);