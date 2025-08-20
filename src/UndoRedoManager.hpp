#pragma once

#include "Sequence.h"
#include "utils.h"

const uint8_t UNDO_REDO_SIZE = 24;

class UndoRedoManager {
    public:
        UndoRedoManager() {
            history[0] = sequence;
        }

        Sequence* getSequence() {
            return &sequence;
        }

        void saveUndoRedoSnapshot() {
            // Write the current sequence to the next slot in the ring buffer
            curPosInHistory = wrap(curPosInHistory + 1, 0, UNDO_REDO_SIZE);
            history[curPosInHistory] = sequence;
        
            indexOfNewestSnapshot = curPosInHistory;
        
            // If the ring buffer has caught up to it's tail, update the location of the tail
            if (indexOfNewestSnapshot == indexOfOldestSnapshot) {
                indexOfOldestSnapshot = wrap(indexOfOldestSnapshot + 1, 0, UNDO_REDO_SIZE);
            }
        }
        
        void undo() {
            if (curPosInHistory == indexOfOldestSnapshot) return; // Can't go further back
        
            curPosInHistory = wrap(curPosInHistory - 1, 0, UNDO_REDO_SIZE);
            sequence = history[curPosInHistory];
        }
        
        void redo() {
            if (curPosInHistory == indexOfNewestSnapshot) return; // Can't go further forwards
        
            curPosInHistory = wrap(curPosInHistory + 1, 0, UNDO_REDO_SIZE);
            sequence = history[curPosInHistory];
        }

        StageDrawInfo stageDrawInfoById[MAX_STAGES];
        bool isInQuantizerConfig = false;
        uint stagePulseTallyById[MAX_STAGES]; // Tracks how many times each stage has pulsed. Mostly for consistent arpeggiation purposes
    private:
        Sequence sequence = Sequence(8, stagePulseTallyById);
        Sequence history[UNDO_REDO_SIZE];
        uint8_t curPosInHistory = 0;
        uint8_t indexOfOldestSnapshot = 0;
        uint8_t indexOfNewestSnapshot = 0;
};