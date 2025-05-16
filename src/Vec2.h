#pragma once

#include "Utils.h"

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

    Vec2 operator-(const Vec2& rhs) {
      return Vec2(x - rhs.x, y - rhs.y);
    }

    Vec2 operator/(const float rhs) {
      return Vec2(x / rhs, y / rhs);
    }

    Vec2 operator*(const float rhs) {
      return Vec2(x * rhs, y * rhs);
    }

    float length() {
      return sqrtf(x * x + y * y);
    }

    Vec2 normal() {
      return Vec2(-y, x);
    }

    Vec2 normalized() {
      return *this / length();
    }

    // 0º is up
    static Vec2 fromPolar(float radius, float degrees) {
      float tau = (degrees - 90) * DEG_TO_TAU;
      float x = radius * cosf(tau);
      float y = radius * sinf(tau);

      return Vec2(x, y);
    }
};