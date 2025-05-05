#pragma once

#define MAX_STAGES 16

#include <vector>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
#include <algorithm>

enum GateMode {
    EACH,
    HELD,
    FIRST,
    NONE
};

bool isPulseActive(uint8_t index, GateMode gateMode) {
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
        float pulsePipAngle = 0;
        float isSkipped = 0;
        float isSelected = 0;
        float shouldSlideIn = 0;

        float radius = 0;
        float angle = 0;
};

class Stage {
    public:
        uint16_t id;
        float output = 1;
        uint8_t pulseCount = 4;
        GateMode gateMode = EACH;
        bool isSkipped = false;
        bool isSelected = false;
        bool shouldSlideIn = false;

        // Render stuff
        float pulsePipsAngle = 0;
        float targetPulsePipsAngle = 0;

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

        Stage(uint16_t id) : id(id) {}
};

class Sequence {
    public:
        Sequence(u_int8_t stageCount) {
            _stages.reserve(MAX_STAGES);

            // Clip stageCount to a reasonable range
            stageCount = max(1, stageCount);
            stageCount = min(MAX_STAGES, stageCount);

            for (int i = 0; i < stageCount; i++) {
                addStage();
                _stages.back().pulseCount = (rand() % 2) + 1;
                _stages.back().output = (rand() % 500) / 100.f;//-((rand() % 100) / 50.f) + 1;
                _stages.back().gateMode = EACH;
            }

            _updateMicrosPerPulse();
            updateNextStageIndex();
        }

        Sequence() : Sequence(0) {}

        void addStage() {
            _stages.push_back(Stage(getNewStageId()));
        }

        void swapStages(size_t indexA, size_t indexB) {
            Stage stageA = _stages[indexA];
            Stage stageB = _stages[indexB];
            _stages.erase(_stages.begin() + indexA);
            _stages.insert(_stages.begin() + indexA, stageB);

            _stages.erase(_stages.begin() + indexB);
            _stages.insert(_stages.begin() + indexB, stageA);

            if (_activeStageIndex == indexA) {
                _activeStageIndex = indexB;
            } else if (_activeStageIndex == indexB) {
                _activeStageIndex = indexA;
            }
        }

        void moveStages(std::vector<size_t> stagesToMove, int direction) {
            if (direction != -1 && direction != 1) return;

            std::vector<int> finalPositions(_stages.size()); // A mapping from the _stages vector to the final result
            std::fill(finalPositions.begin(), finalPositions.end(), -1);

            // Place all stagesToMove into the finalPositions vector, incremented/decremented by 1
            // eg.
            // direction = 1
            // stagesToMove =    0     2     4 [0, 2, 4] 
            //                   ^--v  ^--v  ^-> wraps to index 0
            // finalPositions =  4  0 -1  2 -1
            for (size_t index : stagesToMove) {
                size_t newIndex = wrap(index + direction, 0, finalPositions.size());
                finalPositions[newIndex] = index;
            }


            // Fill in the gaps with unmoved elements
            // before: finalPositions =  4  0 -1  2 -1
            // after:  finalPositions =  4  0  3  2  1
            for (int i = 0; i < finalPositions.size(); i++) {
                // -1 indicates a stage hasn't been assigned to this space yet
                if (finalPositions[i] == -1) {
                    // Fill the gap with the nearest index that isn't in "stagesToMove"
                    // Increment/Decrement by "direction"
                    int possibleIndex = i;

                    // Increment/Decrement foo until it doesn't exist in stagesToMove
                    while (std::find(stagesToMove.begin(), stagesToMove.end(), possibleIndex) != stagesToMove.end()) {
                        possibleIndex = wrap(possibleIndex + direction, 0, finalPositions.size());
                    }

                    finalPositions[i] = possibleIndex;
                }
            }

            // Update active stage index
            size_t newActiveStageIndex = std::find(finalPositions.begin(), finalPositions.end(), _activeStageIndex) - finalPositions.begin();

            _activeStageIndex = newActiveStageIndex;
            updateNextStageIndex();

            // Construct a new vector with all stages in their new positions
            std::vector<Stage> result;
            for (int originalIndex : finalPositions) {
                result.push_back(_stages[originalIndex]);
            }

            _stages = result;
        }

        void insertStage(size_t index, Stage stage) {
            if (stageCount() <= MAX_STAGES) {
                _stages.insert(_stages.begin() + index, stage);

                // Update the active stage index if the new stage is inserted before it
                if (index < _activeStageIndex) {
                    _activeStageIndex++;
                }

                updateNextStageIndex();
            }
        }

        void deleteStage(size_t index) {
            if (_stages.size() > 1 && index >= 0 && index < _stages.size()) {
                _stages.erase(_stages.begin() + index);

                // Move the active stage index back if the deletion is before said index
                if (index < _activeStageIndex) {
                    _activeStageIndex--;
                }

                updateNextStageIndex();
            }
        }

        size_t indexOfStage(Stage* stage) {
            for (size_t i = 0; i < _stages.size(); i++) {
                if (stage == &_stages.at(i)) {
                    return i;
                }
            }

            return -1;
        }

        Stage& getStage(size_t index) {
            return _stages.at(index);
        }

        std::vector<Stage>& getStages() {
            return _stages;
        }

        size_t stageCount() {
            return _stages.size();
        }

        void updateNextStageIndex() {
            size_t currentStageIndex = indexOfActiveStage();
            auto potentialNextIndex = currentStageIndex;

            while (true) {
                potentialNextIndex++;
                potentialNextIndex %= _stages.size();

                if (potentialNextIndex == currentStageIndex || !_stages[potentialNextIndex].isSkipped) {
                    _nextStageIndex = potentialNextIndex;
                    return;
                }
            }
        }

        void update(unsigned long elapsedMicros) {
            if (elapsedMicros - _lastPulseMicros >= _microsPerPulse) {
                _lastPulseMicros += _microsPerPulse;

                if (isLastPulseOfStage() || getActiveStage().isSkipped) {
                    _outputOfLastStage = _output;
                    _activeStageIndex = _nextStageIndex;
                    _currentPulseInStage = 0;
                    
                    updateNextStageIndex();
                } else {
                    _currentPulseInStage++;
                }
            }

            _pulseAnticipation = (elapsedMicros - _lastPulseMicros) / (float)_microsPerPulse;
            _slideProgress = min(_pulseAnticipation, _gateLength) / _gateLength;

            // TODO: Understand how this assignment works.
            // I expected it to overwrite the reference, but instead it seems to 
            // Copy the active stage overwriting the initial active stage
            // How do C++ references work...

            if (getActiveStage().gateMode == HELD) {
                // Close the gate 1 16th note before the end of the held note to create some separation from the next stage
                _gate = !isLastPulseOfStage() || _pulseAnticipation < 0.9375; 
            } else {
                bool isPulseActive = getActiveStage().isPulseActive(_currentPulseInStage);
                bool hasGateLengthElapsed = _pulseAnticipation > _gateLength;

                _gate = isPulseActive && !hasGateLengthElapsed;
            }

            if (isSliding()) {
                _output = lerp(_outputOfLastStage, getActiveStage().output, _slideProgress);
            } else {
                _output = getActiveStage().output;
            }
        }

        size_t indexOfActiveStage() {
            return _activeStageIndex;
        }

        size_t getNextStageIndex() {
            return _nextStageIndex;
        }

        Stage& getActiveStage() {
            return _stages[_activeStageIndex];
        }

        float getPulseAnticipation() {
            return _pulseAnticipation;
        }

        bool isLastPulseOfStage() {
            return _currentPulseInStage >= getActiveStage().pulseCount - 1;
        }

        bool isSliding() {
            return getActiveStage().shouldSlideIn && _currentPulseInStage == 0;
        }

        float getSlideProgress() {
            return _slideProgress;
        }

        uint8_t selectedStagesCount() {
            uint8_t selectedStages = 0;

            for (size_t i = 0; i < _stages.size(); i++) {
                if (_stages[i].isSelected) {
                    selectedStages++;
                }
            }

            return selectedStages;
        }

        // Returns a vector of selected stages in index order
        std::vector<Stage*> getSelectedStages() {
            std::vector<Stage*> selectedStages;

            // Note: Stage deletion and insertion depend on this
            // being in index order
            for (auto& stage : _stages) {
                if (stage.isSelected) {
                    selectedStages.push_back(&stage);
                }
            }

            return selectedStages;
        }

        uint8_t getCurrentPulseInStage() {
            return _currentPulseInStage;
        }

        bool getGate() {
            return _gate;
        }

        // -1 to 1
        float getOutput() {
            return _output;
        }

        void setBpm(float bpm) {
            _bpm = bpm;
            _bpm = min(_bpm, 500);
            _bpm = max(_bpm, 10);
            
            _updateMicrosPerPulse();
        }

        float getBpm() {
            return _bpm;
        }

        uint16_t getNewStageId() {
            auto id = _nextId;

            _nextId++;
            _nextId %= MAX_STAGES;

            return id;
        }
    private:
        std::vector<Stage> _stages;
        size_t _activeStageIndex;
        size_t _nextStageIndex = 0;
        float _outputOfLastStage; // Referenced when sliding between stages
        float _bpm = 120;
        uint8_t _subdivision = 4;
        unsigned long _microsPerPulse;
        unsigned long _lastPulseMicros = 0;
        float _gateLength = 0.75f; // 1 = always on, 0 = never on
        float _pulseAnticipation; // How close are we to the next pulse
        float _slideProgress; // How close are we sliding between notes
        uint8_t _currentPulseInStage = 0; // How many pulses have occurred for the current stage
        uint16_t _nextId = 0;
        float _output;
        bool _gate = false;

        void _updateMicrosPerPulse() {
            _microsPerPulse = 60000000 / _bpm / _subdivision;
        }
};