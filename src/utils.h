#pragma once

#include <math.h>

inline float fwrap(float x, float min, float max) {
    if (max == min) return min;
    if (min > max) return fwrap(x, max, min);
    if (min < 0) return fwrap(x - min, min - min, max - min) + min;

    return (x >= 0 ? min : max) + fmodf(x, max - min);
}

inline int wrap(int x, int min, int max) {
    if (max == min) return min;
    if (min > max) return wrap(x, max, min);
    if (min < 0) return wrap(x - min, min - min, max - min) + min;

    return (x >= 0 ? min : max) + (x % (max - min));
}

// wrap a float value between 0 and 360
inline float wrapDeg(float deg) {
    return (deg >= 0 ? 0 : 360) + fmodf(deg, 360 - 0);
}

inline float coerceInRange(float x, float minBound, float maxBound) {
    return std::min(maxBound, std::max(minBound, x));
}

inline float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

inline float degBetweenAngles(float a, float b) {
  float angA = fwrap(b - a, -360, 360);
  float angB = fwrap(360 + b - a, -360, 360);

  return (abs(angA) < abs(angB)) ? angA : angB;
}

// Returns a random number between 0 and 1
inline float randf() {
    return rand() / (float)RAND_MAX;
}

const inline float degsPerPixel[] = {360.f, 45.f, 22.5f, 15.f, 11.25f, 9.f, 7.5f, 6.429f, 5.625f, 5.f, 4.5f, 4.091f, 3.75f, 3.462f, 3.214f, 3.f, 2.813f, 2.647f, 2.5f, 2.368f, 2.25f, 2.143f, 2.045f, 1.957f, 1.875f, 1.8f, 1.731f, 1.667f, 1.607f, 1.552f, 1.5f, 1.452f, 1.406f, 1.364f, 1.324f, 1.286f, 1.25f, 1.216f, 1.184f, 1.154f, 1.125f, 1.098f, 1.071f, 1.047f, 1.023f, 1.f, 0.978f, 0.957f, 0.938f, 0.918f, 0.9f, 0.882f, 0.865f, 0.849f, 0.833f, 0.818f, 0.804f, 0.789f, 0.776f, 0.763f, 0.75f, 0.738f, 0.726f, 0.714f, 0.703f, 0.692f, 0.682f, 0.672f, 0.662f, 0.652f, 0.643f, 0.634f, 0.625f, 0.616f, 0.608f, 0.6f, 0.592f, 0.584f, 0.577f, 0.57f, 0.563f, 0.556f, 0.549f, 0.542f, 0.536f, 0.529f, 0.523f, 0.523f, 0.517f, 0.506f, 0.5f, 0.495f, 0.489f, 0.484f, 0.479f, 0.479f, 0.474f, 0.464f, 0.459f, 0.455f, 0.45f, 0.446f, 0.446f, 0.441f, 0.433f, 0.437f, 0.425f, 0.421f, 0.425f, 0.417f, 0.409f, 0.405f, 0.402f, 0.402f, 0.395f, 0.391f, 0.388f, 0.385f, 0.385f, 0.378f, 0.375f, 0.375f, 0.369f, 0.366f, 0.366f, 0.366f, 0.36f, 0.357f, 0.354f, 0.349f, 0.346f, 0.346f, 0.341f, 0.346f, 0.336f, 0.336f, 0.331f, 0.331f, 0.328f, 0.328f, 0.321f, 0.319f, 0.321f, 0.317f, 0.313f, 0.31f, 0.308f, 0.306f, 0.306f, 0.302f, 0.3f, 0.298f, 0.298f, 0.299f, 0.292f, 0.294f, 0.29f, 0.288f, 0.288f, 0.283f, 0.285f, 0.283f, 0.28f, 0.276f, 0.274f, 0.274f, 0.271f, 0.274f, 0.268f, 0.266f, 0.266f, 0.265f, 0.263f, 0.26f, 0.259f, 0.259f, 0.257f, 0.254f, 0.253f, 0.253f, 0.251f, 0.254f, 0.25f, 0.247f, 0.245f, 0.245f, 0.243f, 0.241f, 0.239f, 0.239f, 0.237f, 0.237f, 0.237f, 0.233f, 0.232f, 0.233f, 0.232f, 0.228f, 0.231f, 0.226f, 0.227f, 0.227f, 0.223f, 0.225f, 0.222f, 0.221f, 0.222f, 0.221f, 0.217f, 0.217f, 0.216f, 0.215f, 0.213f, 0.212f, 0.214f, 0.21f, 0.209f, 0.211f, 0.209f, 0.207f, 0.205f, 0.204f, 0.205f, 0.202f, 0.201f, 0.201f, 0.2f, 0.201f, 0.2f, 0.197f, 0.196f, 0.197f, 0.196f, 0.193f, 0.195f, 0.195f, 0.192f, 0.191f, 0.19f, 0.19f, 0.188f, 0.189f, 0.192f, 0.185f, 0.188f, 0.184f, 0.183f, 0.184f, 0.185f, 0.183f, 0.182f, 0.179f, 0.181f, 0.181f, 0.181f, 0.176f};