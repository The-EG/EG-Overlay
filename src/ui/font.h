#pragma once
#include "color.h"
#include "../lamath.h"

typedef struct ui_font_t ui_font_t;

void ui_font_init();
void ui_font_cleanup();

ui_font_t *ui_font_get(const char *path, int size, int weight, int slant, int width);

void ui_font_render_text(ui_font_t *font, mat4f_t *proj, int x, int y, const char *text, size_t count, ui_color_t color);

uint32_t ui_font_get_text_width(ui_font_t *font, const char *text, int count);
uint32_t ui_font_get_text_height(ui_font_t *font);
uint32_t ui_font_get_line_spacing(ui_font_t *font);

uint32_t ui_font_get_index_of_width(ui_font_t *font, const char *text, int width);

int ui_font_get_text_wrap_indices(ui_font_t *font, const char *text, int width, int **wrap_indices);