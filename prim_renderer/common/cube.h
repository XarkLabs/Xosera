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

#ifndef CUBE_H
#define CUBE_H

#include <stdbool.h>

#define _FRACTION_MASK(scale) (0xffffffff >> (32 - scale))
#define _WHOLE_MASK(scale)    (0xffffffff ^ FRACTION_MASK(scale))

#define _FLOAT_TO_FIXED(x, scale) ((x) * (float)(1 << scale))
#define _FIXED_TO_FLOAT(x, scale) ((float)(x) / (double)(1 << scale))
#define _INT_TO_FIXED(x, scale)   ((x) << scale)
#define _FIXED_TO_INT(x, scale)   ((x) >> scale)
#define _FRACTION_PART(x, scale)  ((x)&FRACTION_MASK(scale))
#define _WHOLE_PART(x, scale)     ((x)&WHOLE_MASK(scale))

//#define _MUL(x, y, scale) (((long long)(x) * (long long)(y)) >> scale)
#define _MUL(x, y, scale) ((short int)((x) >> (scale / 2)) * (short int)((y) >> (scale / 2)))

//#define _DIV(x, y, scale) (((long long)(x) << scale) / (y))
#define _DIV(x, y, scale) (((x) << (scale / 2)) / (short int)((y) >> (scale / 2)))

#define SCALE 12

#define FX(x)     ((fx32)_FLOAT_TO_FIXED(x, SCALE))
#define INT(x)    ((int)_FIXED_TO_INT(x, SCALE))
#define MUL(x, y) _MUL(x, y, SCALE)
#define DIV(x, y) _DIV(x, y, SCALE)

#define SIN(x)  FX(sinf(_FIXED_TO_FLOAT(x, SCALE)))
#define COS(x)  FX(cosf(_FIXED_TO_FLOAT(x, SCALE)))
#define TAN(x)  FX(tanf(_FIXED_TO_FLOAT(x, SCALE)))
#define SQRT(x) FX(sqrtf(_FIXED_TO_FLOAT(x, SCALE)))

typedef int fx32;

typedef struct
{
    fx32 x, y, z;
} vec3d;

typedef struct
{
    vec3d p[3];
    vec3d col;
} triangle;

typedef struct
{
    triangle * tris;
} mesh;

typedef struct
{
    fx32 m[4][4];
} mat4x4;

void multiply_matrix_vector(vec3d * i, vec3d * o, mat4x4 * m);
void get_projection_matrix(mat4x4 * mat_proj);
void get_rotation_z_matrix(float theta, mat4x4 * mat_rot_z);
void get_rotation_x_matrix(float theta, mat4x4 * mat_rot_x);
void draw_cube(mat4x4 * mat_projection, mat4x4 * mat_rot_z, mat4x4 * mat_rot_x, bool is_lighting_ena);

#endif