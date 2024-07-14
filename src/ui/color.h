#pragma once
#include <stdint.h>

typedef uint32_t ui_color_t;

#define UI_COLOR_R_INT(color) ((color >> 24) & 0xFF)
#define UI_COLOR_G_INT(color) ((color >> 16) & 0xFF)
#define UI_COLOR_B_INT(color) ((color >>  8) & 0xFF)
#define UI_COLOR_A_INT(color) ( color        & 0xFF)

#define UI_COLOR_R(color) UI_COLOR_R_INT(color)/255.f
#define UI_COLOR_G(color) UI_COLOR_G_INT(color)/255.f
#define UI_COLOR_B(color) UI_COLOR_B_INT(color)/255.f
#define UI_COLOR_A(color) UI_COLOR_A_INT(color)/255.f
