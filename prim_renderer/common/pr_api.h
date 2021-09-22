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
 * Primitive Renderer API
 * ------------------------------------------------------------
 */

#ifndef PR_API_H
#define PR_API_H

#include <stdbool.h>

void pr_init();
void pr_init_swap();
void pr_swap(bool is_vsync_enabled);
void pr_draw_line(int x0, int y0, int x1, int y1, int color);
void pr_draw_filled_triangle(int x0, int y0, int x1, int y1, int x2, int y2, int color);
void pr_draw_filled_rectangle(int x0, int y0, int x1, int y1, int color);
void pr_clear();
void pr_finish();

#endif
