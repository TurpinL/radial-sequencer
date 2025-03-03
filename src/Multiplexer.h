#pragma once

#include <Arduino.h>

class Multiplexer {
    public:
        Multiplexer(uint select0, uint select1, uint select2, uint select3) {
            _select0 = select0;
            _select1 = select1;
            _select2 = select2;
            _select3 = select3;

            pinMode(_select0, OUTPUT);
            pinMode(_select1, OUTPUT);
            pinMode(_select2, OUTPUT);
            pinMode(_select3, OUTPUT);
        }

        void select(uint channel) {
            gpio_put(_select0, channel & 1);
            gpio_put(_select1, channel >> 1 & 1);
            gpio_put(_select2, channel >> 2 & 1);
            gpio_put(_select3, channel >> 3 & 1);
        }

    private:
        uint _select0;
        uint _select1;
        uint _select2;
        uint _select3;
};