#pragma once
#include "color.h"
#include <stdint.h>
#include "lamath.h"

void ui_rect_init();
void ui_rect_cleanup();

typedef struct {
    int x;
    int y;
    int width;
    int height;
    ui_color_t color;
} ui_rect_multi_t;

void ui_rect_draw(int x, int y, int width, int height, ui_color_t color, mat4f_t *proj);
void ui_rect_draw_multi(size_t count, ui_rect_multi_t *rects, mat4f_t *proj);
