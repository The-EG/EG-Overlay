#pragma once
#include "settings.h"
#include <windows.h>

void app_init(HINSTANCE hinst, int argc, char **argv);
void app_cleanup();

int app_run();

void app_get_framebuffer_size(int *width, int *height);

#define GET_APP_SETTING_INT(key, val) settings_get_int   (app_get_settings(), key, val)
#define GET_APP_SETTING_STR(key, val) settings_get_string(app_get_settings(), key, val)

settings_t *app_get_settings();

void app_setclipboard_text(const char *text);
char *app_getclipboard_text();

void app_exit();