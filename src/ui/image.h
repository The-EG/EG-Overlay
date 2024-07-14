#pragma once
#include <lua.h>
#include "../lamath.h"

typedef struct ui_image_t ui_image_t;

void ui_image_init();
void ui_image_cleanup();

ui_image_t *ui_image_new_from_file(const char *path);
void ui_image_free(ui_image_t *image);

void ui_image_draw(ui_image_t *image, mat4f_t *proj, int x, int y, int width, int height, float saturation_f, float value_f);

void ui_image_lua_register_ui_funcs(lua_State *L);
ui_image_t *ui_image_from_lua(lua_State *L, int arg);
void ui_image_push_to_lua(ui_image_t *image, lua_State *L, int lua_managed);

void ui_image_size(ui_image_t *image, int *width, int *height);

int ui_image_height_for_width(ui_image_t *image, int width);
int ui_image_width_for_height(ui_image_t *image, int height);