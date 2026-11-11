#include "oval.h"

#include "bearing.h"
#include "gu.h"

// The four joints that give the oval of a wrestler on a wall.
extern s8 D_8014DAB0_ovl_e[4];

static float length_of(Bearing *one, Bearing *two)
{
    return one->length[1] + two->length[1];
}

void fighter_oval(Fighter *fighter, Oval *oval)
{
    Bearing b[8];
    Bearing *one, *two;
    float low[3], high[3];

    if (fighter->flags & 0x400) {
        float p[4][3];
        for (int i = 0; i < 4; i++) {
            float *point = fighter->point[D_8014DAB0_ovl_e[i]];
            float x = point[0], y = point[1], z = point[2];
            switch (fighter->wall) {
            case 0:
            case 2:
                p[i][0] = x;
                p[i][1] = z;
                p[i][2] = y;
                break;
            case 1:
            case 3:
                p[i][0] = y;
                p[i][1] = x;
                p[i][2] = z;
                break;
            default:
                p[i][0] = x;
                p[i][1] = y;
                p[i][2] = z;
                break;
            }
        }
        bearing_between(p[0], p[3], &b[0]);
        bearing_between(p[1], p[2], &b[1]);
        one = &b[0];
        two = &b[1];
        low[0] = p[0][0];
        low[2] = p[0][2];
        high[0] = p[1][0];
        high[2] = p[1][2];
    } else {
        bearing_between(fighter->point[15], fighter->point[5], &b[0]);
        bearing_between(fighter->point[8], fighter->point[5], &b[1]);
        bearing_between(fighter->point[15], fighter->point[1], &b[2]);
        bearing_between(fighter->point[8], fighter->point[1], &b[3]);
        bearing_between(fighter->point[17], fighter->point[15], &b[4]);
        bearing_between(fighter->point[9], fighter->point[8], &b[5]);
        bearing_between(fighter->point[17], fighter->point[5], &b[6]);
        bearing_between(fighter->point[9], fighter->point[5], &b[7]);
        s32 from = 15, to = 8;
        one = &b[2];
        two = &b[3];
        if (length_of(&b[2], &b[3]) < length_of(&b[0], &b[1])) {
            one = &b[0];
            two = &b[1];
        }
        if (length_of(one, two) < length_of(&b[4], &b[5])) {
            one = &b[4];
            two = &b[5];
            from = 17;
            to = 9;
        }
        if (length_of(one, two) < length_of(&b[6], &b[7])) {
            one = &b[6];
            two = &b[7];
            from = 17;
            to = 9;
        }
        low[0] = fighter->point[from][0];
        low[2] = fighter->point[from][2];
        high[0] = fighter->point[to][0];
        high[2] = fighter->point[to][2];
    }

    s32 angle = (s32)two->angle[1];
    oval->angle = turn_sum(turn_between((s32)one->angle[1], angle) / 2, angle);
    oval->half[0] = 36.0f;
    oval->half[2] = (float)((double)length_of(one, two) * 0.25);
    oval->half[1] = 0.0f;
    if ((double)oval->half[2] < 29.7) {
        oval->half[2] = 29.7f;
    }
    // From the middle of the two joints, half the length on.
    float middle[3], along[3];
    middle[0] = (float)((double)(high[0] - low[0]) * 0.5 + (double)low[0]);
    middle[1] = 0.0f;
    middle[2] = (float)((double)(high[2] - low[2]) * 0.5 + (double)low[2]);
    along[0] = 0.0f;
    along[1] = 0.0f;
    along[2] = oval->half[2];
    way_turned(middle, along, oval->angle, oval->centre);
    oval->centre[1] = 0.0f;
}

void way_turned(const float *place, const float *way, s32 angle, float *out)
{
    float x = 0.0f, z = 0.0f;
    if (place) {
        x = place[0];
        z = place[2];
    }
    float c = ultra_cosf(turn_radians(angle));
    float s = ultra_sinf(turn_radians(angle));
    float wx = way[0], wz = way[2];
    out[0] = x + (wx * c + wz * s);
    out[2] = z + (wz * c - wx * s);
}

float oval_reach(Oval *oval, const float *to)
{
    if ((double)oval->half[0] == 0.0) {
        oval->half[0] = 0.1f;
    }
    if ((double)oval->half[2] == 0.0) {
        oval->half[2] = 0.1f;
    }
    // The way to the point, as the oval lies.
    float way[3], own[3];
    point_way(oval->centre, to, way);
    way_turned(NULL, way, turn_between(0, oval->angle), own);
    if ((double)own[0] == 0.0) {
        return oval->half[2];
    }
    if ((double)own[2] == 0.0) {
        return oval->half[0];
    }
    float slope = own[2] / own[0];
    float across = oval->half[0] * oval->half[0];
    float along = oval->half[2] * oval->half[2];
    float x = along / (along / across + slope * slope);
    float z = (float)((1.0 - (double)(x / across)) * (double)along);
    return __builtin_sqrtf(x + z);
}
