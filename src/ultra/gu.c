// libultra's routines for float matrices and vectors, as its assembly does them.
#include "gu.h"

// The 16.16 fixed matrix of mf: the high halves in the first 8 words, the low in the rest.
void guMtxF2L(float mf[4][4], Mtx *m)
{
    float *f = &mf[0][0];
    u32 *w = (u32 *)m;
    for (int i = 0; i < 8; i++) {
        s32 a = (s32)(f[2 * i] * 65536.0f);
        s32 b = (s32)(f[2 * i + 1] * 65536.0f);
        w[i] = (a & 0xFFFF0000) | ((u32)b >> 16);
        w[i + 8] = (a << 16) | (b & 0xFFFF);
    }
}

void guMtxXFMF(float mf[4][4], float x, float y, float z, float *ox, float *oy, float *oz)
{
    *ox = mf[0][0] * x + mf[1][0] * y + mf[2][0] * z + mf[3][0];
    *oy = mf[0][1] * x + mf[1][1] * y + mf[2][1] * z + mf[3][1];
    *oz = mf[0][2] * x + mf[1][2] * y + mf[2][2] * z + mf[3][2];
}

void guNormalize(float *x, float *y, float *z)
{
    float a = *x, b = *y, c = *z;
    float m = 1.0f / __builtin_sqrtf(c * c + (a * a + b * b));
    *x = a * m;
    *y = b * m;
    *z = c * m;
}

static void diagonal(float mf[4][4], float x, float y, float z)
{
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            mf[i][j] = 0;
        }
    }
    mf[0][0] = x;
    mf[1][1] = y;
    mf[2][2] = z;
    mf[3][3] = 1;
}

void guScaleF(float mf[4][4], float x, float y, float z)
{
    diagonal(mf, x, y, z);
}

void guTranslateF(float mf[4][4], float x, float y, float z)
{
    diagonal(mf, 1, 1, 1);
    mf[3][0] = x;
    mf[3][1] = y;
    mf[3][2] = z;
}
