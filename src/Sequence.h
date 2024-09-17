#pragma once

#define MAX_STAGES 16

#include <vector>

enum GateMode {
    EACH,
    HELD,
    FIRST,
    NONE
};

class Stage {
    public:
        float output = 1;
        uint8_t pulseCount = 4;
        GateMode gateMode = EACH;
        bool isSkipped = false;
        bool isSelected = false;
        bool shouldSlideIn = false;

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
};

class Sequence {
    public:
        Sequence(u_int8_t stageCount) {
            // Clip stageCount to a reasonable range
            stageCount = max(1, stageCount);
            stageCount = min(MAX_STAGES, stageCount);

            for (int i = 0; i < stageCount; i++) {
                addStage();
                _stages.back().pulseCount = (rand() % 8) + 1;
                _stages.back().output = -((rand() % 100) / 50.f) + 1;
                _stages.back().gateMode = (GateMode)(i % 4);
                _stages.back().shouldSlideIn = i % 5 == 4;
            }

            _activeStage = &_stages.front();
            _updateMicrosPerPulse();
            updateNextStageIndex();
        }

        void addStage() {
            _stages.push_back(Stage());
        }

        Stage& getStage(size_t index) {
            return _stages.at(index);
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

                if (isLastPulseOfStage() || _activeStage->isSkipped) {
                    _outputOfLastStage = _output;
                    _activeStage = &_stages.at(_nextStageIndex);
                    _currentPulseInStage = 0;

                    updateNextStageIndex();
                } else {
                    _currentPulseInStage++;
                }
            }

            if (_activeStage->gateMode == HELD) {
                _gate = true;
            } else {
                bool isPulseActive = _activeStage->isPulseActive(_currentPulseInStage);
                bool hasGateLengthElapsed = elapsedMicros - _lastPulseMicros > (_microsPerPulse * _gateLength);

                _gate = isPulseActive && !hasGateLengthElapsed;
            }

            _pulseAnticipation = (elapsedMicros - _lastPulseMicros) / (float)_microsPerPulse;
            _slideProgress = min(_pulseAnticipation, _gateLength) / _gateLength;

            if (isSliding()) {
                _output = lerp(_outputOfLastStage, _activeStage->output, _slideProgress);
            } else {
                _output = _activeStage->output;
            }
        }

        size_t indexOfActiveStage() {
            for (size_t i = 0; i < _stages.size(); i++) {
                if (_activeStage == &_stages.at(i)) {
                    return i;
                }
            }
        }

        size_t getNextStageIndex() {
            return _nextStageIndex;
        }

        Stage& getActiveStage() {
            return *_activeStage;
        }

        float getPulseAnticipation() {
            return _pulseAnticipation;
        }

        bool isLastPulseOfStage() {
            return _currentPulseInStage >= _activeStage->pulseCount - 1;
        }

        bool isSliding() {
            return _activeStage->shouldSlideIn && _currentPulseInStage == 0;
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

        std::vector<Stage*> getSelectedStages() {
            std::vector<Stage*> selectedStages;

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
    private:
        std::vector<Stage> _stages;
        Stage* _activeStage; 
        size_t _nextStageIndex;
        float _output;
        float _outputOfLastStage;
        float _bpm = 120;
        uint8_t _subdivision = 2;
        unsigned long _microsPerPulse;
        unsigned long _lastPulseMicros = 0;
        float _gateLength = 0.75f; // 1 = always on, 0 = never on
        float _pulseAnticipation; // How close are we to the next pulse
        float _slideProgress; // How close are we sliding between notes
        uint8_t _currentPulseInStage = 0; // How many pulses have occurred for the current stage

        bool _gate = false;

        void _updateMicrosPerPulse() {
            _microsPerPulse = 60000000 / _bpm / _subdivision;
        }
};