#pragma once

#include "lamath.h"

void overlay_3d_init();
void overlay_3d_cleanup();

void overlay_3d_begin_frame(mat4f_t *view, mat4f_t *proj);
void overlay_3d_end_frame();