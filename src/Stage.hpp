#pragma once
#include <pico/types.h>

enum GateMode {
    EACH,
    HELD,
    FIRST,
    NONE
};

inline bool isPulseActive(uint8_t index, GateMode gateMode) {
    if (gateMode == HELD || gateMode == EACH) {
        return true;
    } else if (gateMode == FIRST && index == 0) {
        return true;
    } else {
        return false;
    }
}

class StageDrawInfo {
    public:
        float output = 0;
        float pulseCount = 0;
        float pulsePipsAngle = 0;
        float isSkipped = 0;
        float isSelected = 0;
        float shouldSlideIn = 0;

        float radius = 0;
        float angle = 0;
};

class Stage {
public:
    uint16_t id;
    uint8_t pulseCount = 4;
    GateMode gateMode = EACH;
    bool isSkipped = false;
    bool isSelected = false;
    bool shouldSlideIn = false;
    bool shouldArpeggiate = false;
    uint8_t arpSteps = 5;
    float arpStepWidth = 0.2;

    // TODO: Make arpeggiation undo/redo compatible
    // size_t arppegiationOffset = 0; // Saves the global pulse count when arpeggiation is toggled on

    // Render stuff
    float pulsePipsAngle = 0;

    bool isPulseActive(uint8_t index) {
        if (isSkipped) return false;

        if (gateMode == HELD || gateMode == EACH) {
            return true;
        } else if (gateMode == FIRST && index == 0) {
            return true;
        } else {
            return false;
        }
    }

    float getOutput(uint stagePulseCount) {
        if (shouldArpeggiate) {
            return output + arpStepWidth * (stagePulseCount % arpSteps);
        } else {
            return output;
        }
    }

    float getBaseOutput() {
        return output;
    }

    void setOutput(float newOutput) {
        output = newOutput;
    }

    Stage(uint16_t id) : id(id) {}

private:
    float output = 1;
};