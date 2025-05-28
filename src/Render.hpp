#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "Vec2.h"
#include "Sequence.h"
#include "Button.h"
#include "InteractionManager.hpp"

void initScreen();

void updateAnimations(
    Sequence *sequence,
    InteractionManager &interactionManager
);

void render(
    Sequence *sequence,
    InteractionManager &interactionManager,
    const std::vector<Button*> &activeButtons
);

void renderIfDmaIsReady(
    Sequence *sequence, 
    InteractionManager &interactionManager,
    const std::vector<Button*> &activeButtons
);