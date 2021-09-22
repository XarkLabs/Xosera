/*
 * vim: set et ts=4 sw=4
 *------------------------------------------------------------
 *  __ __
 * |  |  |___ ___ ___ ___ ___
 * |-   -| . |_ -| -_|  _| .'|
 * |__|__|___|___|___|_| |__,|
 *
 * Xark's Open Source Enhanced Retro Adapter
 *
 * - "Not as clumsy or random as a GPU, an embedded retro
 *    adapter for a more civilized age."
 *
 * ------------------------------------------------------------
 * Copyright (c) 2021 Xark & Contributors
 * MIT License
 *
 * Draw cube
 * ------------------------------------------------------------
 */

#include "cube.h"

#include "pr_api.h"

#include <math.h>
#include <string.h>

#define _FRACTION_MASK(scale) (0xffffffff >> (32 - scale))
#define _WHOLE_MASK(scale)    (0xffffffff ^ FRACTION_MASK(scale))

#define _FLOAT_TO_FIXED(x, scale) ((x) * (float)(1 << scale))
#define _FIXED_TO_FLOAT(x, scale) ((float)(x) / (double)(1 << scale))
#define _INT_TO_FIXED(x, scale)   ((x) << scale)
#define _FIXED_TO_INT(x, scale)   ((x) >> scale)
#define _FRACTION_PART(x, scale)  ((x)&FRACTION_MASK(scale))
#define _WHOLE_PART(x, scale)     ((x)&WHOLE_MASK(scale))

//#define _MUL(x, y, scale) (((long long)(x) * (long long)(y)) >> scale)
#define _MUL(x, y, scale) (((x) >> (scale / 2)) * ((y) >> (scale / 2)))

//#define _DIV(x, y, scale) (((long long)(x) << scale) / (y))
#define _DIV(x, y, scale) (((x) << (scale / 2)) / (y) << (scale / 2))

#define SCALE 16

#define FX(x)     _FLOAT_TO_FIXED(x, SCALE)
#define FXI(x)    _FIXED_TO_INT(x, SCALE)
#define MUL(x, y) _MUL(x, y, SCALE)
#define DIV(x, y) _DIV(x, y, SCALE)

#define SIN(x) FX(sinf(_FIXED_TO_FLOAT(x, SCALE)))
#define COS(x) FX(cosf(_FIXED_TO_FLOAT(x, SCALE)))
#define TAN(x) FX(tanf(_FIXED_TO_FLOAT(x, SCALE)))

typedef int fx32;

typedef struct
{
    fx32 x, y, z;
} vec3d;

typedef struct
{
    vec3d p[3];
} triangle;

typedef struct
{
    triangle * tris;
} mesh;

typedef struct
{
    fx32 m[4][4];
} mat4x4;

int screenWidth  = 320;
int screenHeight = 200;

void MultiplyMatrixVector(vec3d * i, vec3d * o, mat4x4 * m)
{
    o->x   = MUL(i->x, m->m[0][0]) + MUL(i->y, m->m[1][0]) + MUL(i->z, m->m[2][0]) + m->m[3][0];
    o->y   = MUL(i->x, m->m[0][1]) + MUL(i->y, m->m[1][1]) + MUL(i->z, m->m[2][1]) + m->m[3][1];
    o->z   = MUL(i->x, m->m[0][2]) + MUL(i->y, m->m[1][2]) + MUL(i->z, m->m[2][2]) + m->m[3][2];
    fx32 w = MUL(i->x, m->m[0][3]) + MUL(i->y, m->m[1][3]) + MUL(i->z, m->m[2][3]) + m->m[3][3];

    if (w != 0)
    {
        o->x = DIV(o->x, w);
        o->y = DIV(o->y, w);
        o->z = DIV(o->z, w);
    }
}

triangle cube_triangles[] = {
    // South
    {FX(0.0f), FX(0.0f), FX(0.0f), FX(0.0f), FX(1.0f), FX(0.0f), FX(1.0f), FX(1.0f), FX(0.0f)},
    {FX(0.0f), FX(0.0f), FX(0.0f), FX(1.0f), FX(1.0f), FX(0.0f), FX(1.0f), FX(0.0f), FX(0.0f)},

    // East
    {FX(1.0f), FX(0.0f), FX(0.0f), FX(1.0f), FX(1.0f), FX(0.0f), FX(1.0f), FX(1.0f), FX(1.0f)},
    {FX(1.0f), FX(0.0f), FX(0.0f), FX(1.0f), FX(1.0f), FX(1.0f), FX(1.0f), FX(0.0f), FX(1.0f)},

    // North
    {FX(1.0f), FX(0.0f), FX(1.0f), FX(1.0f), FX(1.0f), FX(1.0f), FX(0.0f), FX(1.0f), FX(1.0f)},
    {FX(1.0f), FX(0.0f), FX(1.0f), FX(0.0f), FX(1.0f), FX(1.0f), FX(0.0f), FX(0.0f), FX(1.0f)},

    // West
    {FX(0.0f), FX(0.0f), FX(1.0f), FX(0.0f), FX(1.0f), FX(1.0f), FX(0.0f), FX(1.0f), FX(0.0f)},
    {FX(0.0f), FX(0.0f), FX(1.0f), FX(0.0f), FX(1.0f), FX(0.0f), FX(0.0f), FX(0.0f), FX(0.0f)},

    // Top
    {FX(0.0f), FX(1.0f), FX(0.0f), FX(0.0f), FX(1.0f), FX(1.0f), FX(1.0f), FX(1.0f), FX(1.0f)},
    {FX(0.0f), FX(1.0f), FX(0.0f), FX(1.0f), FX(1.0f), FX(1.0f), FX(1.0f), FX(1.0f), FX(0.0f)},

    // Bottom
    {FX(1.0f), FX(0.0f), FX(1.0f), FX(0.0f), FX(0.0f), FX(1.0f), FX(0.0f), FX(0.0f), FX(0.0f)},
    {FX(1.0f), FX(0.0f), FX(1.0f), FX(0.0f), FX(0.0f), FX(0.0f), FX(1.0f), FX(0.0f), FX(0.0f)}};


void draw_cube(float theta)
{
    mesh meshCube;
    meshCube.tris = &cube_triangles[0];

    // Projection matrix
    fx32  near        = FX(0.1f);
    fx32  far         = FX(1000.0f);
    float fov         = 90.0f;
    fx32  aspectRatio = FX((float)screenHeight / (float)screenWidth);
    fx32  fovRad      = FX(1.0f / tanf(fov * 0.5f / 180.0f * 3.14159f));

    mat4x4 matProj;
    memset(&matProj, 0, sizeof(matProj));
    matProj.m[0][0] = MUL(aspectRatio, fovRad);
    matProj.m[1][1] = fovRad;
    matProj.m[2][2] = DIV(far, (far - near));
    matProj.m[3][2] = DIV(MUL(-far, near), (far - near));
    matProj.m[2][3] = FX(1.0f);
    matProj.m[3][3] = FX(0.0f);

    mat4x4 matRotZ, matRotX;
    memset(&matRotZ, 0, sizeof(matRotZ));
    memset(&matRotX, 0, sizeof(matRotX));

    // Rotation Z
    matRotZ.m[0][0] = FX(cosf(theta));
    matRotZ.m[0][1] = FX(sinf(theta));
    matRotZ.m[1][0] = FX(-sinf(theta));
    matRotZ.m[1][1] = FX(cosf(theta));
    matRotZ.m[2][2] = FX(1.0f);
    matRotZ.m[3][3] = FX(1.0f);

    // Rotation X
    matRotX.m[0][0] = FX(1.0f);
    matRotX.m[1][1] = FX(cosf(theta * 0.5f));
    matRotX.m[1][2] = FX(sinf(theta * 0.5f));
    matRotX.m[2][1] = FX(-sinf(theta * 0.5f));
    matRotX.m[2][2] = FX(cosf(theta * 0.5f));
    matRotX.m[3][3] = FX(1.0f);

    // Draw triangles
    size_t nb_triangles = sizeof(cube_triangles) / sizeof(triangle);
    for (size_t i = 0; i < nb_triangles; ++i)
    {
        triangle * tri = &meshCube.tris[i];
        triangle   triProjected, triTranslated, triRotatedZ, triRotatedZX;

        MultiplyMatrixVector(&tri->p[0], &triRotatedZ.p[0], &matRotZ);
        MultiplyMatrixVector(&tri->p[1], &triRotatedZ.p[1], &matRotZ);
        MultiplyMatrixVector(&tri->p[2], &triRotatedZ.p[2], &matRotZ);

        MultiplyMatrixVector(&triRotatedZ.p[0], &triRotatedZX.p[0], &matRotX);
        MultiplyMatrixVector(&triRotatedZ.p[1], &triRotatedZX.p[1], &matRotX);
        MultiplyMatrixVector(&triRotatedZ.p[2], &triRotatedZX.p[2], &matRotX);

        triTranslated        = triRotatedZX;
        triTranslated.p[0].z = triRotatedZX.p[0].z + FX(3.0f);
        triTranslated.p[1].z = triRotatedZX.p[1].z + FX(3.0f);
        triTranslated.p[2].z = triRotatedZX.p[2].z + FX(3.0f);

        MultiplyMatrixVector(&triTranslated.p[0], &triProjected.p[0], &matProj);
        MultiplyMatrixVector(&triTranslated.p[1], &triProjected.p[1], &matProj);
        MultiplyMatrixVector(&triTranslated.p[2], &triProjected.p[2], &matProj);

        // Scale into view
        triProjected.p[0].x += FX(1.0f);
        triProjected.p[0].y += FX(1.0f);
        triProjected.p[1].x += FX(1.0f);
        triProjected.p[1].y += FX(1.0f);
        triProjected.p[2].x += FX(1.0f);
        triProjected.p[2].y += FX(1.0f);

        fx32 w              = FX(0.5f * (float)screenWidth);
        fx32 h              = FX(0.5f * (float)screenHeight);
        triProjected.p[0].x = MUL(triProjected.p[0].x, w);
        triProjected.p[0].y = MUL(triProjected.p[0].y, h);
        triProjected.p[1].x = MUL(triProjected.p[1].x, w);
        triProjected.p[1].y = MUL(triProjected.p[1].y, h);
        triProjected.p[2].x = MUL(triProjected.p[2].x, w);
        triProjected.p[2].y = MUL(triProjected.p[2].y, h);

        pr_draw_triangle(FXI(triProjected.p[0].x),
                                FXI(triProjected.p[0].y),
                                FXI(triProjected.p[1].x),
                                FXI(triProjected.p[1].y),
                                FXI(triProjected.p[2].x),
                                FXI(triProjected.p[2].y),
                                0xf);
    }
}