// libultra's sinf and cosf, with its polynomial and its steps, so that each
// result has the same bits.
#include "gu.h"

static const double P1 = -0x1.55554bc83656dp-3, P2 = 0x1.110ed3804c2a0p-7;
static const double P3 = -0x1.9f6ffeea56814p-13, P4 = 0x1.5dbdf0e314bfep-19;
static const double RPI = 0x1.45f306dc9c883p-2, PIHI = 0x1.921fb5p+1, PILO = 0x1.110b4611a6263p-25;
static const union { u32 bits; float f; } QNAN = { 0x7F810000 };

// The exponent of x, with the top bit of its fraction.
static s32 exponent(float x)
{
    union { float f; s32 bits; } u = { x };
    return (u.bits >> 22) & 0x1FF;
}

// The sine of dx, which is within pi / 2 of 0.
static double poly(double dx)
{
    double xsq = dx * dx;
    return dx * xsq * ((((P4 * xsq) + P3) * xsq + P2) * xsq + P1) + dx;
}

float ultra_sinf(float x)
{
    s32 xpt = exponent(x);
    if (xpt < 255) {
        return xpt < 230 ? x : (float)poly(x);
    }
    if (xpt < 310) {
        double dx = x;
        double dn = dx * RPI;
        s32 n = (s32)(0 <= dn ? dn + 0.5 : dn - 0.5);
        dn = n;
        dx -= dn * PIHI;
        dx -= dn * PILO;
        float sine = (float)poly(dx);
        return n & 1 ? -sine : sine;
    }
    return x != x ? QNAN.f : 0;
}

float ultra_cosf(float x)
{
    if (exponent(x) < 310) {
        double dx = 0 < x ? x : -x;
        double dn = dx * RPI + 0.5;
        s32 n = (s32)(0 <= dn ? dn + 0.5 : dn - 0.5);
        dn = n;
        dn -= 0.5;
        dx -= dn * PIHI;
        dx -= dn * PILO;
        float sine = (float)poly(dx);
        return n & 1 ? -sine : sine;
    }
    return x != x ? QNAN.f : 0;
}
