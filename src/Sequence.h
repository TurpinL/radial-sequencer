#ifndef RAD_SEQ_SEQUENCE
#define RAD_SEQ_SEQUENCE

#include <vector>

class Stage {
    public:
        float voltage = 2;
};

class Sequence {
    public:
        void addStage() {
            _stages.push_back(Stage());
        }

        Stage &getStage(size_t index) {
            return _stages.at(index);
        }

        size_t stagesCount() {
            return _stages.size();
        }
        
    private:
        std::vector<Stage> _stages;     
};

#endif