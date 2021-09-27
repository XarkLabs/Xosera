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

void get_projection_matrix(mat4x4 * mat_proj)
{
    // projection matrix
    fx32  near        = FX(0.1f);
    fx32  far         = FX(1000.0f);
    fx32  aspect_ratio = FX((float)screen_height / (float)screen_width);
    float fov = 60.0f;
    fx32  fov_rad      = FX(1.0f / tanf(fov * 0.5f / 180.0f * 3.14159f));

    memset(mat_proj, 0, sizeof(mat4x4));
    mat_proj->m[0][0] = MUL(aspect_ratio, fov_rad);
    mat_proj->m[1][1] = fov_rad;
    mat_proj->m[2][2] = DIV(far, (far - near));
    mat_proj->m[3][2] = DIV(MUL(-far, near), (far - near));
    mat_proj->m[2][3] = FX(1.0f);
    mat_proj->m[3][3] = FX(0.0f);
}

void get_rotation_z_matrix(float theta, mat4x4 * mat_rot_z)
{
    memset(mat_rot_z, 0, sizeof(mat4x4));

    // rotation Z
    mat_rot_z->m[0][0] = FX(cosf(theta));
    mat_rot_z->m[0][1] = FX(sinf(theta));
    mat_rot_z->m[1][0] = FX(-sinf(theta));
    mat_rot_z->m[1][1] = FX(cosf(theta));
    mat_rot_z->m[2][2] = FX(1.0f);
    mat_rot_z->m[3][3] = FX(1.0f);
}

void get_rotation_x_matrix(float theta, mat4x4 * mat_rot_x)
{
    memset(mat_rot_x, 0, sizeof(mat4x4));

    // rotation X
    mat_rot_x->m[0][0] = FX(1.0f);
    mat_rot_x->m[1][1] = FX(cosf(theta * 0.5f));
    mat_rot_x->m[1][2] = FX(sinf(theta * 0.5f));
    mat_rot_x->m[2][1] = FX(-sinf(theta * 0.5f));
    mat_rot_x->m[2][2] = FX(cosf(theta * 0.5f));
    mat_rot_x->m[3][3] = FX(1.0f);
}

void draw_cube(mat4x4 * mat_proj, mat4x4 * mat_rot_z, mat4x4 * mat_rot_x, bool is_lighting_ena)
{
    mesh mesh_cube;
    mesh_cube.tris = &cube_triangles[0];

    vec3d vec_camera = {FX(0.0f), FX(0.0f), FX(0.0f)};

    // draw triangles
    size_t nb_triangles = sizeof(cube_triangles) / sizeof(triangle);
    for (size_t i = 0; i < nb_triangles; ++i)
    {
        triangle * tri = &mesh_cube.tris[i];
        triangle   tri_projected, tri_translated, tri_rotated_z, tri_rotated_zx;

        // rotate in Z-axis
        multiply_matrix_vector(&tri->p[0], &tri_rotated_z.p[0], mat_rot_z);
        multiply_matrix_vector(&tri->p[1], &tri_rotated_z.p[1], mat_rot_z);
        multiply_matrix_vector(&tri->p[2], &tri_rotated_z.p[2], mat_rot_z);

        // rotate in X-axis
        multiply_matrix_vector(&tri_rotated_z.p[0], &tri_rotated_zx.p[0], mat_rot_x);
        multiply_matrix_vector(&tri_rotated_z.p[1], &tri_rotated_zx.p[1], mat_rot_x);
        multiply_matrix_vector(&tri_rotated_z.p[2], &tri_rotated_zx.p[2], mat_rot_x);

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

        if (is_lighting_ena) {
            fx32 l = SQRT(MUL(normal.x, normal.x) + MUL(normal.y, normal.y) + MUL(normal.z, normal.z));
            normal.x = DIV(normal.x, l);
            normal.y = DIV(normal.y, l);
            normal.z = DIV(normal.z, l);
        }

        if (MUL(normal.x, (tri_translated.p[0].x - vec_camera.x)) +
            MUL(normal.y, (tri_translated.p[0].y - vec_camera.y)) +
            MUL(normal.z, (tri_translated.p[0].z - vec_camera.z)) < FX(0.0f))
        {
            fx32 dp = FX(1.0f);
            if (is_lighting_ena) {
                // illumination
                vec3d light_direction = {FX(0.0f), FX(0.0f), FX(-1.0f)};
                fx32 l = SQRT(MUL(light_direction.x, light_direction.x) + MUL(light_direction.y, light_direction.y) + MUL(light_direction.z, light_direction.z));
                light_direction.x = DIV(light_direction.x, l);
                light_direction.y = DIV(light_direction.y, l);
                light_direction.z = DIV(light_direction.z, l);

                dp = MUL(normal.x, light_direction.x) + MUL(normal.y, light_direction.y) + MUL(normal.z, light_direction.z);
            }
            tri_translated.col.x = dp;
            tri_translated.col.y = dp;
            tri_translated.col.z = dp;

            // project triangles from 3D to 2D
            multiply_matrix_vector(&tri_translated.p[0], &tri_projected.p[0], mat_proj);
            multiply_matrix_vector(&tri_translated.p[1], &tri_projected.p[1], mat_proj);
            multiply_matrix_vector(&tri_translated.p[2], &tri_projected.p[2], mat_proj);
            tri_projected.col = tri_translated.col;

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
            fx32 col = MUL(tri_projected.col.x, FX(255.0f));
            pr_draw_filled_triangle(INT(tri_projected.p[0].x),
                                    INT(tri_projected.p[0].y),
                                    INT(tri_projected.p[1].x),
                                    INT(tri_projected.p[1].y),
                                    INT(tri_projected.p[2].x),
                                    INT(tri_projected.p[2].y),
                                    INT(col));

            pr_draw_triangle(INT(tri_projected.p[0].x),
                                    INT(tri_projected.p[0].y),
                                    INT(tri_projected.p[1].x),
                                    INT(tri_projected.p[1].y),
                                    INT(tri_projected.p[2].x),
                                    INT(tri_projected.p[2].y),
                                    0);
        }
    }
}