#include "matrix.h"

#include "gu.h"

extern inline void mtx_mul(Matrix a, Matrix b, Matrix out);

// The factor from degrees to radians, in the game's data.
extern float D_80047EC0;

void mtx_rotate(Matrix m, float x, float y, float z)
{
    x *= D_80047EC0;
    y *= D_80047EC0;
    z *= D_80047EC0;
    float sx = ultra_sinf(x), cx = ultra_cosf(x);
    float sy = ultra_sinf(y), cy = ultra_cosf(y);
    float sz = ultra_sinf(z), cz = ultra_cosf(z);
    m[0][0] = cz * cy;
    m[0][1] = sz * cx + cz * sy * sx;
    m[0][2] = sz * sx - cz * sy * cx;
    m[0][3] = 0;
    m[1][0] = -sz * cy;
    m[1][1] = cz * cx - sz * sy * sx;
    m[1][2] = cz * sx + sz * sy * cx;
    m[1][3] = 0;
    m[2][0] = sy;
    m[2][1] = -cy * sx;
    m[2][2] = cy * cx;
    m[2][3] = 0;
    m[3][0] = 0;
    m[3][1] = 0;
    m[3][2] = 0;
    m[3][3] = 1;
}

void mtx_rotate_y(Matrix m, float y)
{
    y *= D_80047EC0;
    float s = ultra_sinf(y), c = ultra_cosf(y);
    m[0][0] = c;
    m[0][1] = 0;
    m[0][2] = -s;
    m[0][3] = 0;
    m[1][0] = 0;
    m[1][1] = 1;
    m[1][2] = 0;
    m[1][3] = 0;
    m[2][0] = s;
    m[2][1] = 0;
    m[2][2] = c;
    m[2][3] = 0;
    m[3][0] = 0;
    m[3][1] = 0;
    m[3][2] = 0;
    m[3][3] = 1;
}
