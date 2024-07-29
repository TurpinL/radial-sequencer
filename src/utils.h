#ifndef RAD_SEQ_UTILS
#define RAD_SEQ_UTILS

#include <math.h>

inline float fwrap(float x, float min, float max) {
    if (max == min) return min;
    if (min > max) return fwrap(x, max, min);
    if (min < 0) return fwrap(x - min, min - min, max - min) + min;

    return (x >= 0 ? min : max) + fmodf(x, max - min);
}

inline float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

float degBetweenAngles(float a, float b) {
  float angA = fwrap(b - a, -360, 360);
  float angB = fwrap(360 + b - a, -360, 360);

  return (abs(angA) < abs(angB)) ? angA : angB;
}

const char* indexToNote[] = {"C", "C", "D", "D", "E", "F", "F", "G", "G", "A", "A", "B"};

#endif