#ifndef RAD_SEQ_VEC2
#define RAD_SEQ_VEC2

#include "utils.h"

const float DEG_TO_TAU = 3.14 * 2 / 360.f;

class Vec2 {
  public:
    float x;
    float y;

    Vec2(float x, float y) {
      this->x = x;
      this->y = y;
    }

    Vec2 operator+(const Vec2& rhs) {
      return Vec2(x + rhs.x, y + rhs.y);
    }

    static Vec2 fromPolar(float radius, float degrees) {
      float tau = degrees * DEG_TO_TAU;
      float x = radius * cosf(tau);
      float y = radius * sinf(tau);

      return Vec2(x, y);
    }
};

#endif