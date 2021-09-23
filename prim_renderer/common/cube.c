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
#define SQRT(x) FX(sqrtf(_FIXED_TO_FLOAT(x, SCALE)))

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

int screen_width  = 320;
int screen_height = 200;

void multiply_matrix_vector(vec3d * i, vec3d * o, mat4x4 * m)
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
    mesh mesh_cube;
    mesh_cube.tris = &cube_triangles[0];

    // projection matrix
    fx32  near        = FX(0.1f);
    fx32  far         = FX(1000.0f);
    float fov         = 90.0f;
    fx32  aspect_ratio = FX((float)screen_height / (float)screen_width);
    fx32  fov_rad      = FX(1.0f / tanf(fov * 0.5f / 180.0f * 3.14159f));

    mat4x4 mat_proj;
    vec3d vec_camera;

    memset(&mat_proj, 0, sizeof(mat_proj));
    mat_proj.m[0][0] = MUL(aspect_ratio, fov_rad);
    mat_proj.m[1][1] = fov_rad;
    mat_proj.m[2][2] = DIV(far, (far - near));
    mat_proj.m[3][2] = DIV(MUL(-far, near), (far - near));
    mat_proj.m[2][3] = FX(1.0f);
    mat_proj.m[3][3] = FX(0.0f);

    mat4x4 mat_rot_z, mat_rot_x;
    memset(&mat_rot_z, 0, sizeof(mat_rot_z));
    memset(&mat_rot_x, 0, sizeof(mat_rot_x));

    // rotation Z
    mat_rot_z.m[0][0] = FX(cosf(theta));
    mat_rot_z.m[0][1] = FX(sinf(theta));
    mat_rot_z.m[1][0] = FX(-sinf(theta));
    mat_rot_z.m[1][1] = FX(cosf(theta));
    mat_rot_z.m[2][2] = FX(1.0f);
    mat_rot_z.m[3][3] = FX(1.0f);

    // rotation X
    mat_rot_x.m[0][0] = FX(1.0f);
    mat_rot_x.m[1][1] = FX(cosf(theta * 0.5f));
    mat_rot_x.m[1][2] = FX(sinf(theta * 0.5f));
    mat_rot_x.m[2][1] = FX(-sinf(theta * 0.5f));
    mat_rot_x.m[2][2] = FX(cosf(theta * 0.5f));
    mat_rot_x.m[3][3] = FX(1.0f);

    // draw triangles
    size_t nb_triangles = sizeof(cube_triangles) / sizeof(triangle);
    for (size_t i = 0; i < nb_triangles; ++i)
    {
        triangle * tri = &mesh_cube.tris[i];
        triangle   tri_projected, tri_translated, tri_rotated_z, tri_rotated_zx;

        // rotate in Z-axis
        multiply_matrix_vector(&tri->p[0], &tri_rotated_z.p[0], &mat_rot_z);
        multiply_matrix_vector(&tri->p[1], &tri_rotated_z.p[1], &mat_rot_z);
        multiply_matrix_vector(&tri->p[2], &tri_rotated_z.p[2], &mat_rot_z);

        // rotate in X-axis
        multiply_matrix_vector(&tri_rotated_z.p[0], &tri_rotated_zx.p[0], &mat_rot_x);
        multiply_matrix_vector(&tri_rotated_z.p[1], &tri_rotated_zx.p[1], &mat_rot_x);
        multiply_matrix_vector(&tri_rotated_z.p[2], &tri_rotated_zx.p[2], &mat_rot_x);

        // offset into the screen
        tri_translated        = tri_rotated_zx;
        tri_translated.p[0].z = tri_rotated_zx.p[0].z + FX(3.0f);
        tri_translated.p[1].z = tri_rotated_zx.p[1].z + FX(3.0f);
        tri_translated.p[2].z = tri_rotated_zx.p[2].z + FX(3.0f);

        // calculate the normal
        vec3d normal, line1, line2;
        line1.x = tri_translated.p[1].x - tri_translated.p[0].x;
        line1.y = tri_translated.p[1].y - tri_translated.p[0].y;
        line1.z = tri_translated.p[1].z - tri_translated.p[0].z;
        
        line2.x = tri_translated.p[2].x - tri_translated.p[0].x;
        line2.y = tri_translated.p[2].y - tri_translated.p[0].y;
        line2.z = tri_translated.p[2].z - tri_translated.p[0].z;

        normal.x = MUL(line1.y, line2.z) - MUL(line1.z, line2.y);
        normal.y = MUL(line1.z, line2.x) - MUL(line1.x, line2.z);
        normal.z = MUL(line1.x, line2.y) - MUL(line1.y, line2.x);

        fx32 l = SQRT(MUL(normal.x, normal.x) + MUL(normal.y, normal.y) + MUL(normal.z, normal.z));
        normal.x = DIV(normal.x, l);
        normal.y = DIV(normal.y, l);
        normal.z = DIV(normal.z, l);

        if (MUL(normal.x, (tri_translated.p[0].x - vec_camera.x)) +
            MUL(normal.y, (tri_translated.p[0].y - vec_camera.y)) +
            MUL(normal.z, (tri_translated.p[0].z - vec_camera.z)) < FX(0.0f))
        {
            // project triangles from 3D to 2D
            multiply_matrix_vector(&tri_translated.p[0], &tri_projected.p[0], &mat_proj);
            multiply_matrix_vector(&tri_translated.p[1], &tri_projected.p[1], &mat_proj);
            multiply_matrix_vector(&tri_translated.p[2], &tri_projected.p[2], &mat_proj);

            // scale into view
            tri_projected.p[0].x += FX(1.0f);
            tri_projected.p[0].y += FX(1.0f);
            tri_projected.p[1].x += FX(1.0f);
            tri_projected.p[1].y += FX(1.0f);
            tri_projected.p[2].x += FX(1.0f);
            tri_projected.p[2].y += FX(1.0f);

            fx32 w               = FX(0.5f * (float)screen_width);
            fx32 h               = FX(0.5f * (float)screen_height);
            tri_projected.p[0].x = MUL(tri_projected.p[0].x, w);
            tri_projected.p[0].y = MUL(tri_projected.p[0].y, h);
            tri_projected.p[1].x = MUL(tri_projected.p[1].x, w);
            tri_projected.p[1].y = MUL(tri_projected.p[1].y, h);
            tri_projected.p[2].x = MUL(tri_projected.p[2].x, w);
            tri_projected.p[2].y = MUL(tri_projected.p[2].y, h);

            // rasterize triangle
            pr_draw_triangle(FXI(tri_projected.p[0].x),
                                    FXI(tri_projected.p[0].y),
                                    FXI(tri_projected.p[1].x),
                                    FXI(tri_projected.p[1].y),
                                    FXI(tri_projected.p[2].x),
                                    FXI(tri_projected.p[2].y),
                                    0xf);
        }
    }
}