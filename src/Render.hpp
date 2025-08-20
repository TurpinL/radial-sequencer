#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "Vec2.h"
#include "Sequence.h"
#include "UndoRedoManager.hpp"
#include "Button.h"
#include "InteractionManager.hpp"

void initScreen();

void updateAnimations(
    UndoRedoManager &undoRedoManager,
    InteractionManager &interactionManager
);

void render(
    UndoRedoManager &undoRedoManager,
    InteractionManager &interactionManager
);

void renderIfDmaIsReady(
    UndoRedoManager &undoRedoManager, 
    InteractionManager &interactionManager
);