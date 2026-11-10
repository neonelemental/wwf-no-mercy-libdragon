#include "angle.h"

static const double HALF_PI = 0x1.921fb54442d18p+0;

// The arc tangent of x by 8 steps of its continued fraction, from the inverse of x outside -1 to 1.
float game_atanf(float x)
{
    int side = 0;
    if (1.0 < (double)x) {
        side = 1;
        x = 1.0f / x;
    }
    if ((double)x < -1.0) {
        side = -1;
        x = 1.0f / x;
    }
    float k = 8.0f, rest = 0.0f;
    for (int i = 0; i < 8; i++) {
        float over = k * k * x * x;
        float under = k + k + 1.0f + rest;
        k = (float)((double)k - 1.0);
        rest = over / under;
    }
    float angle = x / (rest + 1.0f);
    if (side > 0) {
        return (float)(HALF_PI - (double)angle);
    }
    if (side < 0) {
        return (float)(-HALF_PI - (double)angle);
    }
    return angle;
}

// The angle of the point (x, y), from -pi to pi.
float game_atan2f(float y, float x)
{
    if ((double)y == 0.0) {
        return 0.0 <= (double)x ? 0.0f : 0x1.921fb6p+1f;
    }
    if (0.0 <= (double)y) {
        return (float)(HALF_PI - (double)game_atanf(x / y));
    }
    return (float)-((double)game_atanf(x / y) + HALF_PI);
}
