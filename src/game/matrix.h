#ifndef GAME_MATRIX_H
#define GAME_MATRIX_H

typedef float Matrix[4][4];

// The product of a and b. out may be a or b: each row of a is read before
// its row of out is written, and b is read as the writes go.
inline void mtx_mul(Matrix a, Matrix b, Matrix out)
{
    for (int i = 0; i < 4; i++) {
        float a0 = a[i][0], a1 = a[i][1], a2 = a[i][2], a3 = a[i][3];
        for (int j = 0; j < 4; j++) {
            out[i][j] = a0 * b[0][j] + a1 * b[1][j] + a2 * b[2][j] + a3 * b[3][j];
        }
    }
}

// A rotation by x, then y, then z, each in degrees.
void mtx_rotate(Matrix m, float x, float y, float z);
// A rotation about y, in degrees.
void mtx_rotate_y(Matrix m, float y);

#endif
