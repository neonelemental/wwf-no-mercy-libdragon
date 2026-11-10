// libultra's maths that our C gives (src/ultra/). sinf and cosf have other
// names here, as libdragon's C library has its own.

#ifndef GU_H
#define GU_H

#include "ultra.h"

typedef union {
    s32 m[4][4];
    s64 force_structure_alignment;
} Mtx;

void guMtxF2L(float mf[4][4], Mtx *m);
void guMtxXFMF(float mf[4][4], float x, float y, float z, float *ox, float *oy, float *oz);
void guNormalize(float *x, float *y, float *z);
void guScaleF(float mf[4][4], float x, float y, float z);
void guTranslateF(float mf[4][4], float x, float y, float z);

float ultra_sinf(float x);
float ultra_cosf(float x);

#endif
